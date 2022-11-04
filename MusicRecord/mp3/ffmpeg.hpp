#pragma once

#include <assert.h>
#include <tchar.h>
#include <Windows.h>
#include <conio.h>
#include <stdio.h>

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libswscale/swscale.h"
#include "libavdevice/avdevice.h"
#include "libavutil/audio_fifo.h"
#include "libavfilter/avfiltergraph.h"
#include "libavfilter/buffersink.h"
#include "libavfilter/buffersrc.h"
#include <libswresample/swresample.h>
#include <libavutil/mathematics.h>
#include <libavutil/opt.h>
#include <libavutil/common.h>
#include <libavutil/mem.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include "libavcodec/avfft.h"
};
