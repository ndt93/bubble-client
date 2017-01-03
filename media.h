#ifndef __MEDIA_H__
#define __MEDIA_H__

#include "session.h"
#include "bubble_def.h"
#include "macro_def.h"
#include "util.h"
#include "processor.h"

extern "C" {
#include <libavcodec/avcodec.h>
}

class MediaSession
{
public:
    MediaSession(Session *session);
    ~MediaSession();

    int start();
private:
    Session *mpSession;
    bool isRunning;
    char tmpRecvBuf[128*1024 + FF_INPUT_BUFFER_PADDING_SIZE];

    AVCodec *mCodec;
    AVCodecContext *mCodecCtx;
    AVPacket mAvPkt;
    AVFrame *mAvFrame;

    Processor processor;

    int init();
    int processPacket(char *packet);
    int decodeFrame(char *buffer, int size);
};

#endif
