
// MusicRecordDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MusicRecord.h"
#include "MusicRecordDlg.h"
#include "afxdialogex.h"
#include "AutoLockCS.hpp"
#include "mp3/AudioResample.h"
#include <Dbghelp.h>

#pragma comment(lib, "Dbghelp.lib")

#ifdef _DEBUG
#define new DEBUG_NEW
#endif

#define AV_PANEL_SIZE 8

class CAboutDlg : public CDialogEx {
public:
	CAboutDlg();

	// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

protected:
	virtual void DoDataExchange(CDataExchange *pDX); // DDX/DDV support

	// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD) {}

void CAboutDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()

// CMusicRecordDlg dialog

CMusicRecordDlg::CMusicRecordDlg(CWnd *pParent /*=NULL*/)
	: CDialogEx(CMusicRecordDlg::IDD, pParent),
	  m_tCapture(this),
	  m_bRecording(FALSE),
	  m_strFile()
{
	InitializeCriticalSection(&m_csSaver);
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);

	TCHAR dir[_MAX_PATH] = {};
	SHGetSpecialFolderPath(this->GetSafeHwnd(), dir, CSIDL_DESKTOP, 0);
	m_strDir = dir;
}

CMusicRecordDlg::~CMusicRecordDlg()
{
	DeleteCriticalSection(&m_csSaver);
}

void CMusicRecordDlg::DoDataExchange(CDataExchange *pDX)
{
	CDialogEx::DoDataExchange(pDX);
	DDX_Control(pDX, IDC_PROGRESS_VOLUMN, m_ProgressVolumn);
}

BEGIN_MESSAGE_MAP(CMusicRecordDlg, CDialogEx)
ON_WM_SYSCOMMAND()
ON_WM_PAINT()
ON_WM_QUERYDRAGICON()
ON_BN_CLICKED(IDOK, &CMusicRecordDlg::OnBnClickedStart)
ON_BN_CLICKED(IDC_BUTTON_STOP, &CMusicRecordDlg::OnBnClickedButtonStop)
ON_BN_CLICKED(IDC_BUTTON_PATH, &CMusicRecordDlg::OnBnClickedButtonPath)
ON_BN_CLICKED(IDC_BUTTON_BROWSER, &CMusicRecordDlg::OnBnClickedButtonBrowser)
ON_BN_CLICKED(IDC_BUTTON_HELP, &CMusicRecordDlg::OnBnClickedButtonHelp)
ON_WM_ERASEBKGND()
ON_WM_DESTROY()
ON_WM_TIMER()
ON_MESSAGE(MSG_SHOW_NOTE, OnShowNote)
ON_MESSAGE(MSG_UPDATE_PROGRESS, OnUpdateProgress)
END_MESSAGE_MAP()

// CMusicRecordDlg message handlers

BOOL CMusicRecordDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu *pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL) {
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty()) {
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);  // Set big icon
	SetIcon(m_hIcon, FALSE); // Set small icon

	// TODO: Add extra initialization here
	ModifyStyle(0, WS_MINIMIZEBOX | WS_CLIPCHILDREN);

	bool bInitOk = m_tCapture.Initialize();

	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);
	GetDlgItem(IDOK)->EnableWindow(bInitOk);
	GetDlgItem(IDC_BUTTON_PATH)->SetWindowText(m_strDir);

	if (bInitOk)
		GetDlgItem(IDC_STATIC_NOTE)->SetWindowText(DEVICE_READY);
	else
		GetDlgItem(IDC_STATIC_NOTE)->SetWindowText(DEVICE_ERROR);

	m_ProgressVolumn.SetRange(0, MAX_PROGRESS);
	m_ProgressVolumn.SetPos(0);

	m_tCapture.GetParam(m_tSrcParam);
	m_tDstParam.m_uBitWidth = 16; // AV_SAMPLE_FMT_S16
	m_tDstParam.m_uChannels = 2;
	m_tDstParam.m_uSampleRate = m_tSrcParam.m_uSampleRate;

	return TRUE; // return TRUE  unless you set the focus to a control
}

void CMusicRecordDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX) {
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	} else {
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMusicRecordDlg::OnPaint()
{
	if (IsIconic()) {
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
	} else {
		CDialogEx::OnPaint();
	}
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMusicRecordDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

BOOL CMusicRecordDlg::OnEraseBkgnd(CDC *pDC)
{
	return __super::OnEraseBkgnd(pDC);
}

void CMusicRecordDlg::OnDestroy()
{
	__super::OnDestroy();

	m_tCapture.UnInitialize();

	CAutoLockCS AutoLock(m_csSaver);
	m_tWavSaver.Close();
}

void CMusicRecordDlg::OnTimer(UINT_PTR nIDEvent)
{
	// TODO: Add your message handler code here and/or call default
	if (nIDEvent == TIMER_RESET_VOLUMN) {
		m_ProgressVolumn.SetPos(0);
	}

	__super::OnTimer(nIDEvent);
}

void CMusicRecordDlg::OnBnClickedStart()
{
	WAVEFORMATEX wf = {};
	wf.cbSize = 0;
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = m_tDstParam.m_uChannels;
	wf.nSamplesPerSec = m_tDstParam.m_uSampleRate;
	wf.wBitsPerSample = m_tDstParam.m_uBitWidth;
	wf.nBlockAlign = (wf.nChannels * wf.wBitsPerSample) / 8;
	wf.nAvgBytesPerSec =
		m_tDstParam.m_uSampleRate * m_tDstParam.m_uChannels * (m_tDstParam.m_uBitWidth / 8);

	SYSTEMTIME st;
	GetLocalTime(&st);

	USES_CONVERSION;
	MakeSureDirectoryPathExists(W2A(m_strDir.GetBuffer()));

	m_strFile.Format(_T("%s\\Music-%04d%02d%02d-%02d%02d%02d-.wav"), m_strDir.GetBuffer(),
			 st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);

	bool bOK = false;
	{
		CAutoLockCS AutoLock(m_csSaver);
		bOK = m_tWavSaver.Open(m_strFile.GetBuffer(), &wf);
	}
	if (!bOK) {
		MessageBox(_T("初始化文件失败"), _T("错误"), 0);
		return;
	}

	m_bRecording = TRUE;

	GetDlgItem(IDOK)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(TRUE);
	GetDlgItem(IDC_STATIC_NOTE)->SetWindowText(_T("正在录音。。。"));
}

void CMusicRecordDlg::OnBnClickedButtonStop()
{
	m_bRecording = FALSE;

	{
		CAutoLockCS AutoLock(m_csSaver);
		m_tWavSaver.Close();
	}

	GetDlgItem(IDOK)->EnableWindow(TRUE);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);

	CString note;
	note.Format(_T("录音已保存 %s"), m_strFile.GetBuffer());
	GetDlgItem(IDC_STATIC_NOTE)->SetWindowText(note);
}

void CMusicRecordDlg::OnBnClickedButtonHelp()
{
	CString text = _T("1、如果是笔记本，请确保喇叭能正常播放音乐；\r\n\r\n");
	text += _T("2、如果是台式机，请确保插入了耳机等音频输出设备；\r\n\r\n");
	text += _T("3、为保证录制的音乐音量足够大，需将电脑音量、音乐播放器音量都开到最大音量。\r\n");

	MessageBox(text, _T("帮助"), 0);
}

void CMusicRecordDlg::OnDeviceError()
{
	GetDlgItem(IDOK)->EnableWindow(FALSE);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(FALSE);

	CString note = DEVICE_ERROR;
	SendMessage(MSG_SHOW_NOTE, (WPARAM)note.GetBuffer(), 0);
}

void CMusicRecordDlg::OnDeviceReady()
{
	m_tCapture.GetParam(m_tSrcParam);
	m_tDstParam.m_uBitWidth = 16; // AV_SAMPLE_FMT_S16
	m_tDstParam.m_uChannels = 2;
	m_tDstParam.m_uSampleRate = m_tSrcParam.m_uSampleRate;

	GetDlgItem(IDOK)->EnableWindow(!m_bRecording);
	GetDlgItem(IDC_BUTTON_STOP)->EnableWindow(m_bRecording);

	CString note = DEVICE_READY;
	SendMessage(MSG_SHOW_NOTE, (WPARAM)note.GetBuffer(), 0);
}

void CMusicRecordDlg::OnAudioData(const BYTE *data, UINT len, UINT samples)
{
	uint8_t *dataTemp[AV_PANEL_SIZE] = {};
	int linesize[AV_PANEL_SIZE] = {};
	dataTemp[0] = (uint8_t *)data;
	linesize[0] = len;

	if (m_tVolumnCalc.UpdateVolumeMeter(dataTemp, samples, m_tSrcParam.m_uSampleRate)) {
		float vl = m_tVolumnCalc.GetVolumeMeterLevel();
		int pos = int(vl * float(MAX_PROGRESS));

		if (pos != m_ProgressVolumn.GetPos())
			PostMessage(MSG_UPDATE_PROGRESS, pos, 0);

		KillTimer(TIMER_RESET_VOLUMN);
		SetTimer(TIMER_RESET_VOLUMN, 1000, NULL);
	}

	if (!m_bRecording)
		return;

	BYTE *pDataOut = 0;
	int nLenOut = 0;

	CAudioResample::Change(dataTemp, linesize, samples, AV_SAMPLE_FMT_FLT,
			       m_tSrcParam.m_uChannels, m_tSrcParam.m_uSampleRate,
			       AV_SAMPLE_FMT_S16, m_tDstParam.m_uChannels,
			       m_tDstParam.m_uSampleRate, pDataOut, nLenOut);

	if (!pDataOut)
		return;

	{
		CAutoLockCS AutoLock(m_csSaver);
		m_tWavSaver.WriteData(pDataOut, nLenOut);
	}

	delete[] pDataOut;
}

HRESULT CMusicRecordDlg::OnShowNote(WPARAM wp, LPARAM lp)
{
	const TCHAR *pNote = (const TCHAR *)wp;
	GetDlgItem(IDC_STATIC_NOTE)->SetWindowText(pNote);
	return 0;
}

HRESULT CMusicRecordDlg::OnUpdateProgress(WPARAM wp, LPARAM lp)
{
	m_ProgressVolumn.SetPos(int(wp));
	return 0;
}

void CMusicRecordDlg::OnBnClickedButtonPath()
{
	CString path = _T("/select, ");
	path += m_strDir;

	ShellExecute(NULL, _T("open"), _T("Explorer.exe"), path.GetBuffer(), NULL, SW_SHOWDEFAULT);
}

void CMusicRecordDlg::OnBnClickedButtonBrowser()
{
	BROWSEINFO bi;
	bi.hwndOwner = *this;
	bi.pidlRoot = NULL;
	bi.pszDisplayName = NULL;
	bi.lpszTitle = _T("Set Save Path");
	bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_STATUSTEXT;
	bi.lpfn = NULL;
	bi.lParam = 0;
	bi.iImage = 0;

	LPITEMIDLIST pidl = SHBrowseForFolder(&bi);
	if (pidl == NULL)
		return;

	TCHAR pszPath[MAX_PATH + 1] = {};
	if (SHGetPathFromIDList(pidl, pszPath)) {
		m_strDir = pszPath;
		GetDlgItem(IDC_BUTTON_PATH)->SetWindowText(m_strDir);
	}
}
