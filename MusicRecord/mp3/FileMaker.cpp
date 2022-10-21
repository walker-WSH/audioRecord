#include "stdafx.h"
#include "FileMaker.h"

void CFileMaker::InitFfmpeg()
{
	av_register_all();
	avfilter_register_all();
}

//-------------------------------------------------------------------
CFileMaker::CFileMaker() : m_pFormatCtxOutput(), m_arrayFilterCtx() {}

CFileMaker::~CFileMaker()
{
	Close();
}

AVStream *CFileMaker::AddStream(AVFormatContext *pAVFormatCtx, enum AVCodecID CodecID,
				enum AVMediaType MediaType)
{
	AVCodec *pCodec = avcodec_find_encoder(CodecID);
	if (!pCodec) {
		assert(false);
		return NULL;
	}

	AVStream *pStream = avformat_new_stream(pAVFormatCtx, NULL);
	if (!pStream) {
		assert(false);
		return NULL;
	}

	pStream->id = pAVFormatCtx->nb_streams - 1;
	pStream->codec->codec_id = CodecID;
	pStream->codec->codec_type = MediaType;

	avcodec_get_context_defaults3(pStream->codec, pCodec);

	return pStream;
}

int CFileMaker::InitOutputFile(const char *pFileOut, const tAudioInputParams *pAudioParams,
			       int *outAudioIndex)
{
	int nReturn;
	AVStream *pStream;
	AVCodecContext *pCodecCtx;
	AVCodec *pCodec;

	if (avformat_alloc_output_context2(&m_pFormatCtxOutput, NULL, NULL, pFileOut) < 0) {
		assert(false);
		return 1;
	}

	if (pAudioParams) {
		pStream = AddStream(m_pFormatCtxOutput, pAudioParams->encodeID, AVMEDIA_TYPE_AUDIO);
		if (!pStream)
			return 1;

		if (outAudioIndex)
			*outAudioIndex = pStream->id;
		pCodec = avcodec_find_encoder(pAudioParams->encodeID);

		pCodecCtx = pStream->codec;
		pCodecCtx->sample_fmt = (AVSampleFormat)pAudioParams->format;
		pCodecCtx->sample_rate = pAudioParams->sampleRate;
		pCodecCtx->channels = pAudioParams->channel;
		pCodecCtx->channel_layout = av_get_default_channel_layout(pAudioParams->channel);
		pCodecCtx->time_base.den = pAudioParams->sampleRate;
		pCodecCtx->time_base.num = 1;
		if (m_pFormatCtxOutput->oformat->flags & AVFMT_GLOBALHEADER) {
			pCodecCtx->flags |= CODEC_FLAG_GLOBAL_HEADER;
		}

		int err = avcodec_open2(pCodecCtx, pCodec, NULL);
		if (err < 0) {
			assert(false);
			return NULL;
		}
	}

	if (!(m_pFormatCtxOutput->oformat->flags & AVFMT_NOFILE)) {
		nReturn = avio_open(&m_pFormatCtxOutput->pb, pFileOut, AVIO_FLAG_WRITE);
		if (nReturn < 0) {
			return 4;
		}
	}

	nReturn = avformat_write_header(m_pFormatCtxOutput, NULL);
	if (nReturn < 0) {
		return 5;
	}

	return 0;
}

int CFileMaker::Open(const char *pFileOut, const tAudioInputParams *pAudioParams,
		     int *outAudioIndex)
{
	if (outAudioIndex)
		*outAudioIndex = -1;

	if (!pAudioParams) {
		assert(false);
		return 1;
	}

	Close();

	int error = 0;
	do {
		if (InitOutputFile(pFileOut, pAudioParams, outAudioIndex) != 0) {
			error = 1;
			break;
		}

		if (InitFilters() != 0) {
			error = 2;
			break;
		}

		return error;
	} while (0);

	Close();

	return error;
}

void CFileMaker::Close()
{
	unsigned int nStreamNum = 0;
	if (m_pFormatCtxOutput)
		nStreamNum = m_pFormatCtxOutput->nb_streams;

	if (m_arrayFilterCtx) {
		for (unsigned int i = 0; i < nStreamNum; i++) {
			if (m_arrayFilterCtx[i].pFilterGraph) {
				InsertFrame(NULL, i);
				FlushEncoder(i);
			}
		}
	}

	if (m_pFormatCtxOutput) {
		av_write_trailer(m_pFormatCtxOutput);

		for (unsigned int i = 0; i < nStreamNum; i++) {
			if (m_pFormatCtxOutput->streams[i]->codec) {
				avcodec_close(m_pFormatCtxOutput->streams[i]->codec);
				av_freep(&m_pFormatCtxOutput->streams[i]->codec);
			}
			av_freep(&m_pFormatCtxOutput->streams[i]);
		}

		if (!(m_pFormatCtxOutput->oformat->flags & AVFMT_NOFILE)) {
			avio_close(m_pFormatCtxOutput->pb);
		}

		av_freep(&m_pFormatCtxOutput);
	}

	if (m_arrayFilterCtx) {
		for (unsigned int i = 0; i < nStreamNum; i++) {
			if (m_arrayFilterCtx[i].pFilterCtxSink)
				avfilter_free(m_arrayFilterCtx[i].pFilterCtxSink);
			if (m_arrayFilterCtx[i].pFilterCtxSrc)
				avfilter_free(m_arrayFilterCtx[i].pFilterCtxSrc);
			if (m_arrayFilterCtx[i].pFilterGraph)
				avfilter_graph_free(&m_arrayFilterCtx[i].pFilterGraph);
		}

		delete[] m_arrayFilterCtx;
	}

	m_arrayFilterCtx = NULL;
	m_pFormatCtxOutput = NULL;
}

int CFileMaker::InitFilterInner(tFilteringContext *pFilterCtx, AVCodecContext *pEncodeCtx,
				const char *pFilterSpec)
{
	char args[512];
	int ret = 0;

	// 不用释放内存
	AVFilter *buffersrc = NULL;
	AVFilter *buffersink = NULL;

	// 需要释放内存
	AVFilterContext *buffersrc_ctx = NULL;
	AVFilterContext *buffersink_ctx = NULL;

	// 需要释放内存 AVFilterGraph在Close()中释放
	AVFilterInOut *outputs = avfilter_inout_alloc();
	AVFilterInOut *inputs = avfilter_inout_alloc();
	AVFilterGraph *filter_graph = avfilter_graph_alloc();
	if (!outputs || !inputs || !filter_graph) {
		goto end;
	}

	if (pEncodeCtx->codec_type == AVMEDIA_TYPE_VIDEO) {
		buffersrc = avfilter_get_by_name("buffer");
		buffersink = avfilter_get_by_name("buffersink");
		if (!buffersrc || !buffersink) {
			goto end;
		}

		_snprintf_s(args, sizeof(args),
			    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
			    pEncodeCtx->width, pEncodeCtx->height, pEncodeCtx->pix_fmt,
			    pEncodeCtx->time_base.num, pEncodeCtx->time_base.den,
			    pEncodeCtx->sample_aspect_ratio.num,
			    pEncodeCtx->sample_aspect_ratio.den);

		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL,
						   filter_graph);
		if (ret < 0)
			goto end;

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL,
						   filter_graph);
		if (ret < 0)
			goto end;

		ret = av_opt_set_bin(buffersink_ctx, "pix_fmts", (uint8_t *)&pEncodeCtx->pix_fmt,
				     sizeof(pEncodeCtx->pix_fmt), AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
			goto end;
	} else if (pEncodeCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
		buffersrc = avfilter_get_by_name("abuffer");
		buffersink = avfilter_get_by_name("abuffersink");
		if (!buffersrc || !buffersink) {
			goto end;
		}

		_snprintf_s(args, sizeof(args),
			    "time_base=%d/%d:sample_rate=%d:sample_fmt=%s:channel_layout=0x%I64x",
			    pEncodeCtx->time_base.num, pEncodeCtx->time_base.den,
			    pEncodeCtx->sample_rate, av_get_sample_fmt_name(pEncodeCtx->sample_fmt),
			    pEncodeCtx->channel_layout);

		ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, NULL,
						   filter_graph);
		if (ret < 0)
			goto end;

		ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", NULL, NULL,
						   filter_graph);
		if (ret < 0)
			goto end;

		ret = av_opt_set_bin(buffersink_ctx, "sample_fmts",
				     (uint8_t *)&pEncodeCtx->sample_fmt,
				     sizeof(pEncodeCtx->sample_fmt), AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
			goto end;

		ret = av_opt_set_bin(buffersink_ctx, "channel_layouts",
				     (uint8_t *)&pEncodeCtx->channel_layout,
				     sizeof(pEncodeCtx->channel_layout), AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
			goto end;

		ret = av_opt_set_bin(buffersink_ctx, "sample_rates",
				     (uint8_t *)&pEncodeCtx->sample_rate,
				     sizeof(pEncodeCtx->sample_rate), AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
			goto end;
	} else {
		goto end;
	}

	outputs->name = av_strdup("in");
	outputs->filter_ctx = buffersrc_ctx;
	outputs->pad_idx = 0;
	outputs->next = NULL;

	inputs->name = av_strdup("out");
	inputs->filter_ctx = buffersink_ctx;
	inputs->pad_idx = 0;
	inputs->next = NULL;

	if (!outputs->name || !inputs->name)
		goto end;

	if ((ret = avfilter_graph_parse_ptr(filter_graph, pFilterSpec, &inputs, &outputs, NULL)) <
	    0)
		goto end;

	if ((ret = avfilter_graph_config(filter_graph, NULL)) < 0)
		goto end;

	pFilterCtx->pFilterCtxSrc = buffersrc_ctx;
	pFilterCtx->pFilterCtxSink = buffersink_ctx;
	pFilterCtx->pFilterGraph = filter_graph;

	if (inputs)
		avfilter_inout_free(&inputs);
	if (outputs)
		avfilter_inout_free(&outputs);

	return 0;

end:
	if (buffersrc_ctx)
		avfilter_free(buffersrc_ctx);
	if (buffersink_ctx)
		avfilter_free(buffersink_ctx);
	if (inputs)
		avfilter_inout_free(&inputs);
	if (outputs)
		avfilter_inout_free(&outputs);
	if (filter_graph)
		avfilter_graph_free(&filter_graph);

	return ret;
}

int CFileMaker::InitFilters()
{
	unsigned int nStreamNum = m_pFormatCtxOutput->nb_streams;

	m_arrayFilterCtx = new tFilteringContext[nStreamNum];
	memset(m_arrayFilterCtx, 0, sizeof(tFilteringContext) * nStreamNum);

	int ret;
	const char *pFilterSpec;
	for (unsigned int i = 0; i < nStreamNum; i++) {
		if (m_pFormatCtxOutput->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
			pFilterSpec = "null"; /* passthrough (dummy) filter for video */
		} else {
			pFilterSpec = "anull"; /* passthrough (dummy) filter for audio */
		}

		ret = InitFilterInner(&m_arrayFilterCtx[i], m_pFormatCtxOutput->streams[i]->codec,
				      pFilterSpec);
		if (ret) {
			return 1;
		}
	}

	return 0;
}

int CFileMaker::InsertFrame(AVFrame *pFrame, int nStreamIndex)
{
	int ret = 0;

	if (!m_pFormatCtxOutput || nStreamIndex < 0 ||
	    (unsigned int)nStreamIndex >= m_pFormatCtxOutput->nb_streams || !m_arrayFilterCtx ||
	    !m_arrayFilterCtx[nStreamIndex].pFilterGraph) {
		return 1;
	}

	ret = av_buffersrc_add_frame_flags(m_arrayFilterCtx[nStreamIndex].pFilterCtxSrc, pFrame, 0);
	if (ret < 0) {
		return ret;
	}

	while (1) {
		AVFrame *pFiltFrame = av_frame_alloc();
		if (!pFiltFrame) {
			ret = 1;
			break;
		}

		ret = av_buffersink_get_frame(m_arrayFilterCtx[nStreamIndex].pFilterCtxSink,
					      pFiltFrame);
		if (ret < 0) {
			if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				ret = 0;
			av_frame_free(&pFiltFrame);
			break;
		}

		pFiltFrame->pict_type = AV_PICTURE_TYPE_NONE;

		ret = EncodeWriteFrame(pFiltFrame, (unsigned int)nStreamIndex, NULL);
		av_frame_free(&pFiltFrame);

		if (ret < 0)
			break;
	}

	return ret;
}

int CFileMaker::EncodeWriteFrame(AVFrame *pFiltFrame, unsigned int nStreamIndex, int *pGetPkt)
{
	int ret = 0;
	int got_frame_local = 0;
	AVStream *pStream = m_pFormatCtxOutput->streams[nStreamIndex];

	if (!pGetPkt)
		pGetPkt = &got_frame_local;

	AVPacket pkt;
	av_init_packet(&pkt);
	pkt.data = NULL;
	pkt.size = 0;

	if (pStream->codec->codec_type == AVMEDIA_TYPE_VIDEO) {
		ret = avcodec_encode_video2(pStream->codec, &pkt, pFiltFrame, pGetPkt);
	} else {
		ret = avcodec_encode_audio2(pStream->codec, &pkt, pFiltFrame, pGetPkt);
	}

	if (ret < 0)
		return ret;
	if (!(*pGetPkt))
		return 0;

	pkt.stream_index = nStreamIndex;

	pkt.dts = av_rescale_q_rnd(pkt.dts, pStream->codec->time_base, pStream->time_base,
				   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	pkt.pts = av_rescale_q_rnd(pkt.pts, pStream->codec->time_base, pStream->time_base,
				   (AVRounding)(AV_ROUND_NEAR_INF | AV_ROUND_PASS_MINMAX));

	pkt.duration =
		(int)av_rescale_q(pkt.duration, pStream->codec->time_base, pStream->time_base);

	ret = av_interleaved_write_frame(m_pFormatCtxOutput, &pkt);
	av_free_packet(&pkt); // 释放内存

	return ret;
}

int CFileMaker::FlushEncoder(unsigned int nStreamIndex)
{
	int ret;
	int got_frame;

	if (!(m_pFormatCtxOutput->streams[nStreamIndex]->codec->codec->capabilities &
	      CODEC_CAP_DELAY)) {
		return 0;
	}

	while (1) {
		ret = EncodeWriteFrame(NULL, nStreamIndex, &got_frame);

		if (ret < 0)
			break;

		if (!got_frame)
			return 0;
	}

	return ret;
}