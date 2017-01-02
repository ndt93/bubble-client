#ifndef __SESSION__
#define __SESSION__

#include <string>
#include <boost/asio.hpp>

using namespace boost;

class Session {
public:
    Session(std::string server_ip, int server_port);
    ~Session();

    int send(const char *data, size_t size);
    int receive_til_full(char *buffer, size_t size);
    int receive_at_least(char *buffer, size_t size, size_t at_least);
    char* receive_packet();
    int receive_packet_to_buffer(char *buffer, size_t buffer_size);
private:
    asio::io_service mIOService;
    asio::ip::tcp::socket mSocket;
};

#endif
