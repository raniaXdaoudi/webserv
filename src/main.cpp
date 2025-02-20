#include "Server.hpp"
#include "Config.hpp"
#include <cstdlib>
#include <vector>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <config_file>" << std::endl;
        return 1;
    }

    Config config;
    if (!config.parse(argv[1])) {
        std::cerr << "Erreur lors du chargement du fichier de configuration" << std::endl;
        return 1;
    }

    const std::vector<ServerConfig>& servers = config.getServers();
    if (servers.empty()) {
        std::cerr << "Aucun serveur configuré" << std::endl;
        return 1;
    }

    // Pour l'instant, on ne gère que le premier serveur configuré
    Server server(servers[0]);
    
    if (!server.init()) {
        std::cerr << "Échec de l'initialisation du serveur" << std::endl;
        return 1;
    }

    server.run();
    return 0;
} 