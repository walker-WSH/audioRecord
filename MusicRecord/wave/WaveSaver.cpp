#include "stdafx.h"
#include "WaveSaver.h"
#include <assert.h>

CWaveSaver::CWaveSaver() : m_Writer(), m_nTotalDataLen(), m_nCurrentDataLen() {}

CWaveSaver::~CWaveSaver()
{
	Close();
}

bool CWaveSaver::Open(const TCHAR *pFile, const WAVEFORMATEX *pWaveFormat, int nDurationSeconds)
{
	Close();

	if (!m_Writer.Open(pFile, true)) {
		assert(false);
		return false;
	}

	assert(nDurationSeconds > 0);
	assert(pWaveFormat->wFormatTag == WAVE_FORMAT_PCM);
	assert(pWaveFormat->wBitsPerSample == 16 || pWaveFormat->wBitsPerSample == 8);

	m_nCurrentDataLen = 0;
	m_nTotalDataLen = nDurationSeconds * pWaveFormat->nAvgBytesPerSec;

	static DWORD s_dwWaveHeaderSize = 38;
	static DWORD s_dwWaveFormatSize =
		18; // sizeof(WAVEFORMATEX), WAVEFORMATEX must Align with 1 BYTE
	static BYTE riff[4] = {'R', 'I', 'F', 'F'};
	static BYTE wave[4] = {'W', 'A', 'V', 'E'};
	static BYTE fmt[4] = {'f', 'm', 't', 32};
	static BYTE data[4] = {'d', 'a', 't', 'a'};
	unsigned int uTotalLen = (m_nTotalDataLen + s_dwWaveHeaderSize);

	m_Writer.WriteData(riff, 4);
	m_Writer.WriteData(&uTotalLen, sizeof(uTotalLen));
	m_Writer.WriteData(wave, 4);
	m_Writer.WriteData(fmt, 4);

	m_Writer.WriteData(&s_dwWaveFormatSize, sizeof(s_dwWaveFormatSize));
	m_Writer.WriteData((void *)&(pWaveFormat->wFormatTag),
			   sizeof(pWaveFormat->wFormatTag)); // set with WAVE_FORMAT_PCM
	m_Writer.WriteData((void *)&(pWaveFormat->nChannels), sizeof(pWaveFormat->nChannels));
	m_Writer.WriteData((void *)&(pWaveFormat->nSamplesPerSec),
			   sizeof(pWaveFormat->nSamplesPerSec));
	m_Writer.WriteData((void *)&(pWaveFormat->nAvgBytesPerSec),
			   sizeof(pWaveFormat->nAvgBytesPerSec));
	m_Writer.WriteData((void *)&(pWaveFormat->nBlockAlign), sizeof(pWaveFormat->nBlockAlign));
	m_Writer.WriteData((void *)&(pWaveFormat->wBitsPerSample),
			   sizeof(pWaveFormat->wBitsPerSample));
	m_Writer.WriteData((void *)&(pWaveFormat->cbSize), sizeof(pWaveFormat->cbSize));

	m_Writer.WriteData(data, 4);
	m_Writer.WriteData(&m_nTotalDataLen, sizeof(m_nTotalDataLen));

	return true;
}

unsigned int CWaveSaver::WriteData(const BYTE *pData, const int &nLen)
{
	return m_Writer.WriteData((BYTE *)pData, nLen);
}

void CWaveSaver::Close()
{
	m_Writer.Close();
	m_nTotalDataLen = 0;
	m_nCurrentDataLen = 0;
}
