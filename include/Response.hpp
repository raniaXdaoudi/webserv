#ifndef RESPONSE_HPP
#define RESPONSE_HPP

#include <string>
#include <map>

class Response {
private:
    std::string _version;
    int _status_code;
    std::string _status_message;
    std::map<std::string, std::string> _headers;
    std::string _body;

public:
    Response();
    ~Response();

    void setStatus(int code, const std::string& message);
    void setHeader(const std::string& key, const std::string& value);
    void setBody(const std::string& body);
    
    int getStatus() const { return _status_code; }
    const std::string& getBody() const { return _body; }
    std::string toString() const;
};

#endif 