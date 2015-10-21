#include <unistd.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>

#include "CodecEngine.h"
#include "HwCodecLib.h"
#include "Parse264Config.h"
#include "ShmemSet.h"
#include "CmmDriver.h"

#define WAIT_TIME_LENGTH	100

#define MAX_AUDIO_BUFFERSIZE	AVCODEC_MAX_AUDIO_FRAME_SIZE*3/2
#define DOUBLEDIV(X,Y)		((double)X/Y)
#define DIV(X,Y)			((X)/(Y))
#define MUL(X,Y)			((X)*(Y))

#define MAX_TRY_NUM		3
#define MAX_AV_DUR		AV_TIME_BASE

#define TRUE  1
#define FALSE 0

#define YUV_MIN_STREAM		4
#define STREAM_ES_HEADER_SIZE	4

#define SPS_PPS_NUM		10

#define MPEG4_VIDEO_SIZE		480*272
#define H264_VIDEO_SIZE			352*288

#define MAX_VIDEO_SIZE			720*480

#define HARD_CODEC_NUM		4

#define CACHE_FILE_CLEAN 	"sync && echo 3 > /proc/sys/vm/drop_caches"

#define CLEAN_SYSTEM_CACHE()				\
{								\
	FILE *fp = popen(CACHE_FILE_CLEAN,"w");	\
	pclose(fp);						\
}								\


const AVRational avr = {1, AV_TIME_BASE};

/*************************************[视频解码模块]************************************/

typedef struct StreamOcx
{
	//视频相关变量
	AVFormatContext *m_pFmtictx;
	AVStream *m_pAudioSt;
	AVStream *m_pVideoSt;

	AVPacket m_bAVPacket;
	
	//解码流索引
	int m_bAudioIndex;
	int m_bVideoIndex;

	//音视频参数
	AudioParam m_bAudioParam;
	VideoParam m_bVideoParam;

	/*******PrivateUse******/

	//是否存在音视频
	int m_bAudioOpened;
	int m_bVideoOpened;

	//H264相关，主要处理mkv/flv/mp4容器的视频的sps,pps的初始化问题
	int m_bMinSize;
	int m_b264Init;
	int m_b264Fill;
	int m_b264Fail;

	//MPEG4相关，主要处理非法的MPEG4数据流
	int m_bMpeg4Init;
	int m_bMpeg4Fill;

	//音频流存放空间
	DECLARE_ALIGNED(16,uint8_t,m_pAudioStreamBuffer)[MAX_AUDIO_BUFFERSIZE];

	uint8_t *m_pHwVideoStreamBuffer;

	//音频和视频大小
	long m_bAudioBufferSize;
	long m_bVideoBufferSize;

	//是否启用硬件解压
	int m_bHEnable;

	//硬解压的句柄
	void *m_bOpenHWHandle;
	//硬解码当前使用的视频格式
	int m_bHWFormat;

}StreamOcx;

static StreamOcx Ocx;

static int UseOcxNum = 0;

static int HWCodecTag[] = {CODEC_ID_H263,CODEC_ID_MPEG4,CODEC_ID_H264,CODEC_ID_VC1};

static int HWCodecNum = HARD_CODEC_NUM;

//检测是否可以使用硬件加速
static int CheckEnableHWCodec(int codec_id)
{
	int i = 0;

	for(i = 0; i < HWCodecNum; i++)
	{
		if(codec_id == HWCodecTag[i])
			return TRUE;
	}

	return FALSE;
}

//判断是否使用了硬加速
static int IsHWEnable(int codec_id)
{
	if(codec_id < 0)
		return FALSE;

	return CheckEnableHWCodec(codec_id);
}

//得到硬加速需要的格式
static int GetHWCodecType(int codec_id)
{
	switch(codec_id)
	{
	case CODEC_ID_H263:
		av_log(NULL, AV_LOG_ERROR,"Hard Codec: H263 FORMAT!\n");
		return SSBSIPMFCDEC_H263;
	case CODEC_ID_MPEG4:
		av_log(NULL, AV_LOG_ERROR,"Hard Codec: MPEG4 FORMAT!\n");
		return SSBSIPMFCDEC_MPEG4;
	case CODEC_ID_H264:
		av_log(NULL, AV_LOG_ERROR,"Hard Codec: H264 FORMAT!\n");
		return SSBSIPMFCDEC_H264;
	case CODEC_ID_VC1:
		av_log(NULL, AV_LOG_ERROR,"Hard Codec: VC1 FORMAT!\n");
		return SSBSIPMFCDEC_VC1;
	}

	return SSBSIPMFCDEC_NONE;
}

static int CheckVideoParams(int format,VideoParam *vp)
{
	switch(format)
	{
	case SSBSIPMFCDEC_H263:
	case SSBSIPMFCDEC_MPEG4:
		return (MUL(vp->m_bVideoWidth,vp->m_bVideoHeight) > MPEG4_VIDEO_SIZE);
	case SSBSIPMFCDEC_H264:
	case SSBSIPMFCDEC_VC1:
		return (MUL(vp->m_bVideoWidth,vp->m_bVideoHeight) > H264_VIDEO_SIZE);
	case -1:
		return (MUL(vp->m_bVideoWidth,vp->m_bVideoHeight) > MAX_VIDEO_SIZE);
	}

	return 0;
}

static int TestStreamH264ES(int fill,uint8_t* buf)
{
	if(!buf || fill <= 0)
		return -1;

	int tag = 1;

	int i = 0;

	for(i = 0; i < fill - 1; i++)
		tag = (buf[i] == 0x00);

	tag = (tag && buf[fill-1] == 0x01);

	return tag;
}

//得到MPEG4的格式布局，如果标准为00 00 00 BX
static int GetStreamMPEG4Fill(uint8_t* buf,int size)
{
	if(size <= 4)
		return -1;

	int i = 0,value = 0;

	for(i = 0; i < 4; i++)
	{
		value = buf[i]&0xf0;
		if(value == 0xb0)
			return (i+1);
	}

	return -1;
}
//判断是否为P帧
static int TestStreamMPEG4P(int fill,uint8_t* buf)
{
	int val = buf[fill - 1]&0xff;

	return (val == 0xb6);
}
//判断该数据流是否合法，在BX后的第一个字节中的高两位如果等于00 01 10为合法，等于11为非法
static int TestStreamMPEG4(int fill,uint8_t* buf)
{
	int val = buf[fill]&0xff;
	val = (val >> 6)&0x03;

	return val;
}

static int InitCodec(int stream_index)
{
	AVFormatContext *ic = Ocx.m_pFmtictx;
	AVCodecContext *enc;
      AVCodec *codec;

      if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
          return -1;

      enc = ic->streams[stream_index]->codec;

      if (enc->codec_type == AVMEDIA_TYPE_AUDIO) {
          if (enc->channels > 0) {
              enc->request_channels = FFMIN(2, enc->channels);
          } else {
              enc->request_channels = 2;
             }	
        }

      codec = avcodec_find_decoder(enc->codec_id);

	enc->skip_frame = AVDISCARD_BIDIR;//AVDISCARD_DEFAULT;
	enc->skip_idct = AVDISCARD_BIDIR;//AVDISCARD_DEFAULT;
	enc->skip_loop_filter = AVDISCARD_BIDIR;//AVDISCARD_DEFAULT;

	if(!codec || avcodec_open(enc,codec) < 0)
		return -1;

	if (enc->codec_type == AVMEDIA_TYPE_AUDIO)
	{
		Ocx.m_pAudioSt = ic->streams[stream_index];

		Ocx.m_bAudioParam.m_bAudioFreq = enc->sample_rate;
		Ocx.m_bAudioParam.m_bAudioChannel = enc->request_channels;
		Ocx.m_bAudioParam.m_bAudioFormat = enc->sample_fmt;
		Ocx.m_bAudioBufferSize = MAX_AUDIO_BUFFERSIZE;
	}

	if(enc->codec_type == AVMEDIA_TYPE_VIDEO)
	{
		Ocx.m_pVideoSt = ic->streams[stream_index];

		Ocx.m_bVideoParam.m_bVideoWidth = enc->width;
		Ocx.m_bVideoParam.m_bVideoHeight = enc->height;
		Ocx.m_bVideoParam.m_bVideoPixFormat = enc->pix_fmt;
		Ocx.m_bVideoParam.m_bVideoBitRate = enc->bit_rate;
		Ocx.m_bVideoParam.m_bVideoFrameRate = Ocx.m_pVideoSt->r_frame_rate.num/Ocx.m_pVideoSt->r_frame_rate.den;

		Ocx.m_bVideoBufferSize = avpicture_get_size(PIX_FMT_YUV420P,Ocx.m_bVideoParam.m_bVideoWidth,Ocx.m_bVideoParam.m_bVideoHeight);
	}

	return 0;
}

static int UnitCodec(int stream_index)
{
	AVFormatContext *ic = Ocx.m_pFmtictx;
	AVCodecContext *enc;

      if (stream_index < 0 || stream_index >= (int)ic->nb_streams)
          return -1;

      enc = ic->streams[stream_index]->codec;

	avcodec_close(enc);

      switch(enc->codec_type) 
	{
      case AVMEDIA_TYPE_AUDIO:
        	Ocx.m_pAudioSt = NULL;
        	Ocx.m_bAudioIndex = -1;

		Ocx.m_bAudioParam.m_bAudioFreq = 0;
		Ocx.m_bAudioParam.m_bAudioChannel = 0;
		Ocx.m_bAudioParam.m_bAudioFormat = 0;

		Ocx.m_bAudioBufferSize = 0;
        break;
      case AVMEDIA_TYPE_VIDEO:
        	Ocx.m_pVideoSt = NULL;
       	Ocx.m_bVideoIndex = -1;

		Ocx.m_bVideoParam.m_bVideoWidth = 0;
		Ocx.m_bVideoParam.m_bVideoHeight = 0;
		Ocx.m_bVideoParam.m_bVideoPixFormat = PIX_FMT_NONE;
		Ocx.m_bVideoParam.m_bVideoBitRate = 0;
		Ocx.m_bVideoParam.m_bVideoFrameRate = 0;

		if(Ocx.m_pHwVideoStreamBuffer && !Ocx.m_bHEnable)
			CMMFreeMemeory(Ocx.m_pHwVideoStreamBuffer);
        break;
	}

	return 0;
}

int64_t GetTimeHere()
{
	return av_gettime();
}

int usSleep(long us)
{
	struct timeval tv;
	tv.tv_sec = 0;
	tv.tv_usec = us;
	return select(0,NULL,NULL,NULL,&tv);
}

void InitDriver()
{
	avcodec_register_all();
	av_register_all();
}

int CreateOcxByFile(const char *filename)
{
	if(UseOcxNum != 0)
		return -1;

	AVFormatParameters params, *ap = &params;
	AVFormatContext *ic;

	memset(ap,0,sizeof(*ap));

	int audio_index = -1,video_index = -1;

	ap->time_base.den = 1;
	ap->time_base.num = 25;

	int err = av_open_input_file(&ic,filename,NULL,0, ap);

	if(err < 0)
	{
		UseOcxNum = 0;
		return -2;
	}

	err = av_find_stream_info(ic);

	if(err < 0)
	{
		UseOcxNum = 0;
		return -3;
	}

	if(ic->pb)
		ic->pb->eof_reached = 0;

	unsigned int i = 0;

	for(i = 0; i < ic->nb_streams; i++) 
	{
		AVCodecContext *enc = ic->streams[i]->codec;

		switch(enc->codec_type) 
		{
		case AVMEDIA_TYPE_AUDIO:
				audio_index = i;
			break;
		case AVMEDIA_TYPE_VIDEO:
				video_index = i;
			break;
		default:
			break;
        	}
	}

	if(audio_index >= 0 || video_index >= 0)
	{
		memset(&Ocx,0,sizeof(StreamOcx));

		Ocx.m_pFmtictx = ic;

		int audiotag = -1,videotag = -1;

		if(audio_index >= 0)
			audiotag = InitCodec(audio_index);

		if(video_index >= 0)
			videotag = InitCodec(video_index);

		Ocx.m_bAudioIndex = audio_index;
		Ocx.m_bVideoIndex = video_index;

		Ocx.m_bAudioOpened = audiotag >= 0 ? TRUE:FALSE;
		Ocx.m_bVideoOpened = videotag >= 0 ? TRUE:FALSE;

		if(Ocx.m_bVideoOpened && CheckVideoParams(-1,&Ocx.m_bVideoParam))
		{
			UseOcxNum = 0;
			CloseOcxByOCX(Ocx);
			return -4;
		}

		if(Ocx.m_bVideoOpened)
		{
			int codec_id = Ocx.m_pFmtictx->streams[Ocx.m_bVideoIndex]->codec->codec_id;

			Ocx.m_bHWFormat = GetHWCodecType(codec_id);
			Ocx.m_bHEnable = IsHWEnable(codec_id) && CheckVideoParams(Ocx.m_bHWFormat,&Ocx.m_bVideoParam);

			if(Ocx.m_bHEnable)
			{
				Ocx.m_bOpenHWHandle = HwDecodeInit(Ocx.m_bHWFormat);

				if(!Ocx.m_bOpenHWHandle)
				{
					Ocx.m_bHWFormat = SSBSIPMFCDEC_NONE;
					Ocx.m_bHEnable = FALSE;
				}
				else
					Ocx.m_pHwVideoStreamBuffer = (uint8_t*)HwDecodeInBuffer(Ocx.m_bOpenHWHandle,Ocx.m_bVideoBufferSize);
							
			}
			else
				Ocx.m_bHWFormat = SSBSIPMFCDEC_NONE;
		}

		//硬解压初始化失败
		if(!Ocx.m_pHwVideoStreamBuffer)
		{
			HwDecodeDeInit(Ocx.m_bOpenHWHandle);
			Ocx.m_bOpenHWHandle = NULL;
			Ocx.m_bHEnable = FALSE;
			Ocx.m_bHWFormat = SSBSIPMFCDEC_NONE;

			Ocx.m_pHwVideoStreamBuffer = (uint8_t*)CMMAllocMemory(Ocx.m_bVideoBufferSize);

			if(!Ocx.m_pHwVideoStreamBuffer)
			{
				UseOcxNum = 0;
				CloseOcxByOCX(Ocx);
				return -5;
			}
		}
		else//成功
		{
			Ocx.m_bMinSize = YUV_MIN_STREAM;

			Ocx.m_b264Init = 0;
			Ocx.m_b264Fill = 4;
			Ocx.m_b264Fail = 0;

			Ocx.m_bMpeg4Init = 0;
			Ocx.m_bMpeg4Fill = 4;
		}

		UseOcxNum = 1;

		return 0;
	}

	return -1;
}

void *ExtractStream()
{
	if(UseOcxNum != 1)
		return NULL;

	AVPacket *pkt = &Ocx.m_bAVPacket;
	memset(pkt,0,sizeof(AVPacket));

	int ret = -1,trynum = 0;

again:

	ret = av_read_frame(Ocx.m_pFmtictx,pkt);

	if(ret < 0)
	{
		if(ret == AVERROR_EOF || url_feof(Ocx.m_pFmtictx->pb))
			goto ExitOver;

		else
		{	
			usSleep(WAIT_TIME_LENGTH);
			
			if(trynum < MAX_TRY_NUM)
			{
				av_log(NULL, AV_LOG_ERROR,"Try again......\n");	
				trynum++;
				goto again;
			}
			else
			{
				av_log(NULL, AV_LOG_ERROR,"Try fail......\n");	
				goto ExitOver;
			}
		}
	}

	if(av_dup_packet(pkt) < 0)
	{
ExitOver:
		av_free_packet(pkt);

		pkt = NULL;
	}

	return pkt;
}

int GetFrameSize(enum PixelFormat pixfmt,int width,int height)
{
	return avpicture_get_size(pixfmt,width,height);
}

int SeekPositon(double pts,int flush)
{
	if(UseOcxNum != 1)
		return -1;

	int64_t seek_target = (int64_t)pts*AV_TIME_BASE;
	int index = -1;

#if 0
	if(Ocx.m_bAudioIndex >= 0)
	{
		seek_target = pts/av_q2d(Ocx.m_pAudioSt->time_base);
		index = Ocx.m_bAudioIndex;
	}

	if(Ocx.m_bVideoIndex >= 0)
	{
		seek_target = pts/av_q2d(Ocx.m_pVideoSt->time_base);
		index = Ocx.m_bVideoIndex;
	}
	int ret = av_seek_frame(Ocx.m_pFmtictx, index, seek_target, AVSEEK_FLAG_BACKWARD);
#endif

	int ret = avformat_seek_file(Ocx.m_pFmtictx, index, INT64_MIN, seek_target, INT64_MAX, AVSEEK_FLAG_BACKWARD);

	if(flush)
	{
		FlushBuffers(AUDIO);
		FlushBuffers(VIDEO);
	}

	if(ret < 0)
		return -1;

	return 0;
}

int GetEof()
{
	if(UseOcxNum != 1)
		return -1;

	return url_feof(Ocx.m_pFmtictx->pb) != 0;
}

int GetError()
{
	if(UseOcxNum != 1)
		return -1;

	return url_ferror(Ocx.m_pFmtictx->pb) != 0;
}

int IsExistAuido()
{
	if(UseOcxNum != 1)
		return -1;

	return Ocx.m_bAudioOpened;
}

int IsExistVideo()
{
	if(UseOcxNum != 1)
		return -1;

	return Ocx.m_bVideoOpened;
}


void *UseAlloc(int size)
{
	if(size <= 0)
		return NULL;

	return av_malloc(size);
}


void FreeAlloc(void *ptr)
{
	if(ptr == NULL)
		return;

	av_free(ptr);
}

int IsHardWareAccel()
{
	if(UseOcxNum != 1)
		return -1;

	return Ocx.m_bHEnable;
}

AudioParam GetOcxAudioParam()
{
	AudioParam AP;
	memset(&AP,0,sizeof(AudioParam));

	if(UseOcxNum != 1)
		return AP;

	AP = Ocx.m_bAudioParam;

	return AP;
}

VideoParam GetOcxVideoParam(void *ocx)
{
	VideoParam AP;
	memset(&AP,0,sizeof(AudioParam));

	if(UseOcxNum != 1)
		return AP;

	AP = Ocx.m_bVideoParam;

	return AP;
}

void FlushBuffers(int type)
{
	if(UseOcxNum != 1)
		return;

	switch(type)
	{
	case AUDIO:
		if(Ocx.m_bAudioIndex >= 0)
			avcodec_flush_buffers(Ocx.m_pFmtictx->streams[Ocx.m_bAudioIndex]->codec);
		break;
	case VIDEO:
		if(Ocx.m_bVideoIndex >= 0)
			avcodec_flush_buffers(Ocx.m_pFmtictx->streams[Ocx.m_bVideoIndex]->codec);
		break;
	}
}

double GetDuration()
{
	if(UseOcxNum != 1 || !Ocx.m_pFmtictx)
		return 0;

	return Ocx.m_pFmtictx->duration/AV_TIME_BASE;
}

int GetPacketSize()
{
	return sizeof(AVPacket);
}

void *DecodeVideoYUVStream(void* pkt,long *size)
{
	if(UseOcxNum != 1|| !pkt)
		return NULL;

	AVPacket* pkt1 = (AVPacket*)pkt;

	if(!Ocx.m_bVideoOpened || pkt1->stream_index != Ocx.m_bVideoIndex)
	{
		av_free_packet(pkt1);
		return NULL;
	}

	long framesize = 0;
	//硬解压过程
	void *VideoBuffer = NULL;
	int GotPicture = 0;

	if(size)
		*size = framesize;

	if(Ocx.m_bOpenHWHandle && Ocx.m_bHEnable)
	{	
		if(Ocx.m_pHwVideoStreamBuffer && (pkt1->size >= Ocx.m_bMinSize))
		{	
			AVCodecContext* pb = Ocx.m_pFmtictx->streams[Ocx.m_bVideoIndex]->codec;
			
			if(!Ocx.m_b264Init && Ocx.m_bHWFormat == SSBSIPMFCDEC_H264 && pb->extradata_size > 7)	
			{															
				if(TestStreamH264ES(4,pkt1->data) < 1)								
				{																	
					Ocx.m_b264Fill = (pb->extradata[STREAM_ES_HEADER_SIZE]&0x03) + 1;			
					ParseH264SPSPPS(pb->extradata,pb->extradata_size,Ocx.m_pHwVideoStreamBuffer,HwDecodeExe,Ocx.m_bOpenHWHandle);		
					Ocx.m_b264Fail = (TestStreamH264ES(Ocx.m_b264Fill,pkt1->data) < 1);		
				}
				else
				{
					if(pb->extradata_size > 0)
					{
						memcpy(Ocx.m_pHwVideoStreamBuffer,pb->extradata,pb->extradata_size);
						HwDecodeExe(Ocx.m_bOpenHWHandle, pb->extradata_size);
					}
				}
														
				Ocx.m_b264Init = 1;											
			}

			if(!Ocx.m_bMpeg4Init && Ocx.m_bHWFormat == SSBSIPMFCDEC_MPEG4)
			{
				Ocx.m_bMpeg4Fill = GetStreamMPEG4Fill(pkt1->data,pkt1->size);

				if(Ocx.m_bMpeg4Fill < 1)
					goto SoftDec;

				Ocx.m_bMpeg4Init = 1;
			}

			int offsetHw = 0;
			int offsetPk = 0;

			if(Ocx.m_bHWFormat == SSBSIPMFCDEC_H264 && Ocx.m_b264Fail)
			{		

				int len = ParseDataSEISPSPPSConfig(Ocx.m_b264Fill,pkt1->data,pkt1->size);

				if(len == -1)
				{
					av_free_packet(pkt1);
					return NULL;
				}

				int value = pkt1->data[len]&0x1f;

				if(value != 1 && value != 5)
				{
					av_free_packet(pkt1);
					return NULL;				
				}

				offsetHw = STREAM_ES_HEADER_SIZE;
				offsetPk = len;
							
				Ocx.m_pHwVideoStreamBuffer[0] = 0x00;	
				Ocx.m_pHwVideoStreamBuffer[1] = 0x00;	
				Ocx.m_pHwVideoStreamBuffer[2] = 0x00;	
				Ocx.m_pHwVideoStreamBuffer[3] = 0x01;			
			}

			if(Ocx.m_bHWFormat == SSBSIPMFCDEC_MPEG4 && Ocx.m_bMpeg4Fill < STREAM_ES_HEADER_SIZE)
			{
				if(Ocx.m_bMpeg4Fill == 1)
				{
					Ocx.m_pHwVideoStreamBuffer[0] = 0x00;	
					Ocx.m_pHwVideoStreamBuffer[1] = 0x00;	
					Ocx.m_pHwVideoStreamBuffer[2] = 0x01;

					offsetHw = 3;		
				}
				else if(Ocx.m_bMpeg4Fill == 2)
				{
					pkt1->data[0] = 0x01;
					Ocx.m_pHwVideoStreamBuffer[0] = 0x00;	
					Ocx.m_pHwVideoStreamBuffer[1] = 0x00;

					offsetHw = 2;
				}
				else if(Ocx.m_bMpeg4Fill == 3)
				{
					pkt1->data[1] = 0x01;
					Ocx.m_pHwVideoStreamBuffer[0] = 0x00;

					offsetHw = 1;
				}
			}		

			memcpy(Ocx.m_pHwVideoStreamBuffer + offsetHw,pkt1->data + offsetPk,pkt1->size - offsetPk);

			if(Ocx.m_bHWFormat == SSBSIPMFCDEC_MPEG4 && TestStreamMPEG4P(Ocx.m_bMpeg4Fill,Ocx.m_pHwVideoStreamBuffer))
			{
				int value = TestStreamMPEG4(Ocx.m_bMpeg4Fill,Ocx.m_pHwVideoStreamBuffer);

				//当得出是VOP数据时，判断该数据是否为B帧，如果是则直接丢弃
				if(value == 3 || value == 2)
				{
					av_free_packet(pkt1);
					return NULL;				
				}
			}

			if(HwDecodeExe(Ocx.m_bOpenHWHandle, pkt1->size + offsetHw - offsetPk) >= 0)
			{
				VideoBuffer = (void*)HwDecodeOutBuffer(Ocx.m_bOpenHWHandle,&framesize);
			}
			else
			{
				HwDecodeDeInit(Ocx.m_bOpenHWHandle);
				Ocx.m_bOpenHWHandle = NULL;
				Ocx.m_bHEnable = FALSE;
				Ocx.m_bHWFormat = SSBSIPMFCDEC_NONE;
				Ocx.m_pHwVideoStreamBuffer = (uint8_t*)CMMAllocMemory(Ocx.m_bVideoBufferSize);

				if(!Ocx.m_pHwVideoStreamBuffer)
				{
					av_free_packet(pkt1);
					return NULL;
				}

				goto SoftDec;
			}
		}
	}
	else
SoftDec:
	{
		AVFrame *AVFrameUse = avcodec_alloc_frame();
		void *SrcData = pkt1->data;

		avcodec_decode_video2(Ocx.m_pVideoSt->codec, AVFrameUse, &GotPicture, pkt1);

		if (GotPicture != 0)
		{
			avpicture_layout((AVPicture*)AVFrameUse, Ocx.m_bVideoParam.m_bVideoPixFormat, 
						Ocx.m_bVideoParam.m_bVideoWidth, Ocx.m_bVideoParam.m_bVideoHeight, Ocx.m_pHwVideoStreamBuffer, Ocx.m_bVideoBufferSize);
			framesize = Ocx.m_bVideoBufferSize;
			VideoBuffer = (void*)Ocx.m_pHwVideoStreamBuffer;
		}


		pkt1->data = SrcData;
		av_free(AVFrameUse);
	}

	if(size)
		*size = framesize;

	av_free_packet(pkt1);
	
	return VideoBuffer;
}

void *DecodeAudioStream(void* pkt,long *size)
{
	if(UseOcxNum != 1 || !pkt)
		return NULL;

	AVPacket* pkt1 = (AVPacket*)pkt;

	if(!Ocx.m_bAudioOpened || pkt1->stream_index != Ocx.m_bAudioIndex)
	{
		av_free_packet(pkt1);
		return NULL;
	}

	long framesize = 0;

	int	DecodedSize = -1, DataSize = Ocx.m_bAudioBufferSize;
	uint8_t* AudioBuffer = Ocx.m_pAudioStreamBuffer;
	void *SrcData = pkt1->data;

	memset(Ocx.m_pAudioStreamBuffer,0,DataSize);

	while(pkt1->size > 0)
	{
		DecodedSize = avcodec_decode_audio3(Ocx.m_pAudioSt->codec, (int16_t *)AudioBuffer, &DataSize, pkt1);

		if(DecodedSize < 0)
		{
			pkt1->size = 0;
			break;
		}

		pkt1->size -= DecodedSize;
		pkt1->data += DecodedSize;
		

		if(DataSize < 0)
			continue;

		framesize += DataSize;
		AudioBuffer += DataSize;
	}

	if(size)
		*size = framesize;

	pkt1->data = SrcData;

	av_free_packet(pkt1);

	return (void*)Ocx.m_pAudioStreamBuffer;
}

PacketCtx GetPacketCtx(void *pkt)
{
	PacketCtx pctx;
	memset(&pctx,0,sizeof(PacketCtx));

	if(!pkt || UseOcxNum != 1)
		return pctx;

	AVPacket* pkt1  = (AVPacket*)pkt;

	pctx.adts = pkt1->dts;
	pctx.apts = pkt1->pts;

	double time_base = 0;

	if(Ocx.m_bAudioOpened && pkt1->stream_index == Ocx.m_bAudioIndex)
	{
		time_base = av_q2d(Ocx.m_pAudioSt->time_base);
		pctx.type = AUDIO;
	}

	else if(Ocx.m_bVideoOpened && pkt1->stream_index == Ocx.m_bVideoIndex)
	{
		time_base = av_q2d(Ocx.m_pVideoSt->time_base);
		pctx.type = VIDEO;
	}
	else
		pctx.type = NONE;

	pctx.dts = time_base*pkt1->dts;
	pctx.pts = time_base*pkt1->pts;

	pctx.size = pkt1->size;
	pctx.index = pkt1->stream_index;

	return pctx;

}

int GetPacketAttr(void *pkt)
{
	if(!pkt || UseOcxNum != 1)
		return NONE;

	AVPacket* pkt1  = (AVPacket*)pkt;

	if(Ocx.m_bAudioOpened && pkt1->stream_index == Ocx.m_bAudioIndex)
		return AUDIO;
	else if(Ocx.m_bVideoOpened && pkt1->stream_index == Ocx.m_bVideoIndex)
		return VIDEO;

	return NONE;
}

void DestroyPacket(void* pkt)
{
	if(!pkt || UseOcxNum != 1)
		return;

	AVPacket *data = (AVPacket*)pkt;

	av_free_packet(data);
}

double GetTimeBase(int type)
{
	if(UseOcxNum != 1)
		return DOUBLEDIV(1,MAX_AV_DUR);;

	double timebase = 0;

	if(type == AUDIO && Ocx.m_pAudioSt)
		timebase = av_q2d(Ocx.m_pAudioSt->time_base);
	else if(type == VIDEO && Ocx.m_pVideoSt)
		timebase = av_q2d(Ocx.m_pVideoSt->time_base);

	if(timebase == 0)
		timebase = DOUBLEDIV(1,MAX_AV_DUR);

	return timebase;
}

//显示信息
void PrintMessage(const char *fmt,...)
{
	va_list vl;
	va_start(vl,fmt);
	av_log(NULL, AV_LOG_ERROR,fmt,vl);
	va_end(vl);
}

void CloseOcxByOCX(void *ocx)
{
	if(UseOcxNum != 1)
		return;

	if(Ocx.m_bAudioOpened && Ocx.m_bAudioIndex >= 0)
		UnitCodec(Ocx.m_bAudioIndex);

	if(Ocx.m_bVideoOpened && Ocx.m_bVideoIndex >= 0)
		UnitCodec(Ocx.m_bVideoIndex);

	if(Ocx.m_bHEnable)
	{
		if(Ocx.m_bOpenHWHandle)
		{
			HwDecodeDeInit(Ocx.m_bOpenHWHandle);
			Ocx.m_bOpenHWHandle = NULL;
			Ocx.m_bHWFormat = SSBSIPMFCDEC_NONE;
		}

		Ocx.m_bHEnable = FALSE;
	}

	av_close_input_file(Ocx.m_pFmtictx);
	memset(&Ocx,0,sizeof(StreamOcx));
	CLEAN_SYSTEM_CACHE();
	UseOcxNum = 0;
}

