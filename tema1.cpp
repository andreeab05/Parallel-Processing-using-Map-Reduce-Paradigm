#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <pthread.h>
#include <math.h>
#include <string>
#include <cstring>
#include <vector>
#include <unordered_set>
using namespace std;

// Structura de date trimisa ca parametru functiilor de thread
// pentru Mappers
typedef struct MapThreadArgs{
    int threadID;
    char** mapFiles;
    vector<unordered_set<int>> perfectNumbers;
    int *currentFile;
    int noFiles;
    int noExponents;
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
}MapThreadArgs;

// Structura de date trimisa ca parametru functiilor de thread
// pentru Reducers
typedef struct ReduceThreadArgs{
    int threadID;
    int noMappers;
    vector<unordered_set<int>*> lists;
    pthread_mutex_t *mutex;
    pthread_barrier_t *barrier;
}ReduceThreadArgs;

// Functia isPower primeste ca parametrii un numar si un exponent
// si folosind binary search va cauta o baza astfel incat
// baza ^ exponent = numar. Daca nu este gasit un astfel de numar,
// functia va returna false
bool isPower(int number, int exponent){
    int left = 2;
    int right = number / 2;
    int middle;
    while (left <= right) {
        middle = (left + right) / 2;
        if (pow(middle, exponent) == number) {
            return true;
        }
        
        if (pow(middle, exponent) > number) {
            right = middle - 1;
        }
        else {
            left = middle + 1;
        }
    }
    return false;
}

void *MapFunction(void *arg)
{
    MapThreadArgs *args = (MapThreadArgs*) arg;
    FILE *fp;
    char fileLine[1000];
    char fileName[100];
    int number;

    while (true) {
        // Deschidere fisier curent
        pthread_mutex_lock(args->mutex);
        if ((*args->currentFile) < args->noFiles) {
            int fileToRead = *args->currentFile;
            (*args->currentFile)++;
            pthread_mutex_unlock(args->mutex);
            strcpy(fileName, args->mapFiles[(fileToRead)]);
            fp = fopen(fileName, "r");
            if (fp == NULL)
                printf("Nu a reusit sa deschida fisierul!\n");
            fgets(fileLine, 1000, fp);
            while (fgets(fileLine, 1000, fp)) {
                number = atoi(fileLine);
                // Daca numarul citit este 1, acesta este adaugat direct
                // in toate seturile, deoarece 1 este putere perfecta pentru
                // orice exponent
                if(number == 1) {
                    for(int  i = 0; i <= args->noExponents - 2; i++){
                        args->perfectNumbers[i].insert(number);
                    }
                    continue;
                }
                for(int i = 2 ; i <= args->noExponents ; i++) {
                    bool exp = isPower(number, i);
                    if(exp == true){
                        args->perfectNumbers[i - 2].insert(number);
                    }
                }
            }
            fclose(fp);
        }
        else {
            pthread_mutex_unlock(args->mutex);
            break;
        } 
    }
    pthread_barrier_wait(args->barrier);
    pthread_exit(NULL);
}


void *ReduceFunction(void *arg)
{
	ReduceThreadArgs *args = (ReduceThreadArgs *)arg;
    pthread_barrier_wait(args->barrier);
    ofstream f;
    unordered_set<int> uniquePerfectNumbers;
    for(int i = 0 ; i < args->noMappers ; i++) {
        for(auto &it : *args->lists[i]) {
            uniquePerfectNumbers.insert(it);
        }
    }

    const string outPart = "out";
    string fileNumber = to_string(args->threadID + 2);
    string fileName = outPart + fileNumber + ".txt";
    f.open(fileName);
    f << uniquePerfectNumbers.size();
    f.close();
    pthread_exit(NULL);
}

int main(int argc, char **argv)
{
	int i, r;
    int noMappers, noReducers, noFiles, noThreads;
    int currentFile = 0;
    noMappers = atoi(argv[1]);
    noReducers = atoi(argv[2]);
    int noExponents = noReducers + 1;
    FILE *fp = fopen(argv[3], "r");
    char** filesToRead;
    char filename[1000];
    pthread_t *threads = (pthread_t*)malloc(sizeof(pthread_t) * (noMappers + noReducers));
    MapThreadArgs *argsM = (MapThreadArgs*) malloc(sizeof(MapThreadArgs) * noMappers);
    ReduceThreadArgs *argsR = (ReduceThreadArgs*) malloc(sizeof(ReduceThreadArgs) * noReducers);
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, noMappers + noReducers);

    void *status;

    // Deschiderea, citirea si inchiderea fisierului de intrare
    if (fp == NULL) {
        printf("Unable to open file!\n");
        return 1;
    }

    fgets(filename, 1000, fp);
    noFiles = atoi(filename);
    filesToRead = (char**)malloc(sizeof(const char*) * noFiles);

    i = 0;
    for (int j = 1 ; j <= noFiles ; j++) {
        fgets(filename, 1000, fp);
        // Citirea fisierelor de intrare din fisierul principal test.txt
        // si eliminarea newline-ului din numele fisierului, acolo unde exista
        filesToRead[i] = (char*)malloc(sizeof(char) * strlen(filename) + 1);
        if (strchr(filename, '\n') != NULL) {
            strncpy(filesToRead[i], filename, strlen(filename) - 1);
        }
        else {
            strncpy(filesToRead[i], filename, strlen(filename));
        }
        i++;
    }
    fclose(fp);

    noThreads = noMappers + noReducers;

    // Crearea threadurilor Mapper si Reducer
	for (i = 0; i < noThreads; i++) {
        // Creare Mappers
        if (i < noMappers) {
            argsM[i].threadID = i;
            argsM[i].mapFiles = filesToRead;
            argsM[i].noFiles = noFiles;
            argsM[i].currentFile = &currentFile;
            argsM[i].noExponents = noExponents;
            argsM[i].mutex = &mutex;
            argsM[i].barrier = &barrier;
            vector<unordered_set<int>> aux;
            for(int i = 0 ; i < noReducers ; i++) {
                aux.push_back(unordered_set<int>());
            }
            argsM[i].perfectNumbers = aux;
		    r = pthread_create(&threads[i], NULL, MapFunction, &argsM[i]);
            
            if (r) {
			    printf("Eroare la crearea thread-ului %d\n", i);
			    exit(-1);
		    }
        }
        // Creare Reducers
        if (i >= noMappers) {
            int threadID = i - noMappers;
            argsR[threadID].threadID = threadID;
            argsR[threadID].lists = vector<unordered_set<int>*>(noMappers);
            for(int j = 0 ; j < noMappers ; j++) {
                argsR[threadID].lists[j] = &argsM[j].perfectNumbers[threadID];
            }
            argsR[threadID].mutex = &mutex;
            argsR[threadID].noMappers = noMappers;
            argsR[threadID].barrier = &barrier;
            r = pthread_create(&threads[i], NULL, ReduceFunction, &argsR[threadID]);
            
            if (r) {
			    printf("Eroare la crearea thread-ului %d\n", i);
			    exit(-1);
		    }
        }
	}

	for (i = 0; i < noThreads; i++) {
		r = pthread_join(threads[i], &status);

		if (r) {
			printf("Eroare la asteptarea thread-ului %d\n", i);
			exit(-1);
		}
	}
    pthread_mutex_destroy(&mutex);
	pthread_barrier_destroy(&barrier);
	return 0;
}
