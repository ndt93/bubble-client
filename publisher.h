#ifndef __PUBLISHER_H__
#define __PUBLISHER_H__

extern "C" {
#include <libavformat/avformat.h>
#include <libavutil/timestamp.h>
}
#include "concurrent_queue.h"
#include <boost/thread/thread.hpp>
#include <boost/atomic.hpp>

typedef struct OutputStream {
    AVStream *st;
    AVCodecContext *enc;

    /* pts of the next frame that will be generated */
    int64_t next_pts;
    int samples_count;

    AVFrame *frame;
    AVFrame *tmp_frame;

    float t, tincr, tincr2;

    struct SwsContext *sws_ctx;
    struct SwrContext *swr_ctx;
} OutputStream;

class Publisher {
public:
    bool isInitialized;
    boost::atomic<bool> isStarted;

    Publisher(int queue_size=50);
    ~Publisher();

    int init(const char *url, const AVCodecContext *input_codec_ctx);
    int start();
    int stop();

    int pushPacket(const AVPacket *pkt);
private:
    AVFormatContext *mOutFmtCtx;
    AVStream *mOutStream;

    ConcurrentQueue<AVPacket> mPktsQueue;
    boost::thread *mPublishingThread;

    void publish();

    // Sample a dummy audio packet. TODO: Replace with real audio from the camera
    OutputStream mAudioOutStream;

    int initAudioOutputStream();
    AVFrame *allocAudioFrame(enum AVSampleFormat sample_fmt, uint64_t channel_layout,
                             int sample_rate, int nb_samples);
    int openAudio(AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
    /* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
     * 'nb_channels' channels. */
    AVFrame *getAudioFrame(OutputStream *ost);
    /* Encode one audio frame and send it to the muxer
     * return 1 when encoding is finished, 0 otherwise */
    AVPacket writeAudioFrame(OutputStream *ost, int *got_packet);
    void closeStream(OutputStream *ost);
};

inline void log_packet(const AVFormatContext *fmt_ctx, const AVPacket *pkt)
{
    AVRational *time_base = &fmt_ctx->streams[pkt->stream_index]->time_base;

    printf("[DEBUG] pts:%s pts_time:%s dts:%s dts_time:%s duration:%s duration_time:%s stream_index:%d size:%d\n",
           av_ts2str(pkt->pts), av_ts2timestr(pkt->pts, time_base),
           av_ts2str(pkt->dts), av_ts2timestr(pkt->dts, time_base),
           av_ts2str(pkt->duration), av_ts2timestr(pkt->duration, time_base),
           pkt->stream_index, pkt->size);
}

#endif
