/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 13:19:28 by rania             #+#    #+#             */
/*   Updated: 2025/01/31 18:10:41 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "../include/Utils.hpp"

bool endsWith(const std::string &str, const std::string &suffix) {
	if (str.length() < suffix.length())
		return false;
	return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
}
