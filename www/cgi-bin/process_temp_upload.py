#!/usr/bin/env python3
# -*- coding: utf-8 -*-

import os
import sys
import re
import glob
import time

def send_response(status, headers, body):
	print(f"Status: {status}")
	for header, value in headers.items():
		print(f"{header}: {value}")
	print()  # Ligne vide pour séparer les headers du body
	print(body)

def create_processed_dir():
	# Utiliser le DOCUMENT_ROOT fourni par le serveur
	document_root = os.environ.get('DOCUMENT_ROOT', 'www')
	processed_dir = os.path.join(document_root, "upload", "processed")

	try:
		os.makedirs(processed_dir, exist_ok=True)
		return processed_dir
	except Exception as e:
		send_response("500 Internal Server Error",
			{"Content-Type": "text/html"},
			f"<h1>Erreur lors de la création du dossier de traitement</h1><p>{str(e)}</p>")
		sys.exit(1)

def sanitize_filename(filename):
	# Remplacer les caractères non autorisés par un underscore
	filename = re.sub(r'[^\w\.-]', '_', filename)
	return filename

def get_unique_filename(processed_dir, original_filename):
	# Nettoyer le nom de fichier
	base_name, ext = os.path.splitext(sanitize_filename(original_filename))
	counter = 0
	while True:
		if counter == 0:
			new_filename = f"{base_name}{ext}"
		else:
			new_filename = f"{base_name}_{counter}{ext}"

		filepath = os.path.join(processed_dir, new_filename)
		if not os.path.exists(filepath):
			return new_filename
		counter += 1

def process_temp_file(temp_file_path):
	try:
		# Lire le fichier temporaire
		with open(temp_file_path, 'rb') as f:
			content = f.read().decode('utf-8', errors='ignore')

		# Trouver la boundary
		boundary_match = re.search(r'^-+([^\r\n]+)', content)
		if not boundary_match:
			return None, "Boundary non trouvée"

		boundary = boundary_match.group(0)

		# Extraire le nom du fichier original
		filename_match = re.search(r'filename="([^"]+)"', content)
		if not filename_match:
			return None, "Nom de fichier non trouvé"

		original_filename = filename_match.group(1)

		# Trouver où commence le contenu réel (après les en-têtes)
		headers_end = content.find("\r\n\r\n")
		if headers_end == -1:
			return None, "Format de fichier temporaire invalide"

		content_start = headers_end + 4

		# Trouver où se termine le contenu (avant la boundary finale)
		final_boundary = f"\r\n{boundary}--"
		content_end = content.find(final_boundary)
		if content_end == -1:
			# Si pas de boundary finale, prendre tout le reste
			real_content = content[content_start:]
		else:
			real_content = content[content_start:content_end]

		# Créer le répertoire de destination
		processed_dir = create_processed_dir()

		# Générer un nom de fichier unique
		processed_filename = get_unique_filename(processed_dir, original_filename)
		processed_path = os.path.join(processed_dir, processed_filename)

		# Écrire le contenu extrait dans le fichier final
		with open(processed_path, 'wb') as f:
			f.write(real_content.encode('utf-8'))

		return processed_filename, None

	except Exception as e:
		return None, str(e)

def process_all_temp_files():
	try:
		# Obtenir le répertoire des fichiers temporaires
		document_root = os.environ.get('DOCUMENT_ROOT', 'www')
		temp_dir = os.path.join(document_root, "upload", "cgi")

		# Trouver tous les fichiers temporaires (commençant par _)
		temp_files = glob.glob(os.path.join(temp_dir, "_*"))

		if not temp_files:
			send_response("200 OK",
				{"Content-Type": "text/html"},
				"<h1>Aucun fichier temporaire à traiter</h1>")
			return

		processed_files = []
		errors = []

		for temp_file in temp_files:
			processed_filename, error = process_temp_file(temp_file)

			if processed_filename:
				processed_files.append(processed_filename)
				# Supprimer le fichier temporaire après traitement
				os.remove(temp_file)
			else:
				errors.append(f"Erreur lors du traitement de {os.path.basename(temp_file)}: {error}")

		# Générer la réponse
		if processed_files:
			success_html = "<h1 class='success'>Traitement réussi !</h1>"
			success_html += "<div class='file-info'>"
			success_html += "<h2>Fichiers traités :</h2>"
			success_html += "<ul>"
			for filename in processed_files:
				success_html += f"<li>{filename}</li>"
			success_html += "</ul>"
			success_html += "</div>"
		else:
			success_html = "<h1>Aucun fichier n'a été traité avec succès</h1>"

		if errors:
			error_html = "<div class='error-info'>"
			error_html += "<h2>Erreurs :</h2>"
			error_html += "<ul>"
			for error in errors:
				error_html += f"<li>{error}</li>"
			error_html += "</ul>"
			error_html += "</div>"
		else:
			error_html = ""

		send_response("200 OK",
			{"Content-Type": "text/html"},
			f"""
			<!DOCTYPE html>
			<html>
			<head>
				<title>Traitement des fichiers temporaires</title>
				<style>
					body {{ font-family: Arial, sans-serif; margin: 40px; }}
					.success {{ color: green; }}
					.file-info {{ background: #f5f5f5; padding: 15px; border-radius: 5px; margin: 20px 0; }}
					.error-info {{ background: #fff0f0; padding: 15px; border-radius: 5px; margin: 20px 0; color: #c00; }}
					.back-link {{ margin-top: 20px; }}
					a {{ color: #0066cc; text-decoration: none; }}
					a:hover {{ text-decoration: underline; }}
				</style>
			</head>
			<body>
				{success_html}
				{error_html}
				<div class="back-link">
					<a href="/">Retour à l'accueil</a>
				</div>
			</body>
			</html>
			""")

	except Exception as e:
		send_response("500 Internal Server Error",
			{"Content-Type": "text/html"},
			f"<h1>Erreur lors du traitement des fichiers temporaires</h1><p>{str(e)}</p>")

if __name__ == "__main__":
	process_all_temp_files()
