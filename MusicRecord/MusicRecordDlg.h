#pragma once
#include <AFX.H>
#include <AFXCONV.H>
#include "capture/AudioCapture.h"
#include "wave/WaveSaver.h"
#include "mp3/FileMaker.h"
#include "volumn/AudioVolumeController.h"
#include "mp3/audio-resampler.h"
#include "afxcmn.h"

#define MAX_PROGRESS 300

#define DEVICE_ERROR _T("扬声器检测异常，请确保插入了耳机或喇叭正常！")
#define DEVICE_READY _T("设备已就绪。。。")

enum {
	TIMER_RESET_VOLUMN = 200,
};

enum {
	MSG_SHOW_NOTE = WM_USER + 100,
	MSG_UPDATE_PROGRESS,
};

class CMusicRecordDlg : public CDialogEx, public ICaptureCallback {
	// Construction
public:
	CMusicRecordDlg(CWnd *pParent = nullptr); // standard constructor
	virtual ~CMusicRecordDlg();

	// Dialog Data
	enum { IDD = IDD_MUSICRECORD_DIALOG };

protected:
	virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

	// ICaptureCallback
	virtual void OnDeviceError();
	virtual void OnDeviceReady();
	virtual void OnAudioData(const BYTE *data, UINT len, UINT samples);

private:
	HICON m_hIcon;

	BOOL m_bRecording;
	CString m_strDir;
	CString m_strFile;

	CAudioCapture m_tCapture;
	tAudioParam m_tSrcParam;

	CRITICAL_SECTION m_csSaver;
	CWaveSaver m_tWavSaver;
	tAudioParam m_tDstParam;

	AudioVolumeController m_tVolumnCalc;
	CProgressCtrl m_ProgressVolumn;

	std::shared_ptr<audio_resampler> m_pResample;

protected:
	// Generated message map functions
	virtual BOOL OnInitDialog();
	afx_msg void OnSysCommand(UINT nID, LPARAM lParam);
	afx_msg void OnPaint();
	afx_msg HCURSOR OnQueryDragIcon();
	DECLARE_MESSAGE_MAP()

public:
	afx_msg void OnBnClickedStart();
	afx_msg void OnBnClickedButtonStop();
	afx_msg void OnBnClickedButtonPath();
	afx_msg void OnBnClickedButtonBrowser();
	afx_msg void OnBnClickedButtonHelp();
	afx_msg BOOL OnEraseBkgnd(CDC *pDC);
	afx_msg void OnDestroy();
	afx_msg void OnTimer(UINT_PTR nIDEvent);
	HRESULT OnShowNote(WPARAM wp, LPARAM lp);
	HRESULT OnUpdateProgress(WPARAM wp, LPARAM lp);
};
