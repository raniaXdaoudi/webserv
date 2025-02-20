#include "Request.hpp"
#include <sstream>

Request::Request() {}

Request::~Request() {}

void Request::setClientIP(const std::string& ip) {
    _client_ip = ip;
}

const std::string& Request::getClientIP() const {
    return _client_ip;
}

const std::string& Request::getMethod() const {
    return _method;
}

const std::string& Request::getPath() const {
    return _path;
}

const std::string& Request::getFullPath() const {
    return _full_path;
}

const std::string& Request::getVersion() const {
    return _version;
}

const std::string& Request::getQueryString() const {
    return _query_string;
}

const std::map<std::string, std::string>& Request::getHeaders() const {
    return _headers;
}

const std::string& Request::getBody() const {
    return _body;
}

std::string Request::getHeader(const std::string& key) const {
    std::map<std::string, std::string>::const_iterator it = _headers.find(key);
    if (it != _headers.end()) {
        return it->second;
    }
    return "";
}

void Request::parseQueryString(const std::string& path) {
    try {
        _full_path = path;  // Sauvegarde du chemin complet
        size_t pos = path.find('?');
        if (pos != std::string::npos && pos < path.length()) {
            _path = path.substr(0, pos);
            if (pos + 1 < path.length()) {
                _query_string = path.substr(pos + 1);
            } else {
                _query_string = "";
            }
        } else {
            _path = path;
            _query_string = "";
        }
    } catch (const std::exception& e) {
        _path = "/";
        _full_path = "/";
        _query_string = "";
    }
}

bool Request::parse(const std::string& raw_request) {
    try {
        std::istringstream request_stream(raw_request);
        std::string line;
        
        // Parse la première ligne (méthode, chemin, version)
        if (!std::getline(request_stream, line)) {
            return false;
        }
        
        std::istringstream first_line(line);
        if (!(first_line >> _method >> _path >> _version)) {
            return false;
        }
        
        // Parse le query string s'il existe
        parseQueryString(_path);
        
        // Parse les headers
        while (std::getline(request_stream, line) && line != "\r" && line != "") {
            size_t colon_pos = line.find(':');
            if (colon_pos != std::string::npos && colon_pos < line.length() - 1) {
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                // Supprime les espaces au début et à la fin
                while (!value.empty() && (value[0] == ' ' || value[0] == '\t')) {
                    value.erase(0, 1);
                }
                while (!value.empty() && (value[value.length() - 1] == ' ' || 
                       value[value.length() - 1] == '\t' || value[value.length() - 1] == '\r')) {
                    value.erase(value.length() - 1);
                }
                
                _headers[key] = value;
            }
        }
        
        // Parse le body si présent
        std::stringstream body_stream;
        while (std::getline(request_stream, line)) {
            body_stream << line << "\n";
        }
        _body = body_stream.str();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
} 