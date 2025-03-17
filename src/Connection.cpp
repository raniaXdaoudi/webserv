/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Connection.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/04 11:45:00 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:24:21 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Connection.hpp"
#include <iostream>
#include "Request.hpp"
#include "ConfigParser.hpp"
#include "Route.hpp"
#include <sstream>
#include <unistd.h>
#include <ctime>

Connection::Connection(int socket, ConfigParser& config)
	: _socket(socket)
	, _serverSocket(-1)
	, _bytesSent(0)
	, _config(config)
	, _lastActivityTime(time(NULL))
{
}

Connection::~Connection() {
	if (_socket != -1) {
		close(_socket);
	}
}

std::string Connection::getClientInfo() const {
	std::ostringstream info;
	info << "Socket: " << _socket;
	if (_serverSocket != -1) {
		info << ", Server: " << _serverSocket;
	}
	return info.str();
}

void Connection::handleRequest(const std::string& requestData) {
	Request request(requestData, _config);
	std::string filePath = request.getUrl();


}
