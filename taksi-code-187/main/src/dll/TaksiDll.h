//
// TaksiDll.h
// Private to the DLL
// Link this into the process space of the app being recorded.
//

#ifndef _INC_TAKSIDLL
#define _INC_TAKSIDLL
#if _MSC_VER > 1000
#pragma once
#endif

#include "../common/CThreadLockedLong.h"
#include "../common/CWaveDevice.h"

extern CAVIFile g_AVIFile;

struct CTaksiFrameRate
{
	// watch the current frame rate.
	// frame rate calculator
public:
	CTaksiFrameRate()
		: m_dwFreqUnits(0)
		, m_tLastCount(0)
		, m_fLeftoverWeight(0.0)
		, m_dwLastOverhead(0)
		, m_dwOverheadPenalty(0)
	{
	}

	bool InitFreqUnits();
	void InitStart()
	{
		m_tLastCount = 0;
		m_fLeftoverWeight = 0;
		m_dwLastOverhead = 0;
		m_dwOverheadPenalty = 0;
	}

	DWORD CheckFrameRate();

	void EndOfOverheadTime()
	{
		TIMEFAST_t tNow = GetPerformanceCounter();
		m_dwLastOverhead = (DWORD)( tNow - m_tLastCount );
	}

private:
	double CheckFrameWeight( __int64 iTimeDiff );

private:
	DWORD m_dwFreqUnits;		// the units/sec of the TIMEFAST_t timer.

	TIMEFAST_t m_tLastCount;	// when was CheckFrameRate() called?
	float m_fLeftoverWeight;

	// Try to factor out the overhead we induce while recording.
	// Config.m_bUseOverheadCompensation
	DWORD m_dwLastOverhead;
	DWORD m_dwOverheadPenalty;
};
extern CTaksiFrameRate g_FrameRate;

struct CAVIFrame
{
	// an en-queueed frame to be processed.
	// NOTE: Audio block size may not exactly match selected video frame rate. do the best we can.
	CAVIFrame()
		: m_dwFrameDups(0)
	{
	}
	CVideoFrame m_Video;	// the raw (uncompressed) video frame.
#ifdef USE_AUDIO
	WAVE_BLOCKS_t m_AudioStart;		// start of the audio stream
	WAVE_BLOCKS_t m_AudioStop;		// end of the audio stream for these frames.
#endif
	DWORD m_dwFrameDups;	// Dupe the current frame to catch up the frame rate. 0=unused, 1=ideal use.
};

struct CAVIThread
{
	// A worker thread to compress frames and write AVI in the background
public:
	CAVIThread();
	~CAVIThread();

	HRESULT StartAVIThread();
	HRESULT StopAVIThread();

	void WaitForAllFrames();
	CAVIFrame* WaitForNextFrame();
	void SignalFrameAdd( CAVIFrame* pFrame, DWORD dwFrameDups );	// ready to compress/write a raw frame

	int get_FrameBusyCount() const
	{
		return m_iFrameCount.m_lValue;	// how many busy frames are there? (not yet processed)
	}
	void InitFrameQ();

private:
	HRESULT CheckAVIFileReset();
	DWORD ThreadRun();
	static DWORD __stdcall ThreadEntryProc( void* pThis );

#ifdef USE_AUDIO
	static void CALLBACK AudioInProc( HWAVEIN hwi, UINT uMsg, DWORD_PTR dwInstance, DWORD_PTR dwParam1, DWORD_PTR dwParam2 );
	HRESULT WriteAVIAudioBlock( LPBYTE pBuffer, DWORD dwBufferSize, bool bClip = false );
	HRESULT WriteAVIAudioBlock();
public:
	MMRESULT StartAudioRecorder()
	{
		return m_AudioInput.Start();
	}
	MMRESULT StopAudioRecorder()
	{
		return m_AudioInput.Stop();
	}
	HRESULT OpenAudioInputDevice( WAVE_DEVICEID_TYPE iWaveDeviceId, CWaveFormat& WaveFormat );
	HRESULT CloseAudioInputDevice();
	HRESULT WaitAndWriteAVIAudioBlock();
#endif

private:
	CNTHandle m_hThread;
	DWORD m_nThreadId;
	bool m_bStop;

	CNTEvent m_EventDataStart;		// Data ready to work on. AVIThread waits on this
	CNTEvent m_EventDataDone;		// We are done compressing a single frame. foreground waits on this.

	DWORD m_dwTotalFramesProcessed;

	// Make a pool of frames waiting to be compressed/written
	// ASSUME: dont need a critical section on these since i assume int x=y assignment is atomic.
	//  One thread reads the other writes. so it should be safe.
#define AVI_FRAME_QTY		8	// make this variable ??? (full screen raw frames are HUGE!)
	CAVIFrame m_aFrames[AVI_FRAME_QTY];		// buffer to keep current video frame 
	int m_iFrameBusyIdx;	// index to Frame ready to compress.
	int m_iFrameFreeIdx;	// index to Frame ready to fill.
	// (must be locked because it is changed by 2 threads)
	CThreadLockedLong m_iFrameCount;	// how many busy frames are there? 

#ifdef USE_AUDIO
// Maybe the audio buffer values should be configurable? At the very least they probably
// shouldn't be constants as codecs other than PCM may need longer buffers.  For PCM A
// shorter buffer would probably be better for keeping the audio in sync for when the
// the wave in recording exhausts the buffers.
#define AUDIO_FRAME_QTY		8
#define AUDIO_BUFFER_MS		100
	CWaveRecorder m_AudioInput;		// Device for Raw PCM audio input. (loopback from output?)
	CWaveHeaderBase m_AudioBuffers[AUDIO_FRAME_QTY];	// keep these buffers for recording.
	DWORD m_AudioBufferSizeBytes;		// Size of audio buffer in bytes
	CThreadLockedLong m_iAudioBufferCount;	// how many audio buffers are ready
	int m_AudioBufferHeadIdx;			// m_AudioBuffers that is time head.
	//WAVE_BLOCKS_t m_AudioBufferSize;	// Size of each m_AudioBuffers in wave format blocks
	//WAVE_BLOCKS_t m_AudioBufferStart;	// Total Time counter for m_AudioBufferHeadIdx from 0 % m_AudioBufferSize
#endif
};
extern CAVIThread g_AVIThread;

//********************************************************

struct CTaksiProcess
{
	// Info about the Process this DLL is bound to.
	// ASSUME The current application is attached to a graphics API.
	// What am i doing to the process? anthing?
public:
	CTaksiProcess()
		: m_dwConfigChangeCount(0)
		, m_pCustomConfig(NULL)
		, m_bIsProcessIgnored(false)
		, m_bIsProcessDesktop(false)
		, m_bStopGAPIs(false)
	{
		m_Stats.InitProcStats();
		m_Stats.m_dwProcessId = ::GetCurrentProcessId();
	}

	bool IsProcPrime() const
	{
		// is this the prime hooked process?
		return( m_Stats.m_dwProcessId == sg_ProcStats.m_dwProcessId );
	}

	bool CheckProcessMaster() const;
	bool CheckProcessIgnored() const;
	void CheckProcessCustom();

	void StopGAPIs();
	void DetachGAPIs();
	HRESULT AttachGAPIs( HWND hWnd );
	bool StartGAPI( TAKSI_GAPI_TYPE eMode );

	bool OnDllProcessAttach();
	bool OnDllProcessDetach();

	int MakeFileName( TCHAR* pszFileName, const TCHAR* pszExt );
	void UpdateStat( TAKSI_PROCSTAT_TYPE eProp );

public:
	CTaksiProcStats m_Stats;	// For display in the Taksi.exe app.
	TCHAR m_szProcessTitleNoExt[_MAX_PATH]; // use as prefix for captured files.
	HANDLE m_hHeap;					// the process heap to allocate on for me.

	CTaksiConfigCustom* m_pCustomConfig; // custom config for this app/process.
	DWORD m_dwConfigChangeCount;		// my last reconfig check id.

	HWND m_hWndHookTry;				// The last window we tried to hook 

	// if set to true, then CBT should not take any action at all.
	bool m_bIsProcessDesktop;
	bool m_bIsProcessIgnored;		// Is Master TAKSI.EXE or special app.
	bool m_bStopGAPIs;				// I'm not the main app anymore. unhook the graphics modes.
};
extern CTaksiProcess g_Proc;

//*******************************************************

// Globals
extern HINSTANCE g_hInst;	// the DLL instance handle for the process.

// perf measure tool
#if 0 // def _DEBUG
#define CLOCK_START(v) TIMEFAST_t _tClock_##v = ::GetPerformanceCounter();
#define CLOCK_STOP(v,pszMsg) LOG_MSG(( pszMsg, ::GetPerformanceCounter() - _tClock_##v ));
#else
#define CLOCK_START(v)
#define CLOCK_STOP(v,pszMsg)
#endif

#endif // _INC_TAKSIDLL
