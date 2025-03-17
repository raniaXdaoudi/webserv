#include "../include/Logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <sys/stat.h>
#include <iostream>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>

std::ofstream Logger::_logFile;

void Logger::init(const std::string& logPath) {

	struct stat st;
	if (stat("logs", &st) != 0) {
		mkdir("logs", 0755);
	}

	_logFile.open(logPath.c_str(), std::ios::app);
	if (!_logFile.is_open()) {
		throw std::runtime_error("Impossible d'ouvrir le fichier de log");
	}
}

void Logger::close() {
	if (_logFile.is_open()) {
		_logFile.close();
	}
}

std::string Logger::_getFormattedTime() {
	time_t now = time(0);
	struct tm* timeinfo = localtime(&now);
	char buffer[80];
	strftime(buffer, sizeof(buffer), "%d/%b/%Y:%H:%M:%S %z", timeinfo);
	return std::string(buffer);
}

std::string Logger::_getClientIP(int clientSocket) {
	struct sockaddr_in addr;
	socklen_t addr_size = sizeof(addr);
	getpeername(clientSocket, (struct sockaddr*)&addr, &addr_size);
	return std::string(inet_ntoa(addr.sin_addr));
}

void Logger::log(int clientSocket, const Request& request, int statusCode, size_t bodySize) {
	std::string clientIP = _getClientIP(clientSocket);
	std::string timestamp = _getFormattedTime();
	std::string user = "-";
	std::string requestLine = request.getMethod() + " " + request.getUrl() + " HTTP/1.1";

	std::stringstream logMessage;
	logMessage << clientIP << " - " << user << " [" << timestamp << "] \""
			   << requestLine << "\" " << statusCode << " " << bodySize << "\n";

	std::string message = logMessage.str();

	int fd = open("logs/access.log", O_WRONLY | O_APPEND | O_CREAT | O_NONBLOCK, 0644);
	if (fd < 0) {
		std::cerr << "Impossible d'ouvrir le fichier de log" << std::endl;
		return;
	}

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLOUT;

	size_t totalWritten = 0;
	while (totalWritten < message.length()) {
		int ret = poll(&pfd, 1, 1000);
		if (ret <= 0) {
			::close(fd);
			return;
		}

		if (pfd.revents & POLLOUT) {
			ssize_t bytesWritten = write(fd, message.c_str() + totalWritten,
									   message.length() - totalWritten);
			if (bytesWritten <= 0) {
				::close(fd);
				return;
			}
			totalWritten += bytesWritten;
		}
	}

	::close(fd);
	std::cout << "\033[36m" << message << "\033[0m";
}
