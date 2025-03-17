/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigParser.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 13:03:16 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:22:51 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGPARSER_HPP
#define CONFIGPARSER_HPP

#include <string>
#include <vector>
#include <map>
#include "Route.hpp"

struct ServerConfig {
	std::string host;
	int port;
	std::vector<std::string> server_names;
	size_t client_max_body_size;
	std::string root;
	std::map<int, std::string> error_pages;
	std::vector<Route> routes;
	bool is_default;

	ServerConfig() : port(80), client_max_body_size(1024 * 1024), is_default(false) {}


	const Route* getRoute(const std::string& path) const {
		for (std::vector<Route>::const_iterator it = routes.begin(); it != routes.end(); ++it) {
			if (it->getPath() == path) {
				return &(*it);
			}
		}
		return NULL;
	}


	const std::vector<Route>& getRoutes() const {
		return routes;
	}
};

class ConfigParser {
private:
	std::string _root;
	std::map<std::string, std::string> _serverDirectives;
	size_t _clientMaxBodySize;
	std::vector<ServerConfig> _servers;
	std::vector<int> _ports;
	std::string _host;
	bool _autoindex;
	std::map<int, std::string> _errorPages;

	void parseServerDirective(const std::string& line);
	void parseLocationDirective(const std::string& line, Route* route);
	std::string extractLocationPath(const std::string& line);
	std::string trim(const std::string& str, const std::string& chars = " \t\n\r") const;

public:
	ConfigParser();
	~ConfigParser();

	void parse(const std::string& filename);
	const Route* findRoute(const ServerConfig* server, const std::string& path) const;
	size_t getClientMaxBodySize() const;
	std::vector<int> getPorts() const;
	std::string getHost() const;
	bool getAutoindex() const;
	std::string getRoot() const;
	std::string getErrorPage(int code) const;
	const std::vector<ServerConfig>& getServers() const { return _servers; }
};

#endif
