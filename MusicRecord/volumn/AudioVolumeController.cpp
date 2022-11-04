#include "stdafx.h"
#include "AudioVolumeController.h"

#define OUTPUT_AUDIO_CHANNELS 2

VolumeMeter::VolumeMeter() : m_SampleRate()
{
	// Initialize value based on OBS.
	m_CurDB = 0.0f;
}

bool VolumeMeter::Update(unsigned char *data[], unsigned int samplesPerChannel, int nSampleRate)
{
	if (nSampleRate <= 0)
		return false;

	if (m_SampleRate != nSampleRate) {
		_ResetSampleRate(nSampleRate);
	}

	bool updated = _ProcessAudioData(data, samplesPerChannel);

	if (updated) {
		float mul = Fader::DBToMul(m_CurDB);

		m_Level = LogDBToDef(Fader::MulToDB(m_VolMax * mul));
		m_Mag = LogDBToDef(Fader::MulToDB(m_VolMag * mul));
		m_peak = LogDBToDef(Fader::MulToDB(m_VolPeak * mul));
	}

	return updated;
}

void VolumeMeter::_ResetSampleRate(int nSampleRate)
{
	m_SampleRate = nSampleRate;

	m_Channels = OUTPUT_AUDIO_CHANNELS;
	m_UpdateMS = 50;
	m_UpdateFrames = m_UpdateMS * m_SampleRate / 1000;
	m_PeakHoldMS = 1500;
	m_PeakHoldFrames = m_PeakHoldMS * m_SampleRate / 1000;

	m_PeakHoldCount = 0;
	m_IntervalFrames = 0;
	m_IntervalSum = 0.0f;
	m_IntervalMax = 0.0f;

	m_VolPeak = 0.0f;
	m_VolMag = 0.0f;
	m_VolMax = 0.0f;

	m_Level = 0.0f;
	m_Mag = 0.0f;
	m_peak = 0.0f;
}

bool VolumeMeter::_ProcessAudioData(unsigned char *data[], unsigned int samplesPerChannel)
{
	bool updated = false;

	size_t left = samplesPerChannel;
	float *adata[MAX_AV_PLANES];

	for (size_t i = 0; i < MAX_AV_PLANES; i++)
		adata[i] = (float *)data[i];

	while (left) {
		size_t frames = (m_IntervalFrames + left > m_UpdateFrames)
					? m_UpdateFrames - m_IntervalFrames
					: left;

		_SumAndMax(adata, frames, &m_IntervalSum, &m_IntervalMax);

		m_IntervalFrames += (unsigned int)frames;
		left -= frames;

		for (size_t i = 0; i < MAX_AV_PLANES; i++) {
			if (!adata[i])
				break;
			adata[i] += frames;
		}

		/* break if we did not reach the end of the interval */
		if (m_IntervalFrames != m_UpdateFrames)
			break;

		_CalculateIntervalLevels();
		updated = true;
	}

	return updated;
}

void VolumeMeter::_SumAndMax(float *data[MAX_AV_PLANES], size_t frames, float *sum, float *max)
{
	float s = *sum;
	float m = *max;

	for (size_t plane = 0; plane < MAX_AV_PLANES; plane++) {
		if (!data[plane])
			break;

		for (float *c = data[plane]; c < data[plane] + frames; ++c) {
			const float pow = *c * *c;
			s += pow;
			m = (m > pow) ? m : pow;
		}
	}

	*sum = s;
	*max = m;
}

void VolumeMeter::_CalculateIntervalLevels()
{
	const unsigned int samples = m_IntervalFrames * m_Channels;
	const float alpha = 0.15f;
	const float ival_max = sqrtf(m_IntervalMax);
	const float ival_rms = sqrtf(m_IntervalSum / (float)samples);

	if (ival_max > m_VolMax) {
		m_VolMax = ival_max;
	} else {
		m_VolMax = alpha * m_VolMax + (1.0f - alpha) * ival_max;
	}

	if (m_VolMax > m_VolPeak || m_PeakHoldCount > m_PeakHoldFrames) {
		m_VolPeak = m_VolMax;
		m_PeakHoldCount = 0;
	} else {
		m_PeakHoldCount += m_IntervalFrames;
	}

	m_VolMag = alpha * ival_rms + m_VolMag * (1.0f - alpha);

	/* reset interval data */
	m_IntervalFrames = 0;
	m_IntervalSum = 0.0f;
	m_IntervalMax = 0.0f;
}
