#ifndef SESSION_MANAGER_HPP
#define SESSION_MANAGER_HPP

#include <string>
#include <map>
#include <ctime>
#include "Request.hpp"
#include "Response.hpp"

struct Session {
    std::string id;
    std::map<std::string, std::string> data;
    time_t last_access;
    time_t expiry;
};

class SessionManager {
private:
    std::map<std::string, Session> _sessions;
    time_t _session_timeout;
    std::string _cookie_name;

    std::string generateSessionId() const;
    void cleanExpiredSessions();
    std::string generateCookie(const std::string& session_id) const;

public:
    SessionManager(time_t session_timeout = 1800); // 30 minutes par défaut
    ~SessionManager();

    std::string createSession();
    bool hasSession(const std::string& session_id);
    void destroySession(const std::string& session_id);
    
    void setValue(const std::string& session_id, const std::string& key, const std::string& value);
    std::string getValue(const std::string& session_id, const std::string& key) const;
    
    void updateSession(const Request& request);
    void addSessionCookie(Response& response, const std::string& session_id);
};

#endif 