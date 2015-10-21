#include "HwScale.h"
#include "PScale.h"
#include "basetype.h"

#include <stdio.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>
#include <stdlib.h>

#define TRUE	1
#define FALSE	0

#define CHECKBOUNDS(x)	(x <=0 ? -1 : x)

typedef struct{
	pp_params m_bparam;
	int m_bfileID;
	enum PixelFormat dstfmt;
	int m_bset;
	int m_bsize;
	int m_binsize;
	int m_boutsize;

	uint8_t* lgcinaddr;
	uint8_t* lgcoutaddr;
}ScaleParam;

static ScaleParam sparam;
static int ScaleUseNum = 0;

static cspace_t GetFMT(enum PixelFormat srcfmt)
{
	cspace_t sfmt = FMTNONE;

	switch(srcfmt)
	{
	case PIX_FMT_YUV420P:
		sfmt = YC420;
		break;
	case PIX_FMT_YUV422P:
		sfmt = YC422;
		break;
	case PIX_FMT_PAL8:
		sfmt = PAL8;
		break;
	case PIX_FMT_RGB8:
		sfmt = RGB8;
		break;
	case PIX_FMT_RGB565:
	case PIX_FMT_RGB555:
		sfmt = RGB16;
		break;
	case PIX_FMT_RGB24:
		sfmt = RGB24;
		break;

	default:
		sfmt = YC420;
	}

	return sfmt;
}

int InitScaleEngine()
{
	if(ScaleUseNum != 0)
	{
		printf("Another is using scale pp!\n");
		return FALSE;
	}

	int DeviceID = open(PP_DEV_NAME, O_RDWR);

	ScaleUseNum = 1;

	if(DeviceID < 0)
	{
		printf("Can not open scale pp!\n");
		ScaleUseNum = 0;
		return FALSE;
	}

	memset(&sparam,0,sizeof(ScaleParam));
	sparam.m_bfileID = DeviceID;

	return TRUE;
}

int SetScaleParam(enum PixelFormat srcfmt,
			int srcwidth,int srcheight,enum PixelFormat dstfmt,int dstwidth,int dstheight)
{
	if(ScaleUseNum != 1)
	{
		printf("Another is using scale pp!\n");		
		return FALSE;
	}

	if((CHECKBOUNDS(srcwidth) < 0) || (CHECKBOUNDS(srcheight) < 0))
		return FALSE;

	if((CHECKBOUNDS(dstwidth) < 0) || (CHECKBOUNDS(dstheight) < 0))
		return FALSE;

	cspace_t sfmt = GetFMT(srcfmt);
	cspace_t dfmt = GetFMT(dstfmt);

	if((sfmt == FMTNONE) || (dfmt == FMTNONE))
		return FALSE;

	//remain dstfmt
	sparam.dstfmt = dstfmt;
	//set src params
	sparam.m_bparam.SrcFullWidth = srcwidth;
	sparam.m_bparam.SrcFullHeight = srcheight;
	sparam.m_bparam.SrcStartX = 0;
	sparam.m_bparam.SrcStartY = 0;
	sparam.m_bparam.SrcWidth = srcwidth;
	sparam.m_bparam.SrcHeight = srcheight;
	sparam.m_bparam.SrcCSpace = sfmt;
	//set dst params
	sparam.m_bparam.DstFullWidth = dstwidth;
	sparam.m_bparam.DstFullHeight = dstheight;
	sparam.m_bparam.DstStartX = 0;
	sparam.m_bparam.DstStartY = 0;
	sparam.m_bparam.DstWidth = dstwidth;
	sparam.m_bparam.DstHeight = dstheight;
	sparam.m_bparam.DstCSpace = dfmt;

	//set post on path --> DMA
	sparam.m_bparam.OutPath = POST_DMA;
	sparam.m_bparam.Mode = PROGRESSIVE_MODE;

	ioctl(sparam.m_bfileID, PP_SET_PARAMS,&sparam.m_bparam);

	//prealloc inbuf
	int buf_size = ioctl(sparam.m_bfileID, PP_GET_RESERVED_MEM_SIZE);

	sparam.m_binsize = GetFrameSize(srcfmt,srcwidth,srcheight);
	sparam.m_boutsize = GetFrameSize(dstfmt,dstwidth,dstheight);
	sparam.m_bsize = sparam.m_binsize + sparam.m_boutsize;

	if(sparam.m_bsize > buf_size)
		return FALSE;

      sparam.lgcinaddr = (uint8_t *) mmap(0, sparam.m_bsize, PROT_READ | PROT_WRITE,MAP_SHARED, sparam.m_bfileID, 0);
	
	if(!sparam.lgcinaddr)
		return FALSE;

	//prealloc outbuf
	sparam.lgcoutaddr = sparam.lgcinaddr + sparam.m_binsize;

	if(!sparam.lgcoutaddr)
		return FALSE;

      sparam.m_bparam.SrcFrmSt = ioctl(sparam.m_bfileID, PP_GET_RESERVED_MEM_ADDR_PHY);
	ioctl(sparam.m_bfileID, PP_SET_SRC_BUF_ADDR_PHY,&sparam.m_bparam);

      sparam.m_bparam.DstFrmSt = sparam.m_bparam.SrcFrmSt + sparam.m_binsize;
	ioctl(sparam.m_bfileID, PP_SET_DST_BUF_ADDR_PHY,&sparam.m_bparam);

	
	sparam.m_bset = TRUE;

	return sparam.m_bset;
}

void* StartScale(uint8_t* src,int insize,int* outsize)
{
	if(ScaleUseNum != 1 || !src || insize <= 0)
		return NULL;

	memcpy(sparam.lgcinaddr,src,insize);

	ioctl(sparam.m_bfileID, PP_START);

	if(outsize)
		*outsize = sparam.m_boutsize;

	return sparam.lgcoutaddr;	
	
}

void DeInitScaleEngine()
{
	if(ScaleUseNum != 1)
		return;

	if(!sparam.m_bset && sparam.lgcinaddr)
		munmap(sparam.lgcinaddr, sparam.m_bsize);
	close(sparam.m_bfileID);

	ioctl(sparam.m_bfileID,PP_FREE_KMEM,&sparam.m_bparam);

	ScaleUseNum = 0;
}

