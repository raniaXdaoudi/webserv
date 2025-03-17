#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include "Request.hpp"
#include "Response.hpp"

class Logger {
private:
	static std::ofstream _logFile;
	static std::string _getFormattedTime();
	static std::string _getClientIP(int clientSocket);

public:
	static void init(const std::string& logPath = "logs/access.log");
	static void log(int clientSocket, const Request& request, int statusCode, size_t bodySize);
	static void close();
};

#endif
