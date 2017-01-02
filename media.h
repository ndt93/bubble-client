#ifndef __MEDIA_H__
#define __MEDIA_H__

#include "session.h"
#include "bubble_def.h"
#include "macro_def.h"
#include "util.h"

class MediaSession
{
public:
    MediaSession(Session *session);

    int start();
private:
    Session *mpSession;
    bool isRunning;
    char tmpRecvBuf[128*1024];

    int processPacket(char *packet);
};

#endif
