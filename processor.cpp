#include "processor.h"
#include "macro_def.h"
#include <assert.h>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <vector>

Processor::Processor() : mSwsCtx(NULL), mRGBFrame(NULL), mRGBFrameBuffer(NULL), mAreaThreshold(150)
{
    cv::namedWindow("Stream");
    cv::namedWindow("Fg");
    mpMOG = cv::createBackgroundSubtractorMOG2(500, 16, false);
    mMorphOpenKernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(2, 2));
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
    int dest_width = width / 2;
    int dest_height = height / 2;

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

    cv::Mat mat(dest_height, dest_width, CV_8UC3,  mRGBFrame->data[0], mRGBFrame->linesize[0]);
    findForegroundObjects(mat);
    displayFrame(mat);

    return 0;
}

void Processor::findForegroundObjects(cv::Mat& mat)
{
    std::vector<std::vector<cv::Point> > contours;
    std::vector<cv::Vec4i> hierarchy;

    mpMOG->apply(mat, mFgMaskMOG);
    cv::morphologyEx(mFgMaskMOG, mFgMaskMOG, cv::MORPH_OPEN, mMorphOpenKernel);
    cv::findContours(mFgMaskMOG, contours, hierarchy, CV_RETR_EXTERNAL, CV_CHAIN_APPROX_SIMPLE);

    double max_area = 0;

    for (size_t i = 0; i < contours.size(); i++)
    {
        double area = cv::contourArea(contours[i]);
        if (area > max_area)
        {
            max_area = area;
        }
        if (area > mAreaThreshold)
        {
            cv::Rect rect = cv::boundingRect(contours[i]);
            cv::rectangle(mat, rect.tl(), rect.br(), cv::Scalar(0, 0, 255), 1, 8, 0);
        }
    }
}

void Processor::displayFrame(cv::Mat& mat)
{
    cv::imshow("Stream", mat);
    cv::imshow("Fg", mFgMaskMOG);
    cv::waitKey(10);
}

int Processor::allocateConversionCtx(enum AVPixelFormat src_pix_fmt, int width, int height)
{
    if (mSwsCtx)
    {
        return 0;
    }
    assert(mRGBFrame == NULL && mRGBFrameBuffer == NULL);

    int dest_width = width / 2;
    int dest_height = height / 2;
    mSwsCtx = sws_getContext(width, height, src_pix_fmt, dest_width, dest_height,
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
    mRGBFrame->width = dest_width;
    mRGBFrame->height = dest_height;
    mRGBFrame->format = AV_PIX_FMT_BGR24;

    int nbytes = av_image_get_buffer_size(AV_PIX_FMT_BGR24, dest_width, dest_height, 1);
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
                         AV_PIX_FMT_BGR24, dest_width, dest_height, 1);
    return 0;
}
