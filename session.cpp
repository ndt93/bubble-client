#include "session.h"

#include <cstdio>
#include "macro_def.h"
#include "bubble_def.h"
#include "utils.h"

asio::io_service ios;

Session::Session(std::string server_ip, int server_port) : mSocket(ios)
{
    try
    {
        asio::ip::tcp::endpoint endpoint(asio::ip::address::from_string(server_ip), server_port);
        mSocket.connect(endpoint);
        std::printf("[INFO] Connected to %s:%d\n", server_ip.c_str(), server_port);
    } 
    catch (std::exception& e)
    {
        LOG_ERR(e.what());
    }
}

Session::~Session()
{
    mSocket.close();
}

int Session::send(const char *data, size_t size)
{
    int nbytes = -1;
    try
    {
        printf("[INFO] Sending %lu bytes\n", size);
        nbytes = asio::write(mSocket, asio::buffer((void*) data, size));
        printf("[INFO] Sent %d bytes\n", nbytes);
#ifdef DEBUG
        LOG_BUFFER("[DEBUG]Sent:", nbytes, data);
        LOG_BUFFER_HEX("---", nbytes, data);
#endif
    }
    catch (std::exception& e)
    {
        LOG_ERR(e.what());
    }
    return nbytes;
}

char* Session::receive_packet()
{
	char buffer[STRUCT_MEMBER_POS(PackHead, pData)];

	int nbytes = receive_til_full(buffer, sizeof(buffer));
	if (nbytes <= 0 || (size_t)nbytes < sizeof(buffer) || (unsigned char)buffer[0] != PACKHEAD_MAGIC)
	{
		LOG_ERR("Packet header is corrupted");
		return NULL;
	}

	PackHead* packhead = (PackHead*) buffer;
	uint32_t uiPackLength = ntohl(packhead->uiLength);
	size_t packetSize = uiPackLength + STRUCT_MEMBER_POS(PackHead, cPackType);
	assert(packetSize > sizeof(buffer));
	char *packet = new char[packetSize];
	size_t bytes_left = packetSize - sizeof(buffer);
	char *recv_start = packet + sizeof(buffer);

	memcpy(packet, buffer, sizeof(buffer));
	nbytes = receive_til_full(recv_start, bytes_left);
	if (nbytes < 0 || (size_t)nbytes != bytes_left)
	{
		LOG_ERR("Packet data is corrupted");
		delete[] packet;
		return NULL;
	}

	return packet;
}

int Session::receive_til_full(char *buffer, size_t size)
{
    int nbytes = -1;
    system::error_code ec;
    printf("[INFO] Receiving %lu bytes\n", size);
    nbytes = asio::read(mSocket, asio::buffer(buffer, size), ec);
    printf("[INFO] Received %lu bytes\n", size);
    if (ec)
    {
        std::fprintf(stderr, "[ERROR] Failed to receive data: %d\n", ec.value());
    }
    else
    {
#ifdef DEBUG
    LOG_BUFFER("[DEBUG] Received:", nbytes, buffer);
    LOG_BUFFER_HEX("---", nbytes, buffer);
#endif
    }
    return nbytes;
}

int Session::receive_at_least(char *buffer, size_t size, size_t at_least)
{
    int nbytes = -1;
    try
    {
        assert(at_least <= size);
        printf("[INFO] Receiving at least %lu bytes\n", at_least);
        nbytes = asio::read(mSocket, asio::buffer(buffer, size));
        printf("[INFO] Received %d bytes\n", nbytes);
#ifdef DEBUG
        LOG_BUFFER("[DEBUG] Received:", nbytes, buffer);
        LOG_BUFFER_HEX("---", nbytes, buffer);
#endif
    }
    catch (std::exception& e)
    {
        LOG_ERR(e.what());
    }
    return nbytes;
}
