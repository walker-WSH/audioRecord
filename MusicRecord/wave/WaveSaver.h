#pragma once
#include <stdio.h>
#include <windows.h>
#include <MMReg.h>
#include "MemoryMapWriter.h"

class CWaveSaver {
	CMemoryMapWriter m_Writer;
	int m_nTotalDataLen;
	int m_nCurrentDataLen;

public:
	CWaveSaver();
	virtual ~CWaveSaver();

	bool Open(const TCHAR *pFile, const WAVEFORMATEX *pWaveFormat, int nDurationSeconds = 5 * 60);
	unsigned int WriteData(const BYTE *pData, const int &nLen);
	void Close();
};