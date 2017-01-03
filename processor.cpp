#include "processor.h"
#include "macro_def.h"
#include <assert.h>
#include <opencv2/highgui.hpp>

Processor::Processor() : mSwsCtx(NULL), mRGBFrame(NULL), mRGBFrameBuffer(NULL)
{
}

Processor::~Processor()
{
    if (mSwsCtx)
    {
        sws_freeContext(mSwsCtx);
    }
    if (mRGBFrame)
    {
        av_free(mRGBFrame);
    }
    if (mRGBFrameBuffer)
    {
        free(mRGBFrameBuffer);
    }
}

int Processor::process(AVFrame *frame)
{
    int status;
    int width = frame->width;
    int height = frame->height;

    status = allocateConversionCtx((enum AVPixelFormat)frame->format, width, height);
    if (status < 0)
    {
        LOG_ERR("Failed to allocate conversion resource");
        return -1;
    }

    status = sws_scale(mSwsCtx, frame->data, frame->linesize, 0, height, mRGBFrame->data, mRGBFrame->linesize);
    if (status < 0)
    {
        LOG_ERR("Failed to scale frame");
        return -1;
    }

    cv::Mat mat(height, width, CV_8UC3,  mRGBFrame->data[0], mRGBFrame->linesize[0]);
    cv::namedWindow("Stream");
    cv::imshow("Stream", mat);
    cv::waitKey(10);

    return 0;
}

int Processor::allocateConversionCtx(enum AVPixelFormat src_pix_fmt, int width, int height)
{
    if (mSwsCtx)
    {
        return 0;
    }
    assert(mRGBFrame == NULL && mRGBFrameBuffer == NULL);

    mSwsCtx = sws_getContext(width, height, src_pix_fmt, width, height,
                             AV_PIX_FMT_BGR24, SWS_BICUBIC, NULL, NULL, NULL);
    if (!mSwsCtx)
    {
        LOG_ERR("Failed to allocate sws context");
        return -1;
    }

    mRGBFrame = av_frame_alloc();
    if (!mRGBFrame)
    {
        LOG_ERR("Failed to allocate RGB frame");
        sws_freeContext(mSwsCtx);
        mSwsCtx = NULL;
        return -1;
    }
    mRGBFrame->width = width;
    mRGBFrame->height = height;
    mRGBFrame->format = AV_PIX_FMT_BGR24;

    int nbytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, width, height, 1);
    mRGBFrameBuffer = (uint8_t *)av_malloc(nbytes);
    if (!mRGBFrameBuffer)
    {
        LOG_ERR("Failed to allocate RGB frame buffer");
        sws_freeContext(mSwsCtx);
        av_free(mRGBFrame);
        mSwsCtx = NULL;
        mRGBFrame = NULL;
        return -1;
    }
    av_image_fill_arrays(mRGBFrame->data, mRGBFrame->linesize, mRGBFrameBuffer,
                         AV_PIX_FMT_BGR24, width, height, 1);
    return 0;
}
