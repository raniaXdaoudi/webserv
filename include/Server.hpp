#ifndef SERVER_HPP
#define SERVER_HPP

#include <string>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <iostream>
#include <unistd.h>
#include <sstream>
#include "FileHandler.hpp"
#include "Config.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "CGIHandler.hpp"
#include "SessionManager.hpp"

class Server {
private:
    int _socket_fd;
    int _port;
    struct sockaddr_in _address;
    FileHandler _file_handler;
    ServerConfig _config;
    CGIHandler _cgi_handler;
    SessionManager _session_manager;

    Response processRequest(const Request& request);
    Response handleRequest(const Request& request);
    Response handleError(int error_code);
    Response handleRedirection(const std::string& path);
    Response handleCGI(const Request& request, const std::string& path);

public:
    Server(const ServerConfig& config);
    ~Server();

    bool init();
    void run();
    void stop();
};

#endif 