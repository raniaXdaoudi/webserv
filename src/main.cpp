/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: rania <rania@student.42.fr>                +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2025/01/31 12:13:13 by rania             #+#    #+#             */
/*   Updated: 2025/03/17 11:24:36 by rania            ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include <iostream>
#include "../include/Server.hpp"
#include "../include/ConfigParser.hpp"
#include <signal.h>
#include "../include/Logger.hpp"

void sigHandler(int) {
	if (Server::getInstance()) {
		Server::getInstance()->stop();
	}
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
		return 1;
	}

	try {
		ConfigParser config;
		config.parse(argv[1]);

		signal(SIGINT, sigHandler);
		signal(SIGTERM, sigHandler);


		Logger::init();

		Server::createInstance(config);
		Server::getInstance()->start();


		Logger::close();
	} catch (const std::exception& e) {
		std::cerr << "Error: " << e.what() << std::endl;
		return 1;
	}

	return 0;
}
