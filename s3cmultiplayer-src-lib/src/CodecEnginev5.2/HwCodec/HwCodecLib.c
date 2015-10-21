#include "SsbSipMfcDecode.h"
#include "SsbSipH264Decode.h"
#include "SsbSipMpeg4Decode.h"
#include "SsbSipVC1Decode.h"

#include "HwCodecLib.h"
#include <stdio.h>

#if ENALBE_MFC_SINGLE

static int DecodeSelect = -1;

typedef void *(*FP_SsbSipDecodeInit)(void);
typedef int   (*FP_SsbSipDecodeExe)(void *, long);
typedef int   (*FP_SsbSipDecodeDeInit)(void *);
typedef void *(*FP_SsbSipDecodeDecodeGetInBuf)(void *, long);
typedef void *(*FP_SsbSipDecodeDecodeGetOutBuf)(void *, long *);
typedef int   (*FP_SsbSipDecodeGetConfig)(void *, unsigned int, void *);
typedef int   (*FP_SsbSipDecodeSetConfig)(void *, unsigned int, void *);

typedef struct
{
    FP_SsbSipDecodeInit              fp_SsbSipDecodeInit;
    FP_SsbSipDecodeExe               fp_SsbSipDecodeExe;
    FP_SsbSipDecodeDeInit            fp_SsbSipDecodeDeInit;
    FP_SsbSipDecodeDecodeGetInBuf    fp_SsbSipDecodeDecodeGetInBuf;
    FP_SsbSipDecodeDecodeGetOutBuf   fp_SsbSipDecodeDecodeGetOutBuf;
    FP_SsbSipDecodeGetConfig         fp_SsbSipDecodeGetConfig;
    FP_SsbSipDecodeSetConfig         fp_SsbSipDecodeSetConfig;
} FP_LIST;

static FP_LIST    fp_list[4] =
{
    // fp_list[0] : MPEG4 decode API functions
    {
        (FP_SsbSipDecodeInit)SsbSipMPEG4DecodeInit,
        (FP_SsbSipDecodeExe)SsbSipMPEG4DecodeExe,
		(FP_SsbSipDecodeDeInit)SsbSipMPEG4DecodeDeInit,
        (FP_SsbSipDecodeDecodeGetInBuf)SsbSipMPEG4DecodeGetInBuf,
        (FP_SsbSipDecodeDecodeGetOutBuf)SsbSipMPEG4DecodeGetOutBuf,
        (FP_SsbSipDecodeGetConfig)SsbSipMPEG4DecodeGetConfig,
        (FP_SsbSipDecodeSetConfig)SsbSipMPEG4DecodeSetConfig,
    },
    // fp_list[1] : H.263 decode API functions
    {
        (FP_SsbSipDecodeInit)SsbSipMPEG4DecodeInit,
        (FP_SsbSipDecodeExe)SsbSipMPEG4DecodeExe,
        (FP_SsbSipDecodeDeInit)SsbSipMPEG4DecodeDeInit,
        (FP_SsbSipDecodeDecodeGetInBuf)SsbSipMPEG4DecodeGetInBuf,
        (FP_SsbSipDecodeDecodeGetOutBuf)SsbSipMPEG4DecodeGetOutBuf,
        (FP_SsbSipDecodeGetConfig)SsbSipMPEG4DecodeGetConfig,
        (FP_SsbSipDecodeSetConfig)SsbSipMPEG4DecodeSetConfig,
    },
    // fp_list[2] : H.264 decode API functions
    {
        (FP_SsbSipDecodeInit)SsbSipH264DecodeInit,
        (FP_SsbSipDecodeExe)SsbSipH264DecodeExe,
        (FP_SsbSipDecodeDeInit)SsbSipH264DecodeDeInit,
        (FP_SsbSipDecodeDecodeGetInBuf)SsbSipH264DecodeGetInBuf,
        (FP_SsbSipDecodeDecodeGetOutBuf)SsbSipH264DecodeGetOutBuf,
        (FP_SsbSipDecodeGetConfig)SsbSipH264DecodeGetConfig,
        (FP_SsbSipDecodeSetConfig)SsbSipH264DecodeSetConfig,
    },
    // fp_list[3] : VC-1 decode
    {
	  (FP_SsbSipDecodeInit)SsbSipVC1DecodeInit,
	  (FP_SsbSipDecodeExe)SsbSipVC1DecodeExe,
	  (FP_SsbSipDecodeDeInit)SsbSipVC1DecodeDeInit,
	  (FP_SsbSipDecodeDecodeGetInBuf)SsbSipVC1DecodeGetInBuf,
        (FP_SsbSipDecodeDecodeGetOutBuf)SsbSipVC1DecodeGetOutBuf,
        (FP_SsbSipDecodeGetConfig)SsbSipVC1DecodeGetConfig,
        (FP_SsbSipDecodeSetConfig)SsbSipVC1DecodeSetConfig,
    }
};

#endif

void* HwDecodeInit(int type)
{
#if ENALBE_MFC_SINGLE
	switch(type)
	{
	case SSBSIPMFCDEC_MPEG4:
		DecodeSelect = 0;
		printf("MPEG4 decoding is selected!\n");
		break;
	case SSBSIPMFCDEC_H263:
		printf("H263 decoding is selected!\n");
		DecodeSelect = 1;
		break;
	case SSBSIPMFCDEC_H264:
		printf("H264 decoding is selected!\n");
		DecodeSelect = 2;
		break;
	case SSBSIPMFCDEC_VC1:
		printf("VC1 decoding is selected!\n");
		DecodeSelect = 3;
		break;
	default:
		DecodeSelect = -1;
	}

	if(DecodeSelect != -1)
		return fp_list[DecodeSelect].fp_SsbSipDecodeInit();

	return NULL;
#else
	return SsbSipMfcDecodeInit(type);
#endif
}

int HwDecodeDeInit(void* handle)
{
#if ENALBE_MFC_SINGLE
	int CurSelect = DecodeSelect;

	if(handle && DecodeSelect != -1)
	{
		DecodeSelect = -1;
		return fp_list[CurSelect].fp_SsbSipDecodeDeInit(handle);
	}

	return -1;
#else
	return SsbSipMfcDecodeDeInit(handle);
#endif
}

void* HwDecodeInBuffer(void* handle,long insize)
{
#if ENALBE_MFC_SINGLE
	if(handle && DecodeSelect != -1)
		return fp_list[DecodeSelect].fp_SsbSipDecodeDecodeGetInBuf(handle,insize);

	return NULL;
#else
	return SsbSipMfcDecodeGetInBuf(handle,&insize);
#endif
}

void* HwDecodeOutBuffer(void* handle,long *outsize)
{
#if ENALBE_MFC_SINGLE
	if(handle && DecodeSelect != -1)
		return fp_list[DecodeSelect].fp_SsbSipDecodeDecodeGetOutBuf(handle,outsize);

	return NULL;
#else
	return SsbSipMfcDecodeGetOutBuf(handle,outsize);
#endif
}

int HwDecodeExe(void* handle,long size)
{
#if ENALBE_MFC_SINGLE
	if(handle && DecodeSelect != -1)
		return fp_list[DecodeSelect].fp_SsbSipDecodeExe(handle,size);

	return -1;
#else
	return SsbSipMfcDecodeExe(handle,size);
#endif
}

int HwDecodeGetConfig(void* handle,int cfg_type,void* value)
{
#if ENALBE_MFC_SINGLE
	if(handle && DecodeSelect != -1)
		return fp_list[DecodeSelect].fp_SsbSipDecodeGetConfig(handle,cfg_type,value);

	value = NULL;

	return -1;
#else
	return SsbSipMfcDecodeGetConfig(handle,cfg_type,value);
#endif
}

int HwDecodeSetConfig(void* handle,int cfg_type,void* value)
{
#if ENALBE_MFC_SINGLE
	if(handle && DecodeSelect != -1)
		return fp_list[DecodeSelect].fp_SsbSipDecodeSetConfig(handle,cfg_type,value);

	value = NULL;

	return -1;
#else
	return SsbSipMfcDecodeSetConfig(handle,cfg_type,value);
#endif
}
