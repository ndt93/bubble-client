#ifndef __TRANSPORT__
#define __TRANSPORT__

#include <string>
#include <boost/asio.hpp>

using namespace boost;

extern asio::io_service ios;

class Session {
public:
    Session(std::string server_ip, int server_port);
    ~Session();

    int send(const char *data, size_t size);
    int receive_til_full(char *buffer, size_t size);
    int receive_at_least(char *buffer, size_t size, size_t at_least);
    char* receive_packet();
private:
    asio::ip::tcp::socket mSocket;
};

#endif
