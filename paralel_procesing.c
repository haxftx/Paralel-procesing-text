#include "mpi.h"
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define horror 0
#define comedy 1
#define fantasty 2
#define science 3
#define NR_PARAGRAF 500
#define LEN_PARAGRAF 900000
#define TAG 10
#define MASTER 0
#define NUM_THREADS 4

// datele partajate intre thread-uri
pthread_mutex_t f_mutex;
pthread_barrier_t f_barrier;
pthread_mutex_t g_mutex;
pthread_barrier_t g_barrier;
char *input;
int file_count;
char *f_text;
FILE *out;
char **text_lines;
long cores;
int tip_paragraf;
int f_break = 1;
int g_break = 1;

void add_final_size(char *file) {
    // adauga \n\n fisierului la final
    // afla dimensiunea fisierului
    FILE *fp = fopen(file, "a+");
    char buff[3];
    fseek(fp, 0, SEEK_END);
    int fsize = ftell(fp);
    rewind(fp);
    fseek(fp, fsize - 2, SEEK_SET);
    fread(buff, 1, 2, fp);
    if (strncmp(buff, "\n\n", 2) != 0) {
        fsize += 2;
        fprintf(fp, "\n\n");
    }
    file_count = fsize;
    fclose(fp);
}

int count_line(char *str) {
    // numarul de linii dintr-un string
    int count = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == '\n')
            count++;
    }
    return count;
}

char *create_output(char *in) {
    // screaza numele fisierului de output
    char *out = (char *)malloc(strlen(in) * sizeof(char));
    if (!out)
        exit(-1);
    strcpy(out, in);
        out[strlen(input) - 3] = 'o';
        out[strlen(input) - 2] = 'u';
        out[strlen(input) - 1] = 't';
    return out;
}

int is_cons_UP(char chr) {
    return (chr != 'A' && chr != 'E' && chr != 'I' && chr != 'O' && 
                             chr != 'U' &&chr >= 'A' && chr <= 'Z');
}

int is_cons_LOW(char chr) {
    return (chr != 'a' && chr != 'e' && chr != 'i' && chr != 'o' &&
                             chr != 'u' && chr >= 'a' && chr <= 'z');
}

char *f_horror(char *str) {
    // dubleaza consoanele fiecarui cuvant
    int pos = 0, len = strlen(str);
    char *aux = (char *)malloc(len * 2 * sizeof(char));
    if (!aux)
        exit(-1);

    for (int i = 0; i < len; i++) { // fiecare caracter
        if (is_cons_LOW(str[i])) { // dublez consoana
            aux[pos] = str[i];
            aux[pos + 1] = str[i];
            pos += 2;
        } else if (is_cons_UP(str[i])) { // dublez consoana cu lowercase
            aux[pos] = str[i];
            aux[pos + 1] = str[i] + 'a' - 'A';
            pos += 2;
        } else { // scriu caracterul
            aux[pos] = str[i];
            pos++;
        }
    }
    return aux;
}

void f_comedy(char *str) {
    // fiecare litera de pe pozitie para devine upercase
    int pos = 0, len = strlen(str);
    for(int i = 0 ; i < len; i++) {
        // daca e pozitie para si lowercase transform in upercase
        if (pos % 2 == 1 && str[i] >= 'a' && str[i] <= 'z') {
            str[i] -= 'a' - 'A';
        }
        pos++;
        if (str[i] == ' ' || str[i] == '\n')
            pos = 0;
    }
}

void f_fantasy(char *str) {
    // prima litera a fiecarui cuvant devine upercase
    int pos = 0, len = strlen(str);
    for(int i = 0 ; i < len; i++) {
        if (pos == 0 && str[i] >= 'a' && str[i] <= 'z') {
            // daca e prima pozitie si lowercase transform in upercase
            str[i] -= 'a' - 'A';
        }
        pos++;
        if (str[i] == ' ' || str[i] == '\n')
            pos = 0;
    }
}

void f_science(char *str) {
    // fiecare al 7-lea cuvant de pe linie este inversat
    int pos = 0, len = strlen(str);
    for(int i = 0; i < len; i++) {
        if (pos == 6) { // inversez cuvantul
            int tmp, j = i;
            while (j < len && str[j] != ' ' && str[j] != '\n')
                j++;
            j--;
            for (int k = 0; k < (j - i + 1) / 2; k++) {
                tmp = str[i + k];
                str[i + k] = str[j - k];
                str[j - k] = tmp;
            }
            i = j + 2;
            pos = 0;
            continue;   
        }
        if (str[i] == ' ')
            pos++;
        if (str[i] == '\n')
            pos = 0;
    }
}

void procesare_text(char **p_str, int type) {
    // proceseaza textul in dependeta de tipul paragrafului
    char *str = *p_str;
    if (strlen(str) <= 1)
        return;
    if (type == horror) { // horror
        *p_str = f_horror(str);
    } else if (type == comedy) { // comedy
        f_comedy(str);
    } else if (type == fantasty) { // fantasy
        f_fantasy(str);
    } else if (type == science) { // science-fiction
        f_science(str);
    }
}

void split_paragraf(char *str, int *lines_thread) {
    // imparte un paragraf dupa numarul de linii per thread
    for (int i = 1; i < cores; i++) { // fiecare thread
        text_lines[i] = str;
        int characters = 0, line = 0;
        while (line < lines_thread[i]) { // linile atribuite
            while (str[characters] != '\n' && str[characters] != '\0') {
                characters++;
            }
            line++;
            if (str[characters] != '\0')
                characters++;
        }
        if (text_lines[i][characters] != '\0' && characters != 0)
            text_lines[i][characters - 1] = '\0';
        str += characters;
    }
}

char *merge_paragraf(int *size) {
    // creeaza paragraful din liniile procesate de thread-uri
    char type[17], *buff;
    *size = cores - 1;
    if (tip_paragraf == horror) {
        strcpy(type, "horror\n");
        *size += 7;
    } else if (tip_paragraf == comedy) {
        strcpy(type, "comedy\n");
        *size += 7;
    } else if (tip_paragraf == fantasty) {
        strcpy(type, "fantasy\n");
        *size += 8;
    } else if (tip_paragraf == science) {
        strcpy(type, "science-fiction\n");
        *size += 16;
    } // calculez dimensiunea paragrafului
    for (int i = 1; i < cores; i++) {
        *size += strlen(text_lines[i]);
    }
    buff = (char *)malloc((*size) * sizeof(char));
    if (!buff)
        exit(-1);
    strcat(buff, type); // creez paragraul
    for (int i = 1; i < cores; i++) {
        strcat(buff, text_lines[i]);
        if (strlen(text_lines[i]) > 0)
            strcat(buff, "\n\0");
    }

    return buff;
}

void *f(void *arg) {
    // funtia thread-urilor din Master
    int id = *(int *)arg, size = 0, start = 0, end = file_count / NUM_THREADS;
    char *paragraf;
    int line = 0, read_len = file_count / NUM_THREADS, offset = id * read_len;
    // citesc
    if (id == 3) { // ultimul thread ia liniile neimpartite
        read_len = file_count - offset;
    }
    FILE *fp = fopen(input, "r");
    fseek(fp, offset, SEEK_SET);
    char *read_buff = (char *)malloc(read_len * sizeof(char));
    if (!read_buff)
        exit(-1);
    fread(read_buff, 1, read_len, fp);
    fclose(fp);
    // concatenez textul
    for (int i = 0 ; i < NUM_THREADS; i++) {
        pthread_barrier_wait(&f_barrier);
        if (i == id) {
            strcat(f_text, read_buff);
            free(read_buff);
        }
    }
    pthread_barrier_wait(&f_barrier);
    char *text = (char *)malloc((file_count + 2) * sizeof(char));
    if (!text)
        exit(-1);
    strcpy(text, f_text);
    text += file_count / NUM_THREADS * id;
    // ajung la primul paragraf din textul ce trebuie sa-l procesez
    if (strncmp(text, "horror\n", 6) != 0 && strncmp(text, "comedy\n", 6) != 0
                        && strncmp(text, "fantasy\n", 7) != 0  && 
                                strncmp(text, "science-fiction\n", 15) != 0) {
        paragraf = strstr(text, "\n\n");
        size = strlen(text) - strlen(paragraf);
        text += size + 2; start += size + 2;
    } // paragrafele ce le voi trimite
    char **paragrafe = (char **)malloc(NR_PARAGRAF * sizeof(char *));
    if (!paragrafe)
            exit(-1);
    for (int i = 0; i < NR_PARAGRAF; i++) {
        paragrafe[i] = (char *)malloc(LEN_PARAGRAF  * sizeof(char));
        if (!paragrafe[i])
            exit(-1);
    } // salvez paragrafele
    while (start < end - 1) {
        if (strlen(text) == 0)
            break;
        paragraf = strstr(text, "\n\n");
        size = strlen(text) - strlen(paragraf);
        paragraf = text;
        paragraf[size] = '\0';
        text += size + 2; start += size + 2;
        strcpy(paragrafe[line++], paragraf);
        while (text[0] == '\n') {
            text++; start++;
        }
    }
    MPI_Status s;
    // trimite si primeste paragrafele
    for (int i = 0; i < line; i++) { // pentru toate paragrafele
        size = strlen(paragrafe[i]);
        MPI_Send(&size, 1, MPI_INT, id + 1, TAG, MPI_COMM_WORLD);
        MPI_Send(paragrafe[i], size, MPI_CHAR, id + 1, TAG, MPI_COMM_WORLD);
        MPI_Recv(&size, 1, MPI_INT, id + 1 ,TAG, MPI_COMM_WORLD, &s);
        if (size > LEN_PARAGRAF)
            paragrafe[i] = (char *)realloc(paragrafe[i], size);
        MPI_Recv(paragrafe[i], size, MPI_CHAR, id+1 ,TAG, MPI_COMM_WORLD, &s);
    }
    size = 0; // trimit finalizarea comunicarii
    MPI_Send(&size, 1, MPI_INT, id + 1, TAG, MPI_COMM_WORLD);
    // scriu paragrafele in fisier
    for (int i = 0 ; i < NUM_THREADS; i++) {
        pthread_barrier_wait(&f_barrier);
        if (i == id) {
            for (int j = 0 ; j < line; j++) {
                fprintf(out, "%s\n", paragrafe[j]);
                free(paragrafe[j]);
            }
        }
    }
    free(paragrafe);
    pthread_exit(NULL);
}

void *g(void *arg) {
    // functia thread-urilor din Workeri
    int id = *((int *)arg), n_20l, size, lines, res_lines, lines_thread[cores];
    MPI_Status s;
    char *buff;
    
    while (g_break) { // cat primesc paragrafe
        pthread_barrier_wait(&g_barrier);
        if (id == 0) { // primeste si impart paragraful
            MPI_Recv(&size, 1, MPI_INT, MASTER ,TAG, MPI_COMM_WORLD, &s);
            if (size == 0) {
                g_break = 0;
            } else {
                size++;
                buff = (char *)malloc(size * sizeof(char));
                if (!buff)
                    exit(-1);
                MPI_Recv(buff, size, MPI_CHAR, MASTER, TAG, MPI_COMM_WORLD,&s);
                lines = count_line(buff);
                n_20l = (lines / 20) / (cores - 1) ;
                res_lines = lines - (cores - 1) * n_20l;
                for (int i = 1; i < cores; i++) { // liniile pentru thread
                    lines_thread[i] = 20 * n_20l;
                    if (res_lines >= 20) {
                        lines_thread[i] += 20;
                        res_lines -= 20;
                    } else {
                        lines_thread[i] += res_lines;
                        res_lines = 0;
                    }
                } // tipul paragrafului
                if (strncmp(buff, "horror\n", 6) == 0) {
                    tip_paragraf = horror;
                    buff += 7;
                } else if (strncmp(buff, "comedy\n", 6) == 0) {
                    tip_paragraf = comedy;
                    buff += 7;
                } else if (strncmp(buff, "fantasy\n", 7) == 0) {
                    tip_paragraf = fantasty;
                    buff += 8;
                } else if (strncmp(buff, "science-fiction\n", 15) == 0) {
                    tip_paragraf = science;
                    buff += 16;
                }
                split_paragraf(buff, lines_thread);
            }
        }
        pthread_barrier_wait(&g_barrier); // astept sa primesc paragraful
        if (id != 0) { // procesez liniile din paragraf
            procesare_text(&text_lines[id], tip_paragraf);
        } // astept sa se proceseze tot paragraful
        pthread_barrier_wait(&g_barrier);
        if (id == 0 && g_break) { // concatenez si trimit paragraful
            char *n_buff = merge_paragraf(&size);
            MPI_Send(&size, 1, MPI_INT, MASTER, TAG, MPI_COMM_WORLD);
            MPI_Send(n_buff, size, MPI_CHAR, MASTER, TAG, MPI_COMM_WORLD);
        }
        pthread_barrier_wait(&g_barrier);
    }
    pthread_exit(NULL);
}

int main (int argc, char *argv[]) {
    // initializez MPI
    int  numtasks, rank, provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_size(MPI_COMM_WORLD, &numtasks);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    if (rank == MASTER) { // Master
        add_final_size(argv[1]); // adaug \n\n si aflu dimenisunea fisierului
        f_text = (char *)malloc((file_count + 2) * sizeof(char)); // tot textul
        if (!f_text)
            exit(-1);
        memset(f_text, 0, (file_count + 2));
        input = argv[1];
        out = fopen(create_output(input), "w");
        // initializez datele pentru thread-uri
        pthread_barrier_init(&f_barrier, NULL, NUM_THREADS);
        pthread_mutex_init(&f_mutex, NULL);
        pthread_t threads[NUM_THREADS];
        int id, arguments[NUM_THREADS];
        void *status;
        // creez thread-urile
        for (id = 0; id < NUM_THREADS; id++) {
            arguments[id] = id;
            pthread_create(&threads[id], NULL, f, (void *)&arguments[id]);
        } // astept thread-urile
        for (id = 0; id < NUM_THREADS; id++) {
            pthread_join(threads[id], &status);
        } // eliberez memoria alocata
        free(f_text);
        fclose(out);
        pthread_mutex_destroy(&f_mutex);
        pthread_barrier_destroy(&f_barrier);

    } else { // Workeri
        cores = sysconf(_SC_NPROCESSORS_CONF); // numarul de thred-uri de pe PC
        // initializez datele pentru thread-uri
        pthread_t threads[cores];
        int id, arguments[cores];
        void *status;
        pthread_barrier_init(&g_barrier, NULL, cores);
        pthread_mutex_init(&g_mutex, NULL);
        text_lines = (char **)malloc(cores * sizeof(char *));
        if (!text_lines)
                exit(-1);
        // creez thread-urile
        for (id = 0; id < cores; id++) {
            text_lines[id] = NULL;
            arguments[id] = id;
            pthread_create(&threads[id], NULL, g, (void *)&arguments[id]);
        } // astept thread-urile
        for (id = 0; id < cores; id++) {
            pthread_join(threads[id], &status);
        } // eliberez memoria alocata
        free(text_lines);
        pthread_mutex_destroy(&g_mutex);
        pthread_barrier_destroy(&g_barrier);
        
    }
    MPI_Finalize();
}
