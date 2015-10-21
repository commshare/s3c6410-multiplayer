#ifndef __SAMSUNG_SYSLSI_APDEV_S3C_POST_H__
#define __SAMSUNG_SYSLSI_APDEV_S3C_POST_H__

#include <asm/ioctl.h>

#define PP_IOCTL_MAGIC 'P'

#define PP_SET_PARAMS			  _IO(PP_IOCTL_MAGIC, 0)
#define PP_START				  _IO(PP_IOCTL_MAGIC, 1)
#define PP_GET_SRC_BUF_SIZE  		  _IO(PP_IOCTL_MAGIC, 2)
#define PP_SET_SRC_BUF_ADDR_PHY       _IO(PP_IOCTL_MAGIC, 3)
#define PP_SET_SRC_BUF_NEXT_ADDR_PHY  _IO(PP_IOCTL_MAGIC, 4)
#define PP_GET_DST_BUF_SIZE  		  _IO(PP_IOCTL_MAGIC, 5)
#define PP_SET_DST_BUF_ADDR_PHY	  _IO(PP_IOCTL_MAGIC, 6)
#define PP_ALLOC_KMEM                 _IO(PP_IOCTL_MAGIC, 7)
#define PP_FREE_KMEM                  _IO(PP_IOCTL_MAGIC, 8)
#define PP_GET_RESERVED_MEM_SIZE      _IO(PP_IOCTL_MAGIC, 9)
#define PP_GET_RESERVED_MEM_ADDR_PHY  _IO(PP_IOCTL_MAGIC, 10)

#define PP_DEV_NAME		"/dev/s3c-pp"

typedef enum {
	INTERLACE_MODE,
	PROGRESSIVE_MODE
} s3c_pp_scan_mode_t;

typedef enum {
	POST_DMA, POST_FIFO
} s3c_pp_path_t;

typedef enum {
	ONE_SHOT, FREE_RUN
} s3c_pp_run_mode_t;

typedef enum {
	FMTNONE = -1,
	PAL1, PAL2, PAL4, PAL8,
	RGB8, ARGB8, RGB16, ARGB16, RGB18, RGB24, RGB30, ARGB24,
	YC420, YC422, // Non-interleave
	CRYCBY, CBYCRY, YCRYCB, YCBYCR, YUV444 // Interleave
} cspace_t;

typedef enum
{
	HCLK = 0, PLL_EXT = 1, EXT_27MHZ = 3
} pp_clk_src_t;

typedef struct{
	unsigned int SrcFullWidth; 		// Source Image Full Width(Virtual screen size)
	unsigned int SrcFullHeight; 		// Source Image Full Height(Virtual screen size)
	unsigned int SrcStartX; 			// Source Image Start width offset
	unsigned int SrcStartY; 			// Source Image Start height offset
	unsigned int SrcWidth;			// Source Image Width
	unsigned int SrcHeight; 			// Source Image Height
	unsigned int SrcFrmSt; 			// Base Address of the Source Image : Physical Address
	unsigned int SrcNxtFrmSt;
	cspace_t SrcCSpace; 		// Color Space ot the Source Image

	unsigned int DstFullWidth; 		// Source Image Full Width(Virtual screen size)
	unsigned int DstFullHeight; 		// Source Image Full Height(Virtual screen size)
	unsigned int DstStartX; 			// Source Image Start width offset
	unsigned int DstStartY; 			// Source Image Start height offset
	unsigned int DstWidth; 			// Source Image Width
	unsigned int DstHeight; 			// Source Image Height
	unsigned int DstFrmSt; 			// Base Address of the Source Image : Physical Address
	cspace_t DstCSpace; 		// Color Space ot the Source Image

	unsigned int SrcFrmBufNum; 		// Frame buffer number
	s3c_pp_run_mode_t Mode; 	// POST running mode(PER_FRAME or FREE_RUN)
	s3c_pp_path_t InPath;
	s3c_pp_path_t OutPath; 	// Data path of the desitination image

	unsigned int in_pixel_size;
	unsigned int out_pixel_size;
}pp_params;


typedef struct{
	unsigned int pre_phy_addr;
	unsigned char *pre_virt_addr;

	unsigned int post_phy_addr;
	unsigned char *post_virt_addr;
} buff_addr_t;

#endif
