/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 12:15:17 by rania             #+#    #+#             */
/*   Updated: 2025/03/05 14:24:47 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Server.hpp"
#include "../include/Request.hpp"
#include "../include/Response.hpp"
#include "../include/Connection.hpp"
#include "../include/Logger.hpp"
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <iostream>
#include <algorithm>
#include <sys/stat.h>
#include <sstream>
#include <cstring>
#include <time.h>

std::string getCurrentTimestamp() {
	time_t now = time(0);
	struct tm *timeinfo = localtime(&now);
	char buffer[80];
	strftime(buffer, sizeof(buffer), "[%Y-%m-%d %H:%M:%S]", timeinfo);
	return std::string(buffer);
}

Server* Server::_instance = NULL;

Server::Server(ConfigParser& config) : _config(config), _running(false) {
	initializeServerSockets();
}

Server::~Server() {
	if (_instance == this) {
		_instance = NULL;
	}
	for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
		delete it->second;
	}
	_connections.clear();

	for (std::vector<int>::iterator it = _serverSockets.begin(); it != _serverSockets.end(); ++it) {
		close(*it);
	}
	_serverSockets.clear();
}

void Server::initializeServerSockets() {
	const std::vector<ServerConfig>& servers = _config.getServers();
	if (servers.empty()) {
		throw std::runtime_error("Aucun serveur configuré");
	}


	for (std::vector<ServerConfig>::const_iterator it = servers.begin(); it != servers.end(); ++it) {

		int serverSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (serverSocket < 0) {
			throw std::runtime_error("Erreur lors de la création du socket");
		}

		try {
			setupNonBlocking(serverSocket);
		} catch (const std::exception& e) {
			close(serverSocket);
			throw;
		}

		int opt = 1;
		if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
			close(serverSocket);
			throw std::runtime_error("Erreur setsockopt");
		}

		struct sockaddr_in serverAddr;
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(it->port);

		if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
			close(serverSocket);
			throw std::runtime_error("Erreur bind sur le port " + std::to_string(it->port));
		}

		if (listen(serverSocket, SOMAXCONN) < 0) {
			close(serverSocket);
			throw std::runtime_error("Erreur listen");
		}

		_serverSockets.push_back(serverSocket);
		_serverConfigs[serverSocket] = &(*it);
	}
}

void Server::start() {
	_running = true;
	std::vector<struct pollfd> fds;
	bool priorityToRead = true;  

	std::cout << getCurrentTimestamp() << " [INFO] Webserv starting..." << std::endl;

	for (std::vector<int>::iterator it = _serverSockets.begin(); it != _serverSockets.end(); ++it) {
		const ServerConfig* config = _serverConfigs[*it];
		if (config) {
			std::cout << getCurrentTimestamp() << " [INFO] Listening on 0.0.0.0:" << config->port << std::endl;
		}
	}
	std::cout << getCurrentTimestamp() << " [INFO] Configuration loaded from config/webserv.conf" << std::endl;

	while (_running) {
		fds.clear();

		
		for (std::vector<int>::iterator it = _serverSockets.begin(); it != _serverSockets.end(); ++it) {
			struct pollfd pfd;
			pfd.fd = *it;
			pfd.events = POLLIN;
			fds.push_back(pfd);
		}

		
		for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
			struct pollfd pfd;
			pfd.fd = it->first;
			pfd.events = POLLIN | POLLOUT;
			fds.push_back(pfd);
		}

		
		if (poll(&fds[0], fds.size(), 30000) < 0) {
			if (!_running) break;
			continue;
		}

		
		time_t currentTime = time(NULL);
		std::map<int, Connection*>::iterator it = _connections.begin();
		while (it != _connections.end()) {
			if (currentTime - it->second->getLastActivityTime() > 30) {
				std::cout << getCurrentTimestamp() << " [TIMEOUT] Connection " << it->first << " fermée pour inactivité" << std::endl;
				removeConnection(it->first);
				it = _connections.begin();
			} else {
				++it;
			}
		}

		
		for (size_t i = 0; i < fds.size(); ++i) {
			if (fds[i].revents == 0) continue;

			
			bool isServerSocket = false;
			for (std::vector<int>::iterator it = _serverSockets.begin(); it != _serverSockets.end(); ++it) {
				if (fds[i].fd == *it) {
					isServerSocket = true;
					if (fds[i].revents & POLLIN) {
						handleNewConnection(*it);
					}
					break;
				}
			}

			if (!isServerSocket && _connections.find(fds[i].fd) != _connections.end()) {
				Connection* conn = _connections[fds[i].fd];

				
				if (fds[i].revents & (POLLERR | POLLHUP | POLLNVAL)) {
					removeConnection(fds[i].fd);
					continue;
				}

				
				if (priorityToRead && (fds[i].revents & POLLIN)) {
					try {
						handleClientRead(conn);
					} catch (const std::exception& e) {
						std::cerr << getCurrentTimestamp() << " [ERROR] Erreur lecture client: " << e.what() << std::endl;
						removeConnection(fds[i].fd);
					}
				}
				else if (!priorityToRead && (fds[i].revents & POLLOUT) && !conn->getWriteBuffer().empty()) {
					try {
						handleClientWrite(conn);
					} catch (const std::exception& e) {
						std::cerr << getCurrentTimestamp() << " [ERROR] Erreur écriture client: " << e.what() << std::endl;
						removeConnection(fds[i].fd);
					}
				}
			}
		}

		
		priorityToRead = !priorityToRead;
	}

	
	for (std::map<int, Connection*>::iterator it = _connections.begin(); it != _connections.end(); ++it) {
		delete it->second;
	}
	_connections.clear();

	for (std::vector<int>::iterator it = _serverSockets.begin(); it != _serverSockets.end(); ++it) {
		close(*it);
	}
	_serverSockets.clear();
}

void Server::handleClientData(Connection* conn) {
	try {
		Request request(conn->getBuffer(), _config);
		std::string url = request.getUrl();
		std::string host = request.getHeader("Host");


		const ServerConfig* serverConfig = findMatchingServer(conn->getSocket(), host);
		if (!serverConfig) {
			std::cout << getCurrentTimestamp() << " [ERROR] Pas de serveur trouvé pour l'hôte: " << host << std::endl;
			Response response(conn, "", _config, request);
			response.handleError(404);
			return;
		}

		std::string root = (host == "api.127.0.0.1") ? "www/api" : "www";
		std::string fullPath = root + url;


		const Route* currentRoute = _config.findRoute(serverConfig, url);
		if (!currentRoute) {
			std::cout << getCurrentTimestamp() << " [ERROR] Pas de route trouvée pour: " << url << std::endl;
			Response response(conn, "", _config, request);
			response.handleError(404);
			return;
		}

		std::string method = request.getMethod();
		if (!currentRoute->isMethodAllowed(method)) {
			std::cout << getCurrentTimestamp() << " [ERROR] Méthode non autorisée: " << method << std::endl;
			Response response(conn, "", _config, request);
			response.handleError(405);
			return;
		}

		if (url == "/") {
			fullPath = root + "/" + currentRoute->getIndexFile();
		}

		Response response(conn, fullPath, _config, request);
		if (method == "GET") {
			response.handleGetRequest();
		} else if (method == "POST") {
			response.handlePostRequest(request.getBody());
		} else if (method == "DELETE") {
			handleDeleteRequest(conn->getSocket(), request);
		}

		conn->clearBuffer();

	} catch (const std::exception& e) {
		std::cout << getCurrentTimestamp() << " [ERROR] Exception lors du traitement de la requête: "
				  << e.what() << std::endl;

		Response response(conn, "", _config, Request());
		response.handleError(500);
	} catch (...) {
		std::cout << getCurrentTimestamp() << " [ERROR] Exception inconnue lors du traitement de la requête"
				  << std::endl;

		Response response(conn, "", _config, Request());
		response.handleError(500);
	}
}

void Server::setupNonBlocking(int socket) {
	int flags = fcntl(socket, F_GETFL, 0);
	if (flags < 0) {
		throw std::runtime_error("Erreur configuration socket non-bloquant");
	}

	if (fcntl(socket, F_SETFL, flags | O_NONBLOCK) < 0) {
		throw std::runtime_error("Erreur configuration socket non-bloquant");
	}
}

void Server::handleNewConnection(int serverSocket) {
	struct sockaddr_in clientAddr;
	socklen_t clientLen = sizeof(clientAddr);

	int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
	if (clientSocket < 0) {
		return;
	}

	try {
		setupNonBlocking(clientSocket);
		Connection* conn = new Connection(clientSocket, _config);
		conn->setServerSocket(serverSocket);
		_connections[clientSocket] = conn;
	} catch (const std::exception& e) {
		close(clientSocket);
		std::cerr << getCurrentTimestamp() << " [ERROR] Erreur lors de l'acceptation de la connexion: " << e.what() << std::endl;
	}
}

void Server::handleClientRead(Connection* conn) {
	char buffer[4096];
	ssize_t bytesRead = recv(conn->getSocket(), buffer, sizeof(buffer) - 1, 0);

	if (bytesRead <= 0) {
		removeConnection(conn->getSocket());
		return;
	}

	buffer[bytesRead] = '\0';
	std::string newData(buffer, bytesRead);

	
	conn->appendToBuffer(newData);

	
	std::string currentBuffer = conn->getBuffer();

	
	size_t headerEnd = currentBuffer.find("\r\n\r\n");
	if (headerEnd == std::string::npos) {
		return;
	}

	
	std::string headers = currentBuffer.substr(0, headerEnd);
	size_t contentLengthPos = headers.find("Content-Length: ");
	if (contentLengthPos != std::string::npos) {
		size_t endOfLine = headers.find("\r\n", contentLengthPos);
		std::string contentLengthStr = headers.substr(contentLengthPos + 16, endOfLine - (contentLengthPos + 16));
		size_t expectedLength = std::stoul(contentLengthStr);

		
		const ServerConfig* serverConfig = findMatchingServer(conn->getSocket(), "");
		if (serverConfig && expectedLength > serverConfig->client_max_body_size) {
			Response response(conn, "", _config, Request());
			response.handleError(413);
			conn->clearBuffer();
			return;
		}

		size_t currentBodyLength = currentBuffer.length() - (headerEnd + 4);

		
		if (currentBodyLength < expectedLength) {
			return;
		}
	}

	
	try {
		handleClientData(conn);
	} catch (const std::exception& e) {
		std::cerr << getCurrentTimestamp() << " [ERROR] Erreur lors du traitement de la requête: " << e.what() << std::endl;
		removeConnection(conn->getSocket());
		return;
	}

	
	conn->clearBuffer();
}

void Server::handleClientWrite(Connection* conn) {
	const std::string& writeBuffer = conn->getWriteBuffer();
	if (writeBuffer.empty()) {
		return;
	}

	ssize_t bytesSent = send(conn->getSocket(), writeBuffer.c_str(), writeBuffer.length(), 0);
	if (bytesSent <= 0) {
		removeConnection(conn->getSocket());
		return;
	}

	if (static_cast<size_t>(bytesSent) < writeBuffer.length()) {
		conn->clearWriteBuffer();
		conn->appendToWriteBuffer(writeBuffer.substr(bytesSent));
	} else {
		conn->clearWriteBuffer();
	}
}

void Server::removeConnection(int socket) {
	std::cout << getCurrentTimestamp() << " [INFO] Connection closed: " << socket << std::endl;

	std::map<int, Connection*>::iterator it = _connections.find(socket);
	if (it != _connections.end()) {
		delete it->second;
		_connections.erase(it);
	}
	close(socket);
}

const ServerConfig* Server::findMatchingServer(int socket, const std::string& hostHeader) const {
	
	if (!hostHeader.empty()) {
	}

	
	std::map<int, Connection*>::const_iterator conn_it = _connections.find(socket);
	if (conn_it == _connections.end()) {
		if (!hostHeader.empty()) {
			std::cout << getCurrentTimestamp() << " [ERROR] Aucune connexion trouvée pour le socket " << socket << std::endl;
		}
		return NULL;
	}

	
	int serverSocket = conn_it->second->getServerSocket();
	std::map<int, const ServerConfig*>::const_iterator it = _serverConfigs.find(serverSocket);
	if (it == _serverConfigs.end()) {
		if (!hostHeader.empty()) {
			std::cout << getCurrentTimestamp() << " [ERROR] Aucun serveur trouvé pour le socket serveur " << serverSocket << std::endl;
		}
		return NULL;
	}

	
	std::string host = hostHeader;
	size_t pos = host.find(':');
	if (pos != std::string::npos) {
		host = host.substr(0, pos);
	}


	
	const std::vector<std::string>& server_names = it->second->server_names;
	for (std::vector<std::string>::const_iterator name_it = server_names.begin();
		 name_it != server_names.end(); ++name_it) {
		if (*name_it == host) {
			return it->second;
		}
	}
	return it->second;
}

void Server::handleDeleteRequest(int clientSocket, Request &request) {
	Connection* tempConn = _connections[clientSocket];
	const ServerConfig* serverConfig = findMatchingServer(clientSocket, request.getHeader("Host"));
	if (!serverConfig) {
		Response response(tempConn, "", _config, request);
		response.handleError(404);
		return;
	}

	std::string path = request.getUrl();
	std::string fullPath = "www" + path;

	struct stat path_stat;
	stat(fullPath.c_str(), &path_stat);
	if (S_ISDIR(path_stat.st_mode)) {
		Response response(tempConn, "", _config, request);
		response.handleError(403);  
		return;
	}

	const Route* route = _config.findRoute(serverConfig, path);
	if (route && !route->isMethodAllowed("DELETE")) {
		Response response(tempConn, "", _config, request);
		response.handleError(405);  
		return;
	}

	if (unlink(fullPath.c_str()) == 0) {
		Response response(tempConn, "", _config, request);
		response.sendResponse(204, "text/plain");
	} else {
		Response response(tempConn, "", _config, request);
		response.handleError(404);
	}
}

void Server::handleCGI(int clientSocket, const std::string& scriptPath, const Request& request) {
	Response response(_connections[clientSocket], scriptPath, _config, request);
	response.handleCgiRequest();
}

void Server::handleGetRequest(int clientSocket, Request& request) {
	Connection* conn = _connections[clientSocket];
	Response response(conn, "", _config, request);
	response.handleGetRequest();
}

void Server::handlePostRequest(int clientSocket, Request& request) {
	Connection* conn = _connections[clientSocket];
	Response response(conn, "", _config, request);
	response.handlePostRequest(request.getBody());
}
