//
// Config.h
// for CTaksiConfig
// Shared with DLL and EXE
//
#ifndef _INC_Config_H
#define _INC_Config_H
#if _MSC_VER > 1000
#pragma once
#endif

enum TAKSI_HOTKEY_TYPE
{
	// Monitor these hot keys.
	TAKSI_HOTKEY_ConfigOpen,
	TAKSI_HOTKEY_HookModeToggle,
	TAKSI_HOTKEY_IndicatorToggle,
	TAKSI_HOTKEY_RecordBegin,
	TAKSI_HOTKEY_RecordPause,
	TAKSI_HOTKEY_RecordStop,
	TAKSI_HOTKEY_Screenshot,
	TAKSI_HOTKEY_SmallScreenshot,
	TAKSI_HOTKEY_QTY,
};

enum TAKSI_CUSTOM_TYPE
{
	TAKSI_CUSTOM_FrameRate,
	TAKSI_CUSTOM_FrameWeight,
	TAKSI_CUSTOM_Pattern,
	TAKSI_CUSTOM_QTY,
};

enum TAKSI_GAPI_TYPE
{
	// enumerate the available modes we support.
	TAKSI_GAPI_NONE = 0,
	TAKSI_GAPI_DESKTOP,	// HWND_DESKTOP
	TAKSI_GAPI_GDI,		// prefer all other modes over this.
	TAKSI_GAPI_OGL,		// a static linked dll, so can get a false positive.
#ifdef USE_DIRECTX8
	TAKSI_GAPI_DX8,
#endif
#ifdef USE_DIRECTX9
	TAKSI_GAPI_DX9,		// highest priority, always pick DX9 (over others) if it is supported.
#endif
	TAKSI_GAPI_QTY,
};

extern LIBSPEC int Str_MakeFilePath( TCHAR* pszPath, int iMaxCount, const TCHAR* pszDir, const TCHAR* pszName );

struct LIBSPEC CTaksiConfigCustom : public CIniObject
{
	// Custom frame rate/weight settings for a specific app.
	// If the DLL binds to this app. then do something special?
#define TAKSI_CUSTOM_SECTION "TAKSI_CUSTOM"

public:
	virtual const char** get_Props() const
	{
		return sm_Props;
	}
	virtual int get_PropQty() const
	{
		return TAKSI_CUSTOM_QTY;
	}
	virtual bool PropSet( int eProp, const char* pszValue );
	virtual int PropGet( int eProp, char* pszValue, int iSizeMax ) const;

public:
	TCHAR  m_szAppId[_MAX_PATH];	// [friendly name] in the INI file.
	TCHAR  m_szPattern[_MAX_PATH];	// id for the process.	TAKSI_CUSTOM_Pattern (Lower case)
	float  m_fFrameRate;	// video frame rate. TAKSI_CUSTOM_FrameRate
	float  m_fFrameWeight;	// this assumes frames come in at a set rate and we dont have to measure them!

	static const char* sm_Props[TAKSI_CUSTOM_QTY+1];

public:
	// Work data.
	struct CTaksiConfigCustom* m_pNext;
};

enum TAKSI_CFGPROP_TYPE
{
	// enum the tags that can go in the config file.
#define ConfigProp(a,b,c)	TAKSI_CFGPROP_##a,
#include "ConfigProps.tbl"
#undef ConfigProp
	TAKSI_CFGPROP_QTY,
};

#pragma pack(4)	// try to be explicit about this since its shared.
struct LIBSPEC CTaksiConfigData
{
	// Interprocess shared data block.
	// NOTE: cant have a constructor for this! since we put it in CTaksiShared SHARED
	// Some params can be set from CGuiConfig.

	void InitConfig();
	void CopyConfig( const CTaksiConfigData& config );

	bool SetHotKey( TAKSI_HOTKEY_TYPE eHotKey, WORD vKey);
	WORD GetHotKey( TAKSI_HOTKEY_TYPE eHotKey ) const;

	HRESULT FixCaptureDir();

public:
	// CAN be set from CGuiConfig
	TCHAR  m_szCaptureDir[_MAX_PATH];	// files go here!
	bool   m_bDebugLog;					// keep log files or not?
	TCHAR  m_szFileNamePostfix[64];		// just a text append to the name.

	float  m_fFrameRateTarget;			// What do we want our movies frame rate to be? (f/sec)
	CVideoCodec m_VideoCodec;			// Video Compression scheme selected
	bool   m_bVideoHalfSize;			// full vs half sized video frames.

	int m_iAudioDevice;	// Audio input device. WAVE_MAPPER = -1, WAVE_DEVICE_NONE = -2. CWaveRecorder
	CWaveFormat m_AudioFormat;	// Format i might want.

	WORD   m_wHotKey[TAKSI_HOTKEY_QTY];	// Virtual keys + HOTKEYF_ALT for the HotKeys
	bool   m_bUseDirectInput;	// Use direct input for key presses. else just keyboard hook
	bool   m_bUseKeyboard;		// Use WM_KEY hotkeys.

	bool   m_bUseOverheadCompensation;	// for computing the frame rate
	bool   m_bGDIFrame;		// record the frame of GDI windows or not ?
	bool   m_bGDICursor;	// record the cursor in GDI windows
	bool   m_abUseGAPI[TAKSI_GAPI_QTY];	// Use this API? true by default

	// CAN NOT be set from CGuiConfig directly
	TCHAR  m_szImageFormatExt[16];	// the screen shot format tag. JPEG = "image/jpeg", PNG = "image/png"
	bool   m_bShowIndicator;
	POINT m_ptMasterWindow;	// previous position of the master EXE window.
	bool   m_bVideoCodecMsg; // have we put up the warning message about the codec yet?
	bool	m_bMasterTopMost;	// TOPMOST
	// DWORD m_dwHelpShown;	// What help messages have been shown so far. throttle help.
};
extern LIBSPEC CTaksiConfigData sg_Config;	// Read from the INI file. and set via CGuiConfig
#pragma pack()

struct LIBSPEC CTaksiConfig : public CIniObject, public CTaksiConfigData
{
	// ALL Params read from the INI file.
#define TAKSI_INI_FILE	"taksi.ini"
#define TAKSI_SECTION	"TAKSI"
public:
	CTaksiConfig();
	~CTaksiConfig();

	bool ReadIniFile();
	bool WriteIniFile();

	CTaksiConfigCustom* CustomConfig_FindAppId( const TCHAR* pszAppId ) const;
	CTaksiConfigCustom* CustomConfig_Lookup( const TCHAR* pszAppId, bool bCreate=true );
	void CustomConfig_DeleteAppId( const TCHAR* pszAppId );

	CTaksiConfigCustom* CustomConfig_FindPattern( const TCHAR* pszProcessfile ) const;

	static CTaksiConfigCustom* CustomConfig_Alloc();
	static void CustomConfig_Delete( CTaksiConfigCustom* pConfig );

	virtual const char** get_Props() const
	{
		return sm_Props;
	}
	virtual int get_PropQty() const
	{
		return TAKSI_CFGPROP_QTY;
	}
	virtual bool PropSet( int eProp, const char* pValue );
	virtual int PropGet( int eProp, char* pszValue, int iSizeMax ) const;

public:
	static const char* sm_Props[TAKSI_CFGPROP_QTY+1];

	// NOTE: NOT interprocess shared memory. so each dll instance must re-read from INI ??
	CTaksiConfigCustom* m_pCustomList;	// linked list. 
};

#endif // _INC_Config_H
