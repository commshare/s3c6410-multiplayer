#ifndef __PARSE_H264_CONFIG
#define __PARSE_H264_CONFIG

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*CONFIGFUNC)(void*,long);

//读取mp4/mkv/flv的264头信息中的sps,pps
int ParseH264SPSPPS(unsigned char *buf,int size,unsigned char* dstbuffer,CONFIGFUNC cfgfunc,void *handle);
//读取数据信息中的sei,sps,pps信息，并返回数据段起始有效地址,并完成设置
int ParseDataSEISPSPPSConfig(int fill,unsigned char* data,int datasize);

#ifdef __cplusplus
}
#endif

#endif
