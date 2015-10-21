#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <ctype.h>
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <signal.h>
#include <pthread.h>

#include "cmmdriver.h"
#include "s3c_cmm.h"

#define CODEC_MEM_ALLOC_ARG		s3c_cmm_codec_mem_alloc_arg_t
#define CODEC_MEM_FREE_ARG		s3c_cmm_codec_mem_free_arg_t
#define CODEC_CACHE_FLUSH_ARG		s3c_cmm_codec_cache_flush_arg_t
#define CODEC_GET_PHY_ADDR_ARG	s3c_cmm_codec_get_phy_addr_arg_t

#define CMM_DRIVER_NAME			"/dev/s3c-cmm"

typedef struct CmmArg {
	int cmmDeviceID;

	uint8_t* cmmBuffer;

	uint8_t* cmmCache;
	uint8_t* cmmNoCache;
	
	int cmmSize;	

	int cmmCount;
}CmmArg;

static CmmArg UseCmm = {0,0,NULL,NULL,0,0};

//申请CMM空间
uint8_t* CMMAllocMemory(int size)
{
	if(size < 0 || UseCmm.cmmCount >= 1)
	{	
		printf("User Alloc size is below zero or Cmm has been used by others!\n");
		return NULL;
	}

	CODEC_MEM_ALLOC_ARG codec_mem_alloc_arg;

	int cmm_fd = open(CMM_DRIVER_NAME, O_RDWR | O_NDELAY);

	if(cmm_fd < 0)
	{
		printf("CMM Device open error!\n");
		return NULL;
	}

	uint8_t* cached_addr = (uint8_t*)mmap(0, S3C_CMM_CODEC_CACHED_MEM_SIZE,PROT_READ | PROT_WRITE, MAP_SHARED, cmm_fd, 0);

	if(!cached_addr)
	{
		printf("CMM Cahce alloc fail!\n");
		close(cmm_fd);
		return NULL;
	}

	uint8_t* non_cached_addr = (uint8_t*)mmap(0, S3C_CMM_CODEC_NON_CACHED_MEM_SIZE,PROT_READ | PROT_WRITE, MAP_SHARED, cmm_fd, S3C_CMM_CODEC_CACHED_MEM_SIZE);

	if(!cached_addr)
	{
		printf("CMM Cahce alloc fail!\n");
		munmap(cached_addr,S3C_CMM_CODEC_CACHED_MEM_SIZE);
		close(cmm_fd);
		return NULL;
	}

	codec_mem_alloc_arg.buffSize = size;
	codec_mem_alloc_arg.cached_mapped_addr = (unsigned int)cached_addr;
	codec_mem_alloc_arg.non_cached_mapped_addr = (unsigned int)non_cached_addr;
	codec_mem_alloc_arg.cacheFlag = 1; 

	int ret = ioctl(cmm_fd, S3C_CMM_IOCTL_CODEC_MEM_ALLOC, &codec_mem_alloc_arg);

	if (ret == 0) {
		printf("Memory allocation failed, ret = %d\n", ret);
		munmap(cached_addr,S3C_CMM_CODEC_CACHED_MEM_SIZE);
		munmap(non_cached_addr,S3C_CMM_CODEC_NON_CACHED_MEM_SIZE);
		close(cmm_fd);
		return NULL;
	}

	UseCmm.cmmDeviceID = cmm_fd;
	UseCmm.cmmBuffer = (uint8_t*)codec_mem_alloc_arg.out_addr;
	UseCmm.cmmCache = cached_addr;
	UseCmm.cmmNoCache = non_cached_addr;
	UseCmm.cmmSize = size;
	UseCmm.cmmCount = 1;

	return UseCmm.cmmBuffer;
}

//获取CMM的物理地址，以便使用DMA技术
unsigned int CMMPhyAddr(void* buffer)
{
	if(UseCmm.cmmCount !=1 || UseCmm.cmmBuffer != buffer)
	{
		printf("Try use Alloc CMM or Use buffer created by CMM");
		return 0;
	}

	CODEC_CACHE_FLUSH_ARG  codec_cache_flush_arg;
	CODEC_GET_PHY_ADDR_ARG codec_get_phy_addr_arg;

	codec_cache_flush_arg.u_addr = (unsigned int)UseCmm.cmmBuffer;
	codec_cache_flush_arg.size = UseCmm.cmmSize;

	ioctl(UseCmm.cmmDeviceID, S3C_CMM_IOCTL_CODEC_CACHE_FLUSH, &codec_cache_flush_arg);

	codec_get_phy_addr_arg.u_addr = (unsigned int)UseCmm.cmmBuffer;

	ioctl(UseCmm.cmmDeviceID, S3C_CMM_IOCTL_CODEC_GET_PHY_ADDR, &codec_get_phy_addr_arg);

	return codec_get_phy_addr_arg.p_addr;
}

//释放CMM的空间
void CMMFreeMemeory(void* buffer)
{
	if(UseCmm.cmmCount !=1 || UseCmm.cmmBuffer != buffer)
	{
		printf("Try use Alloc CMM or Use buffer created by CMM");
		return;
	}

	CODEC_MEM_FREE_ARG codec_mem_free_arg;
	codec_mem_free_arg.u_addr = (unsigned int)UseCmm.cmmBuffer;

	ioctl(UseCmm.cmmDeviceID, S3C_CMM_IOCTL_CODEC_MEM_FREE, &codec_mem_free_arg);

	munmap(UseCmm.cmmCache,S3C_CMM_CODEC_CACHED_MEM_SIZE);
	munmap(UseCmm.cmmNoCache,S3C_CMM_CODEC_NON_CACHED_MEM_SIZE);

	close(UseCmm.cmmDeviceID);

	memset(&UseCmm,0,sizeof(CmmArg));
}

