#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <mpi.h>
#include <time.h>
#include <sys/time.h>

#define MAX_COMENTARIOS 1000
#define MAX_PALABRA 100
#define MAX_LINEA 1024
#define MAX_COM_LEN 512
#define MAX_NOMBRE 100
#define MAX_PALABRAS 100

typedef struct {
    int id_usuario;
    char nombre[MAX_NOMBRE];
    char comentario[MAX_COM_LEN];
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

int main(int argc, char *argv[]) {
    int rank, size;
    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    clock_t start_clock = clock();
    struct timeval start_sys, end_sys;
    gettimeofday(&start_sys, NULL);
    double start_mpi = MPI_Wtime();

    Comentario comentarios[MAX_COMENTARIOS];
    char palabras_prohibidas[MAX_PALABRAS][MAX_PALABRA];
    int total_comentarios = 0, total_palabras = 0;

    if (rank == 0) {
        total_comentarios = cargar_comentarios(comentarios);
        total_palabras = cargar_palabras_prohibidas(palabras_prohibidas);
    }

    MPI_Bcast(&total_palabras, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(palabras_prohibidas, MAX_PALABRAS * MAX_PALABRA, MPI_CHAR, 0, MPI_COMM_WORLD);
    MPI_Bcast(&total_comentarios, 1, MPI_INT, 0, MPI_COMM_WORLD);

    int por_proceso = total_comentarios / size;
    int resto = total_comentarios % size;
    int inicio = rank * por_proceso + (rank < resto ? rank : resto);
    int fin = inicio + por_proceso + (rank < resto ? 1 : 0);

    Comentario locales[MAX_COMENTARIOS];
    int cant_local = 0;

    if (rank == 0) {
        for (int i = 1; i < size; i++) {
            int i_inicio = i * por_proceso + (i < resto ? i : resto);
            int i_fin = i_inicio + por_proceso + (i < resto ? 1 : 0);
            int cant = i_fin - i_inicio;
            MPI_Send(&cant, 1, MPI_INT, i, 100, MPI_COMM_WORLD);
            MPI_Send(&comentarios[i_inicio], cant * sizeof(Comentario), MPI_BYTE, i, 101, MPI_COMM_WORLD);
        }
        for (int i = inicio; i < fin; i++) {
            locales[cant_local++] = comentarios[i];
        }
    } else {
        int cant = 0;
        MPI_Recv(&cant, 1, MPI_INT, 0, 100, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(locales, cant * sizeof(Comentario), MPI_BYTE, 0, 101, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        cant_local = cant;
    }

    double tiempo_inicio = MPI_Wtime();

    Comentario ofensivos[MAX_COMENTARIOS];
    int total_ofensivos = 0;
    for (int i = 0; i < cant_local; i++) {
        if (contiene_palabra_prohibida(locales[i].comentario, palabras_prohibidas, total_palabras)) {
            ofensivos[total_ofensivos++] = locales[i];
        }
    }

    double tiempo_fin = MPI_Wtime();

    printf("Proceso %d analizó desde %d hasta %d (%d comentarios) en %.6f segundos\n",
           rank, inicio, fin - 1, cant_local, tiempo_fin - tiempo_inicio);

    if (rank == 0) {
        printf("\nComentarios ofensivos detectados:\n\n");
        for (int i = 0; i < total_ofensivos; i++) {
            printf("Usuario: %s\nComentario: %s\n\n", ofensivos[i].nombre, ofensivos[i].comentario);
        }
        for (int src = 1; src < size; src++) {
            int cant = 0;
            MPI_Recv(&cant, 1, MPI_INT, src, 1, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            Comentario recibidos[MAX_COMENTARIOS];
            MPI_Recv(recibidos, cant * sizeof(Comentario), MPI_BYTE, src, 2, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            for (int i = 0; i < cant; i++) {
                printf("Usuario: %s\nComentario: %s\n\n", recibidos[i].nombre, recibidos[i].comentario);
            }
            total_ofensivos += cant;
        }

        double end_mpi = MPI_Wtime();
        gettimeofday(&end_sys, NULL);
        clock_t end_clock = clock();

        double tiempo_mpi = end_mpi - start_mpi;
        double tiempo_sys = (end_sys.tv_sec - start_sys.tv_sec) + (end_sys.tv_usec - start_sys.tv_usec) / 1000000.0;
        double tiempo_clock = (double)(end_clock - start_clock) / CLOCKS_PER_SEC;

        printf("Total de comentarios ofensivos: %d\n\n", total_ofensivos);
        printf("Tiempo total con MPI_Wtime:      %.6f segundos\n", tiempo_mpi);
        printf("Tiempo total con gettimeofday:  %.6f segundos\n", tiempo_sys);
        printf("Tiempo total con clock():       %.6f segundos\n", tiempo_clock);
    } else {
        MPI_Send(&total_ofensivos, 1, MPI_INT, 0, 1, MPI_COMM_WORLD);
        MPI_Send(ofensivos, total_ofensivos * sizeof(Comentario), MPI_BYTE, 0, 2, MPI_COMM_WORLD);
    }

    MPI_Finalize();
    return 0;
}
