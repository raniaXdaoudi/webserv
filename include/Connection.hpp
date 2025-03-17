/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/22 19:37:45 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:22:57 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <string>
#include <vector>
#include "Request.hpp"
#include "ConfigParser.hpp"
#include <ctime>

class Connection {
private:
	int _socket;
	int _serverSocket;
	size_t _bytesSent;
	std::string _buffer;
	std::string _writeBuffer;
	std::vector<char> _requestData;
	ConfigParser& _config;
	time_t _lastActivityTime;

public:
	Connection(int socket, ConfigParser& config);
	~Connection();

	int getSocket() const { return _socket; }
	int getServerSocket() const { return _serverSocket; }
	void setServerSocket(int socket) { _serverSocket = socket; }

	const std::string& getBuffer() const { return _buffer; }
	void appendToBuffer(const std::string& data) { _buffer += data; _lastActivityTime = time(NULL); }
	void clearBuffer() { _buffer.clear(); }

	const std::string& getWriteBuffer() const { return _writeBuffer; }
	void appendToWriteBuffer(const std::string& data) { _writeBuffer += data; _lastActivityTime = time(NULL); }
	void clearWriteBuffer() { _writeBuffer.clear(); }

	size_t getBytesSent() const { return _bytesSent; }
	void setBytesSent(size_t bytes) { _bytesSent = bytes; }

	time_t getLastActivityTime() const { return _lastActivityTime; }
	void updateLastActivityTime() { _lastActivityTime = time(NULL); }

	std::string getClientInfo() const;
	void handleRequest(const std::string& requestData);
};

#endif
