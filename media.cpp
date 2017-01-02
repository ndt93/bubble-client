#include "media.h"

MediaSession::MediaSession(Session *session) : mpSession(session), isRunning(false)
{
}

int MediaSession::start()
{
    int status;
    PackHead *packhead;

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
    std::printf("[INFO] Receiving media frames");

    while (isRunning)
    {
        processPacket(tmpRecvBuf);
        status = mpSession->receive_packet_to_buffer(tmpRecvBuf, sizeof(tmpRecvBuf));
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

    framedata = media_packhead->pData;
    return 0;
}
