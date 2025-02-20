# webserv

A standards-compliant HTTP/1.1 server implemented in C++98. This is a group project part of the 42 school curriculum.

## Description

This project involves developing a web server capable of handling HTTP/1.1 requests, serving static content, executing CGI scripts, and managing multiple virtual server configurations. It deepens understanding of network protocols, socket programming, and process management.

## Technologies Used
- C++98
- Python (for CGI scripts)
- Shell scripting
- HTML/CSS (for test pages)
- Make (build system)

## Features

The server implements:
- HTTP/1.1 request handling (GET, POST, DELETE)
- Multiple virtual servers configuration
- Configuration file management
- Non-blocking server with select/poll/epoll
- CGI script execution
- File upload handling
- Standard HTTP error management
- Redirection
- Default index handling
- Request body size limitation
- Custom root directories

## Getting Started

### Prerequisites
- C++ compiler (g++)
- Make
- Unix-based operating system (Linux/Mac)
- Python (for CGI tests)

### Installation
```bash
git clone https://github.com/yourusername/webserv.git
cd webserv
make
```

### Usage
```bash
./webserv [configuration_file]
```

Example:
```bash
./webserv conf/default.conf
```

## Configuration

The configuration file follows a Nginx-like syntax:

```nginx
server {
    listen 8080;
    server_name localhost;
    root /var/www/html;

    location / {
        index index.html;
        allowed_methods GET POST;
    }

    location /uploads {
        upload_store /tmp/uploads;
        client_max_body_size 10M;
    }

    location *.php {
        fastcgi_pass unix:/var/run/php/php-fpm.sock;
    }
}
```

## Implementation Details

The server:
1. Parses the configuration file
2. Sets up sockets for each listening port
3. Handles connections non-blockingly
4. Parses incoming HTTP requests
5. Routes requests to appropriate handlers
6. Executes CGI scripts when necessary
7. Generates and sends HTTP responses

## Error Handling
The program handles various error cases:
- Configuration syntax errors
- Socket and connection errors
- Malformed HTTP requests
- File access errors
- Connection timeouts
- CGI errors

## Building
The project includes a Makefile with the following rules:
- `make` - Compiles the program
- `make clean` - Removes object files
- `make fclean` - Removes object files and executable
- `make re` - Rebuilds the program

## Return Value
- Returns 0 on success
- Displays appropriate error messages to stderr
- Properly closes all resources

## Testing
The project includes a test suite to verify:
- HTTP/1.1 compliance
- Multiple request handling
- Load limits
- Error handling
- CGI script compatibility

## Authors
This is a group project developed by:
- Rania (radaoudi)
- Tamsi (tbesson)

## License
This project is part of the 42 school curriculum. Please refer to 42's policies regarding code usage and distribution.

## Acknowledgments
- 42 school for providing project requirements and framework
- HTTP/1.1 RFC documentation
- Network programming documentation and resources
