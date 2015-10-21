#ifndef __PLAYER_ENGINE_H
#define __PLAYER_ENGINE_H

#include "basetype.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*CallBack)(void *,int,void*);

//解码后视频格式
enum {
	PAL8 =  PIX_FMT_PAL8,		//PAL8格式
	RGB8 =  PIX_FMT_RGB8,		//RGB8位格式
	RGB16 = PIX_FMT_RGB565,		//RGB16位格式
	RGB24 = PIX_FMT_RGB24,		//RGB24位格式
	YC420 = PIX_FMT_YUV420P,	//YUV420格式
	YC422 = PIX_FMT_YUV422P,	//YUV422格式
};

//播放状态
enum {
	//public
	STOP = -1,			//停止状态
	PAUSE,			//暂停状态
	PLAY,				//播放状态（启动后的默认状态）
	//private
	SEEK,				//快进、快退状态
};
//Openfile返回值信息
enum {
	ENERR	   =  0,	//正常返回，表示打开文件正常
	EREUSE   = -1,	//已经有实例在使用该解码器
	EOPFAIL  = -2,	//打开文件失败
	ENSTREAM = -3,	//文件中没有多媒体流或是无法识别的格式
	ELARGE   = -4,	//媒体文件宽高过于庞大，最大支持720*480大小
	EALLOC   = -5,	//分配内存错误
};

/*
设置参数，包括回调函数,第一个参数是需要的数据格式，
第二、三个参数是图像的宽和高，第四个参数是回掉函数,
第五个参数是用户自定义数据,置零的参数，表示使用系
统默认分别为：RGB16格式，480，272，NULL,NULL
*/

void SetPlayParams(int PixelFormat,int width,int height,CallBack callfun,void *arg);
void SetPlayPixelFormat(int PixelFormat);
void SetPlaySize(int width,int height);
void SetPlayUser(void *arg);
void SetPlayCallBack(CallBack callback);
/*
提供打开文件并直接播放,第一个参数为视频文件名，第二个参数为允许视频播放开关，请参考播放选项
若输入值不在播放选项之内，则默认SHOW_ALL
*/

int OpenFile(const char *filename);

/*
关闭文件
*/

int Stop();

/*
获得当前播放状态
*/

int GetPlayState();

/*
获得当前播放时间,单位：秒
*/

double GetPlayTime();

/*
获得视频的整个播放时间长度
*/

double GetPlayDuration();

/*
快进或快退
*/

//直接跳转到指定位置，范围[0,GetDuration()]
void Seek(double pos);

/*
播放和暂停
*/

void Play();
void Pause();

//PLAY、PAUSE状态翻转,返回翻转后的状态,PAUSE OR PLAY
int TogglePlay();

#ifdef __cplusplus
}
#endif

#endif
