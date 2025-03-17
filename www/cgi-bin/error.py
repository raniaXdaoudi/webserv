#!/usr/bin/env python3
import os
import sys
import time

# Fonction pour logger avec le format du serveur
def log(message, type="CGI"):
	if os.environ.get('WEBSERV_LOG_PREFIX') == '1':
		# Le serveur s'occupera du préfixage
		print(message)
	else:
		# Format autonome pour les tests directs
		timestamp = time.strftime("[%Y-%m-%d %H:%M:%S]")
		print(f"{timestamp} [{type}] {message}")

log("Error CGI script starting...")
log("REQUEST_METHOD: " + os.environ.get('REQUEST_METHOD', 'UNKNOWN'))

try:
	# Simuler une erreur
	raise Exception("Test d'erreur CGI intentionnel")
except Exception as e:
	# Capturer et logger l'erreur avec un type spécifique
	log(f"ERREUR: {str(e)}", "CGI-ERROR")
	log(f"Traceback: {sys.exc_info()[0].__name__}", "CGI-ERROR")

	# Générer une réponse d'erreur
	print("Status: 500 Internal Server Error\r")
	print("Content-Type: text/html\r\n\r")
	print("<html><body>")
	print("<h1>Erreur 500 - Internal Server Error</h1>")
	print(f"<p>Une erreur s'est produite: {str(e)}</p>")
	print("</body></html>")

	# Sortir avec un code d'erreur
	sys.exit(1)
