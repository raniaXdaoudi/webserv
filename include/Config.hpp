#ifndef CONFIG_HPP
#define CONFIG_HPP

#include <string>
#include <vector>
#include <map>

struct ServerConfig {
    int port;
    std::string host;
    std::string root;
    std::string index;
    std::vector<std::string> allowed_methods;
    std::map<std::string, std::string> error_pages;
    std::map<std::string, std::string> redirections;
    size_t client_max_body_size;
};

class Config {
private:
    std::vector<ServerConfig> _servers;
    void parseLine(const std::string& line, ServerConfig& current_server);
    void trim(std::string& str);

public:
    Config();
    ~Config();

    bool parse(const std::string& filename);
    const std::vector<ServerConfig>& getServers() const;
};

#endif 