#if defined(__GCC32__) || defined(__WINS__)
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

#endif
#include <stdlib.h>
#include <stdio.h>

#include "GameAPICore.h"

_LIT(KTxtEPOC32EX,"Whoops!");

//#define ALLDEBUG

const int numEntries = 256;
static void *allocedEntries[numEntries];

int gTotalCalculatedAvailableMemory = 0;

static void CoreEntry(void)
{
#ifdef ALLDEBUG
	void *memory = malloc(64*1024);
	if (!memory)
	{
		printf("ERROR: Thread does not have at least 64K free memory\n");
		User::After(10000000);
		User::Exit(-1);
	}
	free(memory);
#endif
	User::CompressAllHeaps();

	int startsize = 1024*1024;
	bool notFull = true;
	int totalEntries = 0;
	int totalAlloced = 0;
	do
	{
		allocedEntries[totalEntries] = malloc(startsize);
		if (allocedEntries[totalEntries])
		{
			totalEntries++;
			totalAlloced += startsize;
		}
		else
		{
			startsize = startsize / 2;
		}
		if (startsize < 16384 || totalEntries >= numEntries)
		{
			notFull = false;
		}
	} while (notFull);

	totalEntries--;
	for (;totalEntries>=0;totalEntries--)
	{
		free(allocedEntries[totalEntries]);
	}

	int biggest;
	int maxfree = User::Available(biggest);
	gTotalCalculatedAvailableMemory = maxfree;

	User::CompressAllHeaps();

	gTotalCalculatedAvailableMemory = totalAlloced;

	GameAPICoreEntry();
} 

#ifdef __WINS__

EXPORT_C TInt InitEmulator() // main function called by the emulator software
{
//	__UHEAP_MARK;
	CTrapCleanup* cleanup=CTrapCleanup::New(); // get clean-up stack
	TRAPD(error,CoreEntry()); // more initialization, then do example
	__ASSERT_ALWAYS(!error,User::Panic(KTxtEPOC32EX,error));
	delete cleanup; // destroy clean-up stack
//	__UHEAP_MARKEND;

	CloseSTDLIB();
	User::Exit(0);

	return KErrNone;
}

int GLDEF_C E32Dll(TDllReason)
{
	return(KErrNone);
}

#else

static volatile bool threadExit = false;

static TInt threadfunc(TAny * /*aPtr*/)
{
//	printf("New thread...\n");
//	printf("New cleanup stack...\n");

//	__UHEAP_MARK;
	CTrapCleanup* cleanup=CTrapCleanup::New(); // get clean-up stack
	TRAPD(error,CoreEntry()); // more initialization, then do example
	delete cleanup; // destroy clean-up stack
//	__UHEAP_MARKEND;

	threadExit = true;

	return 0;
}

_LIT(threadname, "GameAPIMainThread");

static RThread thread;

void makeThreadEntry(void)
{
//	printf("APICoreEntry\n");

	thread.Create(threadname,threadfunc,32768,8192,6*1024*1024,0);

	thread.SetProcessPriority(EPriorityHigh);
	thread.Resume();

	while ( !threadExit )
	{
		User::After(100000);
	}

//	printf("Wait...\n");
}



GLDEF_C TInt E32Main() // main function called by E32
{
//	__UHEAP_MARK;
	CTrapCleanup* cleanup=CTrapCleanup::New(); // get clean-up stack
	User::CompressAllHeaps();
	TRAPD(error,makeThreadEntry()); // more initialization, then do example
//	__ASSERT_ALWAYS(!error,User::Panic(KTxtEPOC32EX,error));
	delete cleanup; // destroy clean-up stack
//	__UHEAP_MARKEND;

	CloseSTDLIB();
	User::Exit(0);

	return KErrNone; // and return
}

#endif
