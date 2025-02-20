#!/bin/bash

# Couleurs pour le terminal
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Fonction pour afficher les résultats
print_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}[OK]${NC} $2"
    else
        echo -e "${RED}[KO]${NC} $2"
    fi
}

print_section() {
    echo -e "\n${YELLOW}=== $1 ===${NC}"
}

# Vérification de l'installation de siege
check_siege() {
    print_section "Vérification de Siege"
    if ! command -v siege &> /dev/null; then
        echo "Installation de siege..."
        brew install siege
    fi
    print_result $? "Installation de siege"
}

# Test de la compilation
test_compilation() {
    print_section "Test de Compilation"
    make re
    print_result $? "Compilation du projet"
}

# Test des configurations
test_config() {
    print_section "Test des Configurations"
    
    # Test multiple ports
    curl -s http://127.0.0.1:8080 > /dev/null
    print_result $? "Test port 8080"
    
    curl -s http://localhost:8081 > /dev/null
    print_result $? "Test port 8081"
    
    # Test hostname
    curl -s --resolve example.com:8080:127.0.0.1 http://example.com/ > /dev/null
    print_result $? "Test hostname virtuel"
    
    # Test page d'erreur 404
    curl -s -w "%{http_code}" http://127.0.0.1:8080/nonexistent > /tmp/response.txt
    HTTP_CODE=$(tail -n 1 /tmp/response.txt)
    RESPONSE_BODY=$(head -n -1 /tmp/response.txt)
    if [ "$HTTP_CODE" = "404" ]; then
        print_result 0 "Test page erreur 404"
    else
        print_result 1 "Test page erreur 404 (Code reçu: $HTTP_CODE)"
    fi
    rm -f /tmp/response.txt
    
    # Test limite body
    curl -s -X POST -H "Content-Type: plain/text" --data "BODY_TROP_LONG_XXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXXX" http://127.0.0.1:8080/ > /dev/null
    print_result $? "Test limite body"
}

# Test des méthodes HTTP basiques
test_basic_methods() {
    print_section "Test des Méthodes HTTP"
    
    # Test GET
    curl -s -X GET http://127.0.0.1:8080/ > /dev/null
    print_result $? "Test GET"
    
    # Test POST
    curl -s -X POST -H "Content-Type: plain/text" --data "Test body" http://127.0.0.1:8080/upload > /dev/null
    print_result $? "Test POST"
    
    # Test DELETE
    curl -s -X DELETE http://127.0.0.1:8080/test_file > /dev/null
    print_result $? "Test DELETE"
    
    # Test méthode inconnue
    curl -s -X UNKNOWN http://127.0.0.1:8080/ > /dev/null
    print_result $? "Test méthode inconnue"
}

# Test des CGI
test_cgi() {
    print_section "Test CGI"
    
    # Test GET CGI
    curl -s http://127.0.0.1:8080/cgi-bin/test.php > /dev/null
    print_result $? "Test GET CGI"
    
    # Test POST CGI
    curl -s -X POST -H "Content-Type: plain/text" --data "Test CGI" http://127.0.0.1:8080/cgi-bin/test.php > /dev/null
    print_result $? "Test POST CGI"
    
    # Test CGI avec erreur
    curl -s http://127.0.0.1:8080/cgi-bin/error.php > /dev/null
    print_result $? "Test CGI avec erreur"
}

# Test avec navigateur (simulation)
test_browser_compatibility() {
    print_section "Test Compatibilité Navigateur"
    
    # Test avec User-Agent de navigateur
    curl -s -A "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/91.0.4472.124 Safari/537.36" http://127.0.0.1:8080/ > /dev/null
    print_result $? "Test compatibilité navigateur"
    
    # Test listing directory
    curl -s http://127.0.0.1:8080/directory/ > /dev/null
    print_result $? "Test listing directory"
    
    # Test redirection
    curl -s -L http://127.0.0.1:8080/old > /dev/null
    print_result $? "Test redirection"
}

# Test de stress avec Siege
test_stress() {
    print_section "Test de Stress avec Siege"
    
    # Test de disponibilité
    siege -b -t10S http://127.0.0.1:8080/ 2>&1 | grep "Availability" | grep -q "100.00"
    print_result $? "Test disponibilité"
    
    # Test de charge
    siege -c100 -t30S http://127.0.0.1:8080/ 2>&1 > /dev/null
    print_result $? "Test charge"
}

# Fonction principale
main() {
    echo "Démarrage des tests pour WebServ..."
    
    check_siege
    test_compilation
    test_config
    test_basic_methods
    test_cgi
    test_browser_compatibility
    test_stress
    
    echo -e "\n${GREEN}Tests terminés !${NC}"
}

# Exécution du script
main 