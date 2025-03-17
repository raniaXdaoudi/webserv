/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 12:15:20 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:23:45 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <vector>
#include <map>
#include <poll.h>
#include <string>
#include "Connection.hpp"
#include "ConfigParser.hpp"
#include "Request.hpp"
#include "Response.hpp"

class Response;

class Server {
private:
	static Server* _instance;
	ConfigParser& _config;
	std::vector<int> _serverSockets;
	std::vector<struct pollfd> _pollfds;
	std::map<int, Connection*> _connections;
	static const int TIMEOUT_MS = 30000;
	bool _running;


	std::map<int, const ServerConfig*> _serverConfigs;

	void setupNonBlocking(int socket);
	void handleNewConnection(int serverSocket);
	void handleClientData(Connection* connection);
	void removeConnection(int clientSocket);
	void handleDeleteRequest(int clientSocket, Request &request);
	void handleClientWrite(Connection* conn);
	void handleGetRequest(int clientSocket, Request &request);
	void handlePostRequest(int clientSocket, Request &request);
	void handleClient(int clientSocket);

public:
	static Server* getInstance() { return _instance; }
	static void createInstance(ConfigParser& config) {
		if (!_instance) {
			_instance = new Server(config);
		}
	}
	static void destroyInstance() {
		delete _instance;
		_instance = NULL;
	}

	void start();
	void stop() { _running = false; }
	void handleCGI(int clientSocket, const std::string& scriptPath, const Request& request);
	const ServerConfig* findMatchingServer(int socket, const std::string& hostHeader) const;

private:
	Server(ConfigParser& config);
	~Server();

	void initializeServerSockets();
	void handleClientRead(Connection* conn);
};

#endif
