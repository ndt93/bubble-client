#ifndef __PUBLISHER_H__
#define __PUBLISHER_H__

extern "C" {
#include <libavformat/avformat.h>
}

class Publisher {
public:
    bool isInitialized;

    Publisher();
    ~Publisher();

    int init(const char *url, const AVCodecContext *input_codec_ctx);
    int start();
    int stop();
private:
    AVFormatContext *mOutFmtCtx;
    AVStream *mOutStream;
};

#endif
