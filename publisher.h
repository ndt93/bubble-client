#ifndef __PUBLISHER_H__
#define __PUBLISHER_H__

extern "C" {
#include <libavformat/avformat.h>
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

    // Audio sampler. TODO: Replace with real audio from the camera
    OutputStream mAudioOutStream;
    AVPacket mAudioPkt;

    int initAudioOutputStream();
    AVFrame *allocAudioFrame(enum AVSampleFormat sample_fmt, uint64_t channel_layout,
                             int sample_rate, int nb_samples);
    int openAudio(AVFormatContext *oc, AVCodec *codec, OutputStream *ost, AVDictionary *opt_arg);
    /* Prepare a 16 bit dummy audio frame of 'frame_size' samples and
     * 'nb_channels' channels. */
    AVFrame *getAudioFrame(OutputStream *ost);
    /* Encode one audio frame and send it to the muxer
     * return 1 when encoding is finished, 0 otherwise */
    int writeAudioFrame(AVFormatContext *oc, OutputStream *ost);
    void closeStream(AVFormatContext *oc, OutputStream *ost);
};

#endif
