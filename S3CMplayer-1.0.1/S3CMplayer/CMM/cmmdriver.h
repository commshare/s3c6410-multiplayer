#ifndef __CMM_DRIVER_H
#define __CMM_DRIVER_H

#include "basetype.h"

#ifdef __cplusplus
extern "C" {
#endif

//申请CMM空间
uint8_t* CMMAllocMemory(int size);
//获取CMM的物理地址，以便使用DMA技术
unsigned int CMMPhyAddr(void* buffer);
//释放CMM的空间
void CMMFreeMemeory(void* buffer);

#ifdef __cplusplus
}
#endif

#endif
