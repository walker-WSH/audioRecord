#pragma  once
#include <mmdeviceapi.h>
#include <audioclient.h> 
#include <propsys.h>
#include <windows.h>
#include <assert.h>

class ICaptureCallback
{
public:
	virtual ~ICaptureCallback() {}

	virtual void OnDeviceError() = 0;
	virtual void OnDeviceReady() = 0;
	virtual void OnAudioData(const BYTE* data, UINT len, UINT samples) = 0;
};

struct tAudioParam
{
	DWORD m_uSampleRate;
	WORD m_uChannels;
	WORD m_uBitWidth;
};

//--------------------------------------------------------------------
class CAudioCapture
{
	IMMDevice*				m_pAudioDevice;
	IAudioClient*			m_pAudioClient;
	IAudioCaptureClient*	m_pCaptureClient;  

	ICaptureCallback*		m_pCallback;
	tAudioParam				m_tParam;
											    
	HANDLE					m_hExitEvent;
	HANDLE					m_hThread;

public:
	CAudioCapture(ICaptureCallback* pCb);
	~CAudioCapture();

public:
	bool Initialize();
	void UnInitialize();

	void GetParam(tAudioParam& tParamOut);

private:
	bool _InitDevice();	    
	void _UninitDevice();

	static unsigned __stdcall ThreadCapture(void* pParam);
	void _InnerCapture(); 
	bool _CaptureFrame();
};
		 