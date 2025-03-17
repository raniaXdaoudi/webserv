/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Response.cpp                                       :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 12:29:50 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:33:04 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Response.hpp"
#include "../include/Server.hpp"
#include "../include/Utils.hpp"
#include "../include/Logger.hpp"
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <dirent.h>
#include <fcntl.h>
#include <signal.h>
#include <poll.h>

extern char **environ;

Response::Response(Connection* conn, const std::string &filePath, ConfigParser& config, const Request& request)
	: _clientSocket(conn->getSocket())
	, _connection(conn)
	, _filePath(filePath)
	, _config(config)
	, _request(request) {
	_fileContent.clear();
}

Response::~Response() {}

/**
 * Gère les requêtes GET en vérifiant si le fichier demandé existe.
 * Renvoie un fichier d'erreur 404 si la ressource n'existe pas.
 */
void Response::handleGetRequest() {
	std::string routePath = _request.getUrl();

	const ServerConfig* serverConfig = Server::getInstance()->findMatchingServer(_clientSocket, _request.getHeader("Host"));
	const Route* route = _config.findRoute(serverConfig, routePath);

	if (!route) {
		std::cout << getCurrentTimestamp() << " [ERROR] Pas de route trouvée pour: " << routePath << std::endl;
		handleError(404);
		return;
	}



	if (!route->isMethodAllowed("GET")) {
		std::cout << getCurrentTimestamp() << " [ERROR] Méthode GET non autorisée pour: " << routePath << std::endl;
		handleError(405);
		return;
	}


	if (!route->getRedirect().empty()) {
		if (!route->isMethodAllowed("GET")) {
			handleError(405);
			return;
		}
		std::string redirectUrl = route->getRedirect();
		size_t spacePos = redirectUrl.find(" ");
		if (spacePos != std::string::npos) {
			std::string location = redirectUrl.substr(spacePos + 1);
			sendRedirect(location);
			return;
		}
	}


	std::string fullPath = "www" + routePath;

	struct stat buffer;
	if (stat(fullPath.c_str(), &buffer) != 0) {
		std::cout << "\033[31m[ERROR] Fichier non trouvé: " << fullPath << "\033[0m" << std::endl;
		handleError(404);
		return;
	}


	std::string extension = getExtension(routePath);
	if (route->isCgiExtension(extension)) {
		if ((buffer.st_mode & S_IXUSR) == 0) {
			std::cout << "\033[31m[ERROR] Script CGI non exécutable: " << fullPath << "\033[0m" << std::endl;
			handleError(500);
			return;
		}
		_filePath = fullPath;
		handleCgiRequest();
		return;
	}


	if (S_ISDIR(buffer.st_mode)) {


		if (routePath[routePath.length() - 1] != '/') {
			_additionalHeaders["Location"] = routePath + "/";
			sendResponse(301, "text/html");
			return;
		}


		std::string indexPath = fullPath + "/" + route->getIndexFile();

		if (access(indexPath.c_str(), F_OK) != -1) {
			_fileContent = readFile(indexPath);
			if (!_fileContent.empty()) {
				sendResponse(200, "text/html");
				return;
			}
		}


		if (route->isDirectoryListingEnabled()) {
			handleDirectoryListing(fullPath);
			return;
		}

		std::cout << "\033[31m[ERROR] Accès au répertoire interdit (pas d'index.html ni d'autoindex)\033[0m" << std::endl;
		handleError(403);
		return;
	}


	_fileContent = readFile(fullPath);
	if (_fileContent.empty()) {
		std::cout << "\033[31m[ERROR] Impossible de lire le fichier: " << fullPath << "\033[0m" << std::endl;
		handleError(404);
		return;
	}

	sendResponse(200, determineContentType(fullPath));
}

/**
 * Gère les requêtes POST et écrit les données reçues dans un fichier.
 */
void Response::handlePostRequest(const std::string &body) {
	const ServerConfig* serverConfig = Server::getInstance()->findMatchingServer(_clientSocket, _request.getHeader("Host"));
	const Route* route = _config.findRoute(serverConfig, _request.getUrl());

	if (!route) {
		handleError(404);
		return;
	}


	std::string extension = getExtension(_request.getUrl());
	if (route->isCgiExtension(extension)) {
		handleCgiRequest();
		return;
	}


	std::string uploadDir = route->getUploadDir();
	if (uploadDir.empty()) {
		handleError(500);
		return;
	}


	if (uploadDir[0] != '/') {
		uploadDir = "www/" + uploadDir;
	}


	if (mkdir(uploadDir.c_str(), 0755) == -1 && errno != EEXIST) {
		handleError(500);
		return;
	}


	std::string filename = _request.getUrl();
	size_t lastSlash = filename.find_last_of('/');
	if (lastSlash != std::string::npos) {
		filename = filename.substr(lastSlash + 1);
	}


	std::string cleanFilename = filename;
	for (size_t i = 0; i < cleanFilename.length(); ++i) {
		if (!isalnum(cleanFilename[i]) && cleanFilename[i] != '.' && cleanFilename[i] != '_' && cleanFilename[i] != '-') {
			cleanFilename[i] = '_';
		}
	}


	std::string baseFilename = cleanFilename;
	std::string fileExtension = "";
	size_t dotPos = baseFilename.find_last_of('.');
	if (dotPos != std::string::npos) {
		fileExtension = baseFilename.substr(dotPos);
		baseFilename = baseFilename.substr(0, dotPos);
	}

	std::string uniqueFilename = cleanFilename;
	int counter = 1;
	struct stat st;
	while (true) {
		std::string fullPath = uploadDir + "/" + uniqueFilename;
		if (stat(fullPath.c_str(), &st) != 0) {
			break;
		}
		uniqueFilename = baseFilename + "_" + std::to_string(counter) + fileExtension;
		counter++;
	}


	std::string filePath = uploadDir + "/" + uniqueFilename;
	int fd = open(filePath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		handleError(500);
		return;
	}

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLOUT;

	size_t totalWritten = 0;
	while (totalWritten < body.length()) {
		int ret = poll(&pfd, 1, 30000);
		if (ret <= 0) {
			close(fd);
			handleError(500);
			return;
		}

		if (pfd.revents & POLLOUT) {
			ssize_t bytesWritten = write(fd, body.c_str() + totalWritten,
									body.length() - totalWritten);
			if (bytesWritten <= 0) {
				close(fd);
				handleError(500);
				return;
			}
			totalWritten += bytesWritten;
		}
	}

	close(fd);
	_additionalHeaders["Location"] = "/upload/static/" + uniqueFilename;
	sendResponse(201, "text/plain");
}

void Response::handleFileUpload() {
	const ServerConfig* serverConfig = Server::getInstance()->findMatchingServer(_clientSocket, _request.getHeader("Host"));
	const Route* route = _config.findRoute(serverConfig, _request.getUrl());
	if (!route) {
		std::cerr << "\033[31m[ERROR] Route non trouvée\033[0m" << std::endl;
		handleError(404);
		return;
	}


	std::string uploadDir = route->getUploadDir();
	if (uploadDir.empty()) {
		std::cerr << "\033[31m[ERROR] Répertoire d'upload non configuré\033[0m" << std::endl;
		handleError(500);
		return;
	}


	if (uploadDir[0] != '/') {
		uploadDir = "www/" + uploadDir;
	}


	struct stat st;
	if (stat(uploadDir.c_str(), &st) != 0) {
		if (mkdir(uploadDir.c_str(), 0755) != 0) {
			std::cerr << "\033[31m[ERROR] Impossible de créer le répertoire d'upload\033[0m" << std::endl;
			handleError(500);
			return;
		}
	}


	const std::vector<std::string>& uploadedFiles = _request.getUploadedFiles();
	if (uploadedFiles.empty()) {
		std::cerr << "\033[31m[ERROR] Aucun fichier à uploader\033[0m" << std::endl;
		handleError(400);
		return;
	}


	std::string filename = _request.getHeader("X-Original-Filename");
	if (filename.empty()) {
		filename = "uploaded_file";
	}


	std::string cleanFilename = filename;
	for (size_t i = 0; i < cleanFilename.length(); ++i) {
		if (!isalnum(cleanFilename[i]) && cleanFilename[i] != '.' && cleanFilename[i] != '_' && cleanFilename[i] != '-') {
			cleanFilename[i] = '_';
		}
	}


	std::string baseFilename = cleanFilename;
	std::string extension = "";
	size_t dotPos = baseFilename.find_last_of('.');
	if (dotPos != std::string::npos) {
		extension = baseFilename.substr(dotPos);
		baseFilename = baseFilename.substr(0, dotPos);
	}

	std::string uniqueFilename = cleanFilename;
	int counter = 1;
	while (true) {
		std::string fullPath = uploadDir + "/" + uniqueFilename;
		if (stat(fullPath.c_str(), &st) != 0) {
			break;
		}
		uniqueFilename = baseFilename + "_" + std::to_string(counter) + extension;
		counter++;
	}


	std::string fullPath = uploadDir + "/" + uniqueFilename;

	int fd = open(fullPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
	if (fd < 0) {
		std::cerr << "\033[31m[ERROR] Impossible d'ouvrir le fichier pour écriture\033[0m" << std::endl;
		handleError(500);
		return;
	}


	const std::string& content = uploadedFiles[0];

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLOUT;

	size_t totalWritten = 0;
	while (totalWritten < content.length()) {
		int ret = poll(&pfd, 1, 30000);
		if (ret <= 0) {
			close(fd);
			handleError(500);
			return;
		}

		if (pfd.revents & POLLOUT) {
			ssize_t bytesWritten = write(fd, content.c_str() + totalWritten,
									content.length() - totalWritten);
			if (bytesWritten <= 0) {
				close(fd);
				handleError(500);
				return;
			}
			totalWritten += bytesWritten;
		}
	}

	close(fd);
	_additionalHeaders["Location"] = "/upload/cgi/" + uniqueFilename;
	sendResponse(201, "text/plain");
}

void Response::handleFormData(const std::string &body) {
	const char* header = "<!DOCTYPE html>\n<html><head><style>\n"
					  "body { font-family: Arial, sans-serif; margin: 40px; }\n"
					  "h1 { color: #2196F3; }\n"
					  ".data { background: #f5f5f5; padding: 20px; border-radius: 5px; }\n"
					  "</style></head><body>\n"
					  "<h1>Form Data Received</h1>\n"
					  "<div class='data'>\n<pre>";

	const char* footer = "</pre>\n</div></body></html>";

	size_t headerLen = strlen(header);
	size_t bodyLen = body.length();
	size_t footerLen = strlen(footer);
	size_t totalLen = headerLen + bodyLen + footerLen;

	std::vector<char> tempBuffer(totalLen);

	if (headerLen > 0) {
		memcpy(&tempBuffer[0], header, headerLen);
	}
	if (bodyLen > 0) {
		memcpy(&tempBuffer[headerLen], body.c_str(), bodyLen);
	}
	if (footerLen > 0) {
		memcpy(&tempBuffer[headerLen + bodyLen], footer, footerLen);
	}

	_fileContent.clear();
	_fileContent.resize(totalLen);
	for (size_t i = 0; i < totalLen; ++i) {
		_fileContent[i] = static_cast<uint8_t>(tempBuffer[i]);
	}

	sendResponse(200, "text/html");
}

/**
 * Gère les requêtes DELETE pour supprimer un fichier s'il existe.
 */
void Response::handleDeleteRequest() {
	std::string fullPath = "www" + _request.getUrl();
	struct stat buffer;

	if (stat(fullPath.c_str(), &buffer) != 0) {
		handleError(404);
		return;
	}

	if (S_ISDIR(buffer.st_mode)) {
		handleError(403);
		return;
	}

	int fd = open(fullPath.c_str(), O_WRONLY | O_NONBLOCK);
	if (fd < 0) {
		handleError(500);
		return;
	}

	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLOUT;

	int ret = poll(&pfd, 1, 30000);
	if (ret <= 0) {
		close(fd);
		handleError(500);
		return;
	}

	close(fd);

	if (unlink(fullPath.c_str()) != 0) {
		handleError(500);
		return;
	}

	std::string response = "HTTP/1.1 204 No Content\r\n"
						  "Server: webserv/1.0\r\n"
						  "Connection: close\r\n\r\n";

	_connection->clearWriteBuffer();
	_connection->appendToWriteBuffer(response);
}

/**
 * Vérifie si le fichier demandé existe.
 */
bool Response::isValidRequest() {
	if (_filePath.find("..") != std::string::npos ||
		_filePath.find("//") != std::string::npos) {
		throw std::runtime_error("403 Forbidden");
	}

	std::string fullPath = _filePath;
	if (fullPath.substr(0, 4) != "www/") {
		fullPath = "www" + fullPath;
	}

	struct stat buffer;
	if (stat(fullPath.c_str(), &buffer) != 0) {
		return false;
	}

	return (S_ISREG(buffer.st_mode) || S_ISDIR(buffer.st_mode));
}

/**
 * Détermine le type MIME en fonction de l'extension du fichier.
 */
std::string Response::determineContentType(const std::string &filePath) {
	std::string type;
	if (endsWith(filePath, ".html")) type = "text/html";
	else if (endsWith(filePath, ".txt")) type = "text/plain";
	else if (endsWith(filePath, ".css")) type = "text/css";
	else if (endsWith(filePath, ".csv")) type = "text/csv";
	else if (endsWith(filePath, ".js")) type = "application/javascript";
	else if (endsWith(filePath, ".json")) type = "application/json";
	else if (endsWith(filePath, ".pdf")) type = "application/pdf";
	else if (endsWith(filePath, ".xml")) type = "application/xml";
	else if (endsWith(filePath, ".png")) type = "image/png";
	else if (endsWith(filePath, ".jpg") || endsWith(filePath, ".jpeg")) type = "image/jpeg";
	else if (endsWith(filePath, ".gif")) type = "image/gif";
	else if (endsWith(filePath, ".svg")) type = "image/svg+xml";
	else if (endsWith(filePath, ".ico")) type = "image/x-icon";
	else if (endsWith(filePath, ".mp3")) type = "audio/mpeg";
	else if (endsWith(filePath, ".wav")) type = "audio/wav";
	else if (endsWith(filePath, ".mp4")) type = "video/mp4";
	else if (endsWith(filePath, ".webm")) type = "video/webm";
	else type = "application/octet-stream";
	return type;
}

/**
 * Lit un fichier et retourne son contenu sous forme de `std::vector<uint8_t>`.
 */
std::vector<uint8_t> Response::readFile(const std::string &filePath) {
	std::string fullPath = filePath;
	if (fullPath.substr(0, 4) != "www/") {
		fullPath = "www" + filePath;
	}

	int fd = open(fullPath.c_str(), O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		return std::vector<uint8_t>();
	}

	std::vector<uint8_t> fileContent;
	char buffer[4096];
	struct pollfd pfd;
	pfd.fd = fd;
	pfd.events = POLLIN;

	while (true) {
		int ret = poll(&pfd, 1, 30000);
		if (ret <= 0) {
			close(fd);
			return fileContent;
		}

		if (pfd.revents & POLLIN) {
			ssize_t bytesRead = read(fd, buffer, sizeof(buffer));
			if (bytesRead < 0) {
				close(fd);
				return std::vector<uint8_t>();
			}
			if (bytesRead == 0) {
				break;
			}
			fileContent.insert(fileContent.end(), buffer, buffer + bytesRead);
		}
	}

	close(fd);
	return fileContent;
}

/**
 * Envoie la réponse HTTP au client.
 */
void Response::sendResponse(int statusCode, const std::string &contentType) {
	time_t now = time(0);
	char dateBuffer[100];
	strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S GMT", gmtime(&now));

	std::string finalContentType = contentType;
	if (contentType == "text/html") {
		finalContentType += "; charset=UTF-8";
	}

	std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + getStatusMessage(statusCode) + "\r\n"
						  "Server: webserv/1.0\r\n"
						  "Date: " + std::string(dateBuffer) + "\r\n"
						  "Content-Type: " + finalContentType + "\r\n"
						  "Content-Length: " + std::to_string(_fileContent.size()) + "\r\n";

	for (std::map<std::string, std::string>::const_iterator it = _additionalHeaders.begin();
		 it != _additionalHeaders.end(); ++it) {
		response += it->first + ": " + it->second + "\r\n";
	}

	response += "Connection: keep-alive\r\n\r\n";

	std::string fullResponse = response;
	if (!_fileContent.empty()) {
		fullResponse.append(_fileContent.begin(), _fileContent.end());
	}

	_connection->clearWriteBuffer();
	_connection->appendToWriteBuffer(fullResponse);


	std::string timestamp = getCurrentTimestamp();


	if (statusCode == 200) {
		std::cout << timestamp << " [RESPONSE] 127.0.0.1 - 200 OK - Served: "
				  << _filePath << " (" << _fileContent.size() << " bytes)" << std::endl;
	} else if (statusCode == 404) {
		std::cout << timestamp << " [ERROR] 127.0.0.1 - 404 Not Found - Requested: "
				  << (_filePath.empty() ? "www" + _request.getUrl() : _filePath) << std::endl;
	} else if (statusCode == 400) {
		std::cout << timestamp << " [WARNING] 127.0.0.1 - 400 Bad Request - Invalid HTTP format" << std::endl;
	} else {
		std::cout << timestamp << " [RESPONSE] 127.0.0.1 - " << statusCode << " "
				  << getStatusMessage(statusCode) << std::endl;
	}
}

/**
 * Retourne le message correspondant à un code HTTP.
 */
std::string Response::getStatusMessage(int statusCode) const {
	switch (statusCode) {
		case 200: return "OK";
		case 201: return "Created";
		case 204: return "No Content";
		case 301: return "Moved Permanently";
		case 303: return "See Other";
		case 400: return "Bad Request";
		case 403: return "Forbidden";
		case 404: return "Not Found";
		case 405: return "Method Not Allowed";
		case 413: return "Request Entity Too Large";
		case 500: return "Internal Server Error";
		case 504: return "Gateway Timeout";
		default: return "Unknown Status";
	}
}

void Response::handleDirectoryListing(const std::string& path) {
	struct stat st;
	if (stat(path.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
		handleError(404);
		return;
	}

	DIR* dir = opendir(path.c_str());
	if (!dir) {
		handleError(404);
		return;
	}

	std::stringstream html;
	html << "<!DOCTYPE html>\n"
		 << "<html><head>\n"
		 << "<title>Index of " << _filePath << "</title>\n"
		 << "<style>\n"
		 << "body { font-family: Arial, sans-serif; margin: 40px; }\n"
		 << "h1 { color: #2196F3; }\n"
		 << "table { width: 100%; border-collapse: collapse; }\n"
		 << "th, td { text-align: left; padding: 8px; border-bottom: 1px solid #ddd; }\n"
		 << "th { background-color: #f5f5f5; }\n"
		 << "a { color: #1976D2; text-decoration: none; }\n"
		 << "a:hover { text-decoration: underline; }\n"
		 << "</style></head><body>\n"
		 << "<h1>Index of " << _filePath << "</h1>\n"
		 << "<table>\n"
		 << "<tr><th>Name</th><th>Size</th><th>Last Modified</th></tr>\n";

	std::vector<std::string> files;
	struct dirent* entry;
	while ((entry = readdir(dir))) {
		if (std::string(entry->d_name) != ".") {
			files.push_back(entry->d_name);
		}
	}
	closedir(dir);

	_currentPath = path;
	std::sort(files.begin(), files.end(), FileSorter(*this));

	for (std::vector<std::string>::const_iterator it = files.begin(); it != files.end(); ++it) {
		std::string fullPath = path + "/" + *it;
		struct stat fileStat;
		stat(fullPath.c_str(), &fileStat);

		std::string size;
		if (S_ISDIR(fileStat.st_mode)) {
			size = "-";
		} else {
			std::stringstream ss;
			if (fileStat.st_size < 1024)
				ss << fileStat.st_size << " B";
			else if (fileStat.st_size < 1024 * 1024)
				ss << fileStat.st_size / 1024 << " KB";
			else
				ss << fileStat.st_size / (1024 * 1024) << " MB";
			size = ss.str();
		}

		char timeStr[100];
		strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", localtime(&fileStat.st_mtime));

		html << "<tr>"
			 << "<td><a href=\"" << *it << "\">"
			 << (S_ISDIR(fileStat.st_mode) ? "/" : "") << *it
			 << "</a></td>"
			 << "<td>" << size << "</td>"
			 << "<td>" << timeStr << "</td>"
			 << "</tr>\n";
	}

	html << "</table>\n"
		 << "<div class='back'>\n"
		 << "<a href='/'>Back to Home</a>\n"
		 << "</div>\n"
		 << "</body></html>\n";

	std::string response = html.str();
	_fileContent.clear();
	_fileContent.insert(_fileContent.begin(), response.begin(), response.end());
	sendResponse(200, "text/html");
}

void Response::sendRedirect(const std::string& location) {
	std::stringstream html;
	html << "<!DOCTYPE html>\n<html><head><title>Redirection</title></head>"
		 << "<body>Redirecting to <a href='" << location << "'>" << location << "</a></body></html>";
	std::string content = html.str();
	_fileContent.clear();
	_fileContent.insert(_fileContent.begin(), content.begin(), content.end());
	_additionalHeaders["Location"] = location;
	sendResponse(301, "text/html");
}

std::string Response::getExtension(const std::string& path) {
	size_t dot = path.find_last_of('.');
	if (dot != std::string::npos) {
		return path.substr(dot);
	}
	return "";
}

void Response::handleCgiRequest() {
	int pipe_in[2];
	int pipe_out[2];

	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
		handleError(500);
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		close(pipe_in[0]); close(pipe_in[1]);
		close(pipe_out[0]); close(pipe_out[1]);
		handleError(500);
		return;
	}

	if (pid == 0) {
		close(pipe_in[1]);
		close(pipe_out[0]);

		dup2(pipe_in[0], STDIN_FILENO);
		dup2(pipe_out[1], STDOUT_FILENO);

		close(pipe_in[0]);
		close(pipe_out[1]);


		setenv("REQUEST_METHOD", _request.getMethod().c_str(), 1);
		setenv("QUERY_STRING", _request.getQueryString().c_str(), 1);
		setenv("CONTENT_LENGTH", _request.getHeader("Content-Length").c_str(), 1);
		setenv("CONTENT_TYPE", _request.getHeader("Content-Type").c_str(), 1);
		setenv("PATH_INFO", _request.getUrl().c_str(), 1);
		setenv("SCRIPT_FILENAME", _filePath.c_str(), 1);
		setenv("SCRIPT_NAME", _request.getUrl().c_str(), 1);
		setenv("DOCUMENT_ROOT", "www", 1);
		setenv("SERVER_NAME", "webserv", 1);
		setenv("SERVER_PORT", "8080", 1);
		setenv("SERVER_PROTOCOL", "HTTP/1.1", 1);
		setenv("SERVER_SOFTWARE", "webserv/1.0", 1);
		setenv("GATEWAY_INTERFACE", "CGI/1.1", 1);
		setenv("REMOTE_ADDR", "127.0.0.1", 1);
		setenv("REMOTE_HOST", "localhost", 1);
		setenv("REMOTE_PORT", "0", 1);
		setenv("REQUEST_URI", _request.getUrl().c_str(), 1);
		setenv("HTTP_HOST", _request.getHeader("Host").c_str(), 1);
		setenv("HTTP_USER_AGENT", _request.getHeader("User-Agent").c_str(), 1);
		setenv("HTTP_ACCEPT", _request.getHeader("Accept").c_str(), 1);
		setenv("WEBSERV_LOG_PREFIX", "1", 1);


		char* const args[] = {(char*)_filePath.c_str(), NULL};
		execve(_filePath.c_str(), args, environ);
		exit(1);
	}


	close(pipe_in[0]);
	close(pipe_out[1]);


	struct pollfd write_pfd;
	write_pfd.fd = pipe_in[1];
	write_pfd.events = POLLOUT;


	if (_request.getMethod() == "POST") {

		std::string contentType = _request.getHeader("Content-Type");
		bool isMultipart = contentType.find("multipart/form-data") != std::string::npos;

		if (isMultipart && _request.isMultipart() && !_request.getUploadedFiles().empty()) {

			const std::vector<std::string>& uploadedFiles = _request.getUploadedFiles();
			std::string filename = _request.getHeader("X-Original-Filename");


			std::string boundary = "----WebServBoundary" + std::to_string(time(NULL));
			std::string newBody = "--" + boundary + "\r\n";
			newBody += "Content-Disposition: form-data; name=\"file\"; filename=\"" + filename + "\"\r\n";
			newBody += "Content-Type: application/octet-stream\r\n\r\n";
			newBody += uploadedFiles[0];
			newBody += "\r\n--" + boundary + "--\r\n";


			setenv("CONTENT_TYPE", ("multipart/form-data; boundary=" + boundary).c_str(), 1);
			setenv("CONTENT_LENGTH", std::to_string(newBody.length()).c_str(), 1);


			size_t totalWritten = 0;
			while (totalWritten < newBody.length()) {
				int ret = poll(&write_pfd, 1, 30000);
				if (ret <= 0) {
					close(pipe_in[1]);
					close(pipe_out[0]);
					handleError(500);
					return;
				}

				if (write_pfd.revents & POLLOUT) {
					ssize_t writeResult = write(pipe_in[1],
						newBody.c_str() + totalWritten,
						newBody.length() - totalWritten);

					if (writeResult <= 0) {
						close(pipe_in[1]);
						close(pipe_out[0]);
						handleError(500);
						return;
					}
					totalWritten += writeResult;
				}
			}
		} else {

			size_t totalWritten = 0;
			const std::string& body = _request.getBody();

			while (totalWritten < body.length()) {
				int ret = poll(&write_pfd, 1, 30000);
				if (ret <= 0) {
					close(pipe_in[1]);
					close(pipe_out[0]);
					handleError(500);
					return;
				}

				if (write_pfd.revents & POLLOUT) {
					ssize_t writeResult = write(pipe_in[1],
						body.c_str() + totalWritten,
						body.length() - totalWritten);

					if (writeResult <= 0) {
						close(pipe_in[1]);
						close(pipe_out[0]);
						handleError(500);
						return;
					}
					totalWritten += writeResult;
				}
			}
		}
	}
	close(pipe_in[1]);


	struct pollfd read_pfd;
	read_pfd.fd = pipe_out[0];
	read_pfd.events = POLLIN;


	std::string response;
	char buffer[4096];

	while (true) {
		int ret = poll(&read_pfd, 1, 30000);
		if (ret <= 0) {
			close(pipe_out[0]);
			handleError(500);
			return;
		}

		if (read_pfd.revents & POLLIN) {
			ssize_t bytesRead = read(pipe_out[0], buffer, sizeof(buffer));
			if (bytesRead < 0) {
				close(pipe_out[0]);
				handleError(500);
				return;
			}
			if (bytesRead == 0) {
				break;
			}
			response.append(buffer, bytesRead);
		}
	}
	close(pipe_out[0]);


	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {

		std::string cgiOutput = response;
		std::istringstream outputStream(cgiOutput);
		std::string line;


		bool isHeader = true;
		std::string cgiResponse;

		while (std::getline(outputStream, line)) {

			if (isHeader && (line.empty() || line == "\r")) {
				isHeader = false;
				cgiResponse += line + "\n";
				continue;
			}


			if (isHeader && line.find(":") == std::string::npos) {

				std::cout << getCurrentTimestamp() << " [CGI-OUTPUT] " << line << std::endl;
			} else {

				cgiResponse += line + "\n";
			}
		}


		processCgiResponse(cgiResponse);
	} else {
		handleError(500);
	}
}

void Response::handleError(int errorCode) {
	std::cout << getCurrentTimestamp() << " [ERROR] Code " << errorCode
			  << " - " << getStatusMessage(errorCode)
			  << " pour " << (_request.getUrl().empty() ? "requête inconnue" : _request.getUrl())
			  << std::endl;


	_filePath = "www" + _request.getUrl();

	std::string errorPage = "www/error/" + std::to_string(errorCode) + ".html";
	_fileContent = readFile(errorPage);

	if (_fileContent.empty()) {
		std::string defaultError = "<!DOCTYPE html><html><head><title>" +
			std::to_string(errorCode) + " Error</title>" +
			"<style>body{font-family:Arial,sans-serif;margin:40px;}" +
			"h1{color:#d32f2f;}</style></head><body>" +
			"<h1>" + std::to_string(errorCode) + " - " + getStatusMessage(errorCode) + "</h1>";

		if (errorCode == 405) {
			const ServerConfig* serverConfig = Server::getInstance()->findMatchingServer(_clientSocket, _request.getHeader("Host"));
			const Route* route = _config.findRoute(serverConfig, _request.getUrl());
			if (route) {
				std::string allowedMethods;
				const std::vector<std::string>& methods = route->getMethods();
				for (size_t i = 0; i < methods.size(); ++i) {
					if (i > 0) allowedMethods += ", ";
					allowedMethods += methods[i];
				}
				defaultError += "<p>Allowed methods: " + allowedMethods + "</p>";
				_additionalHeaders["Allow"] = allowedMethods;
			}
		}

		defaultError += "<p>" + getStatusMessage(errorCode) + "</p>";
		defaultError += "<a href='/'>Back to Home</a></body></html>";
		_fileContent.assign(defaultError.begin(), defaultError.end());
	}

	sendResponse(errorCode, "text/html");
}

void Response::handleRequest() {
	const ServerConfig* serverConfig = Server::getInstance()->findMatchingServer(_clientSocket, _request.getHeader("Host"));
	const Route* route = _config.findRoute(serverConfig, _request.getUrl());


	if (!route) {
		handleError(404);
		return;
	}


	if (!route->isMethodAllowed(_request.getMethod())) {
		std::string allowedMethods;
		const std::vector<std::string>& methods = route->getMethods();
		for (size_t i = 0; i < methods.size(); ++i) {
			if (i > 0) allowedMethods += ", ";
			allowedMethods += methods[i];
		}
		_additionalHeaders["Allow"] = allowedMethods;
		handleError(405);
		return;
	}


	if (_request.getMethod() == "POST") {
		std::string contentLengthStr = _request.getHeader("Content-Length");
		if (!contentLengthStr.empty()) {
			size_t contentLength = std::atoi(contentLengthStr.c_str());
			if (contentLength > _config.getClientMaxBodySize() * 1024 * 1024) {
				handleError(413);
				return;
			}
		}
	}


	std::string url = _request.getUrl();
	std::string extension = getExtension(url);
	bool isCgiRequest = (url.substr(0, 9) == "/cgi-bin/" || route->isCgiExtension(extension));

	if (isCgiRequest) {
		std::string fullPath = "www" + url;
		struct stat buffer;
		if (stat(fullPath.c_str(), &buffer) == 0) {
			if ((buffer.st_mode & S_IXUSR) == 0) {
				handleError(500);
				return;
			}
			_filePath = fullPath;
			handleCgiRequest();
			return;
		}
		handleError(404);
		return;
	}


	if (_request.getMethod() == "GET") {
		handleGetRequest();
	} else if (_request.getMethod() == "POST") {

		std::string contentType = _request.getHeader("Content-Type");
		if (contentType.find("multipart/form-data") != std::string::npos) {
			handleFileUpload();
		} else {
			handlePostRequest(_request.getBody());
		}
	} else if (_request.getMethod() == "DELETE") {
		handleDeleteRequest();
	}
}

std::string Response::trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t\n\r");
	if (first == std::string::npos)
		return "";
	size_t last = str.find_last_not_of(" \t\n\r");
	return str.substr(first, (last - first + 1));
}

void Response::handleCGIRequest(const std::string& scriptPath) {
	_filePath = scriptPath;
	handleCgiRequest();
}

void Response::processCgiResponse(const std::string& response) {

	int statusCode = 200;
	std::string contentType = "text/html";
	size_t headerEnd = response.find("\r\n\r\n");

	if (headerEnd != std::string::npos) {

		std::string headers = response.substr(0, headerEnd);
		std::string body = response.substr(headerEnd + 4);


		size_t statusPos = headers.find("Status:");
		if (statusPos != std::string::npos) {
			std::string statusLine = headers.substr(statusPos + 7);
			statusLine = statusLine.substr(0, statusLine.find("\r\n"));
			statusCode = std::atoi(statusLine.c_str());
		}


		size_t contentTypePos = headers.find("Content-Type:");
		if (contentTypePos != std::string::npos) {
			std::string contentTypeLine = headers.substr(contentTypePos + 13);
			contentTypeLine = contentTypeLine.substr(0, contentTypeLine.find("\r\n"));
			contentType = trim(contentTypeLine);
		}

		_fileContent.clear();
		_fileContent.insert(_fileContent.begin(), body.begin(), body.end());
	} else {

		_fileContent.clear();
		_fileContent.insert(_fileContent.begin(), response.begin(), response.end());
	}


	if (_request.getMethod() == "POST" && statusCode == 200) {
		statusCode = 201;
	}

	sendResponse(statusCode, contentType);
}

void Response::logResponse() const {
	std::string timestamp = getCurrentTimestamp();
	int statusCode = 200;

	if (statusCode == 200) {
		std::cout << timestamp << " [RESPONSE] 127.0.0.1 - 200 OK - Served: "
				  << _filePath << " (" << _fileContent.size() << " bytes)" << std::endl;
	} else if (statusCode == 404) {
		std::cout << timestamp << " [ERROR] 127.0.0.1 - 404 Not Found - Requested: "
				  << (_filePath.empty() ? "www" + _request.getUrl() : _filePath) << std::endl;
	} else if (statusCode == 400) {
		std::cout << timestamp << " [WARNING] 127.0.0.1 - 400 Bad Request - Invalid HTTP format" << std::endl;
	} else {
		std::cout << timestamp << " [RESPONSE] 127.0.0.1 - " << statusCode << " "
				  << getStatusMessage(statusCode) << std::endl;
	}
}
