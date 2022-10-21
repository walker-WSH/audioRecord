#pragma once
#include <assert.h>
#include "ffmpeg.hpp"

// ��Ҫ���ó�ʼ������
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
		AVFilterContext *pFilterCtxSink; // ��Ҫ�ͷ�
		AVFilterContext *pFilterCtxSrc;  // ��Ҫ�ͷ�
		AVFilterGraph *pFilterGraph;     // ��Ҫ�ͷ�
	};

	AVFormatContext *m_pFormatCtxOutput; // ��Ҫ�ͷ�
	tFilteringContext *m_arrayFilterCtx; // ��Ҫ�ͷ�,�±���AVFormatContext::streams[]һ��

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
