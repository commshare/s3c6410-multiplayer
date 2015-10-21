#ifndef __CODEC_ENGINE_H
#define __CODEC_ENGINE_H

#include "AvWrapper.h"
#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SSBSIPMFCDEC_NONE	-1

enum {
	NONE = -1,
	VIDEO = 0x110,
	AUDIO = 0x119,
};

typedef struct AudioParam
{
	int m_bAudioFreq;
	int m_bAudioChannel;
	int m_bAudioFormat;
}AudioParam;

typedef struct VideoParam
{
	int m_bVideoWidth;
	int m_bVideoHeight;
	enum PixelFormat m_bVideoPixFormat;
	int m_bVideoBitRate;
	int m_bVideoFrameRate;
}VideoParam;

typedef struct PacketInfo
{
	int size;
	
	double pts;
	double dts;

	int64_t apts;
	int64_t adts;

	int index;

	int type;

}PacketCtx;

//得到毫秒级的当前播放时间
int64_t GetTimeHere();

//获得毫秒级的精确延时,注意使用us时，us大小不能超过1000
int usSleep(long us);


/*************************************[视频解码模块]************************************/

//在播放器程序运行的最开始运行初始化
void InitDriver();

//根据文件名直接返回多媒体运行句柄
int CreateOcxByFile(const char *filename);
//提取有效的视频流包，可能是音频或视频
void *ExtractStream();

int GetFrameSize(enum PixelFormat pixfmt,int width,int height);

//快进或快退，使用double，并设置是否直接清除以前帧解码缓冲
int SeekPositon(double pts,int flush);

//判断是否到达文件末尾
int GetEof();
//判断是否出现错误
int GetError();

//判断是否存在音频
int IsExistAuido();
//判断是否攒在视频
int IsExistVideo();

//申请空间，避免内存碎片
void *UseAlloc(int size);

//释放空间
void FreeAlloc(void *ptr);

//是否采用硬件加速
int IsHardWareAccel();

//获取音频参数
AudioParam GetOcxAudioParam();
//获取视频参数
VideoParam GetOcxVideoParam();

//清空音频或视频缓冲
void FlushBuffers(int type);

//获得视频文件播放时长
double GetDuration();

//视频解码模块,能直接获得解码数据YUV格式
void *DecodeVideoYUVStream(void *pkt,long *size);
//音频解码模块
void *DecodeAudioStream(void *pkt,long *size);

//获取包信息
PacketCtx GetPacketCtx(void *pkt);

//分辨包的属性，NONE，AUDIO，VIDEO
int GetPacketAttr(void *pkt);

//销毁一个包
void DestroyPacket(void* pkt);

//获得基准时间
double GetTimeBase(int type);

//显示信息
void PrintMessage(const char *fmt,...);

//关闭音视频文件流
void CloseOcxByOCX();

#ifdef __cplusplus
}
#endif

#endif
