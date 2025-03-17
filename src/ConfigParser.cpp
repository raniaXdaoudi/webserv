/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 13:03:16 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:24:04 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/ConfigParser.hpp"
#include "Server.hpp"
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <sys/stat.h>


extern std::string getCurrentTimestamp();

ConfigParser::ConfigParser() : _root("/"), _clientMaxBodySize(1024 * 1024), _autoindex(false) {}

ConfigParser::~ConfigParser() {}

void ConfigParser::parse(const std::string& filename) {
	std::ifstream file(filename.c_str());
	if (!file.is_open()) {
		throw std::runtime_error("Impossible d'ouvrir le fichier de configuration");
	}

	std::string line;
	Route* currentRoute = NULL;
	bool inServer = false;
	bool inLocation = false;
	bool hasRootLocation = false;

	while (std::getline(file, line)) {
		line = this->trim(line);
		if (line.empty() || line[0] == '#') continue;

		if (line == "server {") {
			if (inServer) {
				throw std::runtime_error("Configuration invalide: server imbriqué");
			}
			inServer = true;
			_servers.push_back(ServerConfig());
			hasRootLocation = false;
			continue;
		}

		if (line == "}") {
			if (inLocation) {
				inLocation = false;
				currentRoute = NULL;
			} else if (inServer) {

				if (!hasRootLocation) {
					_servers.back().routes.push_back(Route("/"));
					Route* defaultRoute = &_servers.back().routes.back();
					defaultRoute->addMethod("GET");
					defaultRoute->addMethod("POST");
					defaultRoute->addMethod("HEAD");
					if (!_servers.back().root.empty()) {
						defaultRoute->setRoot(_servers.back().root);
					}
				}
				inServer = false;
			}
			continue;
		}

		if (line.substr(0, 9) == "location ") {
			if (!inServer) {
				throw std::runtime_error("Configuration invalide: location en dehors du server");
			}
			inLocation = true;
			std::string path = this->extractLocationPath(line);
			if (path == "/") {
				hasRootLocation = true;
			}
			_servers.back().routes.push_back(Route(path));
			currentRoute = &_servers.back().routes.back();
			continue;
		}

		if (inLocation && currentRoute) {
			this->parseLocationDirective(line, currentRoute);
		} else if (inServer) {
			this->parseServerDirective(line);
		}
	}

	file.close();
}

void ConfigParser::parseServerDirective(const std::string& line) {
	size_t pos = line.find_first_of(" \t");
	if (pos == std::string::npos) return;

	std::string directive = this->trim(line.substr(0, pos));
	std::string value = this->trim(line.substr(pos + 1));

	if (directive == "listen") {
		_servers.back().port = std::atoi(this->trim(value, " \t;").c_str());
	}
	else if (directive == "server_name") {
		std::istringstream iss(value);
		std::string name;
		while (iss >> name) {
			name = this->trim(name, " \t;");
			if (!name.empty()) {
				_servers.back().server_names.push_back(name);
			}
		}
	}
	else if (directive == "client_max_body_size") {
		std::string size = value.substr(0, value.length() - 1);
		_servers.back().client_max_body_size = std::atoi(size.c_str()) * 1024 * 1024;
	}
	else if (directive == "root") {
		_servers.back().root = this->trim(value, " \t;");
	}
	else if (directive == "error_page") {
		std::istringstream iss(value);
		std::string code;
		std::string path;
		iss >> code;
		std::getline(iss, path);
		code = this->trim(code, " \t;");
		path = this->trim(path, " \t;");
		if (!code.empty() && !path.empty()) {
			_servers.back().error_pages[std::atoi(code.c_str())] = path;
		}
	}
}

void ConfigParser::parseLocationDirective(const std::string& line, Route* route) {
	if (!route) return;

	size_t pos = line.find_first_of(" \t");
	if (pos == std::string::npos) return;

	std::string directive = this->trim(line.substr(0, pos));
	std::string value = this->trim(line.substr(pos + 1));

	if (directive == "allow_methods") {
		std::istringstream iss(value);
		std::string method;
		while (iss >> method) {
			method = this->trim(method, " \t;");
			if (!method.empty()) {
				route->addMethod(method);
			}
		}
	}
	else if (directive == "root") {
		std::string root = this->trim(value, " \t;");
		route->setRoot(root);
	}
	else if (directive == "autoindex") {
		std::string autoindex = this->trim(value, " \t;");
		route->setDirectoryListing(autoindex == "on");
	}
	else if (directive == "upload_store") {
		std::string dir = this->trim(value, " \t;");
		route->setUploadDir(dir);
	}
	else if (directive == "cgi_pass") {
		std::string ext = value;
		std::string handler;
		size_t pos = ext.find(' ');
		if (pos != std::string::npos) {
			handler = ext.substr(pos + 1);
			ext = ext.substr(0, pos);
		}
		ext = this->trim(ext, " \t;");
		handler = this->trim(handler, " \t;");
		route->addCgiExtension(ext, handler);
	}
	else if (directive == "return") {
		std::string redirect = this->trim(value, " \t;");
		route->setRedirect(redirect);
	}
	else if (directive == "try_files") {
		std::string tryFiles = this->trim(value, " \t;");
		if (tryFiles.find("=404") != std::string::npos) {
			route->setTryFiles(true);
		}
	}
	else if (directive == "index") {
		std::string indexFile = this->trim(value, " \t;");
		route->setIndexFile(indexFile);
	}
}

std::string ConfigParser::extractLocationPath(const std::string& line) {
	size_t start = line.find_first_of(" \t");
	if (start == std::string::npos) {
		throw std::runtime_error("Format de location invalide");
	}
	size_t end = line.find_first_of(" \t{", start + 1);
	if (end == std::string::npos) {
		end = line.length();
	}
	return trim(line.substr(start, end - start));
}

std::string ConfigParser::trim(const std::string& str, const std::string& chars) const {
	size_t first = str.find_first_not_of(chars);
	if (first == std::string::npos)
		return "";
	size_t last = str.find_last_not_of(chars);
	return str.substr(first, (last - first + 1));
}

const Route* ConfigParser::findRoute(const ServerConfig* serverConfig, const std::string& path) const {
	if (!serverConfig) {
		std::cout << getCurrentTimestamp() << " [ERROR] ServerConfig est NULL" << std::endl;
		return NULL;
	}


	std::string normalizedPath = path;
	if (normalizedPath.empty()) {
		normalizedPath = "/";
	}
	if (normalizedPath[0] != '/') {
		normalizedPath = "/" + normalizedPath;
	}

	const Route* bestMatch = NULL;
	size_t bestMatchLength = 0;


	const std::vector<Route>& routes = serverConfig->routes;
	for (std::vector<Route>::const_iterator it = routes.begin(); it != routes.end(); ++it) {
		const std::string& routePath = it->getPath();


		if (routePath == "/" && normalizedPath == "/") {
			return &(*it);
		}


		if (!it->getRedirect().empty()) {
			if (normalizedPath == routePath) {
				return &(*it);
			}
			continue;
		}


		if (normalizedPath.find(routePath) == 0) {
			if (routePath.length() > bestMatchLength) {
				bestMatch = &(*it);
				bestMatchLength = routePath.length();
			}
		}
	}
	return bestMatch;
}


std::vector<int> ConfigParser::getPorts() const { return _ports; }
std::string ConfigParser::getHost() const { return _host; }
size_t ConfigParser::getClientMaxBodySize() const { return _clientMaxBodySize; }
bool ConfigParser::getAutoindex() const { return _autoindex; }
std::string ConfigParser::getRoot() const { return _root; }
std::string ConfigParser::getErrorPage(int code) const {
	std::map<int, std::string>::const_iterator it = _errorPages.find(code);
	return (it != _errorPages.end()) ? it->second : "";
}
