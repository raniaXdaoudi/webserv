#include "SessionManager.hpp"
#include <sstream>
#include <iomanip>
#include <cstdlib>
#include <ctime>

SessionManager::SessionManager(time_t session_timeout) 
    : _session_timeout(session_timeout), _cookie_name("WEBSERV_SESSION") {
    std::srand(static_cast<unsigned int>(std::time(NULL)));
}

SessionManager::~SessionManager() {}

std::string SessionManager::generateSessionId() const {
    const std::string chars = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
    std::string session_id;
    for (int i = 0; i < 32; ++i) {
        session_id += chars[std::rand() % chars.length()];
    }
    return session_id;
}

void SessionManager::cleanExpiredSessions() {
    time_t now = std::time(NULL);
    std::map<std::string, Session>::iterator it = _sessions.begin();
    while (it != _sessions.end()) {
        if (it->second.expiry < now) {
            it = _sessions.erase(it);
        } else {
            ++it;
        }
    }
}

std::string SessionManager::generateCookie(const std::string& session_id) const {
    std::ostringstream cookie;
    cookie << _cookie_name << "=" << session_id << "; Path=/; HttpOnly";
    return cookie.str();
}

std::string SessionManager::createSession() {
    cleanExpiredSessions();
    
    std::string session_id = generateSessionId();
    Session session;
    session.id = session_id;
    session.last_access = std::time(NULL);
    session.expiry = session.last_access + _session_timeout;
    
    _sessions[session_id] = session;
    return session_id;
}

bool SessionManager::hasSession(const std::string& session_id) {
    cleanExpiredSessions();
    return _sessions.find(session_id) != _sessions.end();
}

void SessionManager::destroySession(const std::string& session_id) {
    _sessions.erase(session_id);
}

void SessionManager::setValue(const std::string& session_id, const std::string& key, const std::string& value) {
    std::map<std::string, Session>::iterator it = _sessions.find(session_id);
    if (it != _sessions.end()) {
        it->second.data[key] = value;
        it->second.last_access = std::time(NULL);
        it->second.expiry = it->second.last_access + _session_timeout;
    }
}

std::string SessionManager::getValue(const std::string& session_id, const std::string& key) const {
    std::map<std::string, Session>::const_iterator it = _sessions.find(session_id);
    if (it != _sessions.end()) {
        std::map<std::string, std::string>::const_iterator data_it = it->second.data.find(key);
        if (data_it != it->second.data.end()) {
            return data_it->second;
        }
    }
    return "";
}

void SessionManager::updateSession(const Request& request) {
    std::string cookie = request.getHeader("Cookie");
    size_t pos = cookie.find(_cookie_name + "=");
    if (pos != std::string::npos) {
        pos += _cookie_name.length() + 1;
        size_t end_pos = cookie.find(";", pos);
        std::string session_id = cookie.substr(pos, end_pos - pos);
        
        std::map<std::string, Session>::iterator it = _sessions.find(session_id);
        if (it != _sessions.end()) {
            it->second.last_access = std::time(NULL);
            it->second.expiry = it->second.last_access + _session_timeout;
        }
    }
}

void SessionManager::addSessionCookie(Response& response, const std::string& session_id) {
    response.setHeader("Set-Cookie", generateCookie(session_id));
} 