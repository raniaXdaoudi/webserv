#ifndef REQUEST_HPP
#define REQUEST_HPP

#include <string>
#include <map>

class Request {
private:
    std::string _method;
    std::string _path;
    std::string _full_path;
    std::string _version;
    std::string _query_string;
    std::map<std::string, std::string> _headers;
    std::string _body;
    std::string _client_ip;

    void parseQueryString(const std::string& path);

public:
    Request();
    ~Request();

    bool parse(const std::string& raw_request);
    
    // Getters
    const std::string& getMethod() const;
    const std::string& getPath() const;
    const std::string& getFullPath() const;
    const std::string& getVersion() const;
    const std::string& getQueryString() const;
    const std::map<std::string, std::string>& getHeaders() const;
    const std::string& getBody() const;
    const std::string& getClientIP() const;
    void setClientIP(const std::string& ip);
    std::string getHeader(const std::string& key) const;
};

#endif 