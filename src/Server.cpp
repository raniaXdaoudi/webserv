#include "Server.hpp"
#include "Request.hpp"
#include "Response.hpp"
#include "HttpMethod.hpp"
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <arpa/inet.h>

Server::Server(const ServerConfig& config) 
    : _socket_fd(-1), _port(config.port), _file_handler(config.root), 
      _config(config), _cgi_handler(config.root + "/cgi-bin"), _session_manager() {}

Server::~Server() {
    stop();
}

Response Server::handleError(int error_code) {
    std::string error_code_str;
    std::ostringstream ss;
    ss << error_code;
    error_code_str = ss.str();

    // Messages d'erreur selon la RFC 2068
    std::string error_message;
    switch (error_code) {
        case 400: error_message = "Bad Request"; break;
        case 401: error_message = "Unauthorized"; break;
        case 402: error_message = "Payment Required"; break;
        case 403: error_message = "Forbidden"; break;
        case 404: error_message = "Not Found"; break;
        case 405: error_message = "Method Not Allowed"; break;
        case 406: error_message = "Not Acceptable"; break;
        case 407: error_message = "Proxy Authentication Required"; break;
        case 408: error_message = "Request Timeout"; break;
        case 409: error_message = "Conflict"; break;
        case 410: error_message = "Gone"; break;
        case 411: error_message = "Length Required"; break;
        case 412: error_message = "Precondition Failed"; break;
        case 413: error_message = "Request Entity Too Large"; break;
        case 414: error_message = "Request-URI Too Long"; break;
        case 415: error_message = "Unsupported Media Type"; break;
        case 500: error_message = "Internal Server Error"; break;
        case 501: error_message = "Not Implemented"; break;
        case 502: error_message = "Bad Gateway"; break;
        case 503: error_message = "Service Unavailable"; break;
        case 504: error_message = "Gateway Timeout"; break;
        case 505: error_message = "HTTP Version Not Supported"; break;
        default: error_message = "Unknown Error"; break;
    }

    std::map<std::string, std::string>::const_iterator it = _config.error_pages.find(error_code_str);
    if (it != _config.error_pages.end()) {
        std::string content;
        std::string mime_type;
        std::string error_page_path = it->second;
        if (error_page_path[0] != '/') {
            error_page_path = "/" + error_page_path;
        }
        if (_file_handler.readFile(error_page_path, content, mime_type)) {
            Response response;
            response.setStatus(error_code, error_message);
            response.setHeader("Content-Type", mime_type);
            response.setBody(content);
            return response;
        }
    }

    // Page d'erreur par défaut si la page personnalisée n'est pas trouvée
    Response response;
    response.setStatus(error_code, error_message);
    response.setHeader("Content-Type", "text/html");
    std::ostringstream body;
    body << "<html><head><title>" << error_code << " " << error_message << "</title></head>";
    body << "<body><h1>" << error_code << " " << error_message << "</h1></body></html>";
    response.setBody(body.str());
    return response;
}

Response Server::handleRedirection(const std::string& path) {
    std::map<std::string, std::string>::const_iterator it = _config.redirections.find(path);
    if (it != _config.redirections.end()) {
        Response response;
        response.setStatus(301, "Moved Permanently");
        std::string location = it->second;
        if (location[0] != '/') {
            location = "/" + location;
        }
        response.setHeader("Location", location);
        return response;
    }
    return handleError(404);
}

Response Server::handleCGI(const Request& request, const std::string& path) {
    // Extraire le chemin sans les paramètres query string pour le fichier
    size_t pos = path.find('?');
    std::string script_path = (pos != std::string::npos) ? path.substr(0, pos) : path;
    
    // Exécuter le CGI avec le chemin complet (incluant les paramètres)
    return _cgi_handler.executeCGI(request, _file_handler.getRootDir() + script_path);
}

Response Server::handleRequest(const Request& request) {
    // Format de log RFC 2068: remotehost rfc931 authuser [date] "request" status bytes
    std::cout << request.getClientIP() << " - - [";
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    char date[100];
    strftime(date, sizeof(date), "%d/%b/%Y:%H:%M:%S %z", &tm);
    std::cout << date << "] \"" << request.getMethod() << " " << request.getPath() 
              << " " << request.getVersion() << "\" ";

    Response response = processRequest(request);

    // Compléter le log avec le status et la taille
    std::cout << response.getStatus() << " " << response.getBody().length() << std::endl;

    return response;
}

bool Server::init() {
    _socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (_socket_fd < 0) {
        return false;
    }

    // Configuration du socket comme non-bloquant
    int flags = fcntl(_socket_fd, F_GETFL, 0);
    if (flags == -1 || fcntl(_socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        stop();
        return false;
    }

    // Configuration des options du socket selon RFC 2068
    int opt = 1;
    if (setsockopt(_socket_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        stop();
        return false;
    }

    // Timeout selon RFC 2068 section 8.1.4 (Keep-Alive)
    struct timeval timeout;
    timeout.tv_sec = 15;  // 15 secondes selon la RFC
    timeout.tv_usec = 0;
    if (setsockopt(_socket_fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0 ||
        setsockopt(_socket_fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) < 0) {
        stop();
        return false;
    }

    _address.sin_family = AF_INET;
    _address.sin_addr.s_addr = INADDR_ANY;
    _address.sin_port = htons(_port);

    if (bind(_socket_fd, (struct sockaddr *)&_address, sizeof(_address)) < 0) {
        stop();
        return false;
    }

    // RFC 2068 recommande une file d'attente suffisamment grande
    if (listen(_socket_fd, SOMAXCONN) < 0) {
        stop();
        return false;
    }

    return true;
}

void Server::run() {
    fd_set master_set, working_set;
    struct timeval timeout;
    int max_fd = _socket_fd;
    std::map<int, std::string> client_buffers;

    FD_ZERO(&master_set);
    FD_SET(_socket_fd, &master_set);

    std::cout << "Serveur en écoute sur le port " << _port << std::endl;

    while (true) {
        working_set = master_set;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;

        int activity = select(max_fd + 1, &working_set, NULL, NULL, &timeout);
        if (activity < 0) {
            if (errno != EINTR) {
                std::cerr << "Erreur lors du select: " << strerror(errno) << std::endl;
            }
            continue;
        }

        // Vérifier les nouvelles connexions
        if (FD_ISSET(_socket_fd, &working_set)) {
            struct sockaddr_in client_address;
            socklen_t addrlen = sizeof(client_address);
            
            int new_socket = accept(_socket_fd, (struct sockaddr *)&client_address, &addrlen);
            if (new_socket < 0) {
                if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    std::cerr << "Erreur lors de l'acceptation de la connexion: " << strerror(errno) << std::endl;
                }
                continue;
            }

            // Configurer le nouveau socket comme non-bloquant
            int flags = fcntl(new_socket, F_GETFL, 0);
            if (flags >= 0) {
                fcntl(new_socket, F_SETFL, flags | O_NONBLOCK);
            }

            // Ajouter le nouveau socket au set
            FD_SET(new_socket, &master_set);
            if (new_socket > max_fd) {
                max_fd = new_socket;
            }

            client_buffers[new_socket] = "";
            continue;
        }

        // Gérer les données des clients existants
        for (int i = 0; i <= max_fd; i++) {
            if (i != _socket_fd && FD_ISSET(i, &working_set)) {
                char buffer[4096] = {0};
                ssize_t bytes_read = read(i, buffer, sizeof(buffer) - 1);

                if (bytes_read < 0) {
                    if (errno != EWOULDBLOCK && errno != EAGAIN) {
                        close(i);
                        FD_CLR(i, &master_set);
                        client_buffers.erase(i);
                    }
                    continue;
                }
                else if (bytes_read == 0) {
                    // Connexion fermée par le client
                    close(i);
                    FD_CLR(i, &master_set);
                    client_buffers.erase(i);
                    continue;
                }

                buffer[bytes_read] = '\0';
                client_buffers[i] += buffer;

                // Vérifier si nous avons une requête complète
                if (client_buffers[i].find("\r\n\r\n") != std::string::npos) {
                    Request request;
                    Response response;

                    if (request.parse(client_buffers[i])) {
                        // Obtenir l'IP du client
                        struct sockaddr_in addr;
                        socklen_t addr_len = sizeof(addr);
                        getpeername(i, (struct sockaddr*)&addr, &addr_len);
                        char client_ip[INET_ADDRSTRLEN];
                        inet_ntop(AF_INET, &(addr.sin_addr), client_ip, INET_ADDRSTRLEN);
                        request.setClientIP(client_ip);

                        response = handleRequest(request);
                    } else {
                        response = handleError(400);
                    }

                    std::string response_str = response.toString();
                    ssize_t total_sent = 0;
                    while (total_sent < static_cast<ssize_t>(response_str.length())) {
                        ssize_t sent = send(i, response_str.c_str() + total_sent, 
                                          response_str.length() - total_sent, 0);
                        if (sent < 0) {
                            if (errno != EWOULDBLOCK && errno != EAGAIN) {
                                break;
                            }
                            continue;
                        }
                        total_sent += sent;
                    }

                    // Fermer la connexion après avoir envoyé la réponse
                    close(i);
                    FD_CLR(i, &master_set);
                    client_buffers.erase(i);
                }
            }
        }

        // Mettre à jour max_fd si nécessaire
        while (max_fd > 0 && !FD_ISSET(max_fd, &master_set)) {
            max_fd--;
        }
    }
}

void Server::stop() {
    if (_socket_fd >= 0) {
        // RFC 2068 Section 8.1.4 - Fermeture propre des connexions
        shutdown(_socket_fd, SHUT_RDWR);
        close(_socket_fd);
        _socket_fd = -1;
    }
}

Response Server::processRequest(const Request& request) {
    // RFC 2068 Section 5.1.1 - Vérification de la version HTTP
    if (request.getVersion() != "HTTP/1.1") {
        return handleError(505);
    }

    _session_manager.updateSession(request);

    // RFC 2068 Section 5.1.2 - Vérification de l'URI
    if (request.getPath().length() > 8000) { // Limite raisonnable selon la RFC
        return handleError(414);
    }

    // Redirection
    std::map<std::string, std::string>::const_iterator redirect_it = _config.redirections.find(request.getPath());
    if (redirect_it != _config.redirections.end()) {
        return handleRedirection(request.getPath());
    }

    // RFC 2068 Section 5.1.1 - Vérification des méthodes
    const std::string& method = request.getMethod();
    bool method_allowed = false;
    for (std::vector<std::string>::const_iterator it = _config.allowed_methods.begin();
         it != _config.allowed_methods.end(); ++it) {
        if (*it == method) {
            method_allowed = true;
            break;
        }
    }
    if (!method_allowed) {
        Response response = handleError(405);
        // RFC 2068 Section 10.4.6 - Allow header obligatoire pour 405
        std::string allowed_methods;
        for (std::vector<std::string>::const_iterator it = _config.allowed_methods.begin();
             it != _config.allowed_methods.end(); ++it) {
            if (it != _config.allowed_methods.begin()) {
                allowed_methods += ", ";
            }
            allowed_methods += *it;
        }
        response.setHeader("Allow", allowed_methods);
        return response;
    }

    if (method == "POST") {
        if (request.getHeader("Content-Length").empty()) {
            return handleError(411);
        }
        if (request.getBody().length() > _config.client_max_body_size) {
            return handleError(413);
        }
    }

    // CGI
    if (_cgi_handler.isCGIScript(request.getPath())) {
        return handleCGI(request, request.getFullPath());
    }

    // Traitement des méthodes selon RFC 2068 Section 9
    Response response;
    try {
        if (method == "GET") {
            response = HttpMethod::handleGet(request, _file_handler);
            if (response.getStatus() == 404) {
                return handleError(404);
            }
        }
        else if (method == "POST") {
            response = HttpMethod::handlePost(request, _file_handler);
        }
        else if (method == "DELETE") {
            response = HttpMethod::handleDelete(request, _file_handler);
        }
        else {
            return handleError(501);
        }
    } catch (...) {
        return handleError(500);
    }

    // RFC 2068 Section 14.18 - Date header obligatoire
    char date[1000];
    time_t now = time(0);
    struct tm tm = *gmtime(&now);
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S GMT", &tm);
    response.setHeader("Date", date);

    return response;
} 