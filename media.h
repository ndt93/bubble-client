#ifndef __MEDIA_H__
#define __MEDIA_H__

#include "session.h"
#include "bubble_def.h"
#include "macro_def.h"
#include "util.h"
extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
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

    struct SwsContext *mSwsCtx;
    AVFrame *mRGBFrame;
    uint8_t *mRGBFrameBuffer;

    int init();
    int processPacket(char *packet);
    int decodeFrame(char *buffer, int size);
    int allocateConversionCtx(enum AVPixelFormat pix_fmt, int width, int height);
    int displayFrame(AVFrame *frame, int width, int height);
};

#endif
