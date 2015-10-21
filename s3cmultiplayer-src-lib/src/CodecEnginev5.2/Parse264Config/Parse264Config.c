#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "Parse264Config.h"
#include "LogMsg.h"

#define TRUE	1
#define FALSE	0

#define START_CODE_LEN	4

enum {
	NAL_PFR = 1,
	NAL_IFR = 5,
	NAL_SEI = 6,
	NAL_SPS = 7,
	NAL_PPS = 8
};

static unsigned char start_code[START_CODE_LEN] = {0x00,0x00,0x00,0x01};

static int _haveSPS(unsigned char *buf,int size)
{
	int i = 0,value = 0;
	for(i = 0; i < size; i++)
	{
		value = buf[i]&0x1f;
		
		if(value == NAL_SPS)
			return 1;
	}

	return 0;
}

static int _havePPS(unsigned char *buf,int size)
{
	int i = 0,value = 0;
	for(i = 0; i < size; i++)
	{
		value = buf[i]&0x1f;
		
		if(value == NAL_PPS)
			return 1;
	}

	return 0;
}

static int _legalIFR(int fill,unsigned char *data)
{
	int value = data[fill]&0x1f;
	int xval = data[fill]&0xffff;

	if(value == xval || (value != NAL_IFR))
		return 0;

	return (value == NAL_IFR);
}

static int _legalPFR(int fill,unsigned char *data)
{
	int value = data[fill]&0x1f;
	int xval = data[fill]&0xffff;

	if(value == xval || (value != NAL_PFR))
		return 0;

	return (value == NAL_PFR);
}

static int _legalCFG(int fill,unsigned char *data)
{
	int value = data[fill]&0x1f;
	int xval = data[fill]&0xffff;

	if(value == xval || (value != NAL_SEI || value != NAL_SPS || value != NAL_PPS))
		return 0;

	switch(value)
	{
	case NAL_SEI:
	case NAL_SPS:
	case NAL_PPS:
		return 1;
	}

	return 0;
}

static int _getLEN(int fill,unsigned char* data)
{
	int value = 0;

	switch(fill)
	{
	case 1:
		value = (data[0]&0xff);
		break;
	case 2:
		value = ((data[0]<<8)&0xff00 | data[1]&0xff);
		break;
	case 3:
		value = ((data[0]<<16)&0xff0000 | (data[1]<<8)&0xff00 | data[2]&0xff);
		break;
	case 4:
		value = ((data[0]<<24)&0xff000000 | (data[1]<<16)&0xff0000 | (data[2]<<8)&0xff00 | data[3]&0xff);
		break;
	}

	return value;
}

static int _getSPSPPS(unsigned char *buf,int *spsnum,int *spslen,int *ppsnum,int *ppslen)
{
	*spsnum = buf[5]&0x1f;
	*spslen = ((buf[6]<<8)&0xff00 | (buf[7]&0xff));

	int space = 8 + (*spsnum)*(*spslen);

	*ppsnum = buf[space]&0x1f;
	*ppslen = ((buf[space+1]<<8)&0xff00 | (buf[space+2]&0xff));

	return (space + 3);
}

int ParseH264SPSPPS(unsigned char *buf,int size,unsigned char* dstbuffer,CONFIGFUNC cfgfunc,void *handle)
{
	if(!buf || size <= 0 || !dstbuffer || !cfgfunc || !handle)
	{
		LOG_MSG(LOG_ERROR,"ParseBuffer:","Not any buffer to parse!\n");
		return FALSE;
	}

	if(_haveSPS(buf,size) == 0 || _havePPS(buf,size) == 0)
	{
		LOG_MSG(LOG_ERROR,"ParseBuffer:","Have not sps and pps!\n");
		return FALSE;
	}

	int spsnum = 0,spslen = 0,ppsnum = 0,ppslen = 0;

	int start = _getSPSPPS(buf,&spsnum,&spslen,&ppsnum,&ppslen);

	if(size != (8 + spsnum*spslen + ppsnum*ppslen + 3))
		return FALSE;

	unsigned char *start_ptr = buf + 8;

	int i = 0;

	for(i = 0;i < spsnum; i++)
	{
		memcpy(dstbuffer,start_code,START_CODE_LEN);
		memcpy(dstbuffer + START_CODE_LEN,start_ptr,spslen);
			
		cfgfunc(handle,spslen + START_CODE_LEN);

		start_ptr += spslen;			
	}

	start_ptr = buf + start;

	for(i = 0;i < ppsnum; i++)
	{
		memcpy(dstbuffer,start_code,START_CODE_LEN);
		memcpy(dstbuffer + START_CODE_LEN,start_ptr,ppslen);
			
		cfgfunc(handle,ppslen + START_CODE_LEN);

		start_ptr += ppslen;			
	}

	return TRUE;
}

int ParseDataSEISPSPPSConfig(int fill,unsigned char* data,int datasize)
{
	if((fill < 0 || fill > 4) || !data || datasize <= 0 )
		return -1;

	unsigned char *start_ptr = data;
	int totallen = 0;

	while(totallen < datasize)
	{
		if(_legalCFG(fill,start_ptr))
		{
			int len =  _getLEN(fill,start_ptr);

			start_ptr = start_ptr + fill;
			start_ptr = start_ptr + len;
			totallen += (len + fill);
		}
		else
			break;
	}
#if 0
	if(totallen < datasize)
	{
		int len =  _getLEN(fill,start_ptr);

		if(len == (datasize - totallen - fill))
		{
			if(_legalIFR(fill,start_ptr) || _legalPFR(fill,start_ptr))
				return (totallen + fill);
		}
	}
#endif

	if(totallen < datasize)
	{
		int len =  _getLEN(fill,start_ptr);
		
		if(len == (datasize - totallen - fill))
			return (totallen + fill);
	}

	return -1;
}

