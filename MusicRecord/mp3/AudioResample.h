#pragma once
#include <Windows.h>
#include "ffmpeg.hpp"

class CAudioResample
{
public:	   
	// return: data len
	// note: call "delete[] out_buf" to free memory
	static int Change(uint8_t* src_data[], const int* src_linesize, const int& src_nbsamples,
		const AVSampleFormat& src_fmt, const int& src_channel, const int& src_sample_rate,
		const AVSampleFormat& dst_fmt, const int& dst_channel, const int& dst_sample_rate,
		BYTE*& out_buf, int& out_len);
};