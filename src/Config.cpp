#include "Config.hpp"
#include <fstream>
#include <sstream>
#include <algorithm>

Config::Config() {}

Config::~Config() {}

void Config::trim(std::string& str) {
    // Supprimer les espaces au début
    str.erase(str.begin(), std::find_if(str.begin(), str.end(), std::not1(std::ptr_fun<int, int>(std::isspace))));
    // Supprimer les espaces à la fin
    str.erase(std::find_if(str.rbegin(), str.rend(), std::not1(std::ptr_fun<int, int>(std::isspace))).base(), str.end());
}

void Config::parseLine(const std::string& line, ServerConfig& current_server) {
    std::string trimmed_line = line;
    trim(trimmed_line);

    if (trimmed_line.empty() || trimmed_line[0] == '#') {
        return;
    }

    std::istringstream iss(trimmed_line);
    std::string directive;
    iss >> directive;

    if (directive == "listen") {
        iss >> current_server.port;
    }
    else if (directive == "host") {
        iss >> current_server.host;
    }
    else if (directive == "root") {
        iss >> current_server.root;
    }
    else if (directive == "index") {
        iss >> current_server.index;
    }
    else if (directive == "allowed_methods") {
        std::string method;
        while (iss >> method) {
            current_server.allowed_methods.push_back(method);
        }
    }
    else if (directive == "error_page") {
        std::string code, page;
        iss >> code >> page;
        current_server.error_pages[code] = page;
    }
    else if (directive == "client_max_body_size") {
        std::string size_str;
        iss >> size_str;
        // Convertir en octets (supporte K, M, G)
        size_t multiplier = 1;
        char unit = size_str[size_str.length() - 1];
        if (unit == 'K' || unit == 'k') multiplier = 1024;
        else if (unit == 'M' || unit == 'm') multiplier = 1024 * 1024;
        else if (unit == 'G' || unit == 'g') multiplier = 1024 * 1024 * 1024;
        
        if (multiplier > 1) {
            size_str = size_str.substr(0, size_str.length() - 1);
        }
        current_server.client_max_body_size = std::atoi(size_str.c_str()) * multiplier;
    }
    else if (directive == "redirect") {
        std::string from, to;
        iss >> from >> to;
        current_server.redirections[from] = to;
    }
}

bool Config::parse(const std::string& filename) {
    std::ifstream file(filename.c_str());
    if (!file.is_open()) {
        return false;
    }

    ServerConfig current_server;
    std::string line;
    bool in_server_block = false;

    while (std::getline(file, line)) {
        trim(line);
        
        if (line.empty() || line[0] == '#') {
            continue;
        }

        if (line == "server {") {
            in_server_block = true;
            current_server = ServerConfig(); // Reset server config
            continue;
        }
        
        if (line == "}") {
            if (in_server_block) {
                _servers.push_back(current_server);
                in_server_block = false;
            }
            continue;
        }

        if (in_server_block) {
            parseLine(line, current_server);
        }
    }

    return true;
}

const std::vector<ServerConfig>& Config::getServers() const {
    return _servers;
} 