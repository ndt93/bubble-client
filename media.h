#ifndef __MEDIA_H__
#define __MEDIA_H__

#include "session.h"
#include "bubble_def.h"
#include "macro_def.h"
#include "processor.h"
#include "publisher.h"
#include <boost/atomic.hpp>

extern "C" {
#include <libavcodec/avcodec.h>
}

class MediaSession
{
public:
    MediaSession(Session *session);
    ~MediaSession();

    int start();
    boost::atomic<bool> isRunning;
private:
    Session *mpSession;
    char tmpRecvBuf[128*1024 + FF_INPUT_BUFFER_PADDING_SIZE];

    AVCodec *mCodec;
    AVCodecContext *mCodecCtx;
    AVPacket mAvPkt;
    AVFrame *mAvFrame;

    Publisher publisher;
    Processor processor;

    int init();
    int processPacket(char *packet);
    int decodeFrame(char *buffer, int size);
};

#endif
