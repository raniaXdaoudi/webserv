#ifndef HTTP_METHOD_HPP
#define HTTP_METHOD_HPP

#include <string>
#include "Request.hpp"
#include "Response.hpp"
#include "FileHandler.hpp"

class HttpMethod {
public:
    static Response handleGet(const Request& request, FileHandler& file_handler);
    static Response handlePost(const Request& request, FileHandler& file_handler);
    static Response handleDelete(const Request& request, FileHandler& file_handler);

private:
    static Response createErrorResponse(int status_code, const std::string& message);
};

#endif 