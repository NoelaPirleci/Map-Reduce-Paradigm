#include <iostream>
#include <fstream>
#include <string>
#include <queue>
using namespace std;


// Structure for the mapper threads
struct MapperThreadInfo {
    queue<pair<string, int>> *queueFileNames;
    vector<pair<string, int>> *outputList;
    pthread_mutex_t mutexQueue;
    pthread_mutex_t mutexWritingOutput;
};

string transformWordForProcessing(string word) {
    // Transform the word to lowercase
    string newWord;
    for (char c : word) {
        // Check if the character is a letter
        if (isalpha(c)) { 
            newWord += tolower(c);
        }
    }
    return newWord;
}

// Mapper function
void *mapperFunc(void *arg) {
    
    cout << "Ajunge in functia de mapper!" << endl;

    struct MapperThreadInfo *mapperThreadInfo = (struct MapperThreadInfo *)arg;    
    pair<string, int> currentFile;
    
    // Reading files from the queue and processing them until the queue is empty
    while (true) {

        cout << "In while intra???" << endl;

        // Lock the mutex for reading from the queue
        pthread_mutex_lock(&mapperThreadInfo->mutexQueue);

        // Checking if the queue is empty or not so we can read the next file
        if (mapperThreadInfo->queueFileNames->empty()) {
            // Queue is empty => unlock the mutex
            pthread_mutex_unlock(&mapperThreadInfo->mutexQueue);
            break;
        } else {
            cout << "mai gaseste fisier de unde sa citeasca!" << endl;
            // Queue isn't empty yet => read the next file 
            currentFile = mapperThreadInfo->queueFileNames->front();
            mapperThreadInfo->queueFileNames->pop();
            pthread_mutex_unlock(&mapperThreadInfo->mutexQueue);
        }

        // After getting the file, we need to process it
        // First, we open the current file
        ifstream file(currentFile.first);
        // Verifying if the file is open
        if (!file.is_open()) {
            cout << "Error opening file!" << endl;
            return NULL;
        }

        // Reading word by word from the file and adding the words alongside 
        // the indexes to the output list
        string word;
        cout << "file: "  << currentFile.first << " " << currentFile.second << endl;

        while (file >> word) {
            // get the word to lowercase and remove the punctuation
            string newWord = transformWordForProcessing(word);

            // Lock the mutex for writing to the output list
            pthread_mutex_lock(&mapperThreadInfo->mutexWritingOutput);
            // Adding the word and the index only if the word is not on the list already
            bool alreadyOnTheList = false;
            for (int i = 0; i < mapperThreadInfo->outputList->size(); i++) {
                if (mapperThreadInfo->outputList->at(i).first == newWord) {
                    alreadyOnTheList = true;
                    break;
                } else {
                    alreadyOnTheList = false;
                }
            }
            if (!alreadyOnTheList) {
                pair<string, int> newPair = make_pair(newWord, currentFile.second);
                mapperThreadInfo->outputList->push_back(newPair);
                // print the new pair added
                cout << newPair.first << " " << newPair.second << endl;
            }
            
            // Unlock the mutex for writing to the output list
            pthread_mutex_unlock(&mapperThreadInfo->mutexWritingOutput);
        }

    }

    pthread_exit(NULL);
}


int main(int argc, char **argv)
{
    // First things first, we check if the input format is correct
    if (argc != 4) {
        cout << "Wrong input! Please use the correct format." << endl;
    }

    // Getting the number of mapper threads
    int numberOfMapperThreads = stoi(argv[1]);
    pthread_t mapperThreads[numberOfMapperThreads];

    // Getting the number of reducer threads
    int numberOfReducerThreads = stoi(argv[2]);
    pthread_t reducerThreads[numberOfReducerThreads];

    // Opening the input file
    ifstream inputFile(argv[3]);
    if (!inputFile.is_open()) {
        cout << "Error opening file!" << endl;
        return 1;
    }
    
    // Reading the number of files
    int numberOfFiles;
    inputFile >> numberOfFiles;

    // Reading the file names and storing them in a queue(every element is a pair of file name and index of the file)
    queue<pair<string, int>> *queueFileNames = new queue<pair<string, int>>;
    string fileName;
    for (int i = 0; i < numberOfFiles; i++) {
        inputFile >> fileName;
        queueFileNames->push(make_pair(fileName, i));
    }

    // print the queue
    /*for (int i = 0; i < numberOfFiles; i++) {
        cout << queueFileNames->front().first << " " << queueFileNames->front().second << endl;
        queueFileNames->pop();
    }*/
    
    // Closing the file
    inputFile.close();

    // Initialising the data for the mapper threads
    struct MapperThreadInfo mapperThreadInfo;
    mapperThreadInfo.queueFileNames = queueFileNames;
    mapperThreadInfo.outputList = new vector<pair<string, int>>;
    pthread_mutex_t mutexQueue;
    pthread_mutex_init(&mutexQueue, NULL);
    if (pthread_mutex_init(&mutexQueue, NULL) != 0) {
        cout << "Error when initialising the mutex for the queue!" << endl;
        exit(-1);
    }

    pthread_mutex_t mutexWritingOutput;
    pthread_mutex_init(&mutexWritingOutput, NULL);
    if (pthread_mutex_init(&mutexWritingOutput, NULL) != 0) {
        cout << "Error when initialising the mutex for writing to the output list!" << endl;
        exit(-1);
    }

    // Mapper threads logic
    for (int i = 0; i < numberOfMapperThreads; i++) {
        int ok = pthread_create(&mapperThreads[i], NULL, mapperFunc, &mapperThreadInfo);
        if (ok) {
            cout << "Error when creating the mapper thread!" << endl;
            exit(-1);
        }
    }

    for (int i = 0; i < numberOfMapperThreads; i++) {
        int ok = pthread_join(mapperThreads[i], NULL);
        if (ok) {
            cout << "Error when joining the mapper thread!" << endl;
            exit(-1);
        }
    }

    return 0;
}