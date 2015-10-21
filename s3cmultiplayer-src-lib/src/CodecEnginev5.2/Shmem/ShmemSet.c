#include <sys/types.h>
#include <sys/fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/mman.h>
#include <fcntl.h>

#include "ShmemSet.h"

#define NULL		0
#define SHM_RDWRONLY 	O_WRONLY

void *CreateShareMemory(int sharesize,int *shmid)
{
	if(sharesize <= 0 || !shmid)
		return NULL;

	*shmid = shmget(IPC_PRIVATE,sharesize,SHM_RDWRONLY|IPC_CREAT);

	if(*shmid < 0)
		return NULL;

	return shmat(*shmid, NULL, 0);
}

void DestroyShareMemory(void *shareaddr,int shmid)
{
	if(!shareaddr || shmid < 0)
		return;

	shmdt(shareaddr);
	shmctl(shmid,IPC_RMID,NULL);	
}
