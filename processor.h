#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
}

class Processor
{
public:
    Processor();
    ~Processor();
    int process(AVFrame *frame);
private:
    struct SwsContext *mSwsCtx;
    AVFrame *mRGBFrame;
    uint8_t *mRGBFrameBuffer;

    int allocateConversionCtx(enum AVPixelFormat src_pix_fmt, int width, int height);
};

#endif
