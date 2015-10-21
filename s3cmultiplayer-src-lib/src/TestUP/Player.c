#include <SDL.h>
#include "CodecEngine.h"
#include "PlayerEngine.h"
#include "WaveEngine.h"

#include <unistd.h>
#include <sys/mman.h>
#include <alsa/asoundlib.h>
#include <fcntl.h>
#include <math.h>
#include <sys/time.h>


int DispPlay(void *data,int size,void *arg)
{
	SDL_Surface *screen = (SDL_Surface*)arg;

	if(data && size > 0)
	{
		SDL_LockSurface(screen);
		memcpy(screen->pixels,data,size);
		SDL_UnlockSurface(screen);
		SDL_UpdateRect(screen,0,0,0,0);
	}

	return 0;
}

SDL_Rect sysrect[4] = {{0,0,239,135},{240,0,479,135},{0,136,239,479},{240,136,479,239}};

int GetInRect(int x,int y)
{
	int left,top,right,bottom;
	
	int i = 0;

	for(i = 0; i < 4; i++)
	{
		left = sysrect[i].x;
		top = sysrect[i].y;
		right = left + sysrect[i].w;
		bottom = top + sysrect[i].h;

		if((x >= left && x <= right)&&( y >= top && y <= bottom))
			return i;
	}

	return -1;
}

int main(int argc,char* argv[])
{

	if (SDL_Init(SDL_INIT_VIDEO) < 0)
	{
		return -1;
	}

	atexit(SDL_Quit);

	SDL_Surface *screen = SDL_SetVideoMode(480, 272, 0, SDL_ASYNCBLIT|SDL_FULLSCREEN);

	if (screen == NULL)
	{
		printf("Video setmode fail\n");
		return -1;
	}

	SetPlayCallBack(&DispPlay);
	SetPlayUser(screen);

	if(OpenFile(argv[1]) < 0)
	{
		printf("Open File fail\n");
		return -1;
	}

	SDL_Event event;

	double pos = 20;

	while(SDL_WaitEvent(&event))
	{
		if(event.type == SDL_MOUSEBUTTONDOWN)
		{
			int type = GetInRect(event.button.x,event.button.y);

			switch(type)
			{
			case 0:
				Pause();
				break;
			case 1:
				Play();
				break;
			case 2:
				Seek(pos);
				pos += 20;
				break;
			case 3:
				Stop();
				goto end;
				break;
			}
		}
		SDL_Delay(10);
	}
end:
	SDL_FreeSurface(screen);
	SDL_Quit();
	return 0;
}
