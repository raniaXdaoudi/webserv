/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Route.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/02/04 11:53:00 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:23:37 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ROUTE_HPP
#define ROUTE_HPP

#include <string>
#include <vector>
#include <map>

class Route {
private:
	std::string _path;
	std::string _root;
	std::vector<std::string> _allowed_methods;
	bool _directory_listing;
	std::string _index_file;
	std::string _redirect_url;
	std::map<std::string, std::string> _cgi_handlers;
	std::string _upload_dir;
	size_t _client_max_body_size;
	bool _try_files;

public:
	Route(const std::string& path = "");
	~Route();


	void setPath(const std::string& path);
	void setRoot(const std::string& root);
	void addMethod(const std::string& method);
	void setDirectoryListing(bool enabled);
	void setIndexFile(const std::string& file);
	void setRedirect(const std::string& url);
	void addCgiExtension(const std::string& ext, const std::string& handler);
	void setUploadDir(const std::string& dir);
	void setClientMaxBodySize(size_t size);
	void setTryFiles(bool enabled);


	const std::string& getPath() const;
	const std::string& getRoot() const;
	const std::vector<std::string>& getMethods() const;
	bool isDirectoryListingEnabled() const;
	const std::string& getIndexFile() const;
	const std::string& getRedirect() const;
	const std::map<std::string, std::string>& getCgiHandlers() const;
	const std::string& getUploadDir() const;
	size_t getClientMaxBodySize() const;
	bool getTryFiles() const;


	bool isMethodAllowed(const std::string& method) const;
	bool isCgiExtension(const std::string& ext) const;
	std::string getCgiHandler(const std::string& ext) const;
	bool isExecutable(const std::string& path) const;
	bool isCgiEnabled() const { return !_cgi_handlers.empty(); }
};

#endif
