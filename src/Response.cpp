#include "Response.hpp"
#include <sstream>

Response::Response() : _version("HTTP/1.1"), _status_code(200), _status_message("OK") {}

Response::~Response() {}

void Response::setStatus(int code, const std::string& message) {
    _status_code = code;
    _status_message = message;
}

void Response::setHeader(const std::string& key, const std::string& value) {
    _headers[key] = value;
}

void Response::setBody(const std::string& body) {
    _body = body;
    std::ostringstream ss;
    ss << body.length();
    setHeader("Content-Length", ss.str());
}

std::string Response::toString() const {
    std::ostringstream response;
    
    // Status line
    response << _version << " " << _status_code << " " << _status_message << "\r\n";
    
    // Headers
    for (std::map<std::string, std::string>::const_iterator it = _headers.begin();
         it != _headers.end(); ++it) {
        response << it->first << ": " << it->second << "\r\n";
    }
    
    // Empty line separating headers from body
    response << "\r\n";
    
    // Pour les redirections, nous n'avons pas besoin d'envoyer de corps
    if (_status_code != 301 && _status_code != 302) {
        response << _body;
    }
    
    return response.str();
} 