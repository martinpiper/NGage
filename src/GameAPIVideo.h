#ifndef __VIDEO_H__
#define __VIDEO_H__

#if defined(__GCC32__) || defined(__WINS__)
#include <e32base.h>
#include <w32std.h>
#else
#include <windows.h>
#endif

#define MAX_KEYPRESS			5

#if defined(__GCC32__) || defined(__WINS__)

class GameAPI;

class CGameAPIBitmap : public CBase
{
public:
	~CGameAPIBitmap();
	static CGameAPIBitmap* NewL(int aWidth, int aHeight, TDisplayMode aDisplayMode,GameAPI *gameAPI);

	CFbsBitmap*			FbsBitmap(){return mBitmap;}

	static TDisplayMode GetDisplayMode(TInt aDepth, TBool aColor = ETrue);
	static TInt			GetBpp(TDisplayMode aDisplayMode);
	static TInt			GetColorDepth(TDisplayMode aDisplayMode);

	CGraphicsContext *GetContext(void)
	{
		return mBitmapContext;
	}

protected:
	CGameAPIBitmap();
	void ConstructL(int aWidth, int aHeight, TDisplayMode aDisplayMode);

	CGraphicsContext*		mBitmapContext;
	CFbsBitmap*				mBitmap;
	CFbsBitmapDevice*		mBitmapDevice;
	GameAPI *mGameAPI;
};

class CGameAPIWindow : public CBase
{
public:
	CGameAPIWindow();
	~CGameAPIWindow();

	void StartDisplay(void);
	void CloseDisplay(void);

	CGameAPIBitmap *AllocateBitmapL(TInt aWidth, TInt aHeight, TDisplayMode aDisplayMode,GameAPI *gameAPI);
	void WsBlitBitmap(CGameAPIBitmap* aMameBitmap);

	TInt CurrentKey();
	bool IsKeyPressed(TInt aScanCode);
	void PollWServEvents();

	TSize Size(void)	{ return mSize; }

protected:
	void ConstructL();
	void Destruct();

	void HandleWsEvent(const TWsEvent& aWsEvent);
	void HandleRedrawEvent(const TWsRedrawEvent& mRedrawEvent);
	void StartWServEvents();
	void CancelWServEvents();

	struct TKeyPress
	{
		TInt	iScanCode;
		TInt	iKeyCode;
	};

	TKeyPress	iKeyPress[MAX_KEYPRESS];
	TInt		iKeyPressLevel;
	void KeyDownEvent(TInt aScanCode);
	void KeyPressEvent(TInt aKeyCode);
	void KeyUpEvent(TInt aScanCode);

	TSize mSize;

	RWsSession			mWsSession;
	RWindowGroup		mWsWindowGroup;
	RWindow				mWsWindow;
	CWsScreenDevice*	mWsScreen;
	CWindowGc*			mWindowGc;

	TRequestStatus		mWsEventStatus;
	TRequestStatus		mRedrawEventStatus;
	TWsEvent			mWsEvent;
	TWsRedrawEvent		mRedrawEvent;

	GameAPI *mGameAPI;
};
#else

class CFbsBitmap
{
public:
	CFbsBitmap()
	{
	}
	~CFbsBitmap()
	{
	}
	
	TUint32 *DataAddress(void)
	{
		return 0;
	}
	
	TSize SizeInPixels(void)
	{
		TSize size;
		size.iWidth = 0;
		size.iHeight = 0;
		return size;
	}
};

class CGameAPIBitmap
{
public:
	~CGameAPIBitmap()
	{
	}
	static CGameAPIBitmap* NewL(int aWidth, int aHeight, int aDisplayMode)
	{
		return new CGameAPIBitmap;
	}

	CFbsBitmap *FbsBitmap(void)
	{
		return &mBitmap;
	}
protected:
	CGameAPIBitmap()
	{
	}
	CFbsBitmap mBitmap;
};

class CGameAPIWindow
{
public:
	CGameAPIWindow()
	{
	}

	~CGameAPIWindow()
	{
	}

	void StartDisplay(void)
	{
	}

	void CloseDisplay(void)
	{
	}

	CGameAPIBitmap *AllocateBitmapL(int aWidth, int aHeight, int aDisplayMode)
	{
		return CGameAPIBitmap::NewL(0,0,0);
	}

	void WsBlitBitmap(void* aMameBitmap)
	{
	}

	int CurrentKey()
	{
		return 0;
	}

	bool IsKeyPressed(int aScanCode)
	{
		return false;
	}

	void PollWServEvents()
	{
	}
};
#endif


#endif			/* __VIDEO_H */
