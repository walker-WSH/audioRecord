#include "stdafx.h"
#include "AudioResample.h"
#include "ScopeGuard.hpp"
#include <assert.h>

int CAudioResample::Change(uint8_t *src_data[], const int *src_linesize, const int &src_nbsamples, const AVSampleFormat &src_fmt, const int &src_channel,
			   const int &src_sample_rate, const AVSampleFormat &dst_fmt, const int &dst_channel, const int &dst_sample_rate, BYTE *&out_buf,
			   int &out_len)
{
	out_len = 0;
	out_buf = 0;

	if ((src_fmt == dst_fmt) && (src_sample_rate == dst_sample_rate) && (src_channel == dst_channel)) {
		out_len = src_linesize[0];
		out_buf = new BYTE[out_len];
		memcpy(out_buf, src_data[0], out_len);
		return out_len;
	}

	SwrContext *swr_ctx = NULL; // auto free
	int data_size = 0;
	int ret = 0;
	int64_t src_ch_layout = AV_CH_LAYOUT_STEREO;
	int64_t dst_ch_layout = AV_CH_LAYOUT_STEREO;
	int dst_nb_channels = 0;
	int dst_linesize = 0;
	int src_nb_samples = 0;
	int dst_nb_samples = 0;
	int max_dst_nb_samples = 0;
	uint8_t **dst_data = NULL; // auto free
	int resampled_data_size = 0;

	swr_ctx = swr_alloc();
	assert(swr_ctx);
	if (!swr_ctx)
		return -1;

	ON_BLOCK_EXIT(swr_free, &swr_ctx);

	src_ch_layout = av_get_default_channel_layout(src_channel);
	assert(src_ch_layout > 0);
	if (src_ch_layout <= 0)
		return -1;

	src_nb_samples = src_nbsamples;
	assert(src_nbsamples > 0);
	if (src_nb_samples <= 0)
		return -1;

	if (dst_channel == 1) {
		dst_ch_layout = AV_CH_LAYOUT_MONO;
	} else if (dst_channel == 2) {
		dst_ch_layout = AV_CH_LAYOUT_STEREO;
	} else {
		assert(false); // extendion
		return -1;
	}

	av_opt_set_int(swr_ctx, "in_channel_layout", src_ch_layout, 0);
	av_opt_set_int(swr_ctx, "in_sample_rate", src_sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "in_sample_fmt", src_fmt, 0);

	av_opt_set_int(swr_ctx, "out_channel_layout", dst_ch_layout, 0);
	av_opt_set_int(swr_ctx, "out_sample_rate", dst_sample_rate, 0);
	av_opt_set_sample_fmt(swr_ctx, "out_sample_fmt", (AVSampleFormat)dst_fmt, 0);

	swr_init(swr_ctx);

	max_dst_nb_samples = dst_nb_samples = (int)av_rescale_rnd(src_nb_samples, dst_sample_rate, src_sample_rate, AV_ROUND_UP);
	assert(max_dst_nb_samples > 0);
	if (max_dst_nb_samples <= 0)
		return -1;

	dst_nb_channels = av_get_channel_layout_nb_channels(dst_ch_layout);
	ret = av_samples_alloc_array_and_samples(&dst_data, &dst_linesize, dst_nb_channels, dst_nb_samples, (AVSampleFormat)dst_fmt, 0);
	if (ret < 0) {
		assert(false);
		return -1;
	}

	ON_BLOCK_EXIT(av_freep, &dst_data);
	ON_BLOCK_EXIT(av_freep, &dst_data[0]);

	dst_nb_samples = (int)av_rescale_rnd(swr_get_delay(swr_ctx, src_sample_rate) + src_nb_samples, dst_sample_rate, src_sample_rate, AV_ROUND_UP);
	if (dst_nb_samples <= 0) {
		assert(false);
		return -1;
	}

	if (dst_nb_samples > max_dst_nb_samples) {
		ret = av_samples_alloc(dst_data, &dst_linesize, dst_nb_channels, dst_nb_samples, (AVSampleFormat)dst_fmt, 1);
		max_dst_nb_samples = dst_nb_samples;
	}

	data_size = av_samples_get_buffer_size(NULL, src_channel, src_nbsamples, src_fmt, 1);
	if (data_size <= 0) {
		assert(false);
		return -1;
	}

	resampled_data_size = data_size;

	ret = swr_convert(swr_ctx, dst_data, dst_nb_samples, (const uint8_t **)src_data, src_nbsamples);
	if (ret <= 0) {
		assert(false);
		return -1;
	}

	resampled_data_size = av_samples_get_buffer_size(&dst_linesize, dst_nb_channels, ret, (AVSampleFormat)dst_fmt, 1);
	if (resampled_data_size <= 0) {
		assert(false);
		return -1;
	}

	out_len = resampled_data_size;
	out_buf = new uint8_t[resampled_data_size];
	memcpy(out_buf, dst_data[0], resampled_data_size);

	return resampled_data_size;
}
