//#pragma warning(disable : 4101)
//#pragma warning(disable : 4018)
//#pragma warning(disable : 4244)

#include "SampleManager.h"
#include "GameAPI.h"

u32 gDirectSound_EffectsVolume;
u32 gDirectSound_MusicVolume;
u32 gDirectSound_SamplePlayMixRate = 16384;

DirectSoundBlock gDirectSound;

static s32 *agss_imported_samples;

int scanlines;

u32 *sSeqData;
u32 *sSampData;
u32 *sExtendedData;
int *sExtSeqData;
int *sExtSampData;
int sModPlayerOn;
int sMixer_SamplesPerFrame;
int sMixer_InteruptRate;
int sMixer_SampleRate;

//#define MAXEVERSAMPLESPERFRAME (272)
//#define MAXEVERSAMPLESPERFRAME (708)
#define MAXEVERSAMPLESPERFRAME (320)
#define NUM_FRAMES (1)


u32 SampleInteruptBufferA[((MAXEVERSAMPLESPERFRAME/4)*NUM_FRAMES)];		// Buffer for the samples
//u32 SampleInteruptBufferB[((MAXEVERSAMPLESPERFRAME/4)*NUM_FRAMES)];		// Buffer for the samples

typedef void (tfunc)(void);

#if MAXCHANNELS == 8
#define SAMPROUTSIZE (0x960/4)
#elif MAXCHANNELS == 16
#define SAMPROUTSIZE (0x1100/4)
#elif MAXCHANNELS == 32
#define SAMPROUTSIZE (0x1f80/4)
#endif

u32 *sampbitroutine;

volatile int samplemanager_count;
volatile int mynopvalue;
int donethecopy=0;

int triggertime;
int lastdone;

// Now stereo music mixing
#if MAXCHANNELS == 8
const static int channelpan[] = {0,4,5,1,2,6,7,3};
#elif MAXCHANNELS == 16
const static int channelpan[] = {0,8,9,1,2,10,11,3,4,12,13,5,6,14,15,7};
#elif MAXCHANNELS == 32
const static int channelpan[] = {0,16,17,1,2,18,19,3,4,20,21,5,6,22,23,7,8,24,25,9,10,26,27,11,12,28,29,13,14,30,31,15};
#endif

#if MAXCHANNELS == 8
const static int channeltoggle[] = {0,4,1,5,2,6,3,7};
#elif MAXCHANNELS == 16
const static int channeltoggle[] = {0,8,1,9,2,10,3,11,4,12,5,13,6,14,7,15};
#elif MAXCHANNELS == 32
const static int channeltoggle[] = {0,16,1,17,2,18,3,19,4,20,5,21,6,22,7,23,8,24,9,25,10,26,11,27,12,28,13,29,14,30,15,31};
#endif

void SampleManager_OnceAFrame8ASM(void);
extern u32 SMManSize8;
void SampleManager_OnceAFrame16ASM(void);
extern u32 SMManSize16;
void SampleManager_OnceAFrame32ASM(void);
extern u32 SMManSize32;

const static int sampleroutinearray[] = { (int) &SMManSize8,(int) &SMManSize16,(int) &SMManSize32 };

/* wait for rasterline */
void waitline(unsigned long /*line*/)
{
//	while(VCOUNT!=line){};
}

// This stops the interupt sample loop and DMAs
void SampleManager_InternalStopSamples(void)
{
	samplemanager_count = -1;

//	*(vu32 *)REG_TM0CNT = 0; // Stop timer0

//	*(vu16 *)REG_SOUNDCNT_H |=0x0000; //DirectSound A + B + output ratio full range
	
//	*(vu32 *)REG_FIFO_A = 0;		// Clear first four samples of bank A
//	*(vu32 *)REG_FIFO_B = 0;		// Clear first four samples of bank B

//	*(vu16 *)REG_DMA1CNT_H=0;	//dma control
//	*(vu16 *)REG_DMA2CNT_H=0;	//dma control

	mynopvalue = 0;
	mynopvalue = 1;
	mynopvalue = 2;
	mynopvalue = 3;
}

// This stops the direct sound channels and the DMAs
void SampleManager_Halt(void)
{
	SampleManager_InternalStopSamples();

	Mod_Stop();

	gDirectSound.DirectSoundX_ChannelMask = 0;
	gDirectSound.ChannelA_Blank = 0;
	gDirectSound.ChannelB_Blank = 0;

}

const u32 sampletimings[] = {
	5734	,96			,2926,
	gDirectSound_SamplePlayMixRate	,176		,1596,
	13378	,224		,1254,
	16245	,272		,1032,
	18157	,304		,924,
	21024	,352		,798,
	31536	,528		,532,
	36314	,608		,462,
	40134	,672		,418,
	42048	,704		,399,

	8000	,160		,0,		// Not used in GBA
	16000	,320		,0,		// Not used in GBA
};

void SampleManager_SetMixRate(int MixRate)
{
	int i;
	int newrate;

	SampleManager_InternalStopSamples();

// 0xf9c4 == 1596 of clock freq == 16.78MHz (16*1024*1024 Hz) / 1596 == gDirectSound_SamplePlayMixRate Hz
// LCD scan rate = 59.727Hz so 10.513KHz/59.727 = Sample rate == 176 (0xB0) samples per frame executed
// or use
// 272 samples/frame which gives... 0xFBF8 and 16245 Hz
// or use
// 368 samples/frame which gives... 0xfd04 and 21979 Hz
// or use
// 736 samples/frame which gives... 0xfe84 and 43959 Hz

// 5734		96		2925.92
// gDirectSound_SamplePlayMixRate	176		1596
// 13378	224		1254.09
// 18157	304		924
// 21024	352		798
// 31536	528		532
// 36314	608		462
// 40134	672		418.03		
// 42048	704		399

// Old values
//			sMixer_SamplesPerFrame = 272;
//			sMixer_InteruptRate = 0xfbf8;
//			sMixer_SampleRate = 16245;


	if (MixRate >=  (int) (sizeof(sampletimings)/sizeof(sampletimings[0]))/3)
	{
		MixRate = 0;
	}

	newrate = sampletimings[(MixRate*3)+0];
	sMixer_SamplesPerFrame = sampletimings[(MixRate*3)+1];
	sMixer_InteruptRate = 65536 - sampletimings[(MixRate*3)+2];

	for (i=0;i<MAXCHANNELS;i++)
	{
		if (gDirectSound.DirectSoundX_ChannelMask & (1<<i))
		{
			gDirectSound.channel[i].Frequency = (gDirectSound.channel[i].Frequency * sMixer_SampleRate) / newrate;
		}
	}

	sMixer_SampleRate = newrate;

//	*(vu16 *)REG_SOUNDCNT_H |=0x9AC0; //DirectSound A + B + output ratio full range
//	*(vu16 *)REG_SOUNDCNT_X |=0x0080; //turn sound chip on
	
//	*(vu32 *)REG_FIFO_A = 0;		// Clear first four samples of bank A
//	*(vu32 *)REG_FIFO_B = 0;		// Clear first four samples of bank B

	gDirectSound.SamplesToRetire = sMixer_SamplesPerFrame;

//	*(vu32 *)REG_TM0CNT = sMixer_InteruptRate | TMR_ENABLE; // Start timer0 with no IF Flag to stop code being run
//	*(vu16 *)REG_IE |= TIMER0_INTR_FLAG; //enable timer 0 irq

	// Clear the buffers without using the DMA since there may be a DMA finishing
	for (i =0;i< ((MAXEVERSAMPLESPERFRAME/4)*NUM_FRAMES);i++)
	{
		SampleInteruptBufferA[i] = 0;
//		SampleInteruptBufferB[i] = 0;
	}

	samplemanager_count = 0;	// Enable our vsync routine
}

void SampleManager_Init(int MixRate)
{
	SampleManager_Halt();
	Mod_Stop();

	triggertime = 0;
	lastdone = 0;

	if (donethecopy == 0)
	{
		donethecopy = 1;

		SampleManager_SetMusicVolume(255);
		SampleManager_SetEffectsVolume(255);
	}


	SampleManager_SetMixRate(MixRate);

	gDirectSound.DirectSoundX_ChannelMask = 0;
	gDirectSound.ChannelA_Blank = 0;
	gDirectSound.ChannelB_Blank = 0;
}

void Sample_SetSamplesBase(void *samples)
{
	agss_imported_samples = (s32 *) samples;
}

void SampleManager_SetMusicVolume(int volume)
{
	if (volume < 0)
	{
		volume = 0;
	}

	if (volume > 255)
	{
		volume = 255;
	}

	gDirectSound_MusicVolume = volume;
}

void SampleManager_SetEffectsVolume(int volume)
{
	if (volume < 0)
	{
		volume = 0;
	}

	if (volume > 255)
	{
		volume = 255;
	}

	gDirectSound_EffectsVolume = volume;
}

int SampleManager_FindFreeChannel(int channel_mask)
{
	int channelfound;
	int i;
	int maxchannels;

	maxchannels = MAXCHANNELS;
	channelfound = -1;

	if (sModPlayerOn)
	{
		for (i=0;i<(int)sSeqData[2];i++)
		{
			channel_mask |= (1<<channelpan[i]);
		}
	}

	// Search for a free channel to use
	for (i=0;i<maxchannels;i++)
	{
		if ( ( (gDirectSound.DirectSoundX_ChannelMask | channel_mask) & (1<<channeltoggle[i])) == 0)
		{
			channelfound = channeltoggle[i];
			break;
		}
	}

	if (channelfound == -1)
	{
		// Search for a free channel to use
		for (i=0;i<maxchannels;i++)
		{
			if ( ( gDirectSound.DirectSoundX_ChannelMask & (1<<channeltoggle[i])) == 0)
			{
				channelfound = channeltoggle[i];
				break;
			}
		}
	}

	if (channelfound == -1)
	{
		int nextbest;
		int besttrig;
		nextbest = -1;
		besttrig = 0;
		for (i=0;i<maxchannels;i++)
		{
			if ( (gDirectSound.DirectSoundX_ChannelMask & (1<<channeltoggle[i])))
			{
				if (gDirectSound.channel[channeltoggle[i]].SampleLoop == 0 && (gDirectSound.channel[channeltoggle[i]].LastTriggerTime < (u32) besttrig || nextbest == -1))
				{
					nextbest = i;
					besttrig = gDirectSound.channel[channeltoggle[i]].LastTriggerTime;
					continue;
				}
			}
		}
		channelfound = channeltoggle[nextbest];
	}

	if (channelfound == -1)
	{
		channelfound = lastdone;
		lastdone++;
		if (lastdone >= maxchannels)
		{
			lastdone = 0;
		}
	}

	return channelfound;
}

void Sample_Play(s32 index)
{
#if 1
	int channelfound;
	DirectSoundChannel *channel;

	channelfound = SampleManager_FindFreeChannel(0);

	gDirectSound.DirectSoundX_ChannelMask &= ~(1<<channelfound);

	channel = gDirectSound.channel + channelfound;

	channel->SampleStart	= (u32 *) agss_imported_samples[0+(index*3)+1];
	channel->SampleLength	= agss_imported_samples[1+(index*3)+1]*BASEKHZMULTIPLIER;
	if (agss_imported_samples[2+(index*3)+1] > 0)
	{
		channel->SampleLoop		= agss_imported_samples[2+(index*3)+1];
		channel->SampleLoopLatchLength	= channel->SampleLength;
	}
	else
	{
		channel->SampleLoop		= 0;
	}
	channel->SamplePos		= 0;
	channel->LastTriggerTime = triggertime++;
//	channel->Frequency		= BASEKHZMULTIPLIER*1.56f;
	if (sMixer_SampleRate > 0)
	{
		channel->Frequency		= (BASEKHZMULTIPLIER*gDirectSound_SamplePlayMixRate)/sMixer_SampleRate;
	}
	channel->Volume			= (256 * gDirectSound_EffectsVolume)/255;

	gDirectSound.DirectSoundX_ChannelMask |= (1<<channelfound);
#endif
}

void Sample_PlayVolume(s32 index,s32 volume)
{
#if 1
	int channelfound;
	DirectSoundChannel *channel;

	if (volume > 256)
	{
		volume = 256;
	}

	if (volume < 0)
	{
		volume = 0;
	}

	channelfound = SampleManager_FindFreeChannel(0);

	gDirectSound.DirectSoundX_ChannelMask &= ~(1<<channelfound);

	channel = gDirectSound.channel + channelfound;

	channel->SampleStart	= (u32 *) agss_imported_samples[0+(index*3)+1];
	channel->SampleLength	= agss_imported_samples[1+(index*3)+1]*BASEKHZMULTIPLIER;
	if (agss_imported_samples[2+(index*3)+1] > 0)
	{
		channel->SampleLoop		= agss_imported_samples[2+(index*3)+1];
		channel->SampleLoopLatchLength	= channel->SampleLength;
	}
	else
	{
		channel->SampleLoop		= 0;
	}
	channel->SamplePos		= 0;
	channel->LastTriggerTime = triggertime++;
//	channel->Frequency		= BASEKHZMULTIPLIER*1.56f;
	if (sMixer_SampleRate > 0)
	{
		channel->Frequency		= (BASEKHZMULTIPLIER*gDirectSound_SamplePlayMixRate)/sMixer_SampleRate;
	}
	channel->Volume			= (volume * gDirectSound_EffectsVolume)/255;

	gDirectSound.DirectSoundX_ChannelMask |= (1<<channelfound);
#endif
}


int Sample_PlayLoop(s32 index)
{
#if 1
	int channelfound;
	DirectSoundChannel *channel;

	channelfound = SampleManager_FindFreeChannel(0);

	gDirectSound.DirectSoundX_ChannelMask &= ~(1<<channelfound);

	channel = gDirectSound.channel + channelfound;

	channel->SampleStart	= (u32 *) agss_imported_samples[0+(index*3)+1];
	channel->SampleLength	= agss_imported_samples[1+(index*3)+1]*BASEKHZMULTIPLIER;
	channel->SampleLoop		= 1;
	channel->SampleLoopLatchLength	= channel->SampleLength;
	channel->SamplePos		= 0;
	channel->LastTriggerTime = triggertime++;
//	channel->Frequency		= BASEKHZMULTIPLIER*1.56f;
	if (sMixer_SampleRate > 0)
	{
		channel->Frequency		= (BASEKHZMULTIPLIER*gDirectSound_SamplePlayMixRate)/sMixer_SampleRate;
	}
	channel->Volume			= (256 * gDirectSound_EffectsVolume)/255;

	gDirectSound.DirectSoundX_ChannelMask |= (1<<channelfound);
#endif
	return channelfound;
}

int Sample_PlayLoopVolume(s32 index,s32 volume)
{
#if 1
	int channelfound;
	DirectSoundChannel *channel;

	if (volume > 256)
	{
		volume = 256;
	}

	if (volume < 0)
	{
		volume = 0;
	}

	channelfound = SampleManager_FindFreeChannel(0);

	gDirectSound.DirectSoundX_ChannelMask &= ~(1<<channelfound);

	channel = gDirectSound.channel + channelfound;

	channel->SampleStart	= (u32 *) agss_imported_samples[0+(index*3)+1];
	channel->SampleLength	= agss_imported_samples[1+(index*3)+1]*BASEKHZMULTIPLIER;
	channel->SampleLoop		= 1;
	channel->SampleLoopLatchLength	= channel->SampleLength;
	channel->SamplePos		= 0;
	channel->LastTriggerTime = triggertime++;
//	channel->Frequency		= BASEKHZMULTIPLIER*1.56f;
	if (sMixer_SampleRate > 0)
	{
		channel->Frequency		= (BASEKHZMULTIPLIER*gDirectSound_SamplePlayMixRate)/sMixer_SampleRate;
	}
	channel->Volume			= (volume * gDirectSound_EffectsVolume)/255;

	gDirectSound.DirectSoundX_ChannelMask |= (1<<channelfound);
#endif
	return channelfound;
}

void Sample_StopLoop(int handle)
{
	if (handle < 0)
	{
		return;
	}
	if (handle >= MAXCHANNELS)
	{
		return;
	}

	gDirectSound.DirectSoundX_ChannelMask &= ~(1<<handle);
}

#define MAXMODCHANNELS (32)

int sSkipTick1;
int sBPMMode;
int sBPMCounter;
int sBPMCounterLine;
int sBPMCounterLineTrigger;
int sSeqLatch;
int sTicker;
int sSeqPosLine;
int sSeqPos;
u32 *sCurSeqData;
int sXMModNumInstruments;
int sNextBreakPos;
int sChannelVolume[MAXMODCHANNELS];
int sVolumeSliderModDelta[MAXMODCHANNELS];
int sLastSampleIndex[MAXMODCHANNELS];
int sLastPeriod[MAXMODCHANNELS];
int sLastFineTune[MAXMODCHANNELS];
int sArpeggioControl[MAXMODCHANNELS];
int sArpeggioControlCount[MAXMODCHANNELS];
int sPortamentoControl[MAXMODCHANNELS];
int sLastSlideToNoteControl[MAXMODCHANNELS];
int sSlideToNoteControl[MAXMODCHANNELS];
int sSlideToNotePeriod[MAXMODCHANNELS];
short sLoadedFineTunes[64];
int sIsXMMod;

void Mod_Stop(void)
{
	int i;

	if (sModPlayerOn == 0)
	{
		return;
	}

	for (i=0;i<(int)sSeqData[2];i++)
	{
		gDirectSound.channel[channelpan[i]].Volume = 0;
	}
	for (i=0;i<(int)sSeqData[2];i++)
	{
		gDirectSound.DirectSoundX_ChannelMask &= ~(1<<channelpan[i]);
	}
	sModPlayerOn = 0;
}

static void DoCommonInit(u32 *seqdata,u32 *samples)
{
	int i;

	sModPlayerOn = 0;
	sSeqData = seqdata;
	sSampData = samples;
	sSkipTick1 = 0;
	sTicker = 0;
	sSeqLatch = 6;
	sBPMMode = 0;
	sBPMCounter = 0;
	sBPMCounterLine = 0;
	sBPMCounterLineTrigger = -1;
	sSeqPos = 0;
	sSeqPosLine = 0;
	sCurSeqData = (u32 *)sSeqData[3+sSeqPos];
	sXMModNumInstruments = 0;

	for (i=0;i<MAXMODCHANNELS;i++)
	{
		sChannelVolume[i] = 64;
		sVolumeSliderModDelta[i] = 0;
		sLastSampleIndex[i] = 0;
		sLastPeriod[i] = 0;
		sLastFineTune[i] = 0;
		sArpeggioControl[i] = 0;
		sArpeggioControlCount[i] = 0;
		sPortamentoControl[i] = 0;
		sLastSlideToNoteControl[i] = 0;
		sSlideToNoteControl[i] = 0;
		sSlideToNotePeriod[i] = 0;
	}
}

void Mod_Init(u32 *seqdata,u32 *samples)
{
	int i;

	Mod_Stop();

	DoCommonInit(seqdata,samples);

	sIsXMMod = 0;	

	for (i=0;i<64;i++)
	{
		sLoadedFineTunes[i] = (short) (sSampData[(i*6)+2] & 15);
	}

	sModPlayerOn = 1;
}

void *Mod_LoadSamples(char *filename,void *database)
{
	s32 *data;
	data = (s32 *) GameAPI::File_Load(filename);
	if (!data)
	{
		return 0;
	}

	int numinstr = data[0];
	int i;
	for (i=0;i<numinstr;i++)
	{
		data[0+(i*6)] = data[1+(i*6)];
		data[1+(i*6)] = data[2+(i*6)];
		data[2+(i*6)] = data[3+(i*6)];
		data[3+(i*6)] = data[4+(i*6)];
		data[4+(i*6)] = data[5+(i*6)];
		data[5+(i*6)] = data[6+(i*6)];

		if (data[i*6] < 0)
		{
			if (!database)
			{
				free(data);
				return 0;
			}

			data[i*6] = -data[i*6];
			data[i*6]--;
			data[i*6] = (s32) (((char *)database) + data[i*6]);
		}
		else
		{
			data[i*6] = (s32) (((char *)data) + data[i*6]);
		}
	}

	return data;
}

void *Mod_LoadPatterns(char *filename)
{
	s32 *data;
	data = (s32 *) GameAPI::File_Load(filename);
	if (!data)
	{
		return 0;
	}

	int numpats = data[0];
	int i;
	for (i=0;i<numpats;i++)
	{
		data[3+i] = (s32) (data[3+i]+((char *)data));
	}

	return data;
}

void *XMMod_LoadExtended(char *filename)
{
	s32 *data;
	data = (s32 *) GameAPI::File_Load(filename);
	if (!data)
	{
		return 0;
	}

	data[0] = (s32) (((char *)data)+data[0]);
	data[1] = (s32) (((char *)data)+data[1]);

	return data;
}

void XMMod_Init(u32 *seqdata,u32 *samples,u32 *extended_data)
{
	int i;

	if (!extended_data)
	{
		Mod_Init(seqdata,samples);
		return;
	}

	Mod_Stop();

	DoCommonInit(seqdata,samples);

	sExtSeqData = (int *) extended_data[0];
	sExtSampData = (int *) extended_data[1];

	sExtendedData = extended_data+2;

	sIsXMMod = 1;

	sSeqLatch = sExtendedData[0];
	sBPMMode = sExtendedData[1];

	sXMModNumInstruments = sExtendedData[3];

	for (i=0;i<64;i++)
	{
		sLoadedFineTunes[i] = 0;
	}
	for (i=0;i<sXMModNumInstruments;i++)
	{
		sLoadedFineTunes[i] = (short) ((int *) sSampData)[(i*6)+2];
	}

	sModPlayerOn = 1;
}

int ModSample_GetVolume(u32 *sample_base,int index)
{
	if (index < 0)
	{
		return 0;
	}
	return sample_base[(index*6)+3];
}

int ModSample_GetFineTune(u32 * /*sample_base*/,int index)
{
	s8 value;
	if (index < 0)
	{
		return 0;
	}

	if (sIsXMMod)
	{
		return (((int) sLoadedFineTunes[index])<<16)>>16;
	}
	else
	{
		value = (s8) (sLoadedFineTunes[index] & 15);
		value = (s8) (value << 4);
		value = (s8) (value >> 4);
	}

	return (int) value;
}

static const int XMPeriodTable[104] = 
{
	907,900,894,887,881,875,868,862,856,850,844,838,832,826,820,814,
	808,802,796,791,785,779,774,768,762,757,752,746,741,736,730,725,
	720,715,709,704,699,694,689,684,678,675,670,665,660,655,651,646,
	640,636,632,628,623,619,614,610,604,601,597,592,588,584,580,575,
	570,567,563,559,555,551,547,543,538,535,532,528,524,520,516,513,
	508,505,502,498,494,491,487,484,480,477,474,470,467,463,460,457,
	453,450,447,443,440,437,434,431
};

static const int XMLinearTable[768] = 
{
	535232,534749,534266,533784,533303,532822,532341,531861,
	531381,530902,530423,529944,529466,528988,528511,528034,
	527558,527082,526607,526131,525657,525183,524709,524236,
	523763,523290,522818,522346,521875,521404,520934,520464,
	519994,519525,519057,518588,518121,517653,517186,516720,
	516253,515788,515322,514858,514393,513929,513465,513002,
	512539,512077,511615,511154,510692,510232,509771,509312,
	508852,508393,507934,507476,507018,506561,506104,505647,
	505191,504735,504280,503825,503371,502917,502463,502010,
	501557,501104,500652,500201,499749,499298,498848,498398,
	497948,497499,497050,496602,496154,495706,495259,494812,
	494366,493920,493474,493029,492585,492140,491696,491253,
	490809,490367,489924,489482,489041,488600,488159,487718,
	487278,486839,486400,485961,485522,485084,484647,484210,
	483773,483336,482900,482465,482029,481595,481160,480726,
	480292,479859,479426,478994,478562,478130,477699,477268,
	476837,476407,475977,475548,475119,474690,474262,473834,
	473407,472979,472553,472126,471701,471275,470850,470425,
	470001,469577,469153,468730,468307,467884,467462,467041,
	466619,466198,465778,465358,464938,464518,464099,463681,
	463262,462844,462427,462010,461593,461177,460760,460345,
	459930,459515,459100,458686,458272,457859,457446,457033,
	456621,456209,455797,455386,454975,454565,454155,453745,
	453336,452927,452518,452110,451702,451294,450887,450481,
	450074,449668,449262,448857,448452,448048,447644,447240,
	446836,446433,446030,445628,445226,444824,444423,444022,
	443622,443221,442821,442422,442023,441624,441226,440828,
	440430,440033,439636,439239,438843,438447,438051,437656,
	437261,436867,436473,436079,435686,435293,434900,434508,
	434116,433724,433333,432942,432551,432161,431771,431382,
	430992,430604,430215,429827,429439,429052,428665,428278,
	427892,427506,427120,426735,426350,425965,425581,425197,
	424813,424430,424047,423665,423283,422901,422519,422138,
	421757,421377,420997,420617,420237,419858,419479,419101,
	418723,418345,417968,417591,417214,416838,416462,416086,
	415711,415336,414961,414586,414212,413839,413465,413092,
	412720,412347,411975,411604,411232,410862,410491,410121,
	409751,409381,409012,408643,408274,407906,407538,407170,
	406803,406436,406069,405703,405337,404971,404606,404241,
	403876,403512,403148,402784,402421,402058,401695,401333,
	400970,400609,400247,399886,399525,399165,398805,398445,
	398086,397727,397368,397009,396651,396293,395936,395579,
	395222,394865,394509,394153,393798,393442,393087,392733,
	392378,392024,391671,391317,390964,390612,390259,389907,
	389556,389204,388853,388502,388152,387802,387452,387102,
	386753,386404,386056,385707,385359,385012,384664,384317,
	383971,383624,383278,382932,382587,382242,381897,381552,
	381208,380864,380521,380177,379834,379492,379149,378807,

	378466,378124,377783,377442,377102,376762,376422,376082,
	375743,375404,375065,374727,374389,374051,373714,373377,
	373040,372703,372367,372031,371695,371360,371025,370690,
	370356,370022,369688,369355,369021,368688,368356,368023,
	367691,367360,367028,366697,366366,366036,365706,365376,
	365046,364717,364388,364059,363731,363403,363075,362747,
	362420,362093,361766,361440,361114,360788,360463,360137,
	359813,359488,359164,358840,358516,358193,357869,357547,
	357224,356902,356580,356258,355937,355616,355295,354974,
	354654,354334,354014,353695,353376,353057,352739,352420,
	352103,351785,351468,351150,350834,350517,350201,349885,
	349569,349254,348939,348624,348310,347995,347682,347368,
	347055,346741,346429,346116,345804,345492,345180,344869,
	344558,344247,343936,343626,343316,343006,342697,342388,
	342079,341770,341462,341154,340846,340539,340231,339924,
	339618,339311,339005,338700,338394,338089,337784,337479,
	337175,336870,336566,336263,335959,335656,335354,335051,
	334749,334447,334145,333844,333542,333242,332941,332641,
	332341,332041,331741,331442,331143,330844,330546,330247,
	329950,329652,329355,329057,328761,328464,328168,327872,
	327576,327280,326985,326690,326395,326101,325807,325513,
	325219,324926,324633,324340,324047,323755,323463,323171,
	322879,322588,322297,322006,321716,321426,321136,320846,
	320557,320267,319978,319690,319401,319113,318825,318538,
	318250,317963,317676,317390,317103,316817,316532,316246,
	315961,315676,315391,315106,314822,314538,314254,313971,
	313688,313405,313122,312839,312557,312275,311994,311712,
	311431,311150,310869,310589,310309,310029,309749,309470,
	309190,308911,308633,308354,308076,307798,307521,307243,
	306966,306689,306412,306136,305860,305584,305308,305033,
	304758,304483,304208,303934,303659,303385,303112,302838,
	302565,302292,302019,301747,301475,301203,300931,300660,
	300388,300117,299847,299576,299306,299036,298766,298497,
	298227,297958,297689,297421,297153,296884,296617,296349,
	296082,295815,295548,295281,295015,294749,294483,294217,
	293952,293686,293421,293157,292892,292628,292364,292100,
	291837,291574,291311,291048,290785,290523,290261,289999,
	289737,289476,289215,288954,288693,288433,288173,287913,
	287653,287393,287134,286875,286616,286358,286099,285841,
	285583,285326,285068,284811,284554,284298,284041,283785,
	283529,283273,283017,282762,282507,282252,281998,281743,
	281489,281235,280981,280728,280475,280222,279969,279716,
	279464,279212,278960,278708,278457,278206,277955,277704,
	277453,277203,276953,276703,276453,276204,275955,275706,
	275457,275209,274960,274712,274465,274217,273970,273722,
	273476,273229,272982,272736,272490,272244,271999,271753,
	271508,271263,271018,270774,270530,270286,270042,269798,
	269555,269312,269069,268826,268583,268341,268099,267857 
};

int ModSample_GetRelativeNote(u32 * /*sample_base*/,int index)
{
	if (!sIsXMMod)
	{
		return 0;
	}
	if (index < 0)
	{
		return 0;
	}

	if (sIsXMMod)
	{
		if (index >= sXMModNumInstruments)
		{
			return 0;
		}
	}

	return (int) ((int *)sExtSampData)[(index*8)+0];
}

void ModSample_SetFreq(int channel,int period)
{
	int freq;

	if (period == 0)
	{
		return;
	}

	if (sIsXMMod)
	{
		if (sExtendedData[2] == 0)
		{
			int note;
			int calcperiod;

			note = period;

			calcperiod = ((120 - note) << 6) - (sLastFineTune[channel] / 2);
			if (calcperiod < 1) calcperiod = 1;

			freq = XMLinearTable[calcperiod % 768] >> (calcperiod / 768);
		}
		else
		{
			int finetune;
			unsigned int rnote;
			unsigned int roct;
			int rfine;
			int i;
			unsigned int per1;
			unsigned int per2;
			int calcperiod;
			int note;

//   Period = (PeriodTab[(Note MOD 12)*8 + FineTune/16]*(1-Frac(FineTune/16)) +
//             PeriodTab[(Note MOD 12)*8 + FineTune/16+1]*(Frac(FineTune/16)))
//            *16/2^(Note DIV 12);
//  (The period is interpolated for finer finetune values)
//   Frequency = 8363*1712/Period;

			note = period;
			note += ModSample_GetRelativeNote((u32 *) sExtSampData,sLastSampleIndex[channel]);
			if (note < 1)
			{
				note = 1;
			}
			if (note > 96)
			{
				note = 96;
			}
			finetune = sLastFineTune[channel];
			rnote = (note % 12) << 3;
			roct = note / 12;
			rfine = finetune / 16;
			i = int(rnote) + rfine + 8;
			if (i < 0) i = 0;
			if (i >= 104) i = 103;
			per1 = XMPeriodTable[i];
			if ( finetune < 0 )
			{
				rfine--;
				finetune = -finetune;
			} else rfine++;
			i = int(rnote)+rfine+8;
			if (i < 0) i = 0;
			if (i >= 104) i = 103;
			per2 = XMPeriodTable[i];
			rfine = finetune & 0x0F;
			per1 *= 16-rfine;
			per2 *= rfine;
			calcperiod = ((per1 + per2) << 1) >> roct;

			freq = (8363*1712)/calcperiod;
		}
	}
	else
	{
		if (sLastFineTune[channel] != 0)
		{
			period -= (period * 1000 * sLastFineTune[channel]) / (16784*8);
		}

		if (period < 0)
		{
			period = 0;
		}

		if (period > 0)
		{
			freq = (7093789 / (period*2));
		}
		else
		{
			freq = 7093789;
		}
	}
	if (sMixer_SampleRate > 0)
	{
		gDirectSound.channel[channelpan[channel]].Frequency = ((freq * BASEKHZMULTIPLIER) / sMixer_SampleRate);
	}
}

void ModSample_SetVolume(int channel,int volume)
{
	u32 volcalc;

	if (volume < 0)
	{
		volume = 0;
	}
	if (volume > 64)
	{
		volume = 64;
	}

	volcalc = (volume * 256) / 64;
	volcalc = (volcalc * gDirectSound_MusicVolume) / 255;

	gDirectSound.channel[channelpan[channel]].Volume = volcalc;
}

void Sample_PlayMod(u32 *sample_base,int index,int channel)
{
	channel = channelpan[channel];

	gDirectSound.DirectSoundX_ChannelMask &= ~(1<<channel);

	if (index < 0)
	{
		return;
	}

	gDirectSound.channel[channel].SampleLength = sample_base[(index*6)+1]*BASEKHZMULTIPLIER;
	if (sample_base[(index*6)+5] <= 2)
	{
		gDirectSound.channel[channel].SampleLoop = 0;
	}
	else
	{
		u32 length;
		gDirectSound.channel[channel].SampleLoop = sample_base[(index*6)+4];
		length = gDirectSound.channel[channel].SampleLoop + sample_base[(index*6)+5];
		if (length > sample_base[(index*6)+1])
		{
			length = sample_base[(index*6)+1];
		}
		gDirectSound.channel[channel].SampleLoopLatchLength = length*BASEKHZMULTIPLIER;
		gDirectSound.channel[channel].SampleLoop = sample_base[(index*6)+4]+1;
	}
	gDirectSound.channel[channel].SamplePos = 0;
	gDirectSound.channel[channel].SampleStart = (u32 *) (sample_base[(index*6)+0]);

	gDirectSound.DirectSoundX_ChannelMask |= (1<<channel);
}

void SampleOff_PlayMod(int channel)
{
	channel = channelpan[channel];

	gDirectSound.DirectSoundX_ChannelMask &= ~(1<<channel);
}

u8 unusedeffectarray[16];

static void ResetEffectsForNote(int channel)
{
	sArpeggioControl[channel] = 0;
	sPortamentoControl[channel] = 0;
	sSlideToNoteControl[channel] = 0;
	sVolumeSliderModDelta[channel] = 0;
}

//               Byte  1   Byte  2   Byte  3   Byte 4
//              --------- --------- --------- ---------
//              7654-3210 7654-3210 7654-3210 7654-3210
//              wwww XXXX xxxxxxxxx yyyy ZZZZ zzzzzzzzz
//
//                  wwwwyyyy ( 8 bits) : sample number
//              XXXXxxxxxxxx (12 bits) : sample 'period'
//              ZZZZzzzzzzzz (12 bits) : effect and argument

static void HandleCode(u32 code,int channel)
{
	u32 samplenum;
	u32 period;
	u32 byte1,byte2,byte3,byte4;
	int effect;

	if (code == 0)
	{
		return;
	}

	byte1 = code & 255;
	byte2 = (code>>8) & 255;
	byte3 = (code>>16) & 255;
	byte4 = (code>>24) & 255;

	samplenum = (byte1 & 240) | ((byte3 >> 4)&15);
	period = byte2 | ((byte1&15)<<8);

	effect = byte3 & 15;

	if (sIsXMMod)
	{
		if (samplenum > (u32) sXMModNumInstruments)
		{
			samplenum = 0;
		}
	}

	if (samplenum > 0)
	{
		sChannelVolume[channel] = ModSample_GetVolume(sSampData,samplenum-1);
		sLastFineTune[channel] = ModSample_GetFineTune(sSampData,samplenum-1);
	}

	if (period > 0 || samplenum > 0)
	{
		ResetEffectsForNote(channel);
	}

	if (sIsXMMod)
	{
		if (!(period >= 1 && period <= 96))
		{
			if (period == 97)
			{
				samplenum = 0;
//				SampleOff_PlayMod(channel);
// Emulate a release on the key
				sVolumeSliderModDelta[channel] = -2;
			}
			period = 0;
		}
	}

	switch(effect)
	{
		case 0x0:
		{
			if (byte4 != 0)
			{
				ResetEffectsForNote(channel);

				sArpeggioControl[channel] = byte4;
				sArpeggioControlCount[channel] = 0;
				unusedeffectarray[effect] = (u8) -1;
			}
			break;
		}
		case 0x1:
		{
			ResetEffectsForNote(channel);
			sPortamentoControl[channel] = -((int)byte4);
			unusedeffectarray[effect] = (u8) -1;
			break;
		}
		case 0x2:
		{
			ResetEffectsForNote(channel);
			sPortamentoControl[channel] = byte4;
			unusedeffectarray[effect] = (u8) -1;
			break;
		}
		case 0x3:
		{
			int delta;
			ResetEffectsForNote(channel);
			if (byte4 != 0)
			{
				sLastSlideToNoteControl[channel] = byte4;
			}
			sSlideToNoteControl[channel] = sLastSlideToNoteControl[channel];
			if (period == 0)
			{
				period = sSlideToNotePeriod[channel];
			}
			if (period == 0)
			{
				sSlideToNoteControl[channel] = 0;
				break;
			}
			sSlideToNotePeriod[channel] = period;
			delta = sSlideToNotePeriod[channel] - sLastPeriod[channel];
			if (delta > sSlideToNoteControl[channel])
			{
				delta = sSlideToNoteControl[channel];
			}
			if (delta < -sSlideToNoteControl[channel])
			{
				delta = -sSlideToNoteControl[channel];
			}
			if (delta == 0)
			{
				sSlideToNoteControl[channel] = 0;
			}
			sLastPeriod[channel] += delta;
			ModSample_SetFreq(channel,sLastPeriod[channel]);
			period = 0;
			// Is this right?
			// It makes the Thalamus music slides sound better so it could be
			if (samplenum-1 == (u32) sLastSampleIndex[channel])
			{
				samplenum = 0;
			}
			unusedeffectarray[effect] = (u8) -1;
			break;
		}
		case 0x4:
		{
			ResetEffectsForNote(channel);
			break;
		}
		case 0x7:
		{
			ResetEffectsForNote(channel);
			break;
		}
		case 0x8:
		{
			ResetEffectsForNote(channel);
			break;
		}
		case 0x9:
		{
			ResetEffectsForNote(channel);
			break;
		}
		case 0x5:
		case 0x6:
		case 0xa:
		{
			int modadd,moddel;

			if (effect == 0xa)
			{
				ResetEffectsForNote(channel);
			}

			moddel = byte4 & 15;
			modadd = (byte4>>4) & 15;
			if (modadd != 0 && moddel != 0)
			{
				sVolumeSliderModDelta[channel] = 0;
				break;
			}
			if (modadd >= 0)
			{
				sVolumeSliderModDelta[channel] = modadd;
			}
			if (moddel > 0)
			{
				sVolumeSliderModDelta[channel] = -moddel;
			}
			unusedeffectarray[effect] = (u8) -1;
			break;
		}
		case 0xc:
		{
			ResetEffectsForNote(channel);
			sChannelVolume[channel] = byte4;
			sVolumeSliderModDelta[channel] = 0;
			unusedeffectarray[effect] = (u8) -1;
			break;
		}
		case 0xd:
		{
			int d,t;
			d = byte4 & 15;
			t = (byte4>>4) & 15;
			sNextBreakPos = (d*10)+t;
			unusedeffectarray[effect] = (u8) -1;
			break;
		}
		case 0xe:
		{
			ResetEffectsForNote(channel);
			int extcommand;
			int argument;
			extcommand = (byte4 >> 4) & 0xf;
			argument = byte4 & 0xf;
			switch (extcommand)
			{
				case 0x5:
					if (samplenum > 0 && samplenum <= 32)
					{
						sLoadedFineTunes[samplenum-1] = (short) argument;
					}
					break;
				case 0xa:
					sChannelVolume[channel] += argument;
					break;
				case 0xb:
					sChannelVolume[channel] += argument;
					break;
				default:
					break;
			}

			break;
		}
		case 0xf:
		{
			int origtime;
			origtime = (sBPMMode * 4 * sBPMCounter * 6)/(50*60*sSeqLatch);
			ResetEffectsForNote(channel);
			if (byte4 >= 1 && byte4 <= 32 /*&& sBPMMode == 0*/)
			{
				unusedeffectarray[effect] = (u8) -1;
// This next line fix went in on a day before a delivery for 8-Kings (19/6/2003) because it made some of the jazz music play properly
//				if (sSeqLatch != (int) byte4)
				{
					sSeqLatch = byte4;
// These next line fixes went in on a day before a delivery for 8-Kings (19/6/2003) because it made some of the jazz music play properly
// Still have to test with some of the older mods that use speed change commands... Like Ural Volga
int delta = sTicker;
int deltaTime = (sBPMMode * 4 * sBPMCounter * 6)/(50*60*sSeqLatch) - origtime;
sTicker = sSeqLatch;
delta = sTicker-delta;
if (sBPMMode != 0)
{
	sBPMCounterLineTrigger += deltaTime; 
}
				}
			}
			/**\todo BPM bit */
			if (byte4 > 32 /*&& sSeqLatch == 6*/)
			{
				unusedeffectarray[effect] = (u8) -2;
				if (sBPMMode != (int) byte4)
				{
					sBPMMode = byte4;
					sBPMCounterLineTrigger = -1; 
				}
			}

			if (byte4 == 0)
			{
				sSeqLatch = 6;
				sBPMMode = 0;
				unusedeffectarray[effect] = (u8) -3;
			}

			break;
		}
		default:
			unusedeffectarray[effect] = 1;
			break;
	}

	ModSample_SetVolume(channel,sChannelVolume[channel]);

	if (samplenum > 0)
	{
		sLastSampleIndex[channel] = samplenum-1;
		if (period != 0)
		{
			sLastPeriod[channel] = period;
			ModSample_SetFreq(channel,period);
		}
		else
		{
			ModSample_SetFreq(channel,sLastPeriod[channel]);
		}
		Sample_PlayMod(sSampData,samplenum-1,channel);
	}
	if (samplenum == 0 && period != 0)
	{
		sLastPeriod[channel] = period;
		ModSample_SetFreq(channel,period);
		/**< \todo Find out if period != and sampnum == 0 should or should not restart the sample? */
		//Sample_PlayMod(sSampData,sLastSampleIndex[channel],channel);
	}
}

// This tick happens at the begining of a line before the new codes are figured out
static void HandleGeneralTick(int channel)
{
	if (sSlideToNoteControl[channel])
	{
		int delta;
		delta = sSlideToNotePeriod[channel] - sLastPeriod[channel];
		if (delta > sSlideToNoteControl[channel])
		{
			delta = sSlideToNoteControl[channel];
		}
		if (delta < -sSlideToNoteControl[channel])
		{
			delta = -sSlideToNoteControl[channel];
		}
		if (delta != 0)
		{
			sLastPeriod[channel] += delta;
			ModSample_SetFreq(channel,sLastPeriod[channel]);
		}
		else
		{
			sSlideToNoteControl[channel] = 0;
		}
	}
	if (sPortamentoControl[channel])
	{
		sLastPeriod[channel] += sPortamentoControl[channel];
		if (sLastPeriod[channel] > 856)
		{
			sLastPeriod[channel] = 856;
		}
		if (sLastPeriod[channel] < 113)
		{
			sLastPeriod[channel] = 113;
		}
		ModSample_SetFreq(channel,sLastPeriod[channel]);
	}
	if (sArpeggioControl[channel] != 0)
	{
		int period;
		int octave;

		octave = 403;
		period = sLastPeriod[channel];
		if (period < 453 && period >= 226)
		{
			octave = 202;
		}
		if (period < 226)
		{
			octave = 101;
		}

		period = sLastPeriod[channel];
		switch(sArpeggioControlCount[channel])
		{
			case 0:
			{
				ModSample_SetFreq(channel,period);
				break;
			}
			case 1:
			{
				int halfsteps;
				halfsteps = (octave * ((sArpeggioControl[channel]>>4)&15)) / 12;
				ModSample_SetFreq(channel,period+halfsteps);
				break;
			}
			case 2:
			{
				int halfsteps;
				halfsteps = (octave * (sArpeggioControl[channel]&15)) / 12;
				ModSample_SetFreq(channel,period+halfsteps);
				break;
			}
		}
		sArpeggioControlCount[channel]++;
		if (sArpeggioControlCount[channel] >= 3)
		{
			sArpeggioControlCount[channel] = 0;
		}
	}

	sChannelVolume[channel] += sVolumeSliderModDelta[channel];

	if (sChannelVolume[channel] > 64)
	{
		sChannelVolume[channel] = 64;
		if (sVolumeSliderModDelta[channel] > 0)
		{
			sVolumeSliderModDelta[channel] = 0;
		}
	}
	if (sChannelVolume[channel] < 0)
	{
		sChannelVolume[channel] = 0;
		if (sVolumeSliderModDelta[channel] < 0)
		{
			sVolumeSliderModDelta[channel] = 0;
		}
	}

	ModSample_SetVolume(channel,sChannelVolume[channel]);
}

void Mod_Tick(void)
{
	int i;

	if (sModPlayerOn == 0)
	{
		return;
	}

	if (sBPMMode)
	{
		sSkipTick1++;
		if (sSkipTick1 == 5)	// Was 6
		{
			sSkipTick1 = 0;
		}
		else
		{
			for (i=0;i<(int) sSeqData[2];i++)
			{
				HandleGeneralTick(i);
			}
		}

		sBPMCounter++;
		if (sBPMCounter >= (int)(50.0 * 60 * 50))
		{
			sBPMCounter -= (int)(50.0 * 60 * 50);
			sBPMCounterLineTrigger = -1;
		}

		if (sBPMCounterLineTrigger >= (int)(50.0 * 60 * 50))
		{
			sBPMCounterLineTrigger = -1;
		}

//		sBPMCounterLine = (sBPMMode * 4 * sBPMCounter * 6)/(60*60*sSeqLatch);
		sBPMCounterLine = (sBPMMode * 4 * sBPMCounter * 6)/(50*60*sSeqLatch);
//		sBPMCounterLine = (sBPMMode * 8 * sBPMCounter * 6)/(50*60*sSeqLatch);
		if (sBPMCounterLine > sBPMCounterLineTrigger)
		{
			sBPMCounterLineTrigger = sBPMCounterLine;
			sTicker = 0;
		}
		else
		{
			sTicker = 2;
		}
	}
	else
	{
		sSkipTick1++;
		if (sSkipTick1 == 5)
		{
			sSkipTick1 = 0;
//			return;
		}

		for (i=0;i<(int) sSeqData[2];i++)
		{
			HandleGeneralTick(i);
		}
	}


	sTicker--;
	if (sTicker <= 0)
	{
		int endseqpos;
		endseqpos = 64;
		// If it's an XM mod then get the seq length from the table
		if (sIsXMMod)
		{
			endseqpos = sExtSeqData[1+sSeqPos];
		}
		sTicker = sSeqLatch;

		sNextBreakPos = -1;

		for (i=0;i<(int) sSeqData[2];i++)
		{
			HandleCode(sCurSeqData[(sSeqPosLine*sSeqData[2])+i],i);
		}

//		i = 3;
//		HandleCode(sCurSeqData[(sSeqPosLine*sSeqData[2])+i],i);

		sSeqPosLine++;
		if (sSeqPosLine >= endseqpos || sNextBreakPos != -1)
		{
			sSeqPosLine = 0;
			sSeqPos++;
			if(sNextBreakPos != -1)
			{
				sSeqPosLine = sNextBreakPos;
			}
			if (sSeqPos >= (int) sSeqData[0])
			{
				int seqstop;
				seqstop = 127;
				if (sIsXMMod)
				{
					seqstop = 511;
				}
				sSeqPos = sSeqData[1];
				if (sSeqData[1] >= (unsigned int) seqstop)
				{
					Mod_Stop();
					return;
				}
			}
			sCurSeqData = (u32 *)sSeqData[3+sSeqPos];
		}
	}
}

void SampleManager_VSyncCode(void)
{
	if (samplemanager_count < 0)
	{
		return;
	}

	if (samplemanager_count >= 0)
	{
		samplemanager_count++;
	}

	if (samplemanager_count == NUM_FRAMES)
	{
		samplemanager_count = 0;
	}
//	*(vu16 *)REG_DMA1CNT_H=0;	//dma control
//	*(vu16 *)REG_DMA2CNT_H=0;	//dma control

//	mynopvalue = 0;
//	mynopvalue = 1;
//	mynopvalue = 2;
//	mynopvalue = 3;

//	*(vu32 *)REG_DMA1SAD=((unsigned long)SampleInteruptBufferA) + (samplemanager_count*gDirectSound.SamplesToRetire);	//dma1 source
//	*(vu32 *)REG_DMA1DAD=0x040000a0; //write to FIFO A address
//	*(vu16 *)REG_DMA1CNT_H=0xb660;	//dma control: DMA enabled+ start on FIFO+32bit+repeat
//	*(vu32 *)REG_DMA2SAD=((unsigned long)SampleInteruptBufferB) + (samplemanager_count*gDirectSound.SamplesToRetire);	//dma2 source
//	*(vu32 *)REG_DMA2DAD=0x040000a4; //write to FIFO B address
//	*(vu16 *)REG_DMA2CNT_H=0xb660;	//dma control: DMA enabled+ start on FIFO+32bit+repeat

	Mod_Tick();
}

#if 0
void SampleManager_OnceAFrame(void)
{
	if (samplemanager_count < 0)
	{
		return;
	}

// The modtick has gone in to the vsync routine
//	Mod_Tick();
//	(*((tfunc *)(&sampbitroutine[1])))();
}
#else

void SampleManager_OnceAFrame(void)
{
	if (samplemanager_count < 0)
	{
		return;
	}
// The modtick has gone in to the vsync routine
//	Mod_Tick();
//	(*((tfunc *)(&sampbitroutine[1])))();
#if MAXCHANNELS == 8
		SampleManager_OnceAFrame8ASM();
#elif MAXCHANNELS == 16
		SampleManager_OnceAFrame16ASM();
#elif MAXCHANNELS == 32
		SampleManager_OnceAFrame32ASM();
#endif
};


// NOTE
// When converting these to ASM remember that the gDirecsound.channels array will alter its size depnding on 8/16 channels
// So you need to convert each one separatly

#define DOSTARTGET(CHANNEL,CHAMASK)	\
		if (active & CHAMASK)	\
		{	\
			pos = gDirectSound.channel[CHANNEL].SamplePos;


#define DOLONGGET(CHANNEL,CHAMASK,DESTSAMP)	\
			temp = (((s8 *)gDirectSound.channel[CHANNEL].SampleStart)[pos/BASEKHZMULTIPLIER] * gDirectSound.channel[CHANNEL].Volume)/256;	\
			temp = temp << 24;	\
			temp = temp >> 24;	\
			DESTSAMP += temp;	\
			pos += frequency[CHANNEL];	\
			if ( pos >= gDirectSound.channel[CHANNEL].SampleLength)	\
			{	\
				if (gDirectSound.channel[CHANNEL].SampleLoop)	\
				{	\
					pos -= gDirectSound.channel[CHANNEL].SampleLength;	\
					pos += (gDirectSound.channel[CHANNEL].SampleLoop - 1)*BASEKHZMULTIPLIER;	\
					gDirectSound.channel[CHANNEL].SampleLength = gDirectSound.channel[CHANNEL].SampleLoopLatchLength;	\
				}	\
				else	\
				{	\
					active &= ~CHAMASK;	\
				}	\
			}

#define DOSHORTGET(CHANNEL,CHAMASK,DESTSAMP)	\
			temp = (((s8 *)gDirectSound.channel[CHANNEL].SampleStart)[pos/BASEKHZMULTIPLIER] * gDirectSound.channel[CHANNEL].Volume)/256;	\
			temp = temp << 24;	\
			temp = temp >> 24;	\
			DESTSAMP += temp;	\
			pos += frequency[CHANNEL];	\

#define DOENDGET(CHANNEL)	\
			gDirectSound.channel[CHANNEL].SamplePos = pos;	\
		}


#define VOICEGET(VOICE)	\
		DOSTARTGET(VOICE,(1<<VOICE));	\
		DOSHORTGET(VOICE,(1<<VOICE),samp1);	\
		DOLONGGET(VOICE,(1<<VOICE),samp2);	\
		DOSHORTGET(VOICE,(1<<VOICE),samp3);	\
		DOLONGGET(VOICE,(1<<VOICE),samp4);	\
		DOENDGET(VOICE);


#undef HARDCHANNELS
#define HARDCHANNELS (8)

void SampleManager_OnceAFrame8ASM(void)
{
	u32	frequency[HARDCHANNELS];
	int i;
	int storepos;
	int active;
	s32 *sampstoreA;
	u32 pos;

	active = gDirectSound.DirectSoundX_ChannelMask;

	storepos = samplemanager_count-1;
	if (storepos >= NUM_FRAMES)
	{
		storepos -= NUM_FRAMES;
	}
	if (storepos < 0)
	{
		storepos += NUM_FRAMES;
	}

	storepos *= gDirectSound.SamplesToRetire;

	sampstoreA = (s32 *)SampleInteruptBufferA;
	sampstoreA += storepos/4;

	for (i=0;i<HARDCHANNELS;i++)
	{
		frequency[i] = gDirectSound.channel[i].Frequency;
	}

	for (i=(gDirectSound.SamplesToRetire/4)-1;i>=0;i--)
	{
		s32 samp1,samp2,samp3,samp4,temp;
		u32 samp;

		if (active & (1 | 2 | 4 | 8 | 16 | 32 | 64 | 128))
		{
			samp1 = 0;
			samp2 = 0;
			samp3 = 0;
			samp4 = 0;

			VOICEGET(0);
			VOICEGET(1);
			VOICEGET(2);
			VOICEGET(3);
			VOICEGET(4);
			VOICEGET(5);
			VOICEGET(6);
			VOICEGET(7);

			if (samp1 > 127) samp1 = 127; else if (samp1 < -127) samp1 = -127;
			if (samp2 > 127) samp2 = 127; else if (samp2 < -127) samp2 = -127;
			if (samp3 > 127) samp3 = 127; else if (samp3 < -127) samp3 = -127;
			if (samp4 > 127) samp4 = 127; else if (samp4 < -127) samp4 = -127;

			samp = (samp1 & 255) | ((samp2 & 255)<<8) | ((samp3 & 255)<<16) | ((samp4 & 255)<<24);

			*sampstoreA++ = samp;
		}
		else
		{
			*sampstoreA++ = 0;
		}
	}

	gDirectSound.DirectSoundX_ChannelMask = active;
}

#undef HARDCHANNELS
#define HARDCHANNELS (16)
void SampleManager_OnceAFrame16ASM(void)
{
	u32	frequency[HARDCHANNELS];
	int i;
	int storepos;
	int active;
	s32 *sampstoreA;
	u32 pos;

	active = gDirectSound.DirectSoundX_ChannelMask;

	storepos = samplemanager_count-1;
	if (storepos >= NUM_FRAMES)
	{
		storepos -= NUM_FRAMES;
	}
	if (storepos < 0)
	{
		storepos += NUM_FRAMES;
	}

	storepos *= gDirectSound.SamplesToRetire;

	sampstoreA = (s32 *)SampleInteruptBufferA;
	sampstoreA += storepos/4;

	for (i=0;i<HARDCHANNELS;i++)
	{
		frequency[i] = gDirectSound.channel[i].Frequency;
	}

	for (i=(gDirectSound.SamplesToRetire/4)-1;i>=0;i--)
	{
		s32 samp1,samp2,samp3,samp4,temp;
		u32 samp;

		if (active & (1 | 2 | 4 | 8 | 16 | 32 | 64 | 128 | 256 | 512 | 1024 | 2048 | 4096 | 8192 | 16384 | 32768))
		{
			samp1 = 0;
			samp2 = 0;
			samp3 = 0;
			samp4 = 0;

			VOICEGET(0);
			VOICEGET(1);
			VOICEGET(2);
			VOICEGET(3);
			VOICEGET(4);
			VOICEGET(5);
			VOICEGET(6);
			VOICEGET(7);

			VOICEGET(8);
			VOICEGET(9);
			VOICEGET(10);
			VOICEGET(11);
			VOICEGET(12);
			VOICEGET(13);
			VOICEGET(14);
			VOICEGET(15);

			if (samp1 > 127) samp1 = 127; else if (samp1 < -127) samp1 = -127;
			if (samp2 > 127) samp2 = 127; else if (samp2 < -127) samp2 = -127;
			if (samp3 > 127) samp3 = 127; else if (samp3 < -127) samp3 = -127;
			if (samp4 > 127) samp4 = 127; else if (samp4 < -127) samp4 = -127;

			samp = (samp1 & 255) | ((samp2 & 255)<<8) | ((samp3 & 255)<<16) | ((samp4 & 255)<<24);

			*sampstoreA++ = samp;
		}
		else
		{
			*sampstoreA++ = 0;
		}
	}

	gDirectSound.DirectSoundX_ChannelMask = active;
}

#undef HARDCHANNELS
#define HARDCHANNELS (32)
void SampleManager_OnceAFrame32ASM(void)
{
	u32	frequency[HARDCHANNELS];
	int i;
	int storepos;
	int active;
	s32 *sampstoreA;
	u32 pos;

	active = gDirectSound.DirectSoundX_ChannelMask;

	storepos = samplemanager_count-1;
	if (storepos >= NUM_FRAMES)
	{
		storepos -= NUM_FRAMES;
	}
	if (storepos < 0)
	{
		storepos += NUM_FRAMES;
	}

	storepos *= gDirectSound.SamplesToRetire;

	sampstoreA = (s32 *)SampleInteruptBufferA;
	sampstoreA += storepos/4;

	for (i=0;i<HARDCHANNELS;i++)
	{
		frequency[i] = gDirectSound.channel[i].Frequency;
	}

	for (i=(gDirectSound.SamplesToRetire/4)-1;i>=0;i--)
	{
		s32 samp1,samp2,samp3,samp4,temp;
		u32 samp;

		if (active & (65535 | (65535<<16)))
		{
			samp1 = 0;
			samp2 = 0;
			samp3 = 0;
			samp4 = 0;

			VOICEGET(0);
			VOICEGET(1);
			VOICEGET(2);
			VOICEGET(3);
			VOICEGET(4);
			VOICEGET(5);
			VOICEGET(6);
			VOICEGET(7);
			VOICEGET(8);
			VOICEGET(9);
			VOICEGET(10);
			VOICEGET(11);
			VOICEGET(12);
			VOICEGET(13);
			VOICEGET(14);
			VOICEGET(15);

			VOICEGET(16);
			VOICEGET(17);
			VOICEGET(18);
			VOICEGET(19);
			VOICEGET(20);
			VOICEGET(21);
			VOICEGET(22);
			VOICEGET(23);
			VOICEGET(24);
			VOICEGET(25);
			VOICEGET(26);
			VOICEGET(27);
			VOICEGET(28);
			VOICEGET(29);
			VOICEGET(30);
			VOICEGET(31);

			if (samp1 > 127) samp1 = 127; else if (samp1 < -127) samp1 = -127;
			if (samp2 > 127) samp2 = 127; else if (samp2 < -127) samp2 = -127;
			if (samp3 > 127) samp3 = 127; else if (samp3 < -127) samp3 = -127;
			if (samp4 > 127) samp4 = 127; else if (samp4 < -127) samp4 = -127;

			samp = (samp1 & 255) | ((samp2 & 255)<<8) | ((samp3 & 255)<<16) | ((samp4 & 255)<<24);

			*sampstoreA++ = samp;
		}
		else
		{
			*sampstoreA++ = 0;
		}
	}

	gDirectSound.DirectSoundX_ChannelMask = active;
}

#endif
