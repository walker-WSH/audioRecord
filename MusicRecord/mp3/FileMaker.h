#pragma once
#include <assert.h>
#include "ffmpeg.hpp"

// 需要调用初始化函数
// av_register_all();
// avfilter_register_all();

//-------------------------------------------------------
struct tAudioInputParams {
	AVCodecID encodeID;      // CODEC_ID_AAC
	unsigned int format;     // AV_SAMPLE_FMT_S16
	unsigned int sampleRate; // 44100
	unsigned int channel;
};

class CFileMaker {
	struct tFilteringContext {
		AVFilterContext *pFilterCtxSink; // 需要释放
		AVFilterContext *pFilterCtxSrc;  // 需要释放
		AVFilterGraph *pFilterGraph;     // 需要释放
	};

	AVFormatContext *m_pFormatCtxOutput; // 需要释放
	tFilteringContext *m_arrayFilterCtx; // 需要释放,下标与AVFormatContext::streams[]一致

public:
	CFileMaker();
	~CFileMaker();

public:
	static void InitFfmpeg();

	int Open(const char *pFileOut, const tAudioInputParams *pAudioParams, int *outAudioIndex);
	void Close();

	int InsertFrame(AVFrame *pFrame, int nStreamIndex);

private:
	int InitOutputFile(const char *pFileOut, const tAudioInputParams *pAudioParams, int *outAudioIndex);
	AVStream *AddStream(AVFormatContext *pAVFormatCtx, enum AVCodecID CodecID, enum AVMediaType MediaType);

	int InitFilters();
	int InitFilterInner(tFilteringContext *pFilterCtx, AVCodecContext *pEncodeCtx, const char *pFilterSpec);

	int EncodeWriteFrame(AVFrame *pFiltFrame, unsigned int nStreamIndex, int *pGetPkt);
	int FlushEncoder(unsigned int nStreamIndex);
};
