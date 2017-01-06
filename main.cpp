#include "main.h"
#include "time.h"
#include "utils.h"
#include "media.h"

static const std::string USAGE = "Usage: bubble_client server_ip server_port username password";

#define BUF_SIZE (2 * 1024)
static char buffer[BUF_SIZE];

int main(int argc, char *argv[])
{
    if (argc != 5)
    {
        std::printf("%s\n", USAGE.c_str());
        return 1;
    }
    const std::string server_ip(argv[1]);
    const int server_port = atoi(argv[2]);
    const std::string username(argv[3]);
    const std::string password(argv[4]);
    int status;
    Session session(server_ip, server_port);
    MediaSession media_session(&session);

    status = init_bubble_session(session);
    if (status != 0)
    {
       return 1;
    }
    std::printf("[INFO] Bubble session initialized\n");

    if (!verify_user(session, username, password))
    {
        LOG_WARN("Username or password is invalid");
        return 1;
    }
    std::printf("[INFO] User is logged in\n");

    status = open_stream(session, 0, 1);
    if (status != 0)
    {
        LOG_ERR("Failed to open stream");
        return 1;
    }
    std::printf("[INFO] Stream is opened\n");

    media_session.start();

    return 0;
}

PackHead* write_packhead(uint data_size, char cPackType, char *buffer)
{
    PackHead *packhead = (PackHead *)buffer;
    struct timespec timetic;

    packhead->cHeadChar = PACKHEAD_MAGIC;
    packhead->cPackType = cPackType;
    clock_gettime(CLOCK_MONOTONIC, &timetic);
    packhead->uiTicket = htonl(timetic.tv_sec + timetic.tv_nsec/1000);
    packhead->uiLength = htonl(STRUCT_MEMBER_POS(PackHead, pData)
                               - STRUCT_MEMBER_POS(PackHead, cPackType)
                               + data_size);

    return packhead;
}

bool check_packet_len(uint32_t uiPackLength, size_t expected_data_size)
{
	return uiPackLength == expected_data_size +
	        (STRUCT_MEMBER_POS(PackHead, pData) - STRUCT_MEMBER_POS(PackHead, cPackType));
}

int init_bubble_session(Session& session)
{
    const char init_session_req[] = "GET /bubble/live?ch=0&stream=0 HTTP/1.1\r\n\r\n";
    const size_t init_resp_size = 1142;
    int nbytes;

    nbytes = session.send(init_session_req, sizeof(init_session_req) - 1);
    if (nbytes < 0 || (size_t)nbytes != sizeof(init_session_req) - 1)
    {
        LOG_ERR("Failed to send session init request");
        return -1;
    }
    nbytes = session.receive_til_full(buffer, init_resp_size);
    if (nbytes <= 0)
    {
        LOG_ERR("Failed to initialize bubble session");
        return -1;
    }
    return 0;
}

int open_stream(Session& session, uint channel, uint stream_id)
{
    PackHead *packhead;
    BubbleOpenStream openStreamPack;
    size_t packsize;
    int nbytes;

    std::printf("[INFO] Opening stream: channel=%u stream_id=%u\n", channel, stream_id);

    packsize = GET_PACKSIZE(sizeof(BubbleOpenStream));
    assert(packsize <= BUF_SIZE);

    packhead = write_packhead(sizeof(BubbleOpenStream), PT_OPENSTREAM, buffer);

    memset(&openStreamPack, 0, sizeof(BubbleOpenStream));
    openStreamPack.uiChannel = channel;
    openStreamPack.uiStreamId = stream_id;
    openStreamPack.uiOpened = 1;
    memcpy(packhead->pData, &openStreamPack, sizeof(BubbleOpenStream));

    nbytes = session.send(buffer, packsize);
    if (nbytes < 0 || (size_t)nbytes != packsize)
    {
        LOG_ERR("Failed to send open stream packet");
        return -1;
    }

    return 0;
}

bool verify_user(Session& session, const std::string& username, const std::string& password)
{
    if (username.length() > MAX_USERNAME_LEN || password.length() > MAX_PASSWORD_LEN)
    {
        LOG_WARN("Username or password is too long");
    }

    if (!send_user_creds(session, username, password))
    {
        return false;
    }
    return recv_verify_user_result(session);
}

bool recv_verify_user_result(Session& session)
{
	char *packet = session.receive_packet();
	PackHead *packhead;
	uint32_t uiPackLength;
	MsgPackData *msgpack;
	UserVrfB *vrfResult;

	if (!packet)
	{
		LOG_ERR("Failed to receive user verification result");
		return false;
	}

	packhead = (PackHead*) packet;
	if (packhead->cPackType != 0x00)
	{
		std::fprintf(stderr, "[ERROR] Packet type %02x is invalid\n", packhead->cPackType);
		goto on_error;
	}

	uiPackLength = ntohl(packhead->uiLength);
	if (!check_packet_len(uiPackLength, sizeof(UserVrfB) + STRUCT_MEMBER_POS(MsgPackData, pMsg)))
	{
	    LOG_ERR("Packet size is invalid");
	    goto on_error;
	}

	msgpack = (MsgPackData *)packhead->pData;
	if (msgpack->cMsgType[0] != MSGT_USERVRF_B)
	{
		std::fprintf(stderr, "[ERROR] Message type %02x is invalid\n", msgpack->cMsgType[0]);
		goto on_error;
	}

	vrfResult = (UserVrfB *)msgpack->pMsg;
	if (vrfResult->bVerify != 1)
	{
	    LOG_ERR("Username or password is invalid");
	    goto on_error;
	}

	delete[] packet;
	return true;

on_error:
    delete[] packet;
    return false;
}

bool send_user_creds(Session& session, const std::string& username, const std::string& password)
{
    int packsize = GET_PACKSIZE(STRUCT_MEMBER_POS(MsgPackData, pMsg) + sizeof(UserVrf));
    assert(packsize <= BUF_SIZE);

    PackHead* packhead;
    MsgPackData msg;
    UserVrf usr_creds;

    packhead = write_packhead(STRUCT_MEMBER_POS(MsgPackData, pMsg) + sizeof(UserVrf),
                              PT_MSGPACK, buffer);

    memset(&msg, 0, sizeof(MsgPackData));
    msg.cMsgType[0] = MSGT_USERVRF;
    msg.uiLength = htonl(sizeof(msg.cMsgType) + sizeof(UserVrf));

    strncpy((char*)usr_creds.sUserName, username.c_str(), MAX_USERNAME_LEN);
    strncpy((char*)usr_creds.sPassWord, password.c_str(), MAX_PASSWORD_LEN);

    memcpy(packhead->pData, &msg, STRUCT_MEMBER_POS(MsgPackData, pMsg));
    memcpy(packhead->pData + STRUCT_MEMBER_POS(MsgPackData, pMsg), &usr_creds, sizeof(UserVrf));

    int nbytes = session.send(buffer, packsize);
    if (nbytes != packsize) {
        LOG_ERR("Failed to send user credentials");
        return false;
    }
    return true;
}
