/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.hpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 12:29:38 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:23:31 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <vector>
#include <map>
#include <stdint.h>
#include "ConfigParser.hpp"
#include "Request.hpp"
#include <sys/stat.h>

class Server;
class Connection;

class Response {
private:
	int _clientSocket;
	Connection* _connection;
	std::string _filePath;
	ConfigParser& _config;
	const Request& _request;
	std::vector<uint8_t> _fileContent;
	std::map<std::string, std::string> _statusMessages;
	std::map<std::string, std::string> _additionalHeaders;
	std::string _currentPath;

	class FileSorter {
		Response& _response;
	public:
		FileSorter(Response& response) : _response(response) {}
		bool operator()(const std::string& a, const std::string& b) const {
			if (a == "..") return true;
			if (b == "..") return false;
			struct stat statA, statB;
			stat((_response._currentPath + "/" + a).c_str(), &statA);
			stat((_response._currentPath + "/" + b).c_str(), &statB);
			if (S_ISDIR(statA.st_mode) != S_ISDIR(statB.st_mode))
				return S_ISDIR(statA.st_mode);
			return a < b;
		}
	};

	std::string trim(const std::string& str);
	void processCgiResponse(const std::string& response);

public:
	Response(Connection* conn, const std::string &filePath, ConfigParser& config, const Request& request);
	~Response();

	void handleGetRequest();
	void handlePostRequest(const std::string &body);
	void handleDeleteRequest();
	void handleFileUpload();
	void handleFormData(const std::string &body);
	void handleRequest();
	bool isValidRequest();
	std::string determineContentType(const std::string &filePath);
	std::vector<uint8_t> readFile(const std::string &filePath);
	void sendResponse(int statusCode, const std::string &contentType);
	std::string getStatusMessage(int statusCode) const;
	void handleDirectoryListing(const std::string& path);
	void handleCgiRequest();
	void handleCGIRequest(const std::string& scriptPath);
	void sendRedirect(const std::string& location);
	std::string getExtension(const std::string& path);
	void handleError(int errorCode);
	void addHeader(const std::string& key, const std::string& value) {
		_additionalHeaders[key] = value;
	}
	void logResponse() const;
};

#endif
