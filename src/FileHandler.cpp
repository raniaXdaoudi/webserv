#include "FileHandler.hpp"
#include <sys/stat.h>
#include <sstream>

FileHandler::FileHandler(const std::string& root_dir) : _root_dir(root_dir) {
    initMimeTypes();
}

FileHandler::~FileHandler() {}

void FileHandler::initMimeTypes() {
    _mime_types[".html"] = "text/html";
    _mime_types[".htm"] = "text/html";
    _mime_types[".css"] = "text/css";
    _mime_types[".js"] = "application/javascript";
    _mime_types[".jpg"] = "image/jpeg";
    _mime_types[".jpeg"] = "image/jpeg";
    _mime_types[".png"] = "image/png";
    _mime_types[".gif"] = "image/gif";
    _mime_types[".ico"] = "image/x-icon";
    _mime_types[".txt"] = "text/plain";
}

std::string FileHandler::getMimeType(const std::string& file_extension) const {
    std::map<std::string, std::string>::const_iterator it = _mime_types.find(file_extension);
    if (it != _mime_types.end()) {
        return it->second;
    }
    return "application/octet-stream";
}

bool FileHandler::fileExists(const std::string& path) const {
    struct stat buffer;
    std::string full_path = _root_dir + path;
    return (stat(full_path.c_str(), &buffer) == 0);
}

bool FileHandler::readFile(const std::string& path, std::string& content, std::string& mime_type) {
    std::string full_path = _root_dir + path;
    std::string request_path = path;
    
    // Si le chemin se termine par '/', ajouter index.html
    if (path[path.length() - 1] == '/') {
        request_path += "index.html";
        full_path += "index.html";
    }

    // Vérifier si le fichier existe
    struct stat stat_buffer;
    if (stat(full_path.c_str(), &stat_buffer) != 0) {
        return false;
    }

    // Trouver l'extension du fichier
    size_t dot_pos = full_path.find_last_of(".");
    if (dot_pos != std::string::npos) {
        std::string extension = full_path.substr(dot_pos);
        mime_type = getMimeType(extension);
    } else {
        mime_type = "application/octet-stream";
    }

    // Lire le contenu du fichier
    std::ifstream file(full_path.c_str(), std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::stringstream str_buffer;
    str_buffer << file.rdbuf();
    content = str_buffer.str();
    
    return true;
} 