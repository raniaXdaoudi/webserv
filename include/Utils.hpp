/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 13:19:06 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:23:50 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

#include <string>

bool endsWith(const std::string &str, const std::string &suffix);

class Utils {
public:
	static std::string getFileExtension(const std::string& path) {
		size_t dot = path.find_last_of('.');
		if (dot != std::string::npos) {
			return path.substr(dot);
		}
		return "";
	}
};

std::string getCurrentTimestamp();

#endif
