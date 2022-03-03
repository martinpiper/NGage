#ifndef __GAMEAPICORE_H__
#define __GAMEAPICORE_H__

#if !defined(__GCC32__) && !defined(__WINS__)
#include "Win32StdBase.h"
#endif

extern int gTotalCalculatedAvailableMemory;

extern void GameAPICoreEntry(void);

#endif
