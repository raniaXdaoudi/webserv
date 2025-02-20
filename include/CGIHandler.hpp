#ifndef CGI_HANDLER_HPP
#define CGI_HANDLER_HPP

#include <string>
#include <map>
#include "Request.hpp"
#include "Response.hpp"

class CGIHandler {
private:
    std::string _cgi_dir;
    std::map<std::string, std::string> _cgi_extensions;
    
    std::string getCGIExecutable(const std::string& file_extension) const;
    void setupEnvironment(const Request& request, std::map<std::string, std::string>& env) const;

public:
    CGIHandler(const std::string& cgi_dir);
    ~CGIHandler();

    void addCGIExtension(const std::string& extension, const std::string& interpreter);
    Response executeCGI(const Request& request, const std::string& script_path);
    bool isCGIScript(const std::string& path) const;
};

#endif 