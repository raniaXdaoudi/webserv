/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Route.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/04 11:53:00 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:26:21 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Route.hpp"
#include <algorithm>
#include <sys/stat.h>
#include <iostream>


extern std::string getCurrentTimestamp();

Route::Route(const std::string& path) : _path(path), _directory_listing(false), _client_max_body_size(0), _try_files(false) {
	_index_file = "index.html";

	_allowed_methods.push_back("GET");
	_allowed_methods.push_back("HEAD");
}

Route::~Route() {}


void Route::setPath(const std::string& path) { _path = path; }
void Route::setRoot(const std::string& root) { _root = root; }
void Route::addMethod(const std::string& method) { _allowed_methods.push_back(method); }
void Route::setDirectoryListing(bool enabled) { _directory_listing = enabled; }
void Route::setIndexFile(const std::string& file) { _index_file = file; }
void Route::setRedirect(const std::string& url) {
	_redirect_url = url;
}
void Route::addCgiExtension(const std::string& ext, const std::string& handler) { _cgi_handlers[ext] = handler; }
void Route::setUploadDir(const std::string& dir) { _upload_dir = dir; }
void Route::setClientMaxBodySize(size_t size) { _client_max_body_size = size; }
void Route::setTryFiles(bool enabled) { _try_files = enabled; }


const std::string& Route::getPath() const { return _path; }
const std::string& Route::getRoot() const { return _root; }
const std::vector<std::string>& Route::getMethods() const { return _allowed_methods; }
bool Route::isDirectoryListingEnabled() const { return _directory_listing; }
const std::string& Route::getIndexFile() const { return _index_file; }
const std::string& Route::getRedirect() const { return _redirect_url; }
const std::map<std::string, std::string>& Route::getCgiHandlers() const { return _cgi_handlers; }
const std::string& Route::getUploadDir() const { return _upload_dir; }
size_t Route::getClientMaxBodySize() const { return _client_max_body_size; }
bool Route::getTryFiles() const { return _try_files; }


bool Route::isMethodAllowed(const std::string& method) const {
	return std::find(_allowed_methods.begin(), _allowed_methods.end(), method) != _allowed_methods.end();
}

bool Route::isCgiExtension(const std::string& ext) const {
	return _cgi_handlers.find(ext) != _cgi_handlers.end();
}

std::string Route::getCgiHandler(const std::string& ext) const {
	std::map<std::string, std::string>::const_iterator it = _cgi_handlers.find(ext);
	return (it != _cgi_handlers.end()) ? it->second : "";
}

bool Route::isExecutable(const std::string& path) const {
	struct stat st;
	if (stat(path.c_str(), &st) != 0) {
		std::cerr << getCurrentTimestamp() << " [ERROR] Impossible d'ouvrir le répertoire: " << path << std::endl;
		return false;
	}
	return (st.st_mode & S_IXUSR) != 0;
}
