#ifndef __PROCESSOR_H__
#define __PROCESSOR_H__

extern "C" {
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
#include <libavutil/pixfmt.h>
#include <libavutil/imgutils.h>
}
#include <opencv2/video/background_segm.hpp>

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
    const double mAreaThreshold;

    cv::Ptr<cv::BackgroundSubtractor> mpMOG;
    cv::Mat mFgMaskMOG;
    cv::Mat mMorphOpenKernel;

    int allocateConversionCtx(enum AVPixelFormat src_pix_fmt, int src_w, int src_h, int dst_w, int dst_h);
    void findForegroundObjects(cv::Mat& mat);
    void displayFrame(cv::Mat& mat);
};

#endif
