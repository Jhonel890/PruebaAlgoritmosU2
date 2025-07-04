#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>          // Para clock()
#include <sys/time.h>      // Para gettimeofday()

#define MAX_COMENTARIOS 4000
#define MAX_PALABRA 500
#define MAX_LINEA 1024

typedef struct {
    int id_usuario;
    char nombre[100];
    char comentario[512];
} Comentario;

char quitar_tilde(char c) {
    return tolower((unsigned char)c);
}

void str_to_lower(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = quitar_tilde(str[i]);
    }
}

int cargar_palabras_prohibidas(char palabras[][MAX_PALABRA]) {
    FILE *archivo = fopen("palabrasProhibidas.txt", "r");
    if (!archivo) {
        perror("Error abriendo palabrasProhibidas.txt");
        exit(1);
    }
    int count = 0;
    while (fgets(palabras[count], MAX_PALABRA, archivo)) {
        palabras[count][strcspn(palabras[count], "\n")] = 0;
        str_to_lower(palabras[count]);
        count++;
    }
    fclose(archivo);
    return count;
}

int cargar_comentarios(Comentario comentarios[]) {
    FILE *archivo = fopen("comentarios.csv", "r");
    if (!archivo) {
        perror("Error abriendo comentarios.csv");
        exit(1);
    }
    int count = 0;
    char linea[MAX_LINEA];
    while (fgets(linea, MAX_LINEA, archivo)) {
        linea[strcspn(linea, "\n")] = 0;

        char *token = strtok(linea, ",");
        if (!token) continue;

        comentarios[count].id_usuario = atoi(token);

        token = strtok(NULL, ",");
        if (token) strcpy(comentarios[count].nombre, token);

        token = strtok(NULL, "\n");
        if (token) {
            strcpy(comentarios[count].comentario, token);
            str_to_lower(comentarios[count].comentario);
        }

        count++;
    }
    fclose(archivo);
    return count;
}

int contiene_palabra_prohibida(const char* comentario, char palabras[][MAX_PALABRA], int num_palabras) {
    for (int i = 0; i < num_palabras; i++) {
        if (strstr(comentario, palabras[i])) {
            return 1;
        }
    }
    return 0;
}

int main() {
    // Tiempo con time.h
    clock_t inicio_clock = clock();

    // Tiempo con sys/time.h
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    Comentario comentarios[MAX_COMENTARIOS];
    char palabras_prohibidas[100][MAX_PALABRA];

    int total_comentarios = cargar_comentarios(comentarios);
    int total_palabras = cargar_palabras_prohibidas(palabras_prohibidas);

    int ofensivos = 0;
    printf("Comentarios ofensivos detectados:\n\n");

    for (int i = 0; i < total_comentarios; i++) {
        if (contiene_palabra_prohibida(comentarios[i].comentario, palabras_prohibidas, total_palabras)) {
            printf("Usuario: %s\n", comentarios[i].nombre);
            printf("Comentario: %s\n\n", comentarios[i].comentario);
            ofensivos++;
        }
    }

    printf("Total de comentarios ofensivos: %d\n", ofensivos);

    // Fin de mediciones
    clock_t fin_clock = clock();
    gettimeofday(&end_time, NULL);

    // Cálculos
    double tiempo_clock = (double)(fin_clock - inicio_clock) / CLOCKS_PER_SEC;

    double tiempo_sys = (end_time.tv_sec - start_time.tv_sec) +
                        (end_time.tv_usec - start_time.tv_usec) / 1000000.0;

    printf("\nTiempo total (clock): %.6f segundos\n", tiempo_clock);
    printf("Tiempo total (gettimeofday): %.6f segundos\n", tiempo_sys);

    return 0;
}
