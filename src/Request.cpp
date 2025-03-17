/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.cpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 12:24:01 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:33:13 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Request.hpp"
#include "../include/ConfigParser.hpp"
#include "../include/Utils.hpp"
#include <sstream>
#include <algorithm>
#include <iostream>
#include <sys/stat.h>

Request::Request() :
	_config(*(new ConfigParser())),
	_method(""),
	_url(""),
	_httpVersion(""),
	_headers(),
	_body(""),
	_queryString(""),
	_rawData(""),
	_tempMultipartBuffer(""),
	_headersParsed(false),
	_isMultipart(false),
	_isComplete(false),
	_uploadedFiles() {
}

Request::Request(const std::string& rawRequest, ConfigParser& config)
	: _config(config),
	  _method(""),
	  _url(""),
	  _httpVersion(""),
	  _headers(),
	  _body(""),
	  _queryString(""),
	  _rawData(""),
	  _tempMultipartBuffer(""),
	  _headersParsed(false),
	  _isMultipart(false),
	  _isComplete(false),
	  _uploadedFiles() {
	if (rawRequest.empty()) {
		throw std::runtime_error("Empty request");
	}
	parseRequest(rawRequest);
}

/**
 * Parse la requête brute et sépare les composants HTTP
 */
void Request::parseRequest(const std::string& requestData) {
	if (_isComplete) return;

	if (_body.empty()) {
		size_t headerEnd = requestData.find("\r\n\r\n");
		if (headerEnd == std::string::npos) {
			_body = requestData;
			return;
		}

		std::string headers = requestData.substr(0, headerEnd);
		std::string initialBody = requestData.substr(headerEnd + 4);
		parseHeaders(headers);
		_body = initialBody;
	} else {
		_body += requestData;
	}

	size_t expectedLength = getContentLength();
	if (expectedLength > 0 && _body.length() < expectedLength) {
		return;
	}

	if (isMultipartRequest()) {
		parseMultipartData();
	}

	_isComplete = true;

	if (_isComplete) {
		logRequest();
	}
}

void Request::parseHeaders(const std::string& headers) {
	std::istringstream stream(headers);
	std::string line;


	if (std::getline(stream, line)) {


		try {
			parseFirstLine(line);
		} catch (const std::exception& e) {
			std::cerr << getCurrentTimestamp() << " [ERROR] Erreur lors du parsing de la première ligne: " << e.what() << std::endl;
			_method = "GET";
			_url = "/";
			_httpVersion = "HTTP/1.1";
		}
	}


	while (std::getline(stream, line)) {
		if (line == "\r" || line.empty()) break;

		size_t colonPos = line.find(':');
		if (colonPos != std::string::npos) {
			std::string key = line.substr(0, colonPos);
			std::string value = line.substr(colonPos + 1);
			key = trim(key);
			value = trim(value);
			_headers[key] = value;
		}
	}
}

void Request::parseFirstLine(const std::string& firstLine) {
	std::istringstream lineStream(firstLine);
	std::string method, url, version;

	if (!(lineStream >> method >> url >> version)) {
		throw std::runtime_error("400 Bad Request: Invalid request line");
	}

	_method = method;

	parseQueryString(url);
	_httpVersion = version;
}

void Request::parseQueryString(const std::string& url) {
	try {
		size_t questionPos = url.find('?');
		if (questionPos != std::string::npos) {
			_url = url.substr(0, questionPos);
			_queryString = url.substr(questionPos + 1);
		} else {
			_url = url;
			_queryString = "";
		}
	} catch (const std::exception& e) {
		std::cerr << getCurrentTimestamp() << " [ERROR] Erreur parsing URL: " << e.what() << std::endl;
		_url = "/";
		_queryString = "";
	}
}

void Request::processMultipartBuffer() {
	std::string contentType = getHeader("Content-Type");
	if (!contentType.empty()) {
		size_t boundaryPos = contentType.find("boundary=");
		if (boundaryPos != std::string::npos) {
			std::string boundary = contentType.substr(boundaryPos + 9);
			if (boundary[0] == '"') {
				boundary = boundary.substr(1, boundary.find_last_of('"') - 1);
			}
			boundary = "--" + boundary + "--";

			if (_tempMultipartBuffer.find(boundary) != std::string::npos) {
				_body = _tempMultipartBuffer;
				parseMultipartFormData(contentType);
			}
		}
	}
}

/**
 * Retourne la méthode HTTP (GET, POST, DELETE...)
 */
std::string Request::getMethod() const {
	return _method;
}

/**
 * Retourne l'URL demandée par le client
 */
std::string Request::getUrl() const {
	return _url;
}

/**
 * Retourne la version HTTP utilisée
 */
std::string Request::getHttpVersion() const {
	return _httpVersion;
}

/**
 * Retourne un header spécifique si présent, sinon une chaîne vide
 */
std::string Request::getHeader(const std::string& key) const {
	return _headers.count(key) ? _headers.at(key) : "";
}

/**
 * Retourne le corps de la requête HTTP
 */
std::string Request::getBody() const {
	return _body;
}

void Request::parseBody(const std::string& chunk) {

	size_t contentLength = std::atoi(getHeader("Content-Length").c_str());
	if (contentLength > _config.getClientMaxBodySize() * 1024 * 1024) {
		throw std::runtime_error("413 Request Entity Too Large");
	}


	size_t endPos = chunk.find("\r\n\r\n");
	if (endPos == std::string::npos) {
		endPos = chunk.find("\n\n");
	}


	if (endPos != std::string::npos) {
		_body += chunk.substr(endPos + (chunk[endPos] == '\r' ? 4 : 2));
	} else {
		_body += chunk;
	}
}

void Request::validatePath(const std::string& path) {

	if (path.find("..") != std::string::npos ||
		path.find("//") != std::string::npos) {
		throw std::runtime_error("403 Forbidden");
	}


	if (!fileExists(path)) {
		throw std::runtime_error("404 Not Found");
	}
}

bool Request::fileExists(const std::string& path) const {
	struct stat buffer;
	return (stat(path.c_str(), &buffer) == 0);
}

void Request::parseMultipartFormData(const std::string& contentType) {


	size_t boundaryPos = contentType.find("boundary=");
	if (boundaryPos == std::string::npos) {
		throw std::runtime_error("400 Bad Request - No boundary found");
	}

	std::string boundary = contentType.substr(boundaryPos + 9);
	if (boundary[0] == '"') {
		boundary = boundary.substr(1, boundary.find_last_of('"') - 1);
	}
	size_t semicolonPos = boundary.find(';');
	if (semicolonPos != std::string::npos) {
		boundary = boundary.substr(0, semicolonPos);
	}
	boundary = "--" + boundary;
	std::string finalBoundary = boundary + "--";

	_uploadedFiles.clear();


	size_t pos = 0;
	size_t nextBoundary = std::string::npos;

	while (pos < _body.length()) {

		size_t boundaryStart = _body.find(boundary, pos);
		if (boundaryStart == std::string::npos) break;


		if (_body.substr(boundaryStart, finalBoundary.length()) == finalBoundary) break;


		size_t headersStart = boundaryStart + boundary.length();
		if (headersStart >= _body.length()) break;


		if (_body[headersStart] == '\r' && headersStart + 1 < _body.length() && _body[headersStart + 1] == '\n') {
			headersStart += 2;
		} else if (_body[headersStart] == '\n') {
			headersStart += 1;
		}


		size_t headersEnd = _body.find("\r\n\r\n", headersStart);
		if (headersEnd == std::string::npos) {
			headersEnd = _body.find("\n\n", headersStart);
			if (headersEnd == std::string::npos) break;
		}


		std::string headers = _body.substr(headersStart, headersEnd - headersStart);


		size_t namePos = headers.find("name=\"");
		size_t filenamePos = headers.find("filename=\"");

		if (namePos != std::string::npos) {
			size_t nameEnd = headers.find("\"", namePos + 6);
			std::string fieldName = headers.substr(namePos + 6, nameEnd - (namePos + 6));

			if (fieldName == "file" && filenamePos != std::string::npos) {
				size_t filenameEnd = headers.find("\"", filenamePos + 10);
				std::string filename = headers.substr(filenamePos + 10, filenameEnd - (filenamePos + 10));


				_headers["X-Original-Filename"] = filename;


				size_t contentStart;
				if (headersEnd + 4 <= _body.length()) {
					contentStart = headersEnd + 4;
				} else if (headersEnd + 2 <= _body.length()) {
					contentStart = headersEnd + 2;
				} else {
					break;
				}


				nextBoundary = _body.find(boundary, contentStart);
				if (nextBoundary == std::string::npos) {
					nextBoundary = _body.find(finalBoundary, contentStart);
					if (nextBoundary == std::string::npos) break;
				}


				size_t contentEnd = nextBoundary;
				while (contentEnd > contentStart && (_body[contentEnd - 1] == '\n' || _body[contentEnd - 1] == '\r')) {
					contentEnd--;
				}


				std::string content = _body.substr(contentStart, contentEnd - contentStart);



				if (content.find("Content-Disposition:") != std::string::npos ||
					content.find("Content-Type:") != std::string::npos ||
					content.find("------WebKit") != std::string::npos) {


					static int logCounter = 0;
					if (++logCounter % 10 == 0) {
						std::cout << getCurrentTimestamp() << " [DEBUG] En-têtes détectés dans le contenu, nettoyage supplémentaire nécessaire" << std::endl;
					}


					size_t realContentStart = 0;
					size_t headerEnd = content.find("\r\n\r\n");

					if (headerEnd != std::string::npos && headerEnd + 4 < content.length()) {
						realContentStart = headerEnd + 4;
						content = content.substr(realContentStart);
					} else {
						headerEnd = content.find("\n\n");
						if (headerEnd != std::string::npos && headerEnd + 2 < content.length()) {
							realContentStart = headerEnd + 2;
							content = content.substr(realContentStart);
						} else {


							std::string searchPattern = "Content-Type: application/octet-stream";
							size_t patternPos = content.find(searchPattern);

							if (patternPos != std::string::npos) {
								size_t afterPattern = patternPos + searchPattern.length();


								size_t newlineAfterPattern = content.find("\n", afterPattern);

								if (newlineAfterPattern != std::string::npos && newlineAfterPattern + 1 < content.length()) {

									if (content[newlineAfterPattern + 1] == '\n') {
										realContentStart = newlineAfterPattern + 2;
									} else if (content[newlineAfterPattern + 1] == '\r' &&
											  newlineAfterPattern + 2 < content.length() &&
											  content[newlineAfterPattern + 2] == '\n') {
										realContentStart = newlineAfterPattern + 3;
									}

									if (realContentStart > 0 && realContentStart < content.length()) {
										content = content.substr(realContentStart);
									}
								}
							}
						}
					}
				}
				_uploadedFiles.push_back(content);
			}
		}


		if (nextBoundary != std::string::npos) {
			pos = nextBoundary + boundary.length();
		} else {

			pos = boundaryStart + boundary.length();
		}
	}

	if (_uploadedFiles.empty()) {
		std::cerr << "\033[31m[ERROR] Aucun fichier trouvé dans les données multipart\033[0m" << std::endl;
		throw std::runtime_error("400 Bad Request - No files found");
	}

}

void Request::appendData(const std::string& data) {


	if (_isMultipart && _headersParsed) {
		_body += data;


		std::string contentLength = getHeader("Content-Length");
		if (!contentLength.empty()) {
			size_t expectedLength = std::stoul(contentLength);

			if (_body.length() >= expectedLength) {
				std::string contentType = getHeader("Content-Type");
				parseMultipartFormData(contentType);
			}
		}
		return;
	}


	if (_method.empty()) {

		if (_isMultipart && data.find("--") == 0) {
			_body += data;
			return;
		}

		_rawData += data;
		size_t firstLineEnd = _rawData.find("\r\n");
		if (firstLineEnd == std::string::npos) {
			return;
		}

		std::string firstLine = _rawData.substr(0, firstLineEnd);
		std::istringstream lineStream(firstLine);
		std::string method, url, version;

		if (!(lineStream >> method >> url >> version)) {

			if (_isMultipart) {
				_body += data;
				return;
			}
			throw std::runtime_error("400 Bad Request: Invalid request line");
		}


		if (method != "GET" && method != "POST" && method != "DELETE" &&
			method != "PUT" && method != "HEAD") {
			throw std::runtime_error("400 Bad Request: Invalid method");
		}

		_method = method;
		parseQueryString(url);
		_httpVersion = version;
		_rawData = _rawData.substr(firstLineEnd + 2);
	}


	if (!_headersParsed) {
		_rawData += data;
		size_t headerEnd = _rawData.find("\r\n\r\n");
		if (headerEnd == std::string::npos) {
			return;
		}

		std::string headerSection = _rawData.substr(0, headerEnd);
		parseHeaders(headerSection);
		_headersParsed = true;

		std::string contentType = getHeader("Content-Type");
		if (contentType.find("multipart/form-data") != std::string::npos) {
			_isMultipart = true;
		}

		if (headerEnd + 4 < _rawData.length()) {
			_body = _rawData.substr(headerEnd + 4);
		}
		_rawData.clear();


		if (_isMultipart) {
			std::string contentLength = getHeader("Content-Length");
			if (!contentLength.empty()) {
				size_t expectedLength = std::stoul(contentLength);
				if (_body.length() >= expectedLength) {
					parseMultipartFormData(contentType);
				}
			}
		}
	} else {

		_body += data;


		if (_isMultipart) {
			std::string contentLength = getHeader("Content-Length");
			if (!contentLength.empty()) {
				size_t expectedLength = std::stoul(contentLength);
				if (_body.length() >= expectedLength) {
					std::string contentType = getHeader("Content-Type");
					parseMultipartFormData(contentType);
				}
			}
		}
	}
}

bool Request::isRequestComplete() const {
	if (!_headersParsed) {
		return false;
	}

	std::string contentLength = getHeader("Content-Length");
	if (contentLength.empty()) {
		return true;
	}

	size_t expectedLength = std::stoul(contentLength);
	return _body.length() >= expectedLength;
}

size_t Request::getContentLength() const {
	std::string contentLength = getHeader("Content-Length");
	return contentLength.empty() ? 0 : std::atoi(contentLength.c_str());
}

bool Request::isMultipartRequest() const {
	std::string contentType = getHeader("Content-Type");
	return contentType.find("multipart/form-data") != std::string::npos;
}

void Request::parseMultipartData() {
	std::string contentType = getHeader("Content-Type");
	if (contentType.find("multipart/form-data") != std::string::npos) {
		parseMultipartFormData(contentType);
	}
}

bool isNotSpace(unsigned char ch) {
	return !std::isspace(ch);
}

std::string Request::trim(const std::string& str) const {
	std::string s = str;

	std::string::iterator start = s.begin();
	while (start != s.end() && std::isspace(*start)) {
		++start;
	}
	s.erase(s.begin(), start);


	std::string::reverse_iterator rstart = s.rbegin();
	while (rstart != s.rend() && std::isspace(*rstart)) {
		++rstart;
	}
	s.erase(rstart.base(), s.end());

	return s;
}

void Request::parseUrl(const std::string& fullUrl) {
	try {
		size_t queryPos = fullUrl.find('?');
		if (queryPos != std::string::npos) {
			_url = fullUrl.substr(0, queryPos);
			_queryString = fullUrl.substr(queryPos + 1);

		} else {
			_url = fullUrl;
			_queryString = "";
		}


		 if (_url.empty()) {
			_url = "/";
		}
		size_t pos;
		while ((pos = _url.find("//")) != std::string::npos) {
			_url.erase(pos, 1);
		}
	} catch (const std::exception& e) {
		std::cerr << getCurrentTimestamp() << " [ERROR] Erreur lors du parsing de l'URL: " << e.what() << std::endl;
		_url = "/";
		_queryString = "";
	}
}

void Request::logRequest() const {
	std::string timestamp = getCurrentTimestamp();
	std::cout << timestamp << " [REQUEST] 127.0.0.1 - " << _method << " " << _url << " " << _httpVersion << std::endl;


	std::map<std::string, std::string>::const_iterator it;
	if ((it = _headers.find("Host")) != _headers.end())
		std::cout << timestamp << " [REQUEST] Host: " << it->second << std::endl;
	if ((it = _headers.find("User-Agent")) != _headers.end())
		std::cout << timestamp << " [REQUEST] User-Agent: " << it->second << std::endl;
	if ((it = _headers.find("Accept")) != _headers.end())
		std::cout << timestamp << " [REQUEST] Accept: " << it->second << std::endl;
}

void Request::logBadRequest() const {
	std::cout << getCurrentTimestamp() << " [WARNING] 127.0.0.1 - 400 Bad Request - Invalid HTTP format" << std::endl;
}

