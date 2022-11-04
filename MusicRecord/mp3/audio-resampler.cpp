#include "stdafx.h"
#include "audio-resampler.h"

bool audio_resampler::init_resampler(const struct resample_info *src,
				     const struct resample_info *dst)
{
	int64_t input_layout = av_get_default_channel_layout(src->channel_num);
	int64_t output_layout = av_get_default_channel_layout(dst->channel_num);

	input_info = *src;
	output_info = *dst;
	output_planes = is_planar_format(dst->audio_format) ? dst->channel_num : 1;

	SwrContext *temp = swr_alloc_set_opts(nullptr, output_layout, dst->audio_format,
					      dst->samples_rate, input_layout, src->audio_format,
					      src->samples_rate, 0, nullptr);
	if (!temp) {
		assert(false);
		return false;
	}

	context = std::shared_ptr<SwrContext>(temp, [](SwrContext *obj) { swr_free(&obj); });

	if (input_layout == AV_CH_LAYOUT_MONO && output_info.channel_num > 1) {
		const double matrix[AV_NUM_DATA_POINTERS][AV_NUM_DATA_POINTERS] = {
			{1},
			{1, 1},
			{1, 1, 0},
			{1, 1, 1, 1},
			{1, 1, 1, 0, 1},
			{1, 1, 1, 1, 1, 1},
			{1, 1, 1, 0, 1, 1, 1},
			{1, 1, 1, 0, 1, 1, 1, 1},
		};
		if (swr_set_matrix(context.get(), matrix[output_info.channel_num - 1], 1) < 0)
			OutputDebugStringA("swr_set_matrix failed for mono upmix \n");
	}

	int errcode = swr_init(context.get());
	if (errcode != 0) {
		uninit_resampler();
		assert(false);
		return false;
	}

	return true;
}

void audio_resampler::uninit_resampler()
{
	input_info = resample_info();
	output_info = resample_info();
	output_planes = 0;

	context.reset();

	output_buffer_size = 0;
	if (output_buffer[0])
		av_freep(&output_buffer[0]);
}

bool audio_resampler::resample_audio(const uint8_t *const input_data[AV_NUM_DATA_POINTERS],
				     uint32_t in_frames_per_channel,
				     uint8_t *output_data[AV_NUM_DATA_POINTERS],
				     uint32_t *out_frames_per_channel, uint64_t *ts_offset)
{
	int64_t delay = swr_get_delay(context.get(), input_info.samples_rate);
	int estimated = (int)av_rescale_rnd(delay + (int64_t)in_frames_per_channel,
					    (int64_t)output_info.samples_rate,
					    (int64_t)input_info.samples_rate, AV_ROUND_UP);

	if (ts_offset)
		*ts_offset = (uint64_t)swr_get_delay(context.get(), 1000000000);

	if (estimated > output_buffer_size) {
		output_buffer_size = 0;
		if (output_buffer[0])
			av_freep(&output_buffer[0]);

		int ret = av_samples_alloc(output_buffer, nullptr, output_info.channel_num,
					   estimated, output_info.audio_format, 0);
		if (ret < 0) {
			OutputDebugStringA("av_samples_alloc failed");
			return false;
		}

		output_buffer_size = estimated;
	}

	int ret = swr_convert(context.get(), output_buffer, output_buffer_size,
			      (const uint8_t **)input_data, in_frames_per_channel);
	if (ret < 0) {
		OutputDebugStringA("swr_convert failed");
		return false;
	}

	for (uint32_t i = 0; i < AV_NUM_DATA_POINTERS; i++) {
		if (i < output_planes) {
			output_data[i] = output_buffer[i];
		} else {
			output_data[i] = nullptr;
		}
	}

	*out_frames_per_channel = (uint32_t)ret;
	return true;
}

bool audio_resampler::is_planar_format(enum AVSampleFormat audio_format)
{
	switch (audio_format) {
	case AV_SAMPLE_FMT_NONE:
	case AV_SAMPLE_FMT_U8:
	case AV_SAMPLE_FMT_S16:
	case AV_SAMPLE_FMT_S32:
	case AV_SAMPLE_FMT_FLT:
	case AV_SAMPLE_FMT_DBL:
		return false;

	default:
		return true;
	}
}
