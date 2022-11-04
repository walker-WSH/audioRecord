#include "stdafx.h"
#include "AudioCapture.h"
#include <process.h>

#define BUFFER_TIME_100NS (5 * 10000000)

//-----------------------------------------------------
CAudioCapture::CAudioCapture(ICaptureCallback *pCb)
	: m_pCallback(pCb),
	  m_hThread(),
	  m_tParam(),
	  m_pAudioDevice(),
	  m_pAudioClient(),
	  m_pCaptureClient()
{
	m_hExitEvent = ::CreateEvent(nullptr, TRUE, FALSE, nullptr);
}

CAudioCapture::~CAudioCapture()
{
	UnInitialize();
	::CloseHandle(m_hExitEvent);
}

bool CAudioCapture::Initialize()
{
	bool bRet = _InitDevice();

	::ResetEvent(m_hExitEvent);
	m_hThread = (HANDLE)_beginthreadex(0, 0, ThreadCapture, this, 0, 0);

	return bRet;
}

void CAudioCapture::UnInitialize()
{
	::SetEvent(m_hExitEvent);
	if (m_hThread && (m_hThread != INVALID_HANDLE_VALUE)) {
		WaitForSingleObject(m_hThread, INFINITE);
		CloseHandle(m_hThread);
	}

	m_hThread = 0;
	_UninitDevice();
}

void CAudioCapture::GetParam(tAudioParam &tParamOut)
{
	tParamOut = m_tParam;
}

void CAudioCapture::_UninitDevice()
{
	if (m_pAudioClient)
		m_pAudioClient->Stop();

	if (m_pCaptureClient)
		m_pCaptureClient->Release();
	if (m_pAudioClient)
		m_pAudioClient->Release();
	if (m_pAudioDevice)
		m_pAudioDevice->Release();

	m_pCaptureClient = 0;
	m_pAudioClient = 0;
	m_pAudioDevice = 0;
}

bool CAudioCapture::_InitDevice()
{
	bool bInited = false;
	IMMDeviceEnumerator *pEnumerator = 0;
	do {
		HRESULT res = CoCreateInstance(__uuidof(MMDeviceEnumerator), nullptr, CLSCTX_ALL,
					       __uuidof(IMMDeviceEnumerator),
					       (void **)&pEnumerator);
		if (FAILED(res))
			break;

		res = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &m_pAudioDevice);
		if (FAILED(res))
			break;

		res = m_pAudioDevice->Activate(__uuidof(IAudioClient), CLSCTX_INPROC_SERVER,
					       nullptr, (void **)&m_pAudioClient);
		if (FAILED(res))
			break;

		WAVEFORMATEX *pWaveFormatEx = 0;
		res = m_pAudioClient->GetMixFormat(&pWaveFormatEx);
		if (FAILED(res))
			break;

		assert(pWaveFormatEx->wBitsPerSample == 32); // default format is float

		res = m_pAudioClient->Initialize(AUDCLNT_SHAREMODE_SHARED,
						 AUDCLNT_STREAMFLAGS_LOOPBACK, BUFFER_TIME_100NS, 0,
						 pWaveFormatEx, nullptr);
		if (FAILED(res))
			break;

		res = m_pAudioClient->GetService(__uuidof(IAudioCaptureClient),
						 (void **)&m_pCaptureClient);
		if (FAILED(res))
			break;

		assert(pWaveFormatEx->wBitsPerSample == 32); // default format is float
		m_tParam.m_uBitWidth = pWaveFormatEx->wBitsPerSample;
		m_tParam.m_uChannels = pWaveFormatEx->nChannels;
		m_tParam.m_uSampleRate = pWaveFormatEx->nSamplesPerSec;
		m_tParam.m_uBitWidth = 16;

		m_pAudioClient->Start();
		bInited = true;
	} while (0);

	if (pEnumerator)
		pEnumerator->Release();

	if (!bInited)
		_UninitDevice();

	return bInited;
}

unsigned __stdcall CAudioCapture::ThreadCapture(void *pParam)
{
	CAudioCapture *self = reinterpret_cast<CAudioCapture *>(pParam);
	self->_InnerCapture();
	return 0;
}

bool CAudioCapture::_CaptureFrame()
{
	bool bOK = false;
	do {
		UINT uPktSize = 0;
		LPBYTE pDataBuf = 0;
		UINT32 uNBSamples = 0;
		DWORD uFlags;
		UINT64 u64Pos, u64TimeStamp;

		if (!m_pCaptureClient)
			break;

		HRESULT res = m_pCaptureClient->GetNextPacketSize(&uPktSize);
		if (FAILED(res))
			break;

		if (uPktSize == 0) {
			bOK = true;
			break;
		}

		res = m_pCaptureClient->GetBuffer(&pDataBuf, &uNBSamples, &uFlags, &u64Pos,
						  &u64TimeStamp);
		if (FAILED(res))
			break;

		static unsigned s_uBitWidth = 32;
		UINT uDataSize = uNBSamples * m_tParam.m_uChannels * (s_uBitWidth / 8);

		m_pCallback->OnAudioData(pDataBuf, uDataSize, uNBSamples);
		m_pCaptureClient->ReleaseBuffer(uNBSamples);

		bOK = true;
	} while (0);

	return bOK;
}

bool IsHandleSigned(const HANDLE &hEvent, DWORD dwMilliSecond)
{
	if (!hEvent)
		return false;

	DWORD res = WaitForSingleObject(hEvent, dwMilliSecond);
	return (res == WAIT_OBJECT_0);
}

void CAudioCapture::_InnerCapture()
{
	CoInitialize(nullptr);

	bool bReinitFlag = false;
	while (IsHandleSigned(m_hExitEvent, 10) == false) {
		if (bReinitFlag) {
			_UninitDevice();
			if (_InitDevice()) {
				bReinitFlag = false;
				m_pCallback->OnDeviceReady();
			}
		} else {
			if (!_CaptureFrame()) {
				bReinitFlag = true;
				m_pCallback->OnDeviceError();
			}
		}
	}

	CoUninitialize();
}