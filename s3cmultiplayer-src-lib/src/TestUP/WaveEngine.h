#ifndef __WAVE_ENGINE_H
#define __WAVE_ENGINE_H

#include "basetype.h"

#ifdef __cplusplus
extern "C" {
#endif

/********************************************
注意：在一般情况下，不需要使用PauseWave、ResumeWave
以及ResetWave。因为没有数据写入时，音频设备会进入等待
状态，声卡不会发声类似暂停，当有数据写入声卡继续发声
********************************************/

//初始化音频参数
int InitWaveEngine(int rate,int format,int channels);
//播放音频数据
int PlayWave(uint8_t* buffer,int size);
//暂停播放
int PauseWave();
//继续播放
int ResumeWave();
//重置
int ResetWave();
//析构音频设备
void UnitWaveEngine();

//void SetWaveLRChannel();
//设置音量
//void SetWaveSilence(int silence);
void SetWaveVolume(int value);
int GetWaveVolume();


#ifdef __cplusplus
}
#endif

#endif
