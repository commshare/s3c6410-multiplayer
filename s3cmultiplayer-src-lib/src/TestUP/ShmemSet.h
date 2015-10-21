#ifndef __SHMEM_SET_H
#define __SHMEM_SET_H


# ifdef __cplusplus
extern "C" {
# endif

void *CreateShareMemory(int sharesize,int *shmid);
void DestroyShareMemory(void *shareaddr,int shmid);

# ifdef __cplusplus
}
# endif

#endif
