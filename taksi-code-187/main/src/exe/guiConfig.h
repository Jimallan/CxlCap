//
// guiConfig.h
//
#ifndef _INC_guiConfig_H
#define _INC_guiConfig_H
#if _MSC_VER > 1000
#pragma once
#endif

#include "../common/CWndGDI.h"
#include "../common/CWindow.h"
#include "../common/CWndToolTip.h"
#include "resource.h"

struct CTaksiConfigCustom;
#define IDT_UpdateStats 100	// timer id for updating the stats in real time.

struct CGuiConfig : public CWindowChild
{
	// The config dialog window.
public:
	CGuiConfig();
	static INT_PTR CALLBACK DialogProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
	void SetStatusText( const TCHAR* pszText );
	void OnChanges();

	CTaksiConfigCustom* Custom_FindSelect( int index ) const;
	void Custom_Init( CTaksiConfigCustom* pCfg );
	void Custom_Update( CTaksiConfigCustom* pCfg );
	void Custom_Read();
	bool Custom_ReadHook();
	bool Custom_ReadHook( CTaksiConfigCustom* pCfg );

	void UpdateProcStats( const CTaksiProcStats& stats, DWORD dwMask );
	void UpdateVideoCodec( const CVideoCodec& codec, bool bMessageBox );
	bool UpdateAudioDevices( int uDeviceId );
	bool UpdateAudioCodec( const CWaveFormat& codec );
	void UpdateSettings( const CTaksiConfig& config );

	bool OnCommandCheck( HWND hWndCtrl );
	void OnCommandRestore();
	void OnCommandSave();
	bool OnCommandCaptureBrowse();
	void OnCommandVideoCodecButton();
	void OnCommandAudioCodecButton();
	void OnCommandKeyChange( HWND hControl );
	void OnCommandCustomNewButton();
	void OnCommandCustomDeleteButton();
	void OnCommandCustomKillFocus();

	bool OnTimer( UINT idTimer );
	bool OnNotify( int id, NMHDR* pHead );
	bool OnCommand( int id, int iNotify, HWND hControl );
	bool OnHelp( LPHELPINFO pHelpInfo );
	bool OnInitDialog( HWND hWnd, LPARAM lParam );

	void UpdateGuiTabCur();
	void SetSaveState( bool bChanged );

protected:
	CTaksiConfigCustom* m_pCustomCur;
	bool m_bDataUpdating;

public:
	CWndToolTip m_ToolTips;

	HWND m_hControlTab;
	HWND m_hWndTab[6]; // N tabs
	int m_iTabCur;	// persist this. 0 to COUNTOF(m_hWndTab)

#define GuiConfigControl(a,b,c) HWND m_hControl##a;
#include "GuiConfigControls.tbl"
#undef GuiConfigControl

};
extern CGuiConfig g_GUIConfig;

#endif // _INC_guiConfig_H
