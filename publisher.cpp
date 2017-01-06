#include "publisher.h"
#include "macro_def.h"
#include "assert.h"
extern "C" {
#include <libavutil/time.h>
#include <libavutil/opt.h>
#include <libavutil/avassert.h>
#include <libavutil/imgutils.h>
#include <libswscale/swscale.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
}

#define STREAM_FRAME_RATE 25
#define OUT_RES_WIDTH 640
#define OUT_RES_HEIGHT 360

Publisher::Publisher(int queue_size) :
    isInitialized(false), isStarted(false), mOutFmtCtx(NULL), mFramesQueue(queue_size), mPublishingThread(NULL)
{
    av_register_all();
    avformat_network_init();
}

Publisher::~Publisher()
{
    if (isStarted)
    {
        stop();
    }
    av_write_trailer(mOutFmtCtx);

    closeStream(&mVideoOutStream);
    closeStream(&mAudioOutStream);

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

    mPublishingThread = new boost::thread(&Publisher::publish, this);

    return 0;
}

void Publisher::publish()
{
    std::printf("[INFO] Publishing thread has started\n");
    AVFrame *video_frame;
    AVPacket video_pkt, audio_pkt;
    int64_t start_time;
    int got_packet;
    int status;

    memset(&video_pkt, 0, sizeof(AVPacket));
    av_init_packet(&video_pkt);
    mVideoOutStream.next_pts = 0;
    start_time = av_gettime();
    while (isStarted)
    {
        mFramesQueue.waitAndPop(video_frame);
        encodeVideoFrame(&mVideoOutStream, video_frame);
        status = avcodec_receive_packet(mVideoOutStream.enc, &video_pkt);
        while (status == 0)
        {
#ifdef DEBUG
            std::printf("[DEBUG] Publishing %d bytes packet @%p\n", video_pkt.size, video_pkt.data);
#endif
            av_packet_rescale_ts(&video_pkt, (AVRational){1, STREAM_FRAME_RATE} , mVideoOutStream.st->time_base);
            video_pkt.stream_index = mVideoOutStream.st->index;
            video_pkt.pos = -1;
#ifdef DEBUG
            std::printf("[DEBUG] %lld %lld %lld\n", video_pkt.pts, video_pkt.dts, video_pkt.duration);
            std::printf("[DEBUG] Send %8d video frames to output URL\n", mVideoOutStream.next_pts);
#endif
            status = av_interleaved_write_frame(mOutFmtCtx, &video_pkt);
            if (status < 0) {
                LOG_ERR("Publisher: Error muxing video packet");
            }
            status = avcodec_receive_packet(mVideoOutStream.enc, &video_pkt);
        }
        mVideoOutStream.next_pts++;


        audio_pkt = encodeAudioFrame(&mAudioOutStream, &got_packet);
        if (got_packet)
        {
            av_packet_rescale_ts(&audio_pkt, mAudioOutStream.enc->time_base, mAudioOutStream.st->time_base);
            audio_pkt.stream_index = mAudioOutStream.st->index;
#ifdef DEBUG
            log_packet(mOutFmtCtx, &audio_pkt);
#endif
            status = av_interleaved_write_frame(mOutFmtCtx, &audio_pkt);
            if (status < 0) {
                LOG_ERR("Publisher: Error muxing audio packet");
            }
        }

        av_packet_unref(&video_pkt);
        av_frame_free(&video_frame);
        av_packet_unref(&audio_pkt);
    }
}

int Publisher::stop()
{
    if (!isStarted)
    {
        LOG_WARN("Publisher: publisher has not been started");
        return 0;
    }

    isStarted = false;

    if (mPublishingThread)
    {
        mPublishingThread->join();
        delete mPublishingThread;
        mPublishingThread = NULL;
    }

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

    avformat_alloc_output_context2(&mOutFmtCtx, NULL, "flv", url);
    if (!mOutFmtCtx)
    {
        LOG_ERR("Publisher: avformat_alloc_output_context2 failed");
        return -1;
    }

    out_fmt = mOutFmtCtx->oformat;

    video_codec = avcodec_find_encoder(input_codec_ctx->codec_id);
    status = initVideoOutputStream(video_codec, &mVideoOutStream, input_codec_ctx, NULL);
    if (status != 0)
    {
        LOG_ERR("Failed to initialize video output stream");
        goto on_error;
    }

    status = initAudioOutputStream();
    if (status != 0)
    {
        LOG_ERR("Failed to initialize audio output stream");
        goto on_error;
    }

    av_dump_format(mOutFmtCtx, 0, url, 1);

    if (!(out_fmt->flags & AVFMT_NOFILE))
    {
        status = avio_open(&mOutFmtCtx->pb, url, AVIO_FLAG_WRITE);
        if (status < 0) {
            std::fprintf(stderr, "[ERROR] Publisher: failed to open output URL, %s\n", av_err2str(status));
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

int Publisher::pushFrame(const AVFrame *frame)
{
    AVFrame *new_frame = av_frame_clone(frame);
    if (!new_frame)
    {
        LOG_ERR("Publisher: Failed to allocate new frame");
        return -1;
    }

    if (mFramesQueue.tryPush(new_frame))
    {
        return 0;
    }
    LOG_WARN("Publisher: queue is full");
    return -1;
}

int Publisher::initVideoOutputStream(AVCodec *codec, OutputStream *ost,
                                     const AVCodecContext *input_ctx, AVDictionary *opt_arg)
{
    std::printf("[INFO] Initializing video output stream and encoder...\n");
    int ret;
    AVCodecContext *c;
    AVDictionary *opt = NULL;

    c = avcodec_alloc_context3(codec);
    if (!c) {
        fprintf(stderr, "[ERROR] Could not alloc an encoding context\n");
        closeStream(ost);
        return -1;
    }
    ost->enc = c;

    c->codec_id = codec->id;
    c->bit_rate = 400000;
    c->width = OUT_RES_WIDTH;
    c->height = OUT_RES_HEIGHT;
    c->time_base = (AVRational){1, STREAM_FRAME_RATE};
    c->gop_size = 12; /* emit one intra frame every twelve frames at most */
    c->pix_fmt = input_ctx->pix_fmt ;
    if (c->codec_id == AV_CODEC_ID_MPEG2VIDEO)
    {
        c->max_b_frames = 2;
    }
    if (c->codec_id == AV_CODEC_ID_MPEG1VIDEO)
    {
        c->mb_decision = 2;
    }

    av_dict_copy(&opt, opt_arg, 0);

    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] Could not open video codec: %s\n", av_err2str(ret));
        closeStream(ost);
        return -1;
    }

    ost->st = avformat_new_stream(mOutFmtCtx, NULL);
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] Could not copy the stream parameters\n");
        closeStream(ost);
        return -1;
    }
    ost->st->time_base = c->time_base;

    ret = allocateConversionCtx(input_ctx->pix_fmt, input_ctx->width, input_ctx->height,
                                OUT_RES_WIDTH, OUT_RES_HEIGHT);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] Publisher: allocateConversionCtx\n");
        closeStream(ost);
        return -1;
    }

    std::printf("[INFO] Initialized video output stream and encoder...\n");
    return 0;
}

int Publisher::allocateConversionCtx(enum AVPixelFormat src_pix_fmt, int src_w, int src_h, int dst_w, int dst_h)
{
    std::printf("[INFO] Allocating conversion context...\n");
    uint8_t *frame_buffer;
    mVideoOutStream.sws_ctx = sws_getContext(src_w, src_h, src_pix_fmt, dst_w, dst_h,
                                             src_pix_fmt, SWS_BICUBIC, NULL, NULL, NULL);
    if (!mVideoOutStream.sws_ctx)
    {
        LOG_ERR("Failed to allocate sws context");
        return -1;
    }

    mVideoOutStream.frame = av_frame_alloc();
    if (!mVideoOutStream.frame)
    {
        LOG_ERR("Failed to allocate RGB frame");
        return -1;
    }
    mVideoOutStream.frame->width = dst_w;
    mVideoOutStream.frame->height = dst_h;
    mVideoOutStream.frame->format = src_pix_fmt;

    int nbytes = av_image_get_buffer_size(src_pix_fmt, dst_w, dst_h, 1);
    frame_buffer = (uint8_t *)av_malloc(nbytes);
    if (!frame_buffer)
    {
        LOG_ERR("Failed to allocate RGB frame buffer");
        return -1;
    }
    av_image_fill_arrays(mVideoOutStream.frame->data, mVideoOutStream.frame->linesize, frame_buffer,
                         src_pix_fmt, dst_w, dst_h, 1);
    return 0;
}

int Publisher::encodeVideoFrame(OutputStream *ost, AVFrame *frame)
{
#ifdef DEBUG
    std::printf("[INFO] Encoding video frame...\n");
#endif
    int ret;

    ret = sws_scale(ost->sws_ctx, frame->data, frame->linesize, 0, frame->height,
                    ost->frame->data, ost->frame->linesize);
    if (ret < 0)
    {
        return -1;
    }

    ost->frame->pts = ost->next_pts;

    ret = avcodec_send_frame(ost->enc, ost->frame);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] video avcodec_send_frame: %s\n", av_err2str(ret));
        return -1;
    }

#ifdef DEBUG
    std::printf("[INFO] Got video frame\n");
#endif
    return 0;
}

int Publisher::initAudioOutputStream()
{
    std::printf("[INFO] Initializing audio sampler...\n");
    AVCodec *audio_codec;
    AVCodecParameters *audio_codecpar;
    int status;

    mAudioOutStream.st = avformat_new_stream(mOutFmtCtx, NULL);
    if (!mAudioOutStream.st)
    {
        LOG_ERR("Publisher: audio avformat_new_stream failed");
        return -1;
    }

    audio_codec = avcodec_find_encoder(AV_CODEC_ID_AAC);
    audio_codecpar = mAudioOutStream.st->codecpar;
    audio_codecpar->codec_type = AVMEDIA_TYPE_AUDIO;
    audio_codecpar->codec_id = AV_CODEC_ID_AAC;
    audio_codecpar->bit_rate = 64000;
    audio_codecpar->sample_rate = 44100;
    if (audio_codec->supported_samplerates)
    {
        audio_codecpar->sample_rate = audio_codec->supported_samplerates[0];
        for (int i = 0; audio_codec->supported_samplerates[i]; i++)
        {
            if (audio_codec->supported_samplerates[i] == 44100)
            {
                audio_codecpar->sample_rate = 44100;
            }
        }
    }
    audio_codecpar->channels = av_get_channel_layout_nb_channels(audio_codecpar->channel_layout);
    audio_codecpar->channel_layout = AV_CH_LAYOUT_STEREO;
    if (audio_codec->channel_layouts)
    {
        audio_codecpar->channel_layout = audio_codec->channel_layouts[0];
        for (int i = 0; audio_codec->channel_layouts[i]; i++)
        {
            if (audio_codec->channel_layouts[i] == AV_CH_LAYOUT_STEREO)
            {
                audio_codecpar->channel_layout = AV_CH_LAYOUT_STEREO;
            }
        }
    }
    audio_codecpar->channels = av_get_channel_layout_nb_channels(audio_codecpar->channel_layout);
    mAudioOutStream.st->time_base = (AVRational){1, audio_codecpar->sample_rate};

    status = openAudioEncoder(audio_codec, &mAudioOutStream, NULL);
    if (status != 0)
    {
        LOG_ERR("Publisher: openAudio failed");
        closeStream(&mAudioOutStream);
        return -1;
    }

    std::printf("[INFO] Initialized audio output stream and dummy audio packet\n");
    return 0;
}

AVFrame *Publisher::allocAudioFrame(enum AVSampleFormat sample_fmt,
                                    uint64_t channel_layout,
                                    int sample_rate, int nb_samples)
{
    std::printf("[INFO] Allocating audio frame...\n");
    AVFrame *frame = av_frame_alloc();
    int ret;

    if (!frame)
    {
        fprintf(stderr, "Error allocating an audio frame\n");
        exit(1);
    }

    frame->format = sample_fmt;
    frame->channel_layout = channel_layout;
    frame->sample_rate = sample_rate;
    frame->nb_samples = nb_samples;

    if (nb_samples)
    {
        ret = av_frame_get_buffer(frame, 0);
        if (ret < 0)
        {
            LOG_ERR("Publisher: Error allocating an audio buffer\n");
            av_frame_free(&frame);
            return NULL;
        }
    }

    std::printf("[INFO] Audio frame allocated \n");
    return frame;
}

int Publisher::openAudioEncoder(AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg)
{
    std::printf("[INFO] Opening audio encoder...\n");
    AVCodecContext *c;
    int nb_samples;
    int ret;
    AVDictionary *opt = NULL;

    c = avcodec_alloc_context3(codec);
    if (!c)
    {
        fprintf(stderr, "[ERROR] Could not alloc an encoding context\n");
        return -1;
    }
    ost->enc = c;
    c->sample_fmt = codec->sample_fmts ? codec->sample_fmts[0] : AV_SAMPLE_FMT_FLTP;
    c->bit_rate = ost->st->codecpar->bit_rate;
    c->sample_rate = ost->st->codecpar->sample_rate;
    c->channels = ost->st->codecpar->channels;
    c->channel_layout = ost->st->codecpar->channel_layout;
    c->time_base = ost->st->time_base;

    av_dict_copy(&opt, opt_arg, 0);
    ret = avcodec_open2(c, codec, &opt);
    av_dict_free(&opt);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] Could not open audio codec: %s\n", av_err2str(ret));
        return -1;
    }

    /* Init signal generator */
    ost->t = 0;
    ost->tincr = 2 * M_PI * 110.0 / c->sample_rate;
    /* Increment frequency by 110 Hz per second */
    ost->tincr2 = 2 * M_PI * 110.0 / c->sample_rate / c->sample_rate;

    if (c->codec->capabilities & AV_CODEC_CAP_VARIABLE_FRAME_SIZE)
    {
        nb_samples = 10000;
    }
    else
    {
        nb_samples = c->frame_size;
    }

    ost->frame = allocAudioFrame(c->sample_fmt, c->channel_layout, c->sample_rate, nb_samples);
    ost->tmp_frame = allocAudioFrame(AV_SAMPLE_FMT_S16, c->channel_layout, c->sample_rate, nb_samples);

    /* Copy the stream parameters to the muxer */
    ret = avcodec_parameters_from_context(ost->st->codecpar, c);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] Could not copy the stream parameters\n");
        return -1;
    }

    /* Create resampler context */
    ost->swr_ctx = swr_alloc();
    if (!ost->swr_ctx)
    {
        fprintf(stderr, "[ERROR] Could not allocate resampler context\n");
        return -1;
    }

    /* Set options */
    av_opt_set_int(ost->swr_ctx, "in_channel_count", c->channels, 0);
    av_opt_set_int(ost->swr_ctx, "in_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "in_sample_fmt", AV_SAMPLE_FMT_S16, 0);
    av_opt_set_int(ost->swr_ctx, "out_channel_count", c->channels, 0);
    av_opt_set_int(ost->swr_ctx, "out_sample_rate", c->sample_rate, 0);
    av_opt_set_sample_fmt(ost->swr_ctx, "out_sample_fmt", c->sample_fmt, 0);

    /* Initialize the resampling context */
    if ((ret = swr_init(ost->swr_ctx)) < 0)
    {
        fprintf(stderr, "[ERROR] Failed to initialize the resampling context\n");
        return -1;
    }

    std::printf("[INFO] Audio encoder is ready\n");
    return 0;
}

/* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
 * 'nb_channels' channels. */
AVFrame *Publisher::getAudioFrame(OutputStream *ost)
{
#ifdef DEBUG
    std::printf("[INFO] Sample a raw audio frame\n");
#endif
    AVFrame *frame = ost->tmp_frame;
    int j, i, v;
    int16_t *q = (int16_t*)frame->data[0];

    for (j = 0; j <frame->nb_samples; j++)
    {
        v = (int)(sin(ost->t) * 10000);
        for (i = 0; i < ost->enc->channels; i++)
        {
            *q++ = v;
        }
        ost->t += ost->tincr;
        ost->tincr += ost->tincr2;
    }

    frame->pts = ost->next_pts;
    ost->next_pts  += frame->nb_samples;

#ifdef DEBUG
    std::printf("[INFO] Raw audio frame is generated\n");
#endif
    return frame;
}

/*
 * Encode one audio frame and save it to mAudioPkt
 */
AVPacket Publisher::encodeAudioFrame(OutputStream *ost, int *got_packet)
{
#ifdef DEBUG
    std::printf("[INFO] Encoding audio frame...\n");
#endif
    AVCodecContext *c;
    AVFrame *frame;
    AVPacket pkt;
    int ret;
    int dst_nb_samples;

    c = ost->enc;
    memset(&pkt, 0, sizeof(AVPacket));
    av_init_packet(&pkt);

    frame = getAudioFrame(ost);

    if (!frame)
    {
        LOG_ERR("Publisher: getAudioFrame failed");
        *got_packet = 0;
        return pkt;
    }

    /* Convert samples from native format to destination codec format, using the resampler */
    /* Compute destination number of samples */
    dst_nb_samples = av_rescale_rnd(swr_get_delay(ost->swr_ctx, c->sample_rate) + frame->nb_samples,
                                    c->sample_rate, c->sample_rate, AV_ROUND_UP);
    av_assert0(dst_nb_samples == frame->nb_samples);

    /* When we pass a frame to the encoder, it may keep a reference to it
     * internally; make sure we do not overwrite it here
     */
    ret = av_frame_make_writable(ost->frame);
    if (ret < 0)
    {
        LOG_ERR("Publisher: av_frame_make_writable");
        *got_packet = 0;
        return pkt;
    }

    /* Convert to destination format */
    ret = swr_convert(ost->swr_ctx, ost->frame->data, dst_nb_samples,
                      (const uint8_t **) frame->data, frame->nb_samples);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] Error while converting\n");
        *got_packet = 0;
        return pkt;
    }
    frame = ost->frame;

    frame->pts = av_rescale_q(ost->samples_count, (AVRational){1, c->sample_rate}, c->time_base);
    ost->samples_count += dst_nb_samples;

    ret = avcodec_send_frame(c, frame);
    if (ret < 0)
    {
        fprintf(stderr, "[ERROR] avcodec_send_frame: %s\n", av_err2str(ret));
        *got_packet = 0;
        return pkt;
    }
    ret = avcodec_receive_packet(c, &pkt);
    if (ret < 0)
    {
        fprintf(stderr, "[WARN] avcodec_receive_packet: %s\n", av_err2str(ret));
        *got_packet = 0;
        return pkt;
    }

#ifdef DEBUG
    std::printf("[INFO] Audio frame encoded to packet\n");
#endif
    *got_packet = 1;
    return pkt;
}

void Publisher::closeStream(OutputStream *ost)
{
    avcodec_free_context(&ost->enc);
    av_frame_free(&ost->frame);
    av_frame_free(&ost->tmp_frame);
    sws_freeContext(ost->sws_ctx);
    swr_free(&ost->swr_ctx);
}
