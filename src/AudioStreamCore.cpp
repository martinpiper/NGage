#if defined(__GCC32__) || defined(__WINS__)
// CMdaAudioOutputStream Not working with EXEDLL target

#include <e32base.h>
#include <e32cons.h>
#include <stdlib.h>
#include <stdio.h>
#include <basched.h>
#include <eikapp.h>
#include <eikdoc.h>
#include <aknnotewrappers.h>	// for CAknInformationNote
#include <eikenv.h>			 // for CEikonEnv
#include <eikdef.h>
#include <e32math.h> 

#include <mda\common\audio.h>
#include <Mda\Client\Utility.h>
#include <Mda\Common\Resource.h>
#include <MdaAudioOutputStream.h>

#include "SampleManager.h"

#include "GameAPI.h"

// CONSTANTS

// waveform generator buffer size
//const TInt KBufferSize = 8192;
//const TInt KBufferSize = 8000;		// Size in bytes, but it is 16 bit output in the real world
//const TInt KBufferSize = 32000;		// Size in bytes, but it is 16 bit output in the real world
// This seems to be the smallest buffer size the phone copes with
// Equivilent to 2 full buffs == 2 frames at 50 fps
// For 16000 Hz
const TInt KBufferSize = 1280;		// Size in bytes, but it is 16 bit output in the real world
// For 8000 Hz
//const TInt KBufferSize = 640;		// Size in bytes, but it is 16 bit output in the real world

// slider max values
const TInt KVolSliderLength	= 10;
const TInt KFreqSliderLength = 10;

//const TInt KInitialVolume = 1;
const TInt KInitialVolume = 4;
//const TInt KInitialVolume = 9;
const TInt KInitialFreq = 5;

class CStreamAudioEngine;
CStreamAudioEngine *iEngine = 0;

CActiveScheduler *scheduler = 0;
volatile bool threadExited = false;
RThread gThreadForSoundSystem;
volatile bool doschedularstop = false;


static void InteruptDebugOut(const char *text)
{
/*
	FILE *fp = fopen("c:\\intdbg.txt","w");
	if (!fp)
	{
		return;
	}
	fprintf(fp,text);
	fflush(fp);
	fclose(fp);
*/
}

static void ThreadDebugOut(const char *text)
{
/*
	FILE *fp = fopen("c:\\thrddbg.txt","w");
	if (!fp)
	{
		return;
	}
	fprintf(fp,text);
	fflush(fp);
	fclose(fp);
*/
}

// FORWARD DECLARATIONS

class CEikonEnv;

//
// CStreamAudioEngine - audio ouput stream engine, uses CMdaAudioOutputStream
// and implements the callback functions from MMdaAudioOutputStreamCallback.
//
class CStreamAudioEngine : public MMdaAudioOutputStreamCallback , public CBase
{
public:
	enum TEngineStatus
	{
		EEngineNotReady,
		EEngineReady,
		EEnginePlaying		
	};

	static CStreamAudioEngine* NewL();
	
	~CStreamAudioEngine();	
	void PlayL();
	void StopL();
	void StopStream()
	{
		if (iStream)
		{
			iStream->Stop();
		}
	}

	// from MMdaAudioOutputStreamCallback
	virtual void MaoscOpenComplete(TInt aError);
	virtual void MaoscBufferCopied(TInt aError, const TDesC8& aBuffer);
	virtual void MaoscPlayComplete(TInt aError);

	// inline gets and sets
	inline TInt Volume();
	inline TInt Frequency();
	inline TInt Waveform();
	inline void SetFrequency(TInt aFreq);
	inline void SetWaveform(TInt aForm);
	inline TBool StreamReady();	
	inline TBool StreamPlaying();	

	void SetVolume(TInt aVol);

protected:
	// protected contructor
	CStreamAudioEngine();
	// 2nd phase constructor
	void ConstructL();
	// displays error messages
	void ShowErrorMsg(TInt aResourceBuf, TInt aError);

private:
	TInt iFreq;				 // frequency
	TInt iVolume;			   // volume
	TInt iVolStep;	  
	volatile TEngineStatus iStatus;	  // engine status

	TPtr8 iDescBuf;			 // a descriptor that encapsulates the two buffers

	TUint8* iGenBuffer;		 // buffer for generated pcm waves

	CMdaAudioOutputStream* iStream;
	TMdaAudioDataSettings iSettings;

	volatile bool mAbort;
};

// INLINES

inline TInt CStreamAudioEngine::Volume() { return iVolume; }
inline TInt CStreamAudioEngine::Frequency() { return iFreq; }
inline void CStreamAudioEngine::SetFrequency(TInt aFreq) { iFreq = aFreq; }
inline TBool CStreamAudioEngine::StreamReady() { return (iStatus == EEngineReady)?ETrue:EFalse; }
inline TBool CStreamAudioEngine::StreamPlaying() { return (iStatus == EEnginePlaying)?ETrue:EFalse; }

CStreamAudioEngine::CStreamAudioEngine()
	: iFreq(KInitialFreq),
	iVolume(KInitialVolume),
	iVolStep(0),
	iStatus(EEngineNotReady),
	iDescBuf(0,0),
	mAbort(false)
{
}

CStreamAudioEngine::~CStreamAudioEngine()
{
//	delete iStream;
//	delete iGenBuffer; 
//	delete iWaveGen;	
}

CStreamAudioEngine* CStreamAudioEngine::NewL()
{
	CStreamAudioEngine* self = new (ELeave) CStreamAudioEngine();
//	CleanupStack::PushL(self);
	self->ConstructL();
//	CleanupStack::Pop(); // self
	return self;
}

void CStreamAudioEngine::ConstructL()
{
	// create new wave generator object and a buffer for it
	iGenBuffer = new (ELeave) TUint8[KBufferSize];

	// create and initialize output stream		  
//	printf("CMdaAudioOutputStream::NewL\n");
	iStream = CMdaAudioOutputStream::NewL(*this);
//	printf("iSettings.Query\n");
//	printf("iStream.Open\n");
	iSettings.iFlags = 	TMdaAudioDataSettings::ENoNetworkRouting;

	iStream->Open(&iSettings);
}

void CStreamAudioEngine::MaoscOpenComplete(TInt aError)
{
//	printf("MaoscOpenComplete %d\n",aError);
	if (aError==KErrNone)
	{
		// set stream properties to 16bit,8KHz mono
//		iStream->SetAudioPropertiesL(TMdaAudioDataSettings::ESampleRate8000Hz, 
//									 TMdaAudioDataSettings::EChannelsMono);
		iStream->SetAudioPropertiesL(TMdaAudioDataSettings::ESampleRate16000Hz, 
									 TMdaAudioDataSettings::EChannelsMono);

//		SampleManager_Init(MIXER_RATE_HZ_8000);
		SampleManager_Init(MIXER_RATE_HZ_16000);

		// set the appropriate volume step. values for the target
		// device are scaled down to get the optimal volume level
#if defined(__WINS__)
		// for emulator (MaxVolume == 65535)
		iVolStep = iStream->MaxVolume() / KVolSliderLength;
#else
		// for target device (MaxVolume == 9)
		iVolStep = iStream->MaxVolume() / (KVolSliderLength-1);
#endif
		iStream->SetVolume(iVolStep * KInitialVolume);		
		
		iStream->SetPriority(EPriorityNormal, EMdaPriorityPreferenceNone);

		iDescBuf.Set(iGenBuffer,KBufferSize,KBufferSize);

		iStatus = EEngineReady;
		for (int i=0;i<iDescBuf.Length();i++)
		{
			iDescBuf[i] = 0;
		}
		PlayL();
	}
}

//static int position = 0;
void CStreamAudioEngine::MaoscBufferCopied(TInt aError, const TDesC8& /*aBuffer*/)
{
	// a continuous stream is being played: fill the buffer and write it to
	// stream. the error value will be KErrNone only if the previous buffer was
	// successfully copied (if Stop has been called, aError is KErrAbort)
	if (aError==KErrNone)
	{		
//		for (int frames=0;frames<((iDescBuf.Length()/2)/160);frames++)
		for (int frames=0;frames<((iDescBuf.Length()/2)/gDirectSound.SamplesToRetire);frames++)
		{
			SampleManager_OnceAFrame();
			SampleManager_VSyncCode();

			signed short *dest;
//			dest = (signed short *) (&iDescBuf[frames*160*2]);
			dest = (signed short *) (&iDescBuf[frames*gDirectSound.SamplesToRetire*2]);
			for (int i=0;i<gDirectSound.SamplesToRetire;i++)
			{
				dest[i] = (signed short) ((((unsigned char *)SampleInteruptBufferA)[i])<<8);
//				dest[i] = (signed short) dest[i];
			}
		}

//		printf("Fill %d\n",position++);
		iStream->WriteL(iDescBuf);

		if (mAbort)
		{
			InteruptDebugOut("DBG: Pre abort\n");
			mAbort = false;
			InteruptDebugOut("DBG: Post abort\n");
			doschedularstop = true;
		}
	}
}

void CStreamAudioEngine::MaoscPlayComplete(TInt /*aError*/)
{
	iStatus = EEngineReady;	
}

void CStreamAudioEngine::PlayL()
{
	if(iStatus == EEngineReady)
	{
		iStatus = EEnginePlaying;

		// write the buffer to server. the playing starts immediately.
		iStream->WriteL(iDescBuf);		
	}
}

void CStreamAudioEngine::StopL()
{
	if (iStatus == EEngineReady || iStatus == EEnginePlaying)
	{
		mAbort = true;
	}
}


/*
-------------------------------------------------------------------------------

	SetVolume

	Description: sets the volume. the CMdaAudioOutputStream::SetVolume() can
				 be used to change the volume both before or during playback.
				 the parameter should be between 0 and the value returned by 
				 CMdaAudioOutputStream::MaxVolume()

	Return value: N/A

-------------------------------------------------------------------------------
*/
void CStreamAudioEngine::SetVolume(TInt aVol)
{	
	iVolume = aVol;

	// if value has changed, pass it directly to audiostream object. 
	if((iVolume * iVolStep) != iStream->Volume())		
		iStream->SetVolume(iVolume * iVolStep);
	
}

void CStreamAudioEngine::ShowErrorMsg(TInt aResourceBuf, TInt aError)
{
	printf("CStreamAudioEngine::ShowErrorMsg(%d,%d);\n",aResourceBuf,aError);
}

int tickcount=0;

static TInt callbacktick(TAny* /*aPtr*/)
{
	// See if this stops the backlight from going off
	User::ResetInactivityTime();

	return 0;
}

static TInt callbacktick2(TAny* /*aPtr*/)
{
	if (doschedularstop)
	{
		InteruptDebugOut("DBG: Pre istream stop\n");
		iEngine->StopStream();
		InteruptDebugOut("DBG: Pre scheduler stop\n");
		doschedularstop = false;
//		scheduler->Stop();
		CActiveScheduler::Stop();
		InteruptDebugOut("DBG: Post scheduler stop\n");
	}

	return 0;
}

_LIT(threadname, "GameAPISoundStreamManager");

static CPeriodic *periodic;
static CPeriodic *periodic2;

void threadfuncfunc(void)
{
	// No need to check as we are a new gThreadForSoundSystem
//	printf("CActiveScheduler scheduler;\n");
	doschedularstop = false;

	scheduler = new CActiveScheduler;
	CActiveScheduler::Install(scheduler);
//	printf("CActiveScheduler::Install(scheduler);\n");
//	printf("new TestActive\n");

	periodic = CPeriodic::NewL(CActive::EPriorityStandard);

	// Nice long wait of about 10 seconds :)
	periodic->Start(1000,10000000,TCallBack(callbacktick,0));


	periodic2 = CPeriodic::NewL(CActive::EPriorityStandard);
	periodic2->Start(1000,100000,TCallBack(callbacktick2,0));

	iEngine = CStreamAudioEngine::NewL();

//	printf("...CStreamAudioEngine::NewL();\n");
//	printf("CActiveScheduler::Start()...\n");

	ThreadDebugOut("THRD: Pre scheduler start\n");

//	scheduler->Start();
	CActiveScheduler::Start();

// Once the sound system calls stop this makes the scheduler fall out from above

	ThreadDebugOut("THRD: Scheduler exit\n");

	CActiveScheduler::Install(0);

	ThreadDebugOut("THRD: Post scheduler cleanup\n");

//	periodic->Deque();
	delete periodic;
	periodic = 0;

	ThreadDebugOut("THRD: post periodic delete\n");

	delete periodic2;
	periodic2 = 0;

	ThreadDebugOut("THRD: post periodic2 delete\n");


	delete iEngine;
	iEngine = 0;

	ThreadDebugOut("THRD: post iengine delete\n");


	delete scheduler;
	scheduler = 0;

	ThreadDebugOut("THRD: post sceduler delete\n");
}

TInt threadfunc(TAny * /*aPtr*/)
{
//	printf("New gThreadForSoundSystem...\n");
//	printf("New cleanup stack...\n");

//	__UHEAP_MARK;
	CTrapCleanup* cleanup=CTrapCleanup::New(); // get clean-up stack
	TRAPD(error,threadfuncfunc()); // more initialization, then do example

	ThreadDebugOut("THRD: Pre cleanup stack delete\n");
	delete cleanup; // destroy clean-up stack
	ThreadDebugOut("THRD: Post cleanup stack delete\n");
//	__UHEAP_MARKEND;

	threadExited = true;

	return 0;
}

// This starts the audio stream and sample manager... Oh how simple it sounds now ;)
// The sequence of events is quite involved.
// 1) Create and start and new gThreadForSoundSystem
// 1.1) Create clean-up stack
// 1.2) Create and install a new CActiveScheduler
// 1.3) Allocate a new sound engine
// 1.3.1) The new sound engine allocates a CMdaAudioOutputStream which needs a CActiveSchedular to work
// 1.4) Start the active scheduler
// 2) MaoscOpenComplete is called as a callback when the audio stream is opened and the active scheduler runs
// 3) While gThreadForSoundSystem is executing wait for the engine to get allocated
// 4) Then wait for the sound engine stream to start playing
void AudioStreamCoreAPIEntry(void)
{
	if (iEngine)
	{
		return;
	}
//	printf("APICoreEntry\n");

	threadExited = false;
	gThreadForSoundSystem.Create(threadname,threadfunc,8192,8192,(32*1024),0);

	gThreadForSoundSystem.SetProcessPriority(EPriorityHigh);
	gThreadForSoundSystem.Resume();

	// Wait for the sound engine to start when the gThreadForSoundSystem runs
	while ( ((volatile void *)iEngine) == 0 )
	{
		User::After(10000);
	}

	// Wait for the stream to start playing
	while( !iEngine->StreamPlaying() )
	{
		User::After(10000);
	}

//	printf("Wait...\n");
}

void AudioStreamCoreAPIExit(void)
{
	if (!iEngine)
	{
		return;
	}

	iEngine->StopL();

	int i=0;
//	while(!iEngine->StreamReady())
	while(threadExited == false)
	{
//		printf("Waiting for shutdown\n");
		User::After(100000);
		i++;
		if (i>100)
		{
			printf("Timeout on sound shutdown...\n");
			User::Exit(-1);
		}
	}

//	printf("Suspend gThreadForSoundSystem\n");
//	gThreadForSoundSystem.Suspend();
	gThreadForSoundSystem.Kill(1);
	gThreadForSoundSystem.Close();
}

#endif
