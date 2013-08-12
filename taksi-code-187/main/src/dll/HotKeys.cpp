//
// HotKeys.cpp
//
#include "../stdafx.h"
#include "TaksiDll.h"
#include "HotKeys.h"
#include <commctrl.h>	// HOTKEYF_ALT

CTaksiHotKeys g_HotKeys;		// what does the user want to do?
CTaksiKeyboard g_UserKeyboard;		// keyboard hook handle. if i cant hook DI. Just for this process.

#ifdef USE_DIRECTI
#define DIRECTINPUT_VERSION 0x0800
#include <dinput.h>

CTaksiDI g_UserDI;	// DirectInput wrapper.

typedef HRESULT (WINAPI *DIRECTINPUT8CREATE)(HINSTANCE, DWORD, REFIID, LPVOID, IDirectInput8**);
static DIRECTINPUT8CREATE s_DirectInput8Create = NULL;

static IRefPtr<IDirectInput8> s_lpDI;
static IRefPtr<IDirectInputDevice8> s_lpDIDevice;
#endif

//**************************************************************************************

#ifdef USE_DIRECTI

CTaksiDI::CTaksiDI()
	: m_bSetup(false)
{
	ZeroMemory(m_abHotKey,sizeof(m_abHotKey));
}

HRESULT CTaksiDI::SetupDirectInput()
{
	m_bSetup = false;
	if (!FindDll(_T("dinput8.dll"))) 
	{
		HRESULT hRes = LoadDll(_T("dinput8.dll"));
		if ( IS_ERROR(hRes))
		{
			return hRes;
		}
	}

	DEBUG_MSG(("SetupDirectInput: using DirectInput." LOG_CR ));

	s_DirectInput8Create = (DIRECTINPUT8CREATE)GetProcAddress("DirectInput8Create");
	if (!s_DirectInput8Create) 
	{
		HRESULT hRes = HRes_GetLastErrorDef( HRESULT_FROM_WIN32(ERROR_CALL_NOT_IMPLEMENTED));
		LOG_MSG(( "SetupDirectInput: lookup for DirectInput8Create failed. (0x%x)" LOG_CR, hRes ));
		return hRes;
	}

	// why is this translation needed? this will only reliably translate into
	// scan codes for the left keys. why not just use the DIK_ defines in ProcessDirectInut()?
	static const BYTE sm_vKeysExt[ COUNTOF(m_bScanExt) ] = 
	{
		VK_SHIFT,		// HOTKEYF_SHIFT
		VK_CONTROL,		// HOTKEYF_CONTROL
		VK_MENU,		// HOTKEYF_ALT
	};
	for ( int i=0; i<COUNTOF(m_bScanExt); i++ )
	{
		m_bScanExt[i] = ::MapVirtualKey( sm_vKeysExt[i], 0);
	}

	m_bSetup = true;
	DEBUG_MSG(("SetupDirectInput: done." LOG_CR ));
	return S_OK;
}

void CTaksiDI::ProcessDirectInput()
{
	// process keyboard input using DirectInput
	// called inside PresentFrameBegin() ONLY
	if ( !IsValidDll())	// need to call SetupDirectInput()
		return;
	ASSERT( m_bSetup );
	ASSERT( s_DirectInput8Create );

	HRESULT hRes;
	if ( ! s_lpDIDevice.IsValidRefObj())
	{
		// Create device
		hRes = s_DirectInput8Create(g_hInst, DIRECTINPUT_VERSION, 
			IID_IDirectInput8, IREF_GETPPTR(s_lpDI,IDirectInput8), NULL);
		if (FAILED(hRes))
		{
			// DirectInput not available; take appropriate action 
			LOG_MSG(( "DirectInput8Create failed. 0x%x" LOG_CR, hRes ));
			return;
		}
		if (!s_lpDI.IsValidRefObj())
		{
			return;
		}
		hRes = s_lpDI->CreateDevice(GUID_SysKeyboard, 
			IREF_GETPPTR(s_lpDIDevice,IDirectInputDevice8), NULL);
		if (FAILED(hRes))
		{
			LOG_MSG(( "TaksiPresent: s_lpDI->CreateDevice() FAILED. 0x%x" LOG_CR, hRes ));
		}
		if (!s_lpDIDevice.IsValidRefObj())
		{
			return;
		}
		hRes = s_lpDIDevice->SetDataFormat(&c_dfDIKeyboard);
		if (FAILED(hRes))
		{
			LOG_MSG(( "TaksiPresent: s_lpDIDevice->SetDataFormat() failed. 0x%x" LOG_CR, hRes ));
			return;
		} 
		// Acquire device
		hRes = s_lpDIDevice->Acquire();
	}

	char buffer[256]; 
	hRes = s_lpDIDevice->GetDeviceState(sizeof(buffer),(LPVOID)&buffer);
	if (FAILED(hRes))
		return;

	// Need to also handle the right set of keys.  MapVirtualKey() only really 
	// works for getting the left keys (despite what MSDN says).
	static const BYTE sm_diKeys[ COUNTOF(m_bScanExt) ] = 
	{
		DIK_RSHIFT,		// HOTKEYF_SHIFT
		DIK_RCONTROL,	// HOTKEYF_CONTROL
		DIK_RMENU		// HOTKEYF_ALT
	};
	// check for HOTKEYF_SHIFT, HOTKEYF_CONTROL, HOTKEYF_ALT
	BYTE bHotMask = 0;
	for ( int i=0; i<COUNTOF(m_bScanExt); i++ )
	{
		if ( (buffer[ m_bScanExt[i]] & 0x80) || (buffer[ sm_diKeys[i]] & 0x80) ) 
			bHotMask |= (1<<i);
	}

	for ( int i=0; i<TAKSI_HOTKEY_QTY; i++ )
	{
		WORD wHotKey = sg_Config.GetHotKey((TAKSI_HOTKEY_TYPE) i);
		BYTE iScanCode = ::MapVirtualKey( LOBYTE(wHotKey), 0);
		if ( buffer[iScanCode] & 0x80 ) 
		{
			// key down.
			m_abHotKey[i] = true;
			continue;
		}
		// key up.
		if ( ! m_abHotKey[i] )
			continue;
		m_abHotKey[i] = false;
		if ( HIBYTE(wHotKey) != bHotMask )
			continue;

		// action on key up.
		g_HotKeys.ScheduleHotKey((TAKSI_HOTKEY_TYPE) i );
	}
}

void CTaksiDI::CloseDirectInput()
{
	if ( !IsValidDll())
		return;
	if (s_lpDIDevice.IsValidRefObj())
	{
		s_lpDIDevice->Unacquire();
		s_lpDIDevice.ReleaseRefObj();
	}
	s_lpDI.ReleaseRefObj();
}

#endif

//**************************************************************** 

bool CTaksiKeyboard::InstallHookKeys( bool bDummy )
{
	// install keyboard hooks to all threads of this process.
	// ARGS:
	//  bDummy = a null hook just to keep us (this dll) loaded in process space.

	UninstallHookKeys();
	DEBUG_TRACE(( "CTaksiKeyboard::InstallHookKeys(%d)." LOG_CR, bDummy ));

	DWORD dwThreadId = ::GetWindowThreadProcessId( g_Proc.m_Stats.m_hWndCap, NULL );
	m_hHookKeys = ::SetWindowsHookEx( WH_KEYBOARD, ( bDummy ) ? DummyKeyboardProc : KeyboardProc, g_hInst, dwThreadId );

	DEBUG_MSG(( "CTaksiKeyboard::InstallHookKeys(%d)=%08x" LOG_CR, bDummy, (UINT_PTR)m_hHookKeys ));
	return( m_hHookKeys != NULL );
}

void CTaksiKeyboard::UninstallHookKeys(void)
{
	// remove keyboard hooks 
	if (m_hHookKeys == NULL)
		return;
	::UnhookWindowsHookEx( m_hHookKeys );
	LOG_MSG(( "CTaksiKeyboard::UninstallHookKeys 0x%x" LOG_CR, (UINT_PTR)m_hHookKeys ));
	m_hHookKeys = NULL;
}

LRESULT CALLBACK CTaksiKeyboard::DummyKeyboardProc(int code, WPARAM wParam, LPARAM lParam) // static
{
	// do not process message. just pass thru
	return ::CallNextHookEx(g_UserKeyboard.m_hHookKeys, code, wParam, lParam); 
}

LRESULT CALLBACK CTaksiKeyboard::KeyboardProc(int code, WPARAM wParam, LPARAM lParam) // static
{
	// SetWindowsHookEx WH_KEYBOARD
	// NOTE: NO idea what context this is called in!
	if (code==HC_ACTION)
	{
		// NOT using DirectInpout.
		// process the key events 
		// a key was pressed/released. wParam = virtual key code
		if (lParam & (1<<31)) 
		{
			// a key release.
			DEBUG_MSG(( "KeyboardProc HC_ACTION wParam=0%x" LOG_CR, wParam ));
			BYTE bHotMask = g_UserKeyboard.m_bHotMask;
			if (lParam&(1<<29))
				bHotMask |= HOTKEYF_ALT;

			for (int i=0; i<TAKSI_HOTKEY_QTY; i++ )
			{
				WORD wHotKey = sg_Config.GetHotKey((TAKSI_HOTKEY_TYPE)i);
				if (wParam != LOBYTE(wHotKey))
					continue;
				if (bHotMask != HIBYTE(wHotKey))
					continue;
				g_HotKeys.DoHotKey((TAKSI_HOTKEY_TYPE) i );
			}

			switch ( wParam)
			{
			case VK_SHIFT:
				g_UserKeyboard.m_bHotMask &= ~HOTKEYF_SHIFT;
				break;
			case VK_CONTROL:
				g_UserKeyboard.m_bHotMask &= ~HOTKEYF_CONTROL;
				break;
			}
		}
		else
		{
			// a key press.
			switch ( wParam)
			{
			case VK_SHIFT:
				g_UserKeyboard.m_bHotMask |= HOTKEYF_SHIFT;
				break;
			case VK_CONTROL:
				g_UserKeyboard.m_bHotMask |= HOTKEYF_CONTROL;
				break;
			}
		}
	}

	// We must pass the all messages on to CallNextHookEx.
	return ::CallNextHookEx(g_UserKeyboard.m_hHookKeys, code, wParam, lParam);
}

//********************************************************

void CTaksiHotKeys::DoHotKey( TAKSI_HOTKEY_TYPE eHotKey )
{
	// Do the action now or schedule it for later.
	LOG_MSG(( "CTaksiHotKeys::DoHotKey: VKEY_* (%d) pressed." LOG_CR, eHotKey ));

	switch(eHotKey)
	{
	case TAKSI_HOTKEY_ConfigOpen:	// Open the config dialog window.
		sg_Shared.HotKey_ConfigOpen();
		return;
	case TAKSI_HOTKEY_HookModeToggle:
		sg_Shared.HotKey_HookModeToggle();
		return;
	case TAKSI_HOTKEY_IndicatorToggle:
		sg_Shared.HotKey_IndicatorToggle();
		return;
	case TAKSI_HOTKEY_RecordBegin:	// toggle video capturing mode
	case TAKSI_HOTKEY_RecordPause:
	case TAKSI_HOTKEY_RecordStop:
	case TAKSI_HOTKEY_Screenshot:
	case TAKSI_HOTKEY_SmallScreenshot:
		// schedule to be in the PresentFrameBegin() call.
		ScheduleHotKey(eHotKey);
		return;
	}
	// shouldnt get here!
	ASSERT(0);
}

HRESULT CTaksiHotKeys::AttachHotKeysToApp()
{
	// Use DirectInput, if configured so, 
	// Otherwise use thread-specific keyboard hook.
	// Called from PresentFrameBegin()

	ASSERT(!m_bAttachedHotKeys);
	m_bAttachedHotKeys = true;	// keyboard configuration done
	DEBUG_MSG(( "CTaksiProcess::AttachHotKeysToApp" LOG_CR ));

#ifdef USE_DIRECTI
	if (sg_Config.m_bUseDirectInput)
	{
		if ( g_UserDI.SetupDirectInput() == S_OK )
		{
			// install dummy keyboard hook so that we don't get unmapped,
			// when going to exclusive mode (i.e. unhooking CBT)
			g_UserKeyboard.InstallHookKeys(true);
			return S_OK;
		}
		DEBUG_WARN(( "PresentFrameBegin: KEYBOARD INIT: Unable to load dinput8.dll." LOG_CR));
	}
#endif
	// if we're not done at this point, use keyboard hook
	// install keyboard hook 
	if ( sg_Config.m_bUseKeyboard )
	{
		if ( ! g_UserKeyboard.InstallHookKeys(false))
		{
			return HRes_GetLastErrorDef(MK_E_MUSTBOTHERUSER);
		}
	}
	return S_OK;
}

void CTaksiHotKeys::DetachHotKeys()
{
#ifdef USE_DIRECTI
	// release DirectInput objects 
	g_UserDI.CloseDirectInput();
#endif
	g_UserKeyboard.UninstallHookKeys();	// uninstall keyboard hook
	m_bAttachedHotKeys = false;
}
