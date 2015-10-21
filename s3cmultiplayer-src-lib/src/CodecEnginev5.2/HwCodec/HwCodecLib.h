#ifndef __HWCODEC_H
#define __HWCODEC_H

#include "SsbSipMfcDecode.h"

#ifdef __cplusplus
extern "C"{
#endif

#define ENALBE_MFC_SINGLE 1

void* HwDecodeInit(int type);
int HwDecodeDeInit(void* handle);

void* HwDecodeInBuffer(void* handle,long insize);
void* HwDecodeOutBuffer(void* handle,long *outsize);

int HwDecodeExe(void* handle,long size);

int HwDecodeGetConfig(void* handle,int cfg_type,void* value);
int HwDecodeSetConfig(void* handle,int cfg_type,void* value);

#ifdef __cplusplus
}
#endif

#endif
