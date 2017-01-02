#ifndef __MAIN__
#define __MAIN__

#include <cstdio>
#include <string>
#include "macro_def.h"
#include "bubble_def.h"
#include "session.h"

PackHead* write_packhead(uint data_size, char cPackType, char *buffer);
bool check_packet_len(uint32_t uiPackLength, size_t expected_data_size);

int init_bubble_session(Session& session);

bool verify_user(Session& session, const std::string& username, const std::string& password);
bool send_user_creds(Session& session, const std::string& username, const std::string& password);
bool recv_verify_user_result(Session& session);

int open_stream(Session& session, uint channel, uint stream_id);

#endif
