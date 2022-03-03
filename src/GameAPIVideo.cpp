#if defined(__GCC32__) || defined(__WINS__)
#include <gdi.h>
#include "GameAPIVideo.h"
#include "GameAPI.h"
#include "Blitter.h"
#include <string.h>
#include <AknAppUI.h>

CGameAPIBitmap::CGameAPIBitmap()
{
}

class Fudge : public CAknAppUi
{
public:
	Fudge()
	{
	}
	~Fudge()
	{
	}

	void Tastic(void)
	{
		SetKeyBlockMode(ENoKeyBlock);
	}
};

CGameAPIBitmap* CGameAPIBitmap::NewL(TInt aWidth, TInt aHeight, TDisplayMode aDisplayMode,GameAPI *gameAPI)
{
//	Fudge *thisui = new Fudge();
//	thisui->Tastic();

	CGameAPIBitmap*	self = new(ELeave) CGameAPIBitmap;
//	CleanupStack::PushL(self);
	self->mGameAPI = gameAPI;
	self->ConstructL(aWidth, aHeight, aDisplayMode);
//	CleanupStack::Pop();


	return self;
}

CGameAPIBitmap::~CGameAPIBitmap()
{
//	CGraphicsContext *context = GetContext();
//	context->DiscardFont();
//	context->Device()->ReleaseFont(mFont);

	delete mBitmapContext;
	mBitmapContext = 0;
	delete mBitmapDevice;
	mBitmapDevice = 0;
	delete mBitmap;
	mBitmap = 0;
}


TDisplayMode CGameAPIBitmap::GetDisplayMode(TInt aBpp, TBool aColor)
{
	if (aBpp == 1)
		return EGray2;
	else if (aBpp == 2)
		return EGray4;
	else if (aBpp == 4)
		return aColor ? EColor16 : EGray16;
	else if (aBpp == 8)
		return aColor ? EColor256 : EGray256;
	else if (aBpp == 16)
		return EColor64K;
	else if (aBpp == 12)
		return EColor4K;
	else if (aBpp == 24)
		return EColor16M;
	else if (aBpp == 32)
		return ERgb;

	ASSERT(EFalse);
	return ENone;
}

TInt CGameAPIBitmap::GetBpp(TDisplayMode aDisplayMode)
{
	const TInt bpp[] =
		{
			-1,
			1,
			2,
			4,
			8,
			4,
			8,
			16,
			32,
			32,
			16
		};
	return bpp[aDisplayMode];
}

TInt CGameAPIBitmap::GetColorDepth(TDisplayMode aDisplayMode)
{
	const TInt bpp[] =
		{
			-1,
			1,
			2,
			4,
			8,
			4,
			8,
			16,
			24,
			32,
			12
		};
	return bpp[aDisplayMode];
}

void CGameAPIBitmap::ConstructL(TInt aWidth, TInt aHeight, TDisplayMode aDisplayMode)
{
	mBitmap = new (ELeave) CFbsBitmap;
	mBitmap->Create(TSize(aWidth, aHeight), aDisplayMode);

	mBitmapDevice = CFbsBitmapDevice::NewL(mBitmap);
	mBitmapDevice->CreateContext(mBitmapContext);

	CGraphicsContext *context = GetContext();
	context->Reset();
	context->SetPenColor(TRgb(255,255,255));

	TFontSpec fontspec = TFontSpec();
	fontspec.iFontStyle.SetPosture(EPostureUpright);
//	context->Device()->GetNearestFontInTwips(mFont,fontspec);
//	context->UseFont(mFont);
}

CGameAPIWindow::CGameAPIWindow()
{
}

CGameAPIWindow::~CGameAPIWindow()
{
	Destruct();
}

void CGameAPIWindow::StartDisplay(void)
{
	ConstructL();
}

/*
Below is the sample code for doing this:
enum TAknKeySoundOpcode { EEnableKeyBlock = 10, EDisableKeyBlock = 11 };
EXPORT_C void CAknAppUi::SetKeyBlockMode(TAknKeyBlockMode aMode)
{
	iBlockMode = aMode;
	UpdateKeyBlockMode();
}
void CAknAppUi::UpdateKeyBlockMode()
{
	TRawEvent event;
	TAknKeySoundOpcode opcode = EEnableKeyBlock;
	if (iBlockMode == ENoKeyBlock)
	{
		opcode = EDisableKeyBlock;
	}
	event.Set((TRawEvent::TType)opcode);
	iEikonEnv->WsSession().SimulateRawEvent(event);
}
*/

#if 1
class TBlockMessage : public TWsEvent
{
public:
	void SetEvent(TBool aBlock);
};

void TBlockMessage::SetEvent(TBool /*aBlock*/)
{
	const int opcode = 0x32;
	SetType(opcode);
	SetTimeNow();
	iEventData[0] = 0;
}
#endif

void CGameAPIWindow::ConstructL()
{
	TInt	error;

	error = mWsSession.Connect();
	User::LeaveIfError(error);
	mWsScreen=new(ELeave) CWsScreenDevice(mWsSession);
	User::LeaveIfError(mWsScreen->Construct());
	User::LeaveIfError(mWsScreen->CreateContext(mWindowGc));

	mWsWindowGroup=RWindowGroup(mWsSession);
	User::LeaveIfError(mWsWindowGroup.Construct((TUint32)this));
	mWsWindowGroup.SetOrdinalPosition(0);

	mWsWindow=RWindow(mWsSession);
	User::LeaveIfError(mWsWindow.Construct(mWsWindowGroup, (TUint32)this));
	mWsWindow.Activate();
	mSize = mWsScreen->SizeInPixels();
	mWsWindow.SetSize(mSize);
	mWsWindow.SetVisible(ETrue);

	StartWServEvents();

#if 0
	const TInt target = mWsSession.GetFocusWindowGroup(); 
	//assumes non-block window on top
	TBlockMessage event;
	event.SetEvent(EFalse);
//	mWsSession.SendEventToWindowGroup(target, event);
	mWsSession.SimulateRawEvent(event);
#endif
#if 0
	TRawEvent event;
	event.Set((TRawEvent::TType)11);
	mWsSession.SimulateRawEvent(event);
#endif
	TRawEvent event;
	event.Set((TRawEvent::TType)0x33);
	mWsSession.SimulateRawEvent(event);
}

void CGameAPIWindow::Destruct()
{
	if (mWsWindow.WsHandle())
		mWsWindow.Close();

	if (mWsWindowGroup.WsHandle())
		mWsWindowGroup.Close();

	delete mWindowGc;
	mWindowGc = 0;

	delete mWsScreen;
	mWsScreen = 0;

	if (mWsSession.WsHandle())
		mWsSession.Close();
}

CGameAPIBitmap *CGameAPIWindow::AllocateBitmapL(TInt aWidth, TInt aHeight, TDisplayMode aDisplayMode,GameAPI *gameAPI)
{
	CGameAPIBitmap*	mameBitmap;

	mGameAPI = gameAPI;

	mameBitmap = CGameAPIBitmap::NewL(aWidth, aHeight, aDisplayMode,gameAPI);

	return mameBitmap;
}

void CGameAPIWindow::WsBlitBitmap(CGameAPIBitmap* aMameBitmap)
{
	mWindowGc->Activate(mWsWindow);

	TRect  rect = TRect(mWsWindow.Size());
	mWsWindow.Invalidate(rect);
	mWsWindow.BeginRedraw(rect);

	if (aMameBitmap)
	{
		mWindowGc->BitBlt(TPoint(0,0), aMameBitmap->FbsBitmap());
	}

	mWsWindow.EndRedraw();
	mWindowGc->Deactivate();
	mWsSession.Flush();
}


void CGameAPIWindow::CloseDisplay()
{
	CancelWServEvents();

	Destruct();
}

TInt TranslateKeyEvent(TInt aKeyCode)
{
	return aKeyCode;
}

static int TranslateScanCode(int scanCode)
{
	switch(scanCode)
	{
		case 14:
			return KEYCODE_LEFT;
		case 15:
			return KEYCODE_RIGHT;
		case 16:
			return KEYCODE_UP;
		case 17:
			return KEYCODE_DOWN;
		case 164:
		case 148:
			return KEYCODE_LEFT_MENU;
		case 165:
		case 149:
			return KEYCODE_RIGHT_MENU;
	}

	return scanCode;
}

void CGameAPIWindow::KeyDownEvent(TInt aScanCode)
{
	

	if (iKeyPressLevel < MAX_KEYPRESS)
	{
		for (TInt i=iKeyPressLevel-1 ; i>=0 ; i--)
		{
			if (iKeyPress[i].iScanCode == aScanCode)
			{
				return;
			}
		}

		iKeyPress[iKeyPressLevel].iScanCode = iKeyPress[iKeyPressLevel].iKeyCode = aScanCode;
		iKeyPressLevel++;
	}
	else
	{
		for (TInt i=iKeyPressLevel-1 ; i>=0 ; i--)
		{
			if (iKeyPress[i].iScanCode == 0)
			{
				iKeyPress[i].iScanCode = iKeyPress[i].iKeyCode = aScanCode;
				break;
			}
		}
	}

	mGameAPI->CallBack_KeyDown(aScanCode);
}

static int thecode = 0;

void CGameAPIWindow::KeyPressEvent(TInt aKeyCode)
{
//	iKeyPress[iKeyPressLevel-1].iKeyCode = aKeyCode;
	thecode = aKeyCode;
}

void CGameAPIWindow::KeyUpEvent(TInt aScanCode)
{
	thecode = 0;

	TInt i;

	for (i=iKeyPressLevel-1 ; i>=0 ; i--)
	{
		if (iKeyPress[i].iScanCode == aScanCode)
		{
			iKeyPress[i].iScanCode = iKeyPress[i].iKeyCode = 0;
			break;
		}
	}

	while(iKeyPressLevel>0 && (iKeyPress[iKeyPressLevel-1].iKeyCode == 0))
		iKeyPressLevel--;

	mGameAPI->CallBack_KeyUp(aScanCode);
}

TInt CGameAPIWindow::CurrentKey()
{
	return thecode;
//	if (iKeyPressLevel == 0)
//		return 0;
//	return iKeyPress[iKeyPressLevel-1].iKeyCode;
}

bool CGameAPIWindow::IsKeyPressed(TInt aScanCode)
{
	if (iKeyPressLevel == 0 || aScanCode == 0)
		return false;

	TInt i;
	for (i=0 ; i<iKeyPressLevel ; i++)
	{
		if (iKeyPress[i].iScanCode == aScanCode)
		{
			return true;
		}
	}	

	return false;
}

void CGameAPIWindow::HandleWsEvent(const TWsEvent& aWsEvent)
{
	if (aWsEvent.Type() == EEventKeyDown)
	{
		KeyDownEvent(TranslateScanCode(aWsEvent.Key()->iScanCode));
	} 
	else if (aWsEvent.Type() == EEventKey)
	{
		KeyPressEvent(TranslateKeyEvent(aWsEvent.Key()->iCode));
	} 
	else if (aWsEvent.Type() == EEventKeyUp)
	{
		KeyUpEvent(TranslateScanCode(aWsEvent.Key()->iScanCode));
	} 
}

void CGameAPIWindow::StartWServEvents()
{
	mWsEventStatus = KRequestPending;
	mWsSession.EventReady(&mWsEventStatus);

	mRedrawEventStatus = KRequestPending;
	mWsSession.RedrawReady(&mRedrawEventStatus);
}

void CGameAPIWindow::CancelWServEvents()
{
	if (mWsEventStatus != KRequestPending)
		mWsSession.EventReadyCancel();

	if (mRedrawEventStatus != KRequestPending)
		mWsSession.RedrawReadyCancel();
}


void CGameAPIWindow::PollWServEvents()
{
	while (mWsEventStatus != KRequestPending)
	{
		mWsSession.GetEvent(mWsEvent);
		HandleWsEvent(mWsEvent);
		mWsEventStatus = KRequestPending;
		mWsSession.EventReady(&mWsEventStatus);
		
	}
	
}

#endif
