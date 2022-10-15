#pragma once
#include <windows.h>

class CAutoLockCS
{
	CRITICAL_SECTION& m_cs;

public:
	explicit CAutoLockCS(CRITICAL_SECTION& cs) : m_cs(cs)
	{
		EnterCriticalSection(&m_cs);
	}

	~CAutoLockCS()
	{
		LeaveCriticalSection(&m_cs);
	}
};

