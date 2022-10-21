#pragma once
#include <math.h>

#define LOG_OFFSET_DB 6.0f
#define LOG_RANGE_DB 96.0f

/* equals -log10f(LOG_OFFSET_DB) */
#define LOG_OFFSET_VAL -0.77815125038364363f

/* equals -log10f(-LOG_RANGE_DB + LOG_OFFSET_DB) */
#define LOG_RANGE_VAL -2.00860017176191756f

#define MAX_AV_PLANES 8

// OBS_FADER_CUBIC type
class Fader {
public:
	Fader()
	{
		m_MaxDB = 0.0f;
		m_MinDB = -INFINITY;
		m_CurDB = 0.0f;
		m_CurMul = 1.0f;
	}

	void SetDeflection(float volume)
	{
		m_CurDB = _CubicDeflectionToDB(volume);

		if (m_CurDB > m_MaxDB) {
			m_CurDB = m_MaxDB;
		}

		if (m_CurDB < m_MinDB) {
			m_CurDB = -INFINITY;
		}

		m_CurMul = DBToMul(m_CurDB);
	}

	float GetCurDB() { return m_CurDB; }

	float GetCurMul() { return m_CurMul; }

	static float MulToDB(const float mul) { return (mul == 0.0f) ? -INFINITY : (20.0f * log10f(mul)); }

	static float DBToMul(const float db) { return isfinite((double)db) ? powf(10.0f, db / 20.0f) : 0.0f; }

private:
	float _CubicDeflectionToDB(const float def)
	{
		if (def == 1.0f)
			return 0.0f;
		else if (def <= 0.0f)
			return -INFINITY;

		return MulToDB(def * def * def);
	}

	float _CubicDBToDeflection(const float db)
	{
		if (db == 0.0f)
			return 1.0f;
		else if (db == -INFINITY)
			return 0.0f;

		return cbrtf(DBToMul(db));
	}

private:
	float m_MaxDB;
	float m_MinDB;
	float m_CurDB;
	float m_CurMul;
};

// OBS_FADER_LOG type
class VolumeMeter {
public:
	VolumeMeter();
	bool Update(unsigned char *data[], unsigned int samples, int nSampleRate);

	void SetCurDB(float db) { m_CurDB = db; }

	float GetLevel() { return m_Level; }

	static float LogDefToDB(const float def)
	{
		if (def >= 1.0f)
			return 0.0f;
		else if (def <= 0.0f)
			return -INFINITY;

		return -(LOG_RANGE_DB + LOG_OFFSET_DB) * powf((LOG_RANGE_DB + LOG_OFFSET_DB) / LOG_OFFSET_DB, -def) + LOG_OFFSET_DB;
	}

	static float LogDBToDef(const float db)
	{
		if (db >= 0.0f)
			return 1.0f;
		else if (db <= -96.0f)
			return 0.0f;

		return (-log10f(-db + LOG_OFFSET_DB) - LOG_RANGE_VAL) / (LOG_OFFSET_VAL - LOG_RANGE_VAL);
	}

private:
	void _ResetSampleRate(int nSampleRate);
	bool _ProcessAudioData(unsigned char *data[], unsigned int samples);
	void _SumAndMax(float *data[MAX_AV_PLANES], size_t frames, float *sum, float *max);
	void _CalculateIntervalLevels();

private:
	float m_CurDB;
	int m_SampleRate;

	unsigned int m_Channels;
	unsigned int m_UpdateMS;
	unsigned int m_UpdateFrames;
	unsigned int m_PeakHoldMS;
	unsigned int m_PeakHoldFrames;

	unsigned int m_PeakHoldCount;
	unsigned int m_IntervalFrames;
	float m_IntervalSum;
	float m_IntervalMax;

	float m_VolPeak;
	float m_VolMag;
	float m_VolMax;

	float m_Level;
	float m_Mag;
	float m_peak;
};

class AudioVolumeController {
public:
	AudioVolumeController() { _UpdateVolume(1.0f); }
	virtual ~AudioVolumeController() {}

	// Process audio data and check if need to update UI
	bool UpdateVolumeMeter(unsigned char *data[], unsigned int samples, int nSampleRate) { return m_VolumeMeter.Update(data, samples, nSampleRate); }

	// Get level value to update volume meter UI
	float GetVolumeMeterLevel() { return m_VolumeMeter.GetLevel(); }

private:
	void _UpdateVolume(float volume)
	{
		m_Fader.SetDeflection(volume);
		m_VolumeMeter.SetCurDB(m_Fader.GetCurDB());
	}

	bool _CloseFloat(float f1, float f2, float precision) { return fabsf(f1 - f2) <= precision; }

private:
	Fader m_Fader;
	VolumeMeter m_VolumeMeter;
};
