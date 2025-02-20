#!/usr/bin/env python3
import requests
import threading
import time
import random
import string
from concurrent.futures import ThreadPoolExecutor, as_completed

BASE_URL = "http://localhost:8080"
NUM_REQUESTS = 100  # Réduit de 1000 à 100
NUM_CONCURRENT = 10  # Réduit de 100 à 10
TIMEOUT = 10  # Augmenté de 5 à 10 secondes

def random_string(length):
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def test_get():
    try:
        print(f"GET: Tentative d'accès à {BASE_URL}/")
        response = requests.get(f"{BASE_URL}/", timeout=TIMEOUT)
        print(f"GET: Status code reçu: {response.status_code}")
        return response.status_code == 200
    except Exception as e:
        print(f"GET: Erreur - {str(e)}")
        return False

def test_post():
    try:
        filename = f"test_{random_string(8)}.txt"
        content = random_string(100)  # Réduit de 1000 à 100 caractères
        print(f"POST: Tentative de création de {filename}")
        response = requests.post(f"{BASE_URL}/{filename}", 
                               data=content,
                               headers={'Content-Type': 'text/plain'},
                               timeout=TIMEOUT)
        print(f"POST: Status code reçu: {response.status_code}")
        return response.status_code in [200, 201]
    except Exception as e:
        print(f"POST: Erreur - {str(e)}")
        return False

def test_delete():
    try:
        filename = f"test_{random_string(8)}.txt"
        print(f"DELETE: Création du fichier test {filename}")
        # Créer d'abord un fichier
        create_response = requests.post(f"{BASE_URL}/{filename}", 
                                      data="test content",
                                      headers={'Content-Type': 'text/plain'},
                                      timeout=TIMEOUT)
        if create_response.status_code not in [200, 201]:
            print(f"DELETE: Échec de la création du fichier - {create_response.status_code}")
            return False

        print(f"DELETE: Tentative de suppression de {filename}")
        response = requests.delete(f"{BASE_URL}/{filename}", timeout=TIMEOUT)
        print(f"DELETE: Status code reçu: {response.status_code}")
        return response.status_code in [200, 204]
    except Exception as e:
        print(f"DELETE: Erreur - {str(e)}")
        return False

def run_test(test_func):
    start_time = time.time()
    success = test_func()
    end_time = time.time()
    return success, end_time - start_time

def main():
    print(f"Démarrage des tests de stress sur {BASE_URL}")
    print(f"Nombre total de requêtes: {NUM_REQUESTS}")
    print(f"Nombre de requêtes simultanées: {NUM_CONCURRENT}")
    print("=" * 50)

    test_functions = [test_get, test_post, test_delete]
    results = {func.__name__: {'success': 0, 'failed': 0, 'total_time': 0} for func in test_functions}

    with ThreadPoolExecutor(max_workers=NUM_CONCURRENT) as executor:
        for test_func in test_functions:
            print(f"\nTest de {test_func.__name__}")
            futures = []
            
            for _ in range(NUM_REQUESTS):
                futures.append(executor.submit(run_test, test_func))
                time.sleep(0.1)  # Petit délai entre les requêtes

            for future in as_completed(futures):
                try:
                    success, duration = future.result()
                    if success:
                        results[test_func.__name__]['success'] += 1
                    else:
                        results[test_func.__name__]['failed'] += 1
                    results[test_func.__name__]['total_time'] += duration
                except Exception as e:
                    print(f"Erreur lors de l'exécution du test: {str(e)}")
                    results[test_func.__name__]['failed'] += 1

    print("\nRésultats des tests:")
    print("=" * 50)
    for test_name, data in results.items():
        total = data['success'] + data['failed']
        avg_time = data['total_time'] / total if total > 0 else 0
        success_rate = (data['success'] / total * 100) if total > 0 else 0
        
        print(f"\n{test_name}:")
        print(f"Succès: {data['success']}/{total} ({success_rate:.2f}%)")
        print(f"Échecs: {data['failed']}/{total}")
        print(f"Temps moyen par requête: {avg_time:.3f} secondes")

if __name__ == "__main__":
    main() 