#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import cgi
import re

def send_response(status, headers, body):
	print(f"Status: {status}")
	for header, value in headers.items():
		print(f"{header}: {value}")
	print()  # Ligne vide pour séparer les headers du body
	print(body)

def create_upload_dir():
	# Utiliser le DOCUMENT_ROOT fourni par le serveur
	document_root = os.environ.get('DOCUMENT_ROOT', 'www')
	upload_dir = os.path.join(document_root, "upload", "cgi")

	try:
		os.makedirs(upload_dir, exist_ok=True)
		return upload_dir
	except Exception as e:
		send_response("500 Internal Server Error",
			{"Content-Type": "text/html"},
			f"<h1>Erreur lors de la création du dossier d'upload</h1><p>{str(e)}</p>")
		sys.exit(1)

def sanitize_filename(filename):
	# Remplacer les caractères non autorisés par un underscore
	filename = re.sub(r'[^\w\.-]', '_', filename)
	return filename

def get_unique_filename(upload_dir, original_filename):
	# Nettoyer le nom de fichier
	base_name, ext = os.path.splitext(sanitize_filename(original_filename))
	counter = 0
	while True:
		if counter == 0:
			new_filename = f"{base_name}{ext}"
		else:
			new_filename = f"{base_name}_{counter}{ext}"

		filepath = os.path.join(upload_dir, new_filename)
		if not os.path.exists(filepath):
			return new_filename
		counter += 1

def handle_upload():
	try:
		# Vérifier la méthode de la requête
		if os.environ.get('REQUEST_METHOD') != 'POST':
			send_response("405 Method Not Allowed",
				{"Content-Type": "text/html", "Allow": "POST"},
				"<h1>Méthode non autorisée</h1><p>Seule la méthode POST est autorisée.</p>")
			return

		# Vérifier le type de contenu
		content_type = os.environ.get('CONTENT_TYPE', '')
		if not content_type.startswith('multipart/form-data'):
			send_response("400 Bad Request",
				{"Content-Type": "text/html"},
				"<h1>Type de contenu invalide</h1><p>Le type de contenu doit être multipart/form-data.</p>")
			return

		# Créer le répertoire d'upload si nécessaire
		upload_dir = create_upload_dir()

		# Traiter le formulaire
		form = cgi.FieldStorage()

		if 'file' not in form:
			send_response("400 Bad Request",
				{"Content-Type": "text/html"},
				"<h1>Fichier manquant</h1><p>Aucun fichier n'a été envoyé.</p>")
			return

		fileitem = form['file']
		if not fileitem.filename:
			send_response("400 Bad Request",
				{"Content-Type": "text/html"},
				"<h1>Nom de fichier invalide</h1><p>Le fichier envoyé n'a pas de nom.</p>")
			return

		# Générer un nom de fichier unique en conservant le nom original
		filename = get_unique_filename(upload_dir, fileitem.filename)
		filepath = os.path.join(upload_dir, filename)

		# Sauvegarder le fichier
		with open(filepath, 'wb') as f:
			f.write(fileitem.file.read())

		# Construire l'URL relative pour le fichier uploadé
		relative_path = os.path.join("/upload/cgi", filename)

		# Envoyer la réponse de succès
		send_response("201 Created",
			{
				"Content-Type": "text/html",
				"Location": relative_path
			},
			f"""
			<!DOCTYPE html>
			<html>
			<head>
				<title>Upload réussi</title>
				<style>
					body {{ font-family: Arial, sans-serif; margin: 40px; }}
					.success {{ color: green; }}
					.file-info {{ background: #f5f5f5; padding: 15px; border-radius: 5px; margin: 20px 0; }}
					.back-link {{ margin-top: 20px; }}
					a {{ color: #0066cc; text-decoration: none; }}
					a:hover {{ text-decoration: underline; }}
				</style>
			</head>
			<body>
				<h1 class="success">Upload réussi !</h1>
				<div class="file-info">
					<p><strong>Nom du fichier :</strong> {filename}</p>
					<p><strong>Chemin d'accès :</strong> {relative_path}</p>
				</div>
				<div class="back-link">
					<a href="/">Retour à l'accueil</a>
				</div>
			</body>
			</html>
			""")

	except Exception as e:
		send_response("500 Internal Server Error",
			{"Content-Type": "text/html"},
			f"<h1>Erreur lors de l'upload</h1><p>{str(e)}</p>")

if __name__ == "__main__":
	handle_upload()
