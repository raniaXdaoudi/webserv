#ifndef FILEHANDLER_HPP
#define FILEHANDLER_HPP

#include <string>
#include <map>
#include <fstream>

class FileHandler {
private:
    std::string _root_dir;
    std::map<std::string, std::string> _mime_types;

    void initMimeTypes();
    std::string getMimeType(const std::string& file_extension) const;

public:
    FileHandler(const std::string& root_dir);
    ~FileHandler();

    bool readFile(const std::string& path, std::string& content, std::string& mime_type);
    bool fileExists(const std::string& path) const;
    const std::string& getRootDir() const { return _root_dir; }
};

#endif 