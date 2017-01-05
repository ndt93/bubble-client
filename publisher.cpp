#include "publisher.h"
#include "macro_def.h"
#include "assert.h"
#include <boost/thread/thread.hpp>

Publisher::Publisher(int queue_size) :
    isInitialized(false), isStarted(false), mOutFmtCtx(NULL), mOutStream(NULL)
{
    av_register_all();
    avformat_network_init();
}

Publisher::~Publisher()
{
    if (mOutFmtCtx)
    {
        avformat_free_context(mOutFmtCtx);
        if (!(mOutFmtCtx->flags & AVFMT_NOFILE))
        {
            avio_close(mOutFmtCtx->pb);
        }
    }
}

int Publisher::start()
{
    if (isStarted)
    {
        LOG_WARN("Publisher: publisher is already started");
        return 0;
    }

    isStarted = true;
    return 0;
}

int Publisher::stop()
{
    if (!isStarted)
    {
        LOG_WARN("Publisher: publisher has not been started");
        return 0;
    }

    av_write_trailer(mOutFmtCtx);
    isStarted = false;
    return 0;
}

int Publisher::init(const char *url, const AVCodecContext *input_codec_ctx)
{
    if (isInitialized)
    {
        return 0;
    }
    AVCodec *video_codec;
    AVOutputFormat *out_fmt;
    int status;

    avformat_alloc_output_context2(&mOutFmtCtx, NULL, "mp4", url);
    if (!mOutFmtCtx)
    {
        LOG_ERR("Publisher: avformat_alloc_output_context2 failed");
        return -1;
    }

    out_fmt = mOutFmtCtx->oformat;

    video_codec = avcodec_find_encoder(input_codec_ctx->codec_id);
    mOutStream = avformat_new_stream(mOutFmtCtx, video_codec);
    if (!mOutStream)
    {
        LOG_ERR("Publisher: avformat_new_streamc failed");
        goto on_error;
    }
    avcodec_parameters_from_context(mOutStream->codecpar, input_codec_ctx);

    av_dump_format(mOutFmtCtx, 0, url, 1);

    if (!(out_fmt->flags & AVFMT_NOFILE))
    {
        status = avio_open(&mOutFmtCtx->pb, url, AVIO_FLAG_WRITE);
        if (status < 0) {
            LOG_ERR("Publisher: failed to open output URL");
            goto on_error;
        }
    }

    status = avformat_write_header(mOutFmtCtx, NULL);
    if (status < 0) {
        LOG_ERR("Publisher: failed to write output stream header");
        goto on_error;
    }

    isInitialized = true;
    return 0;

on_error:
    if (mOutFmtCtx)
    {
        if (!(mOutFmtCtx->flags & AVFMT_NOFILE))
        {
            avio_close(mOutFmtCtx->pb);
        }
        avformat_free_context(mOutFmtCtx);
        mOutFmtCtx = NULL;
    }
    return -1;
}

int Publisher::pushPacket(const AVPacket *pkt)
{
    AVPacket new_pkt;
    int status;

    status = av_new_packet(&new_pkt, pkt->size);
    if (status != 0)
    {
        LOG_ERR("Publisher: failed to create new AvPacket");
        return -1;
    }
    status = av_copy_packet(&new_pkt, pkt);
    if (status != 0)
    {
        LOG_ERR("Publisher: failed to create copy packet");
        return -1;
    }

    if (mPktsQueue.push(new_pkt))
    {
        return 0;
    }
    LOG_WARN("Publisher: queue is full");
    av_packet_unref(&new_pkt);
    return -1;
}
