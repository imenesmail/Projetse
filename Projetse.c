#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>
#include <time.h>

// Définition des constantes : nombre de bus et nombre de trajets par bus
#define NB_X 5
#define NB_Y 4
#define NB_TRAJETS 10

// Mutex pour protéger les variables partagées
pthread_mutex_t verrou = PTHREAD_MUTEX_INITIALIZER;

// Sémaphores pour gérer l’attente selon la direction
sem_t file_x, file_y;

// Variables de synchronisation
int nb_dans_tunnel = 0;           // Nombre de bus actuellement dans le tunnel
int direction_actuelle = 0;       // 0 = aucun sens, 1 = X->Y, 2 = Y->X
int en_attente_x = 0;             // Nombre de bus de X en attente
int en_attente_y = 0;             // Nombre de bus de Y en attente
int passage_x = 0;                // Compteur de passages consécutifs de X->Y
int passage_y = 0;                // Compteur de passages consécutifs de Y->X

// Fonction pour gérer l’entrée dans le tunnel
void entrer_tunnel(int direction) {
    pthread_mutex_lock(&verrou);
    if (direction == 1) { // Direction X -> Y
        en_attente_x++;
        // Si le tunnel est dans l’autre sens et X a déjà fait 5 trajets d’affilée, attendre
        if ((direction_actuelle == 2) && (passage_x >= 5 && en_attente_y > 0)) {
            pthread_mutex_unlock(&verrou);
            sem_wait(&file_x);
            pthread_mutex_lock(&verrou);
        }
        en_attente_x--;
        nb_dans_tunnel++;
        direction_actuelle = 1;
        passage_x++;
        passage_y = 0;
    } else { // Direction Y -> X
        en_attente_y++;
        if ((direction_actuelle == 1) && (passage_y >= 5 && en_attente_x > 0)) {
            pthread_mutex_unlock(&verrou);
            sem_wait(&file_y);
            pthread_mutex_lock(&verrou);
        }
        en_attente_y--;
        nb_dans_tunnel++;
        direction_actuelle = 2;
        passage_y++;
        passage_x = 0;
    }
    pthread_mutex_unlock(&verrou);
}

// Fonction pour sortir du tunnel et potentiellement débloquer l’autre sens
void sortir_tunnel(int direction) {
    pthread_mutex_lock(&verrou);
    nb_dans_tunnel--;
    // Si le tunnel est vide, possibilité de changer de sens
    if (nb_dans_tunnel == 0) {
        direction_actuelle = 0;
        if (direction == 1 && en_attente_y > 0) {
            for (int i = 0; i < en_attente_y; i++)
                sem_post(&file_y);
        } else if (direction == 2 && en_attente_x > 0) {
            for (int i = 0; i < en_attente_x; i++)
                sem_post(&file_x);
        }
    }
    pthread_mutex_unlock(&verrou);
}

// Fonction représentant le comportement d’un bus
void* thread_bus(void* arg) {
    int id = ((int*)arg)[0];        // ID du bus
    int origine = ((int*)arg)[1];   // Ville d’origine (1 = X, 2 = Y)
    char* ville = origine == 1 ? "X" : "Y";

    for (int i = 1; i <= NB_TRAJETS; i++) {
        // Aller
        entrer_tunnel(origine == 1 ? 1 : 2);
        printf("Bus %d de %s : %s -> %s (Trajet %d)\n", id, ville,
               origine == 1 ? "X" : "Y", origine == 1 ? "Y" : "X", i);
        usleep((rand() % 500 + 1000) * 1000); // Sleep entre 1s et 1.5s
        sortir_tunnel(origine == 1 ? 1 : 2);

        // Retour
        entrer_tunnel(origine == 1 ? 2 : 1);
        printf("Bus %d de %s : %s -> %s (Trajet %d)\n", id, ville,
               origine == 1 ? "Y" : "X", origine == 1 ? "X" : "Y", i);
        usleep((rand() % 500 + 1000) * 1000);
        sortir_tunnel(origine == 1 ? 2 : 1);
    }
    return NULL;
}

int main() {
    srand(time(NULL)); // Initialisation de la graine aléatoire

    // Initialisation des sémaphores
    sem_init(&file_x, 0, 0);
    sem_init(&file_y, 0, 0);

    pthread_t threads[NB_X + NB_Y];
    int parametres[NB_X + NB_Y][2];

    // Création des threads pour les bus de X
    for (int i = 0; i < NB_X; i++) {
        parametres[i][0] = i + 1;
        parametres[i][1] = 1;
        pthread_create(&threads[i], NULL, thread_bus, parametres[i]);
    }

    // Création des threads pour les bus de Y
    for (int i = 0; i < NB_Y; i++) {
        parametres[NB_X + i][0] = i + 1;
        parametres[NB_X + i][1] = 2;
        pthread_create(&threads[NB_X + i], NULL, thread_bus, parametres[NB_X + i]);
    }

    // Attente de fin de tous les threads
    for (int i = 0; i < NB_X + NB_Y; i++) {
        pthread_join(threads[i], NULL);
    }

    // Nettoyage
    sem_destroy(&file_x);
    sem_destroy(&file_y);
    pthread_mutex_destroy(&verrou);

    return 0;
}
