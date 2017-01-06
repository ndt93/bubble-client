#include "media.h"
#include <cstdio>

//static clock_t session_start;
//static uint framecount = 0;

MediaSession::MediaSession(Session *session) :
    mpSession(session), isRunning(false), mCodec(NULL), mCodecCtx(NULL), mAvFrame(NULL)
{
    avcodec_register_all();
}

MediaSession::~MediaSession()
{
    if (publisher.isStarted)
    {
        publisher.stop();
    }
    if (mCodecCtx)
    {
        avcodec_close(mCodecCtx);
        av_free(mCodecCtx);
    }
    if (mAvFrame)
    {
        av_frame_free(&mAvFrame);
    }
}

int MediaSession::init()
{
    std::printf("[INFO] Initializing media session...\n");
    mCodec = avcodec_find_decoder(AV_CODEC_ID_H264);
    if (!mCodec)
    {
        LOG_ERR("Codec not found");
        return -1;
    }

    mCodecCtx = avcodec_alloc_context3(mCodec);
    mAvFrame = av_frame_alloc();

    if (avcodec_open2(mCodecCtx, mCodec, NULL) < 0)
    {
        LOG_ERR("Failed to open codec");
        goto on_error;
    }

    av_init_packet(&mAvPkt);
    std::printf("[INFO] Initialized media session\n");
    return 0;

on_error:
    avcodec_close(mCodecCtx);
    av_free(mCodecCtx);
    av_frame_free(&mAvFrame);
    mCodecCtx = NULL;
    return -1;
}

int MediaSession::start()
{
    int status;
    PackHead *packhead;

    status = init();
    if (status != 0)
    {
        LOG_ERR("Failed to initialize media session");
        return -1;
    }

    std::printf("[INFO] Receiving media frames\n");
    status = mpSession->receive_packet_to_buffer(tmpRecvBuf, sizeof(tmpRecvBuf));
    if (status != 0)
    {
        LOG_ERR("Failed to receive first media frame");
    }
    packhead = (PackHead *)tmpRecvBuf;
    if (packhead->cPackType == 0x08)
    {
        LOG_ERR("The media server is full. Try again later");
        return -1;
    }

    isRunning = true;

    while (isRunning)
    {
        status = processPacket(tmpRecvBuf);
        if (status != 0)
        {
            LOG_ERR("Failed to process media packet");
            break;
        }

        status = mpSession->receive_packet_to_buffer(tmpRecvBuf, sizeof(tmpRecvBuf));
        if (status != 0)
        {
            LOG_ERR("Failed to receive media packet");
            break;
        }
    }

    return 0;
}

int MediaSession::processPacket(char *packet)
{
    PackHead *packhead;
    uint32_t uiPackLen;
    size_t packsize;
    MediaPackData *media_packhead;
    uint32_t uiMediaPackLen;
    char *framedata;
    int status;

    packhead = (PackHead *)packet;
    uiPackLen = ntohl(packhead->uiLength);
    packsize = uiPackLen + STRUCT_MEMBER_POS(PackHead, cPackType);

    media_packhead = (MediaPackData *)packhead->pData;
    uiMediaPackLen = ntohl(media_packhead->uiLength);
    if (uiMediaPackLen + PACKHEAD_SIZE + STRUCT_MEMBER_POS(MediaPackData, pData) > packsize)
    {
        LOG_ERR("Frame data is larger than packet size");
        return -1;
    }

#ifdef DEBUG
    std::printf("[INFO] Media packet chl: %d type: %d\n", media_packhead->cId, media_packhead->cMediaType);
#endif
    framedata = media_packhead->pData;
    switch (media_packhead->cMediaType)
    {
    case MT_IDR:
    case MT_PSLICE:
    {
        status = decodeFrame(framedata, uiMediaPackLen);
        /*framecount++;
        if (framecount == 0) {
            session_start = clock();
        } else {
            std::printf("[FPS] %.3lf\n", framecount / ((double)(clock() - session_start)/CLOCKS_PER_SEC));
        }*/
    }
        break;
    case MT_AUDIO:
        break;
    default:
        LOG_ERR("Unknown media pack type");
    }

    return 0;
}


int MediaSession::decodeFrame(char *buffer, int size)
{
    int status;
    mAvPkt.size = size;
    mAvPkt.data = (uint8_t *)buffer;

    status = avcodec_send_packet(mCodecCtx, &mAvPkt);
    if (status != 0)
    {
        std::fprintf(stderr, "[ERROR] avcodec_send_packet: %d\n", status);
        return -1;
    }

    while (true)
    {
        status = avcodec_receive_frame(mCodecCtx, mAvFrame);
        if (status == AVERROR(EAGAIN) || status == AVERROR_EOF)
        {
            break;
        }
        if (status != 0)
        {
            LOG_ERR("Error decoding frame");
            return -1;
        }
#ifdef DEBUG
        std::printf("[INFO] Frame decoded w: %d h: %d!!!\n", mCodecCtx->width, mCodecCtx->height);
#endif

        processor.process(mAvFrame);

        if (!publisher.isInitialized)
        {
            //status = publisher.init("rtmp://a.rtmp.youtube.com/live2/cxy2-h593-symr-44e8", mCodecCtx);
            status = publisher.init("out.flv", mCodecCtx);
            if (status != 0)
            {
                LOG_ERR("Failed to initialize publisher");
                continue;
            }
            else
            {
                status = publisher.start();
                if (status != 0)
                {
                    LOG_ERR("Failed to start publisher");
                    continue;
                }
            }
         }
        publisher.pushFrame(mAvFrame);
    }

    return 0;
}
