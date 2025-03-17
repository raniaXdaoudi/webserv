/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Request.hpp                                        :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 12:22:46 by rania             #+#    #+#             */
/*   Updated: 2025/03/01 19:42:00 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>
#include <sstream>
#include "ConfigParser.hpp"
#include <vector>

class Request {
private:
	ConfigParser& _config;
	std::string _method;
	std::string _url;
	std::string _httpVersion;
	std::map<std::string, std::string> _headers;
	std::string _body;
	std::string _queryString;
	std::string _rawData;
	std::string _tempMultipartBuffer;  
	bool _headersParsed;
	bool _isMultipart;  
	bool _isComplete;  
	std::vector<std::string> _uploadedFiles;

	void parseFirstLine(const std::string& firstLine);
	void parseQueryString(const std::string& url);
	void parseHeaders(const std::string& headerSection);
	void processMultipartBuffer();
	void parseMultipartFormData(const std::string& contentType);
	void parseRequest(const std::string& requestData);
	void parseUrl(const std::string& fullUrl);
	bool fileExists(const std::string& path) const;
	void tryParseHeaders();
	bool isRequestComplete() const;
	size_t getContentLength() const;  
	bool isMultipartRequest() const;  
	void parseMultipartData();  
	std::string trim(const std::string& str) const;  

public:
	Request(const std::string& rawRequest, ConfigParser& config);
	Request();
	~Request() {}

	void parseBody(const std::string& chunk);
	void appendData(const std::string& data);

	std::string getMethod() const;
	std::string getUrl() const;
	std::string getHttpVersion() const;
	std::string getHeader(const std::string& key) const;
	const std::map<std::string, std::string>& getHeaders() const { return _headers; }
	std::string getBody() const;
	std::string getQueryString() const { return _queryString; }
	void validatePath(const std::string& path);
	const std::vector<std::string>& getUploadedFiles() const { return _uploadedFiles; }
	bool isMultipart() const { return _isMultipart; }
	bool isHeadersParsed() const { return _headersParsed; }
	void logRequest() const;
	void logBadRequest() const;
};

#endif
