#ifndef __PUBLISHER_H__
#define __PUBLISHER_H__

extern "C" {
#include <libavformat/avformat.h>
}
#include "concurrent_queue.h"

class Publisher {
public:
    bool isInitialized;
    bool isStarted;

    Publisher(int queue_size=50);
    ~Publisher();

    int init(const char *url, const AVCodecContext *input_codec_ctx);
    int start();
    int stop();

    int pushPacket(const AVPacket *pkt);
private:
    AVFormatContext *mOutFmtCtx;
    AVStream *mOutStream;
};

#endif
