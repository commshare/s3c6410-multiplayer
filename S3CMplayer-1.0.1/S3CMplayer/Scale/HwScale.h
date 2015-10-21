#ifndef __HWSCALE_H
#define __HWSCALE_H

#include "basetype.h"

#ifdef __cplusplus
extern "C" {
#endif

//初始化Scale引擎,返回句柄
int InitScaleEngine();

//设置缩放命令和参数，失败返回0，成功返回1
int SetScaleParam(enum PixelFormat srcfmt,
			int srcwidth,int srcheight,enum PixelFormat dstfmt,int dstwidth,int dstheight);

//执行缩放，返回缩放后的存储地址
void* StartScale(uint8_t* src,int insize,int* outsize);

//撤销缩放功能
void DeInitScaleEngine();
#ifdef __cplusplus
}
#endif

#endif
