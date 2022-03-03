#ifndef __SAMPLEMANAGER_H__
#define __SAMPLEMANAGER_H__

#include "agb.h"

#ifdef __cplusplus
extern "C" {
#endif

extern u32 SampleInteruptBufferA[];

// This is the accruacy muiltiplier for sample positions. The smaller the value the fewer frequency steps you will have
#define BASEKHZMULTIPLIER	(1024)
// This defines the hard limit for the maximum number of mixed channels.
// NOTE This must be either 8 or 16. No other values are supported
#ifndef MAXCHANNELS
#define MAXCHANNELS (32)
#endif

#define MIXER_RATE_HZ_5734		(0)
#define MIXER_RATE_HZ_10512		(1)
#define MIXER_RATE_HZ_13378		(2)
#define MIXER_RATE_HZ_16245		(3)
#define MIXER_RATE_HZ_18157		(4)
#define MIXER_RATE_HZ_21024		(5)
#define MIXER_RATE_HZ_31536		(6)
#define MIXER_RATE_HZ_36314		(7)
#define MIXER_RATE_HZ_40134		(8)
#define MIXER_RATE_HZ_42048		(9)

#define MIXER_RATE_HZ_8000		(10)
#define MIXER_RATE_HZ_16000		(11)

// Sample length, pos and loop in this block is length in 32bit chunks. Loop if based from 1 so 0 means no loop
typedef struct DirectSoundChannel
{
	u32		SamplePos;
	u32		SampleLength;
	u32		SampleLoop;
	u32		SampleLoopLatchLength;
	u32*	SampleStart;
	u32		LastTriggerTime;
	u32		Frequency;
	u32		Volume;			// 0 - 256 where 256 = loud
} DirectSoundChannel;

typedef struct DirectSoundBlock
{
	s32		DirectSoundX_ChannelMask;
	int		ChannelA_Blank;
	int		ChannelB_Blank;
	int		SamplesToRetire;

	DirectSoundChannel channel[32];
} DirectSoundBlock;

extern DirectSoundBlock gDirectSound;

extern int sBPMMode;
extern int sBPMCounter;
extern int sBPMCounterLine;
extern int sBPMCounterLineTrigger;
extern int sSeqLatch;

//#include "BinSamples.h"

extern void SampleManager_Init(int MixRate);	/* Sets the mix rate and inits the sample manager */
extern void SampleManager_SetMixRate(int MixRate);	/* Sets the mix rate without stopping samples. Generally don't use this */
extern void SampleManager_Halt(void);	/* Stops everything, samples, music etc */
extern void SampleManager_SetMusicVolume(int volume);
extern void SampleManager_SetEffectsVolume(int volume);
extern void SampleManager_VSyncCode(void);	/* Call this at the same time each frame. The start of the vsync is a good place */
extern void SampleManager_OnceAFrame(void);	/* Call this at any time once per frame. This must not be interupted by SampleManager_VSyncCode() */
extern void Sample_Play(s32 index);	/*Plays a sample based on index */
extern void Sample_PlayVolume(s32 index,s32 volume);	/* Plays a sample based on index and volume (0<=volume<=256) */
extern int Sample_PlayLoop(s32 index);	/* Plays a sample with a loop */
extern int Sample_PlayLoopVolume(s32 index,s32 volume);	/* Go on have a guess */
extern void Sample_StopLoop(int handle);	/* Stops a looping sample with the handle that was passed back from a looped sample play */
extern void Sample_PlayMod(u32 *sample_base,int index,int channel);	/* Plays a sample from a mod file */
extern void Sample_SetSamplesBase(void *samples);	/* Sets the base address for samples */
extern void Mod_Init(u32 *seqdata,u32 *samples);	/* Init a mod file */
extern void XMMod_Init(u32 *seqdata,u32 *samples,u32 *extended_data);	/* Init an extended mod file */
extern void Mod_Stop(void);	/* Stop a mod file */
extern void Mod_Tick(void);	/* Ticks the mod file player which is called from inside the vsync routine */

extern void *Mod_LoadSamples(char *filename,void *database);
extern void *Mod_LoadPatterns(char *filename);
extern void *XMMod_LoadExtended(char *filename);


#ifdef __cplusplus
}      /* extern "C" */
#endif

#endif
