#include "HttpMethod.hpp"
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

Response HttpMethod::createErrorResponse(int status_code, const std::string& message) {
    Response response;
    response.setStatus(status_code, message);
    response.setHeader("Content-Type", "text/html");
    std::ostringstream body;
    body << "<html><body><h1>" << message << "</h1></body></html>";
    response.setBody(body.str());
    return response;
}

Response HttpMethod::handleGet(const Request& request, FileHandler& file_handler) {
    std::string content;
    std::string mime_type;
    
    if (!file_handler.readFile(request.getPath(), content, mime_type)) {
        return createErrorResponse(404, "Not Found");
    }
    
    Response response;
    response.setStatus(200, "OK");
    response.setHeader("Content-Type", mime_type);
    response.setBody(content);
    return response;
}

Response HttpMethod::handlePost(const Request& request, FileHandler& file_handler) {
    // Construire le chemin complet en utilisant le root_dir de file_handler
    std::string full_path = file_handler.getRootDir() + request.getPath();
    struct stat buffer;
    if (stat(full_path.c_str(), &buffer) == 0) {
        return createErrorResponse(409, "Conflict - Resource already exists");
    }

    // Créer le fichier avec le contenu du body
    std::ofstream file(full_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return createErrorResponse(500, "Internal Server Error - Cannot create file");
    }

    file << request.getBody();
    file.close();

    Response response;
    response.setStatus(201, "Created");
    response.setHeader("Content-Type", "text/plain");
    response.setBody("Resource created successfully");
    return response;
}

Response HttpMethod::handleDelete(const Request& request, FileHandler& file_handler) {
    // Construire le chemin complet en utilisant le root_dir de file_handler
    std::string full_path = file_handler.getRootDir() + request.getPath();
    
    // Vérifier si le fichier existe
    struct stat buffer;
    if (stat(full_path.c_str(), &buffer) != 0) {
        return createErrorResponse(404, "Not Found");
    }

    // Supprimer le fichier
    if (unlink(full_path.c_str()) != 0) {
        return createErrorResponse(500, "Internal Server Error - Cannot delete file");
    }

    Response response;
    response.setStatus(200, "OK");
    response.setHeader("Content-Type", "text/plain");
    response.setBody("Resource deleted successfully");
    return response;
} 