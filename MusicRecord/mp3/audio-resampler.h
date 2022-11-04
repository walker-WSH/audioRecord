#pragma once
#include <memory>

extern "C" {
#include <libavutil/avutil.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
};

struct resample_info {
	uint32_t samples_rate;
	uint32_t channel_num;
	enum AVSampleFormat audio_format;
};

class audio_resampler {
public:
	audio_resampler() = default;
	virtual ~audio_resampler() { uninit_resampler(); }

	bool init_resampler(const struct resample_info *src, const struct resample_info *dst);
	void uninit_resampler();

	bool resample_audio(const uint8_t *const input_data[AV_NUM_DATA_POINTERS],
			    uint32_t in_frames_per_channel,
			    uint8_t *output_data[AV_NUM_DATA_POINTERS],
			    uint32_t *out_frames_per_channel, uint64_t *ts_offset);

protected:
	bool is_planar_format(enum AVSampleFormat audio_format);

private:
	struct resample_info input_info;
	struct resample_info output_info;
	uint32_t output_planes = 0;

	std::shared_ptr<SwrContext> context = nullptr;

	int output_buffer_size = 0;
	uint8_t *output_buffer[AV_NUM_DATA_POINTERS] = {nullptr};
};
