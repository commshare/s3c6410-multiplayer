#ifndef __HWCODEC_H
#define __HWCODEC_H

#ifdef __cplusplus
extern "C"{
#endif

#define SSBSIPMFCDEC_MPEG4		0x3031
#define SSBSIPMFCDEC_H263		0x3032
#define SSBSIPMFCDEC_H264		0x3033
#define SSBSIPMFCDEC_VC1		0x3034

#define SSBSIPMF_PHYADDR_FRAM_BUF	0x00001002

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
