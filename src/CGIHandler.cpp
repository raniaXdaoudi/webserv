#include "CGIHandler.hpp"
#include <unistd.h>
#include <sys/wait.h>
#include <sstream>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <vector>

CGIHandler::CGIHandler(const std::string& cgi_dir) : _cgi_dir(cgi_dir) {
    // Configuration par défaut des interpréteurs CGI
    _cgi_extensions[".php"] = "/usr/bin/php-cgi";
    _cgi_extensions[".py"] = "/Users/tamsibesson/.pyenv/shims/python3";
    _cgi_extensions[".pl"] = "/usr/bin/perl";
}

CGIHandler::~CGIHandler() {}

void CGIHandler::addCGIExtension(const std::string& extension, const std::string& interpreter) {
    _cgi_extensions[extension] = interpreter;
}

std::string CGIHandler::getCGIExecutable(const std::string& file_extension) const {
    std::map<std::string, std::string>::const_iterator it = _cgi_extensions.find(file_extension);
    if (it != _cgi_extensions.end()) {
        return it->second;
    }
    return "";
}

bool CGIHandler::isCGIScript(const std::string& path) const {
    try {
        // Extraire le chemin sans les paramètres query string
        size_t pos = path.find('?');
        std::string clean_path = (pos != std::string::npos) ? path.substr(0, pos) : path;
        
        // Vérifier l'extension
        size_t dot_pos = clean_path.find_last_of('.');
        if (dot_pos == std::string::npos) {
            return false;
        }
        
        std::string extension = clean_path.substr(dot_pos);
        return _cgi_extensions.find(extension) != _cgi_extensions.end();
    } catch (const std::exception& e) {
        return false;
    }
}

void CGIHandler::setupEnvironment(const Request& request, std::map<std::string, std::string>& env) const {
    // Variables communes
    env["SERVER_SOFTWARE"] = "WebServ/1.0";
    env["SERVER_NAME"] = "localhost";
    env["GATEWAY_INTERFACE"] = "CGI/1.1";
    env["SERVER_PROTOCOL"] = "HTTP/1.1";
    env["SERVER_PORT"] = "8080";
    env["REMOTE_ADDR"] = request.getClientIP();
    env["REQUEST_METHOD"] = request.getMethod();
    env["PATH_INFO"] = request.getPath();
    env["PATH_TRANSLATED"] = _cgi_dir + request.getPath();
    env["SCRIPT_NAME"] = request.getPath();
    env["DOCUMENT_ROOT"] = _cgi_dir;
    env["REDIRECT_STATUS"] = "200";
    env["PATH"] = "/usr/local/bin:/usr/bin:/bin:/usr/sbin:/sbin:/Users/tamsibesson/.pyenv/shims";
    env["PYTHONPATH"] = "/Users/tamsibesson/.pyenv/versions/3.9.6/lib/python3.9";

    // Variables spécifiques à la méthode
    if (request.getMethod() == "GET") {
        env["QUERY_STRING"] = request.getQueryString();
    }
    else if (request.getMethod() == "POST") {
        env["CONTENT_TYPE"] = request.getHeader("Content-Type");
        env["CONTENT_LENGTH"] = request.getHeader("Content-Length");
        env["QUERY_STRING"] = "";  // Vide pour POST
    }

    // Headers HTTP
    std::map<std::string, std::string>::const_iterator it;
    const std::map<std::string, std::string>& headers = request.getHeaders();
    for (it = headers.begin(); it != headers.end(); ++it) {
        std::string header_name = "HTTP_" + it->first;
        // Convertir les - en _ et mettre en majuscules
        for (std::string::iterator c = header_name.begin(); c != header_name.end(); ++c) {
            if (*c == '-') *c = '_';
            *c = toupper(*c);
        }
        env[header_name] = it->second;
    }
}

Response CGIHandler::executeCGI(const Request& request, const std::string& script_path) {
    if (!isCGIScript(script_path)) {
        Response response;
        response.setStatus(400, "Bad Request");
        response.setBody("Not a CGI script");
        return response;
    }

    // Vérifier si le script existe
    if (access(script_path.c_str(), F_OK) == -1) {
        Response response;
        response.setStatus(404, "Not Found");
        response.setBody("CGI script not found");
        return response;
    }

    // Vérifier si le script est exécutable
    if (access(script_path.c_str(), X_OK) == -1) {
        Response response;
        response.setStatus(500, "Internal Server Error");
        response.setBody("CGI script is not executable");
        return response;
    }

    // Créer les pipes pour la communication
    int input_pipe[2];
    int output_pipe[2];

    if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0) {
        Response response;
        response.setStatus(500, "Internal Server Error");
        response.setBody("Failed to create pipes");
        return response;
    }

    pid_t pid = fork();
    if (pid < 0) {
        close(input_pipe[0]); close(input_pipe[1]);
        close(output_pipe[0]); close(output_pipe[1]);
        Response response;
        response.setStatus(500, "Internal Server Error");
        response.setBody("Failed to fork process");
        return response;
    }

    if (pid == 0) {
        // Processus enfant
        close(input_pipe[1]);
        close(output_pipe[0]);

        dup2(input_pipe[0], STDIN_FILENO);
        dup2(output_pipe[1], STDOUT_FILENO);

        // Configurer l'environnement
        std::map<std::string, std::string> env;
        setupEnvironment(request, env);

        // Convertir l'environnement en tableau de chaînes
        std::vector<std::string> env_strings;
        for (std::map<std::string, std::string>::const_iterator it = env.begin(); it != env.end(); ++it) {
            env_strings.push_back(it->first + "=" + it->second);
        }
        char* envp[env_strings.size() + 1];
        for (size_t i = 0; i < env_strings.size(); ++i) {
            envp[i] = const_cast<char*>(env_strings[i].c_str());
        }
        envp[env_strings.size()] = NULL;

        // Préparer les arguments pour execve
        std::string interpreter = getCGIExecutable(script_path.substr(script_path.find_last_of(".")));
        char* argv[] = {
            const_cast<char*>(interpreter.c_str()),
            const_cast<char*>(script_path.c_str()),
            NULL
        };

        // Exécuter le script
        execve(interpreter.c_str(), argv, envp);
        std::cerr << "Erreur execve: " << strerror(errno) << std::endl;
        exit(1);
    }

    // Processus parent
    close(input_pipe[0]);
    close(output_pipe[1]);

    // Envoyer le corps de la requête au script si nécessaire
    if (request.getMethod() == "POST") {
        write(input_pipe[1], request.getBody().c_str(), request.getBody().length());
    }
    close(input_pipe[1]);

    // Lire la sortie du script
    char buffer[4096];
    std::string output;
    ssize_t bytes_read;
    while ((bytes_read = read(output_pipe[0], buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        output += buffer;
    }
    close(output_pipe[0]);

    // Attendre la fin du processus
    int status;
    waitpid(pid, &status, 0);

    // Analyser la sortie et créer la réponse
    Response response;
    if (WIFEXITED(status)) {
        // Séparer les en-têtes du corps
        size_t header_end = output.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            std::string headers = output.substr(0, header_end);
            std::string body = output.substr(header_end + 4);

            // Parser les en-têtes
            std::istringstream header_stream(headers);
            std::string line;
            while (std::getline(header_stream, line) && line != "\r") {
                size_t colon = line.find(": ");
                if (colon != std::string::npos) {
                    std::string key = line.substr(0, colon);
                    std::string value = line.substr(colon + 2);
                    response.setHeader(key, value);
                }
            }
            response.setBody(body);
        } else {
            // Pas d'en-têtes, tout est considéré comme le corps
            response.setHeader("Content-Type", "text/html");
            response.setBody(output);
        }
    } else {
        response.setStatus(500, "Internal Server Error");
        response.setBody("CGI script execution failed");
    }

    return response;
} 