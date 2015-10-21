#ifndef __BASE_TYPE_H
#define __BASE_TYPE_H

enum PixelFormat {
    PIX_FMT_NONE= -1,
    PIX_FMT_YUV420P,   
    PIX_FMT_YUYV422,  
    PIX_FMT_RGB24,    
    PIX_FMT_BGR24,    
    PIX_FMT_YUV422P,  
    PIX_FMT_YUV444P,   
    PIX_FMT_YUV410P,   
    PIX_FMT_YUV411P,   
    PIX_FMT_GRAY8,     
    PIX_FMT_MONOWHITE, 
    PIX_FMT_MONOBLACK, 
    PIX_FMT_PAL8,      
    PIX_FMT_YUVJ420P,  
    PIX_FMT_YUVJ422P,  
    PIX_FMT_YUVJ444P,  
    PIX_FMT_XVMC_MPEG2_MC,
    PIX_FMT_XVMC_MPEG2_IDCT,
    PIX_FMT_UYVY422,   
    PIX_FMT_UYYVYY411, 
    PIX_FMT_BGR8,      
    PIX_FMT_BGR4,      
    PIX_FMT_BGR4_BYTE,
    PIX_FMT_RGB8,      
    PIX_FMT_RGB4,      
    PIX_FMT_NV12,      
    PIX_FMT_NV21,      

    PIX_FMT_ARGB,     
    PIX_FMT_RGBA,      
    PIX_FMT_ABGR,      
    PIX_FMT_BGRA,      

    PIX_FMT_GRAY16BE,  
    PIX_FMT_GRAY16LE,  
    PIX_FMT_YUV440P,  
    PIX_FMT_YUVJ440P,  
    PIX_FMT_YUVA420P,  
    PIX_FMT_VDPAU_H264,
    PIX_FMT_VDPAU_MPEG1,
    PIX_FMT_VDPAU_MPEG2,
    PIX_FMT_VDPAU_WMV3,
    PIX_FMT_VDPAU_VC1, 
    PIX_FMT_RGB48BE,   
    PIX_FMT_RGB48LE,   

    PIX_FMT_RGB565BE,  
    PIX_FMT_RGB565LE, 
    PIX_FMT_RGB555BE,  
    PIX_FMT_RGB555LE,  

    PIX_FMT_BGR565BE, 
    PIX_FMT_BGR565LE, 
    PIX_FMT_BGR555BE, 
    PIX_FMT_BGR555LE,  

    PIX_FMT_VAAPI_MOCO, 
    PIX_FMT_VAAPI_IDCT, 
    PIX_FMT_VAAPI_VLD,  

    PIX_FMT_YUV420P16LE, 
    PIX_FMT_YUV420P16BE, 
    PIX_FMT_YUV422P16LE, 
    PIX_FMT_YUV422P16BE,  
    PIX_FMT_YUV444P16LE,  
    PIX_FMT_YUV444P16BE,  
    PIX_FMT_VDPAU_MPEG4,  
    PIX_FMT_DXVA2_VLD,

    PIX_FMT_RGB444BE,
    PIX_FMT_RGB444LE,
    PIX_FMT_BGR444BE,
    PIX_FMT_BGR444LE, 
    PIX_FMT_Y400A,  
    PIX_FMT_NB,
};

enum SampleFormat {
    SAMPLE_FMT_NONE = -1,
    SAMPLE_FMT_U8,              
    SAMPLE_FMT_S16,             
    SAMPLE_FMT_S32,            
    SAMPLE_FMT_FLT,             
    SAMPLE_FMT_DBL,            
    SAMPLE_FMT_NB              
};

#if AV_HAVE_BIGENDIAN
#   define PIX_FMT_NE(be, le) PIX_FMT_##be
#else
#   define PIX_FMT_NE(be, le) PIX_FMT_##le
#endif

#define PIX_FMT_RGB32   PIX_FMT_NE(ARGB, BGRA)
#define PIX_FMT_RGB32_1 PIX_FMT_NE(RGBA, ABGR)
#define PIX_FMT_BGR32   PIX_FMT_NE(ABGR, RGBA)
#define PIX_FMT_BGR32_1 PIX_FMT_NE(BGRA, ARGB)

#define PIX_FMT_GRAY16 PIX_FMT_NE(GRAY16BE, GRAY16LE)
#define PIX_FMT_RGB48  PIX_FMT_NE(RGB48BE,  RGB48LE)
#define PIX_FMT_RGB565 PIX_FMT_NE(RGB565BE, RGB565LE)
#define PIX_FMT_RGB555 PIX_FMT_NE(RGB555BE, RGB555LE)
#define PIX_FMT_RGB444 PIX_FMT_NE(RGB444BE, RGB444LE)
#define PIX_FMT_BGR565 PIX_FMT_NE(BGR565BE, BGR565LE)
#define PIX_FMT_BGR555 PIX_FMT_NE(BGR555BE, BGR555LE)
#define PIX_FMT_BGR444 PIX_FMT_NE(BGR444BE, BGR444LE)

#define PIX_FMT_YUV420P16 PIX_FMT_NE(YUV420P16BE, YUV420P16LE)
#define PIX_FMT_YUV422P16 PIX_FMT_NE(YUV422P16BE, YUV422P16LE)
#define PIX_FMT_YUV444P16 PIX_FMT_NE(YUV444P16BE, YUV444P16LE)

#if defined(__GNUC__) || defined(__DMC__)
#define HAVE_STDINT_H	1
#include <stdint.h>
#elif defined(_MSC_VER)
typedef signed __int8		int8_t;
typedef unsigned __int8		uint8_t;
typedef signed __int16		int16_t;
typedef unsigned __int16	uint16_t;
typedef signed __int32		int32_t;
typedef unsigned __int32	uint32_t;
typedef signed __int64		int64_t;
typedef unsigned __int64	uint64_t;
#ifndef _UINTPTR_T_DEFINED
#ifdef  _WIN64
typedef unsigned __int64    uintptr_t;
#else
typedef unsigned int   uintptr_t;
#endif
#define _UINTPTR_T_DEFINED
#endif
/* Older Visual C++ headers don't have the Win64-compatible typedefs... */
#if ((_MSC_VER <= 1200) && (!defined(DWORD_PTR)))
#define DWORD_PTR DWORD
#endif
#if ((_MSC_VER <= 1200) && (!defined(LONG_PTR)))
#define LONG_PTR LONG
#endif
#else	/* !__GNUC__ && !_MSC_VER */
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int16_t;
typedef unsigned short uint16_t;
typedef signed int int32_t;
typedef unsigned int uint32_t;
typedef signed long long int64_t;
typedef unsigned long long uint64_t;
#ifndef _SIZE_T_DEFINED_
#define _SIZE_T_DEFINED_
typedef unsigned int size_t;
#endif
typedef unsigned int uintptr_t;
#endif /* __GNUC__ || _MSC_VER */

typedef struct AVPacket {
    int64_t pts;
    int64_t dts;
    uint8_t *data;
    int   size;
    int   stream_index;
    int   flags;
    int   duration;
    void  (*destruct)(struct AVPacket *);
    void  *priv;
    int64_t pos;
    int64_t convergence_duration;
} AVPacket;

typedef AVPacket MediaPacket;

#endif


