#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import datetime
import cgi
import urllib.parse

# Fonction pour parser les paramètres GET de QUERY_STRING
def parse_query_string(query_string):
    params = {}
    if query_string:
        for param in query_string.split('&'):
            if '=' in param:
                key, value = param.split('=', 1)
                params[key] = urllib.parse.unquote_plus(value)
    return params

# Fonction pour lire les données POST
def read_post_data():
    if os.environ.get('REQUEST_METHOD') == 'POST':
        content_length = int(os.environ.get('CONTENT_LENGTH', 0))
        post_data = sys.stdin.read(content_length)
        return dict(parse_query_string(post_data))
    return {}

# Récupérer les paramètres GET et POST
method = os.environ.get('REQUEST_METHOD', '')
params = {}

if method == 'GET':
    query_string = os.environ.get('QUERY_STRING', '')
    params = parse_query_string(query_string)
elif method == 'POST':
    content_length = int(os.environ.get('CONTENT_LENGTH', 0))
    post_data = sys.stdin.read(content_length)
    params = parse_query_string(post_data)

# Générer l'en-tête de la réponse
print("Content-Type: text/html\r\n\r\n")

# Générer le corps de la réponse
print("""
<!DOCTYPE html>
<html lang="fr">
<head>
    <meta charset="UTF-8">
    <title>Réponse CGI</title>
</head>
<body>
    <div class="container">
        <h1>Réponse du Script CGI</h1>
        <p class="info">Méthode utilisée : <span class="method">{}</span></p>
        <div class="params">
            <h2>Paramètres reçus :</h2>
""".format(method))

if params:
    for key, value in params.items():
        print(f"            <p><strong>{key}</strong>: {value}</p>")
else:
    print("            <p>Aucun paramètre reçu</p>")

print("""
        </div>
        <p><a href="/cgi-test.html">Retour au formulaire</a></p>
    </div>
</body>
</html>
""") 