#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <math.h>

#include "PlayerEngine.h"
#include "CodecEngine.h"
#include "HwScale.h"
#include "ShmemSet.h"
#include "QueueEngine.h"

//音频队列最大长度，用户根据情况可修改
#define CACHE_AUIDO_BUFFER_NUM	25
//视频队列最大长度，用户根据情况可修改
#define CACHE_VIDEO_BUFFER_NUM	25
//解码图片队列最大长度，用户根据情况可修改
#define CACHE_PICTURE_BUFFER_NUM	5

//等待时间10ms
#define DEFAULT_AV_WAIT			10000

//没有引入对应的头文件，所以自己定义了TRUE和FALSE
#define FALSE	0
#define TRUE	1

//默认的图片格式
#define DEFAULT_PIXFORMAT	RGB16
//默认解码后的图片高度和宽度
#define DEFAULT_WIDTH		480
#define DEFAULT_HEIGHT		272

//无时间戳的值
#define AV_NOPTS_VALUE        INT64_C(0x8000000000000000)

//标准时间的基准
#define AV_TIME_BASE		1000000

//播放时间与播放时长误差计算
#define AV_DIFF_DURATION(X,Y)	(X-Y)

//设置时差阈值
#define AV_NOSYNC_THRESHOLD 	0.08

//设置时差上限阈值
#define AV_SYNC_THRESHOLD 	0.01

/*
整体思路是：读取包进程对音频和视频分而治之，若是音频不解包直接送入队列
，若是视频解包后直接送入队列
*/


//内部命令
enum {
	NONE_CMD = -1,
	PAUSE_CMD,			//暂停命令	
	PLAY_CMD,			//播放命令（启动后的默认命令）
	STOP_CMD			//停止命令
};

//基准选项
enum {
	AUDIO_CLOCK,		//以时间为基准
	VIDEO_CLOCK			//以视频为基准
};

//队列状态标志
enum {
	QUEUE_ERROR = -1,		//队列发生不可预知的内存错误
	QUEUE_EMPTY = -2,		//队列为空
	QUEUE_FULL	= -3		//队列已满
};

//使用时，只有视频时整个包的内容都有效，音频只有数据缓冲有效

typedef struct AvParam {
	int pixFormat;		//目标格式
	int width;			//视频高度
	int height;			//视频宽度
	CallBack callFunc;	//回调函数
	void *arg;			//用户数据
	pthread_mutex_t mutex;  //防止用户输入同时更改状态的参数，比如状态和命令
}AvParam;

typedef struct AvManager {
	int already;		//是否准备完毕

	int playAudio;		//播放音频
	int playVideo;		//播放视频

	int masterClkByAV;	//主要时间戳标记－－－》以音频或视频为主
	double delayWithoutAudioTime;	//没有音频时，每帧视频的等待时间

	double audioClock;	//记录当前音频时间
	double videoClock;	//记录当前视频时间

	double audioTimeBase;	//音频时间基准
	double videoTimeBase;	//视频时间基准

	int64_t audioDelay;	//声音暂停

	double playerClock;	//播放音视频的绝对时间

	double duration;		//视频的播放长度，单位秒
	double audioBasePara;	//该变量主要用于保留声音与频率的乘积
	
	int avCommand;		//视频命令
	int avState;		//视频状态
	int autoEnd;		//视频是否自动停止
	int avSeek;			//记录当使用SEEK命令时，其他线程所采取动作的标记
	double avPos;		//用户输入的快进和快退距离

	AudioParam ap;		//音频参数
	VideoParam vp;		//视频参数

	int managerEnd;		//管理停止标记，即停止管理，意味视频结束

	pthread_t readAvEngine;	//读取和分离解码的线程
	pthread_t renderAudio;  //播放声音的线程
	pthread_t renderVideo;  //显示视频线程
	pthread_t synProcess;	//同步线程

	AvParam  avParams;	//记录视频的必要参数

	QueueBuffer	*MediaAudio;//音频队列
	QueueBuffer *MediaVideo;//视频队列

	QueueBuffer *MediaPicture;//解码图片队列

	int ManagerOver;		//播放彻底结束

	double last_audio_pts;	//记录当前音频PTS
	double last_video_pts;	//记录当前视频PTS

	double delay_timer;	//总延时
	double last_delay;	//上次延时
	double last_pts;
}AvManager;

static AvManager Manager;
static pthread_mutex_t actionMutex = PTHREAD_MUTEX_INITIALIZER;	//保护各种动作

static __inline double AVTimeToSecond(int64_t pts,double timebase)
{
	return pts*timebase;
}

static __inline int64_t AVTimeToPts(double time,double timebase)
{
	return (int64_t)(time/timebase);
}


//音频已满
static int AudioFlow(AvManager *Manager)
{
	if(Manager->playAudio)
		if(ParseQueueBufferLength(Manager->MediaAudio) >= CACHE_AUIDO_BUFFER_NUM)
			return QUEUE_FULL;

	return QUEUE_ERROR;	
}

//视频队列已满
static int VideoFlow(AvManager *Manager)
{
	if(Manager->playVideo)
	  	if(ParseQueueBufferLength(Manager->MediaVideo) >= CACHE_VIDEO_BUFFER_NUM)
			return QUEUE_FULL;

	return QUEUE_ERROR;
}

//队列是否已满
static int QueueIsFull(AvManager *Manager)
{
	int ret1 = AudioFlow(Manager);
	int ret2 = VideoFlow(Manager);

	return (ret1 == QUEUE_FULL || ret2 == QUEUE_FULL);
}

//队列发送，分为音频队列和视频队列发送
static int SendDataToAvQueue(AvManager *Manager,void *data)
{
	if(!Manager || !data)
		return QUEUE_ERROR;

	int type = GetPacketAttr(data);

	MediaPacket *mpkt = (MediaPacket*)data;

	if(Manager->masterClkByAV == NONE  && type != NONE)
	{
		Manager->masterClkByAV = type;

		switch(type)
		{
		case AUDIO:
			printf("Auido first!\n");
			break;
		case VIDEO:
			printf("Video first\n");
			break;
		}
	}

	if(mpkt->size > 0)
	{
		if(Manager->playAudio && type == AUDIO)
		{
			PutDataToTail(Manager->MediaAudio,mpkt);
		}
		else if(Manager->playVideo && type == VIDEO)
		{
			PutDataToTail(Manager->MediaVideo,mpkt);
		}
		else
			DestroyPacket(mpkt);
	}

	return QUEUE_ERROR;
}

//接收队列,只返回类型
static int RecvDataFromAvQueue(QueueBuffer *buffer,MediaPacket *Packet)
{
	if(!buffer)
		return QUEUE_ERROR;

	int len = ParseQueueBufferLength(buffer);

	if(len <= 0)
		return QUEUE_EMPTY;

	return FetchDataFromHead(buffer,Packet);
}

//处理合理的快进和快退数据
static double CrroectAvPos(AvManager *manager,double pos)
{
	if(pos < 0)
		pos = 0;

	else if(pos >= manager->duration)
		pos = manager->duration;

	return pos;
}

//判断此时是否缓冲区还有数据，表示视频正在播放，如果是这样

static int RenderOver(AvManager *manager)
{
	if(!manager)
		return 0;

	int ret1,ret2;

	if(manager->playAudio)
		ret1 = ParseQueueBufferLength(manager->MediaAudio);

	if(manager->playVideo)
		ret2 = ParseQueueBufferLength(manager->MediaVideo);

	return (ret1 > 0 || ret2 > 0);
}


//同步计算视频时间戳
double SynVideo(AvManager *manager,double pts)
{
	double delay = manager->videoTimeBase;;
	
	if(pts != 0)
		manager->videoClock = pts;
	else
		pts = manager->videoClock;

	delay += GetRepeat()*(delay * 0.5);

	manager->videoClock += delay;

	return pts;
}

//等待时间
int SynWait(AvManager *manager,double delay,int flags)
{
	double actual_delay = 0;
	int acutal_pts = 0;

	manager->delay_timer += delay;
	actual_delay = manager->delay_timer - ((double)GetTimeHere()/AV_TIME_BASE);

	if(actual_delay < AV_SYNC_THRESHOLD)
		actual_delay = 0;//AV_SYNC_THRESHOLD;

	acutal_pts = (int)(actual_delay*AV_TIME_BASE);

	if(flags == VIDEO && acutal_pts > 0)
		usSleep(acutal_pts);

	return acutal_pts;
}

//同步线程
void *SynProcess(void *arg)
{
	if(!arg)
		return NULL;

	AvManager *manager = (AvManager*)arg;
	double av_diff,delay;
	MediaPacket Packet;
		
	for(;manager->avState != STOP;)
	{
		memset(&Packet,0,sizeof(MediaPacket));

		int ret = RecvDataFromAvQueue(manager->MediaPicture,&Packet);
		
		if(ret < 0)
		{	
			usSleep(DEFAULT_AV_WAIT);
			continue;
		}

		if(manager->playAudio)
		{
			manager->last_video_pts = AVTimeToSecond(Packet.pts,manager->videoTimeBase);
			av_diff = manager->last_video_pts - manager->last_audio_pts;

			delay = manager->last_video_pts - manager->last_pts;

			if(delay < 0)
				delay = manager->last_delay;

			if(av_diff < -AV_NOSYNC_THRESHOLD)
			{
				delay = 0;
				goto video_end;
			}
			else if(av_diff > AV_NOSYNC_THRESHOLD)
			{
				delay *= 2;
				SynWait(manager,delay,VIDEO);
			}

		}
		else 
		{
			int onlydelay = (int)(manager->delayWithoutAudioTime*AV_TIME_BASE);
			usSleep(onlydelay);
		}

video_end:
		if(Packet.data && Packet.size > 0 && manager->avParams.callFunc)
		{
			//显示图片，这个方法由用户填写
			manager->avParams.callFunc(Packet.data,Packet.size,manager->avParams.arg);
		}

		printf("avdiff=%f\n",av_diff);
		manager->last_delay = delay;
		manager->last_pts = manager->last_video_pts;
		//释放视频包
		if(Packet.data)
			FreeAlloc(Packet.data);
	}

	return NULL;
}


//刷新显示线程
void *RenderAudio(void *arg)
{
	if(!arg)
		return NULL;

	AvManager *manager = (AvManager*)arg;
	MediaPacket spkt;
	long avsize = 0;
	int ret = 0;

	//只有在没有停止信号下，才能不断地读取数据
	for(;manager->avState != STOP;)
	{
		//如果是暂停且没有快进时，才可以不断等待
		if(manager->avState == PAUSE && !manager->avSeek)
		{
			usSleep(DEFAULT_AV_WAIT);
			continue;
		}

		ret = RecvDataFromAvQueue(manager->MediaAudio,&spkt);

		if(ret < 0)
		{
			usSleep(DEFAULT_AV_WAIT);
			continue;			
		}

		if(manager->avSeek && !IsHardWareAccel())
		{
			FlushBuffers(AUDIO);
			DestroyPacket(&spkt);
			continue;
		}

		if(manager->audioDelay)
		{
			usSleep(manager->audioDelay);
			manager->audioDelay = 0;
		}

		if(spkt.data && spkt.size > 0)
		{
			void *abuf = DecodeAudioStream(&spkt,&avsize);

			//播放音乐
			if(abuf && avsize > 0)
			{
				PlayWave(abuf,avsize);

				//声音时间戳部分
				manager->audioClock = (double)avsize/(double)manager->audioBasePara;

				if(spkt.pts != AV_NOPTS_VALUE)
					manager->playerClock = AVTimeToSecond(spkt.pts,manager->audioTimeBase) + manager->audioClock;
				else
					manager->playerClock += manager->audioClock;

				manager->last_audio_pts = manager->playerClock;

			}
		}
		else
			FreeAlloc(spkt.data);		
	}

	return NULL;
}

//刷新视频解码线程
void *RenderVideo(void *arg)
{
	if(!arg)
		return NULL;

	AvManager *manager = (AvManager*)arg;
	MediaPacket spkt;
	int size = 0,ret = 0;
	long avsize = 0;

	//只有在没有停止信号下，才能不断地读取数据
	for(;manager->avState != STOP;)
	{
		if(ParseQueueBufferLength(manager->MediaPicture) > CACHE_PICTURE_BUFFER_NUM)
		{
			usSleep(DEFAULT_AV_WAIT);
			continue;
		}

		if(manager->avState == PAUSE && !manager->avSeek)
		{
			usSleep(DEFAULT_AV_WAIT);
			continue;
		}

		memset(&spkt,0,sizeof(MediaPacket));

		ret = RecvDataFromAvQueue(manager->MediaVideo,&spkt);

		if(ret < 0)
		{
			usSleep(DEFAULT_AV_WAIT);
			continue;			
		}

		if(manager->avSeek && !IsHardWareAccel())
		{
			FlushBuffers(VIDEO);
		}

		if(spkt.data && spkt.size > 0)
		{
			void *vbuf = DecodeVideoYUVStream(&spkt,&avsize);

			if(vbuf && avsize > 0)
			{
				vbuf = StartScale(vbuf,avsize,&size,VideoOutPyaddr(),NULL);

				if(vbuf && size > 0)
				{
					MediaPacket mpkt;
					memset(&mpkt,0,sizeof(MediaPacket));
			
					mpkt.data = UseAlloc(size);

					if(!mpkt.data)
						continue;

					memcpy(mpkt.data,vbuf,size);
					mpkt.size = size;

					//时间戳部分

					double v_pts = 0;

					if(spkt.pts != AV_NOPTS_VALUE)
						v_pts = AVTimeToSecond(spkt.pts,manager->videoTimeBase);
					else if(spkt.dts != AV_NOPTS_VALUE)
						v_pts = AVTimeToSecond(spkt.dts,manager->videoTimeBase);
					else 
						v_pts = 0;

					double pts = SynVideo(manager,v_pts);

					mpkt.pts = AVTimeToPts(pts,manager->videoTimeBase);

					PutDataToTail(manager->MediaPicture,&mpkt);
				}
			}
		}
		else
			FreeAlloc(spkt.data);

		if(!manager->playAudio)
			manager->playerClock = manager->videoClock;
	}

	return NULL;
}

//读取包进程
void *ReadAvData(void *arg)
{
	if(!arg)
		return NULL;

	AvManager *manager = (AvManager*)arg;

	int size = 0;
	double cur_pos = 0;
	void *data = NULL;
	long avsize = 0;

	for(;;)
	{
		//先处理命令，命令可以改变状态
		switch(manager->avCommand)
		{
		case PAUSE_CMD:
			manager->avState = PAUSE;
			break;
		case PLAY_CMD:
			manager->avState = PLAY;
			break;
		case STOP_CMD:
			manager->avState = STOP;
			break;
		}
		//命令已经修改完毕状态，使命已经完成，恢复默认
		manager->avCommand = NONE_CMD;

		//如果是停止状态，则表示人为停止
		if(manager->avState == STOP)
		{
			manager->autoEnd = FALSE;
			break;
		}

		if(manager->avSeek)
		{
			cur_pos = CrroectAvPos(manager,manager->avPos);
			manager->playerClock = cur_pos;
			manager->avSeek = SeekPositon(cur_pos,0) >= 0;
		}

		//如果是快进，清空所有

		if(manager->avSeek)
		{
			if(manager->playAudio)
				QueueBufferFlush(manager->MediaAudio);
			if(manager->playVideo)
			{
				QueueBufferFlush(manager->MediaVideo);
				QueueBufferFlush(manager->MediaPicture);
			}
		}

		if(QueueIsFull(manager))
		{
			usSleep(DEFAULT_AV_WAIT);
			continue;			
		}

		//如果队列没满，则开始读包
		data = ExtractStream();

		if(!data)
		{
			if(GetError())
			{
				manager->autoEnd = TRUE;
				break;				
			}
		}
		else
			SendDataToAvQueue(manager,data);

		if(!manager->avSeek && RenderOver(manager) == 0)
		{
			manager->autoEnd = TRUE;
			break;
		}

		if(manager->playAudio)
		{
			double diff = AV_DIFF_DURATION(manager->playerClock,manager->duration);

			if(diff <= 0.5 && diff >= -0.5)
			{
				manager->autoEnd = TRUE;
				break;
			}
		}

		//如果当前状态是SEEK，那么你到此必须还原SEEK的上一次状态
		if(manager->avSeek)
		{
			usSleep(DEFAULT_AV_WAIT*10);
			manager->avSeek = FALSE;
		}
	}

	Manager.already = FALSE;

	if(manager->autoEnd)
	{
		pthread_mutex_lock(&actionMutex);
		printf("av stop!\n");
		manager->avState = STOP;
		printf("clear queure\n");

		if(manager->playAudio)
		{
			if(ParseQueueBufferLength(manager->MediaAudio) > 0)
				UninitQueueBuffer(manager->MediaAudio);
			pthread_join(manager->renderAudio,NULL);
		}

		if(Manager.playVideo)
		{
			if(ParseQueueBufferLength(manager->MediaVideo) > 0)
				UninitQueueBuffer(manager->MediaVideo);
			pthread_join(manager->renderVideo,NULL);
			if(ParseQueueBufferLength(manager->MediaPicture) > 0)
				UninitQueueBuffer(manager->MediaPicture);
			pthread_join(manager->synProcess,NULL);
		}
		printf("unload waveengine\n");
		manager->managerEnd = TRUE;
		UnitWaveEngine();
		printf("unload scale\n");
		DeInitScaleEngine();
		printf("close file\n");
		CloseOcxByOCX();
		printf("render over!\n");
		pthread_mutex_unlock(&actionMutex);
	}

	printf("ready to quit player!\n");

	while(!manager->managerEnd)
		usSleep(DEFAULT_AV_WAIT);

	if(manager->autoEnd)
		manager->ManagerOver = TRUE;

	printf("quit player!\n");

	Manager.playerClock = 0;
	Manager.duration = 0;

	return NULL;
}

/*
设置参数，包括回调函数,第一个参数是需要的数据格式，
第二、三个参数是图像的宽和高，第四个参数是回掉函数,
第五个参数是用户自定义数据,置零的参数，表示使用系
统默认分别为：RGB16格式，480，272，NULL,NULL
*/

void SetPlayParams(int PixelFormat,int width,int height,CallBack callfun,void *arg)
{
	SetPlayPixelFormat(PixelFormat);
	SetPlaySize(width,height);
	SetPlayCallBack(callfun);
	SetPlayUser(arg);
}

void SetPlayPixelFormat(int PixelFormat)
{
	pthread_mutex_lock(&actionMutex);

	if(PixelFormat <= 0)
		Manager.avParams.pixFormat = DEFAULT_PIXFORMAT;
	else	if(Manager.avParams.pixFormat != PixelFormat)
	{
		Manager.avParams.pixFormat = PixelFormat;

	}

	pthread_mutex_unlock(&actionMutex);
}

void SetPlaySize(int width,int height)
{
	pthread_mutex_lock(&actionMutex);

	if(width <= 0 || height <= 0)
	{
		Manager.avParams.width = DEFAULT_WIDTH;
		Manager.avParams.height = DEFAULT_HEIGHT;
	}
	else	if(Manager.avParams.width != width || Manager.avParams.height != height)
	{
		Manager.avParams.width = width;
		Manager.avParams.height = height;
	}

	pthread_mutex_unlock(&actionMutex);
}

void SetPlayUser(void *arg)
{
	pthread_mutex_lock(&actionMutex);

	if(!arg)
		Manager.avParams.arg = NULL;
	else	if(Manager.avParams.arg != arg)
	{
		Manager.avParams.arg = arg;
	}

	pthread_mutex_unlock(&actionMutex);
}

void SetPlayCallBack(CallBack callback)
{
	pthread_mutex_lock(&actionMutex);

	if(!callback)
		Manager.avParams.callFunc = NULL;
	else	if(Manager.avParams.callFunc != callback)
	{
		Manager.avParams.callFunc = callback;
	}

	pthread_mutex_unlock(&actionMutex);
}

//打开文件并进行初始化

int OpenFile(const char* filename)
{
	InitDriver();

	int ret = CreateOcxByFile(filename);

	if(ret != ENERR)
	{
		printf("Open Stream fail\n");
		return ret;
	}

	int pix = Manager.avParams.pixFormat;
	int w = Manager.avParams.width;
	int h = Manager.avParams.height;

	CallBack callfun = Manager.avParams.callFunc;
	void *arg = Manager.avParams.arg;

	memset(&Manager,0,sizeof(AvManager));
	
	SetPlayParams(pix,w,h,callfun,arg);

	pix = Manager.avParams.pixFormat;
	w = Manager.avParams.width;
	h = Manager.avParams.height;

	printf("pix=%d,w=%d,h=%d\n",pix,w,h);

	if(Manager.avParams.arg)
		printf("Have some parameters\n");

	if(Manager.avParams.callFunc)
		printf("Have som function!\n");

	InitScaleEngine();

	int audio = IsExistAuido();
	int video = IsExistVideo();

	Manager.playAudio = audio;
	Manager.playVideo = video;

	Manager.playerClock = 0;

	if(audio)
	{
		printf("Set Audio Params!\n");
		Manager.ap = GetOcxAudioParam();
		InitWaveEngine(Manager.ap.m_bAudioFreq,Manager.ap.m_bAudioFormat,Manager.ap.m_bAudioChannel);
		Manager.audioBasePara = 2*Manager.ap.m_bAudioFreq*Manager.ap.m_bAudioChannel;
		Manager.audioTimeBase = GetTimeBase(AUDIO);
	}

	if(video)
	{
		printf("Set video params!\n");
		Manager.vp = GetOcxVideoParam();
		SetScaleParam(PIX_FMT_YUV420P,Manager.vp.m_bVideoWidth,Manager.vp.m_bVideoHeight,pix,w,h);
		if(Manager.vp.m_bVideoFrameRate > 0 && Manager.vp.m_bVideoFrameRate <= 100)
			Manager.delayWithoutAudioTime = (1.0/Manager.vp.m_bVideoFrameRate);
		else
		{
			Manager.vp.m_bVideoFrameRate = 25;
			Manager.delayWithoutAudioTime = (1.0/25);
		}

		Manager.videoTimeBase = GetTimeBase(VIDEO);

		printf("Delay frame per time = %f,frame/per = %d\n",Manager.delayWithoutAudioTime,Manager.vp.m_bVideoFrameRate);
	}

	Manager.duration = GetDuration();

	printf("Duration = %f\n",Manager.duration);

	Manager.avCommand = NONE_CMD;
	Manager.avState = PLAY;
	Manager.ManagerOver = FALSE;
	Manager.autoEnd = FALSE;
	Manager.avPos = 0;
	Manager.avSeek = FALSE;
	Manager.masterClkByAV = NONE;
	Manager.delay_timer = (double)GetTimeHere()/AV_TIME_BASE;
	Manager.last_delay = 40e-3;
	Manager.last_pts = 0;

	Manager.managerEnd = FALSE;

	Manager.audioClock = 0;
	Manager.videoClock = 0;

	if(Manager.playAudio)
	{
		
		Manager.MediaAudio = InitQueueBuffer();
	
		if(!Manager.MediaAudio)
			return EALLOC;

		pthread_create(&Manager.renderAudio,NULL,&RenderAudio,&Manager);

		if(!Manager.renderAudio)
		{
			printf("Can not create render audio thread!\n");
			return EALLOC;
		}
	}

	if(Manager.playVideo)
	{

		Manager.MediaVideo = InitQueueBuffer();
	
		if(!Manager.MediaVideo)
			return EALLOC;

		Manager.MediaPicture = InitQueueBuffer();

		if(!Manager.MediaPicture)
			return EALLOC;

		pthread_create(&Manager.synProcess,NULL,&SynProcess,&Manager);

		if(!Manager.synProcess)
		{
			printf("Start syn thread error!\n");
			return EALLOC;
		}

		usSleep(20);

		pthread_create(&Manager.renderVideo,NULL,&RenderVideo,&Manager);

		if(!Manager.renderVideo)
		{
			printf("Can not create render video thread!\n");
			return EALLOC;
		}
	}

	pthread_create(&Manager.readAvEngine,NULL,&ReadAvData,&Manager);

	if(!Manager.readAvEngine)
	{
		printf("Can not create read thread!\n");
		return EALLOC;
	}

	Manager.already = TRUE;

	return ENERR;
}

int Stop()
{
printf("stop: ready .....\n");
	if(Manager.already == TRUE)
	{

		pthread_mutex_lock(&actionMutex);
printf("stop: set command and state .....\n");
		Manager.avCommand = STOP_CMD;
		Manager.avState = STOP;
printf("stop: ready to stop audio....\n");
		if(Manager.playAudio)
		{
			if(ParseQueueBufferLength(Manager.MediaAudio) > 0)
				UninitQueueBuffer(Manager.MediaAudio);printf("1\n");
			pthread_join(Manager.renderAudio,NULL);
		}
printf("stop: ready to stop video.....\n");
		if(Manager.playVideo)
		{
			if(ParseQueueBufferLength(Manager.MediaVideo) > 0)
				UninitQueueBuffer(Manager.MediaVideo);
			pthread_join(Manager.renderVideo,NULL);
			if(ParseQueueBufferLength(Manager.MediaPicture) > 0)
				UninitQueueBuffer(Manager.MediaPicture);
			pthread_join(Manager.synProcess,NULL);
		}
printf("stop: ready to stop reader.....\n");	
		Manager.managerEnd = TRUE;
		pthread_join(Manager.readAvEngine,NULL);
		Manager.already = FALSE;
		UnitWaveEngine();
		DeInitScaleEngine();
		CloseOcxByOCX();
printf("stop: stop another parameters...\n");
		Manager.ManagerOver = TRUE;
		Manager.playerClock = 0;
		Manager.duration = 0;
		Manager.delay_timer = 0;
		pthread_mutex_unlock(&actionMutex);
	}

	return 0;
}

int GetPlayState()
{
	if(Manager.avState == STOP && !Manager.ManagerOver)
		return PLAY;

	return Manager.avState;
}

double GetPlayTime()
{
	double Clock = Manager.playerClock;

	if(!Manager.playAudio)
		Clock = Manager.playerClock - Manager.audioClock;

	return Clock;
}

double GetPlayDuration()
{
	return Manager.duration;
}

void Seek(double pos)
{
	if(Manager.already == TRUE)
	{
		if(!Manager.avSeek)
		{
			Manager.avPos = pos;
			Manager.avSeek = TRUE;
		}	
	}
}

void Play()
{
	if(Manager.already == TRUE)
	{
		if(Manager.avCommand != PLAY_CMD)
			Manager.avCommand = PLAY_CMD;
	}
}

void Pause()
{
	if(Manager.already == TRUE)
	{
		if(Manager.avCommand != PAUSE_CMD)
			Manager.avCommand = PAUSE_CMD;
	}
}

int TogglePlay()
{
	if(Manager.already == TRUE)
	{
		if(Manager.avCommand == PAUSE_CMD ||
		   Manager.avCommand == PLAY_CMD)
			Manager.avCommand = !Manager.avCommand;
	}

	return Manager.avState;
}


