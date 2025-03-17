#!/usr/bin/env python3
import os
import sys
import time
import urllib.parse

# Fonction pour logger avec le format du serveur
def log(message, type="CGI"):
	if os.environ.get('WEBSERV_LOG_PREFIX') == '1':
		# Le serveur s'occupera du préfixage
		print(message)
	else:
		# Format autonome pour les tests directs
		timestamp = time.strftime("[%Y-%m-%d %H:%M:%S]")
		print(f"{timestamp} [{type}] {message}")

log("CGI script starting...")
log("REQUEST_METHOD: " + os.environ.get('REQUEST_METHOD', 'UNKNOWN'))

method = os.environ.get('REQUEST_METHOD', 'GET')

if method not in ['GET', 'POST']:
	print("Status: 405 Method Not Allowed")
	print("Content-Type: text/plain")
	print()  # Ligne vide importante
	print(f"Méthode {method} non autorisée. Seules GET et POST sont permises.")
	sys.exit(0)

# Traitement des paramètres de requête
query_string = os.environ.get('QUERY_STRING', '')
params = {}
if query_string:
	params = dict(urllib.parse.parse_qsl(query_string))
	log(f"Paramètres reçus: {params}")

# Corps de la réponse
body = f"""<html><body>
<h1>Test CGI</h1>
<p>Le CGI fonctionne!</p>
<h2>Informations de debug:</h2>
<pre>
Méthode: {method}
PATH_INFO: {os.environ.get('PATH_INFO', 'Non défini')}
QUERY_STRING: {query_string}
REMOTE_ADDR: {os.environ.get('REMOTE_ADDR', 'Non défini')}
</pre>

<h2>Paramètres reçus:</h2>
<ul>
"""

# Ajouter chaque paramètre à la réponse
for key, value in params.items():
	body += f"<li><strong>{key}</strong>: {value}</li>\n"

if not params:
	body += "<li>Aucun paramètre reçu</li>\n"

body += """</ul>
</body></html>"""

# En-têtes CGI (sans \r\n, juste \n)
print("Status: 200 OK")
print("Content-Type: text/html")
print(f"Content-Length: {len(body)}")
print()  # Ligne vide importante pour séparer les en-têtes du corps
print(body)  # Corps de la réponse

log("CGI script finished")

# Générer la réponse HTTP
print("Content-Type: text/html\r\n\r")
print("<html><body>")
print("<h1>Test CGI Script</h1>")
print("<p>This is a test CGI script.</p>")
print("</body></html>")
