# NGage

Some very old N-Gage Symbian related code I found in some very old backups.
This code was used for N-Gage 8-Kings games development ( https://www.gamesthatwerent.com/2020/11/8-kings/ ) and also some GameBoy Advance engine work.



# Code

Looking through this code, as was typical for each new platform, it looks like I grabbed bits of platform specific example code from the SDK to get a small working framework. This was normal practice back in those days because we had to get stuff up an running on each new platform very quickly. This framework has minimal code to provide input, video, audio and then exposed that with very minimal interfaces to cross platform code. This has triggered some very old memories for me, especially the way the application had to use an EXE to overcome the really very silly and restrictive Symbian App limitation of global variables/data.



## src\GameAPICore.cpp

The main entry point seems to be one of either these depending on the build target:
	EXPORT_C TInt InitEmulator()
	GLDEF_C TInt E32Main()
	
Both of these do platform specific initialisation and then eventually call: static void CoreEntry(void)


CoreEntry() seems to be the main start of the cross platform code

## src\GameAPIVideo.cpp

Seems to be a bunch of not very nice Symbian related code for displaying a bitmap window using the bare minimum. Probably cobbled together from one of the Symbian examples.
Uses:
	CGraphicsContext
	CFbsBitmap
	CFbsBitmapDevice
	
## src\AudioStreamCore.cpp

This seems to be even worse Symbian audio example code that simply plays a ring buffer sample. It has precisely two external functions which must have been called by external code, cannot find their usage:
	void AudioStreamCoreAPIEntry(void)
	void AudioStreamCoreAPIExit(void)

Internally the CStreamAudioEngine class uses CMdaAudioOutputStream and implements the callback functions from MMdaAudioOutputStreamCallback.
There also seems to be a couple of periodic timers created, to stop the screen back light from going off and to also kill the audio stream when requested.

CStreamAudioEngine::MaoscBufferCopied() seems to be the main audio callback, it calls:
	SampleManager_OnceAFrame();
	SampleManager_VSyncCode();
It then retires samples from a calculated buffer, using gDirectSound.SamplesToRetire and the actual buffer SampleInteruptBufferA, to the Symbian sample buffer.


## src\SampleManager.cpp

This seems to be fairly cross platform code to handle sample buffer mixing with a notion of several channels.
It calculates and stores sample data into SampleInteruptBufferA, ready for the platform specific audio code to use.
