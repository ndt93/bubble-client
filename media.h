#ifndef __MEDIA_H__
#define __MEDIA_H__

#include "session.h"
#include "bubble_def.h"
#include "macro_def.h"
#include "util.h"
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
    char tmpRecvBuf[128*1024];

    AVCodec *mCodec;
    AVCodecContext *mCodecCtx;
    AVPacket mAvPkt;
    AVFrame *mAvFrame;

    int init();
    int processPacket(char *packet);
};

#endif
