#include <iostream>
#include <fstream>
#include <string>
#include <queue>
#include <algorithm>
#include <map>
using namespace std;


// Structure for the mapper threads
struct MapperThreadInfo {
    int id;
    queue<pair<string, int>> *queueFileNames;
    map<string, vector<int>> *outputList;
    pthread_mutex_t *mutexQueue;
    pthread_mutex_t *mutexWritingOutput;
    pthread_barrier_t *barrier;
};


// Structure for the reducer threads
struct ReducerThreadInfo {
    map<string, vector<int>>  *inputListFromMappers;
    vector<pair<string, int>> *finalList;
    pthread_mutex_t *mutexReadingInput;
    pthread_mutex_t *mutexWritingFinalList;
    pthread_mutex_t *mutexWritingFiles;
    pthread_barrier_t *barrier;
    pthread_barrier_t *barrierWritingInFiles;
    vector<pair<string, vector<int>>> *wordsAndListOfIndexes[26];
    vector<char> *alphabetForWordsAndListOfIndexes;
    vector<char> *alphabetForMergingWords;
    vector<char> *alphabetForWritingInFiles;
};


string transformWordForProcessing(string word) {
    // Transform the word to lowercase
    string newWord;
    for (char c : word) {
        // Check if the character is a letter
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) { 
            newWord += tolower(c);
        } else {
            // if the character is not a letter, we will remove it
            newWord += "";
        }
    }
    return newWord;
}

// Mapper function
void *mapperFunc(void *arg) {
    
    struct MapperThreadInfo *mapperThreadInfo = (struct MapperThreadInfo *)arg;    
    pair<string, int> currentFile;
    
    // Reading files from the queue and processing them until the queue is empty
    while (true) {

        // Lock the mutex for reading from the queue
        pthread_mutex_lock(mapperThreadInfo->mutexQueue);

        // Checking if the queue is empty or not so we can read the next file
        if (mapperThreadInfo->queueFileNames->empty()) {
            // Queue is empty => unlock the mutex
            pthread_mutex_unlock(mapperThreadInfo->mutexQueue);
            break;
        } else {
            // Queue isn't empty yet => read the next file 
            currentFile = mapperThreadInfo->queueFileNames->front();
            mapperThreadInfo->queueFileNames->pop();
            pthread_mutex_unlock(mapperThreadInfo->mutexQueue);
        }

        // After getting the file, we need to process it
        // First, we open the current file
        ifstream file(currentFile.first);
        // Verifying if the file is open
        if (!file.is_open()) {
            cout << "Error opening file!" << endl;
            return NULL;
        }

        // Getting a vector for each file to store the words and the indexes of the files in which they appear
        vector<pair<string, int>> currentPairs;

        // Reading the words from the file
        string word;
        while (file >> word) {
            // get the word to lowercase and remove the punctuation
            string newWord = transformWordForProcessing(word);

            // Adding the word and the index only if the word is not on the list already
            bool alreadyOnTheList = false;
            for (const auto& pair : currentPairs) {
                if (pair.first == newWord && pair.second == currentFile.second) {
                    alreadyOnTheList = true;
                    break;
                } else {
                    alreadyOnTheList = false;
                }
            }
            if (!alreadyOnTheList) {
                pair<string, int> newPair = make_pair(newWord, currentFile.second);
                currentPairs.push_back(newPair);
            }
        }

        // Lock the mutex for writing to the output list
        pthread_mutex_lock(mapperThreadInfo->mutexWritingOutput);
        for (const auto& pair : currentPairs) {
            // Check if the word is already in the output list
            if (mapperThreadInfo->outputList->find(pair.first) == mapperThreadInfo->outputList->end()) {
                // If the word is not in the list, we add it
                vector<int> indexes;
                indexes.push_back(pair.second);
                mapperThreadInfo->outputList->insert(make_pair(pair.first, indexes));
            } else {
                // If the word is already in the list, we add the index to the list of indexes
                (*mapperThreadInfo->outputList)[pair.first].push_back(pair.second);
            }
        }
        // Unlock the mutex for writing to the output list
        pthread_mutex_unlock(mapperThreadInfo->mutexWritingOutput);

    }

    // Before starting the reducer threads, we need to wait for all the mapper threads to finish their work
    pthread_barrier_wait(mapperThreadInfo->barrier);

    pthread_exit(NULL);
}


void *reducerFunc(void *arg) {

    struct ReducerThreadInfo *reducerThreadInfo = (struct ReducerThreadInfo *)arg;

    pthread_barrier_wait(reducerThreadInfo->barrier);
    
    // Put each word in the vector of words and indexes according to the first letter of the word

    // Lock the mutex for reading the input list
    pthread_mutex_lock(reducerThreadInfo->mutexReadingInput);

    // We will go through the alphabet and add the pairs(words+indexes) in the vector of words and indexes
    while (!reducerThreadInfo->alphabetForWordsAndListOfIndexes->empty()) {
        char letter = reducerThreadInfo->alphabetForWordsAndListOfIndexes->back();
        reducerThreadInfo->alphabetForWordsAndListOfIndexes->pop_back();
        pthread_mutex_unlock(reducerThreadInfo->mutexReadingInput);

        int currentIndex = letter - 'a';

        // For every pair from the list received from the mappers, we check if the first letter is the same as the current letter
        for (pair<string, vector<int>> word : *reducerThreadInfo->inputListFromMappers) {
            // If the first letter is the same, we add the pair in the vector of words and indexes
            if (word.first[0] == letter) {
                pair<string, vector<int>> currentPair = make_pair(word.first, vector<int>{word.second});
                reducerThreadInfo->wordsAndListOfIndexes[currentIndex]->push_back(currentPair);
            } else if (word.first[0] > letter) {
                // Else, if the first letter is greater than the current letter, we break the loop
                break;
            }
        }
        pthread_mutex_lock(reducerThreadInfo->mutexReadingInput);
    }

    pthread_mutex_unlock(reducerThreadInfo->mutexReadingInput);

    // Now we have each pair of word and index in the vector of words and indexes (according to its letter)
    // Barrier to make sure each thread has finished adding the words in the vector
    pthread_barrier_wait(reducerThreadInfo->barrierWritingInFiles);

    // Merge the words that are the same
    pthread_mutex_lock(reducerThreadInfo->mutexReadingInput);
    while(!reducerThreadInfo->alphabetForMergingWords->empty()) {

        char letter = reducerThreadInfo->alphabetForMergingWords->back();
        reducerThreadInfo->alphabetForMergingWords->pop_back();
        pthread_mutex_unlock(reducerThreadInfo->mutexReadingInput);

        int index = letter - 'a';

        // If two words in the vector have the same word, but are from different files, we need to merge them
        for (unsigned long int i = 0; i < reducerThreadInfo->wordsAndListOfIndexes[index]->size(); i++) {
            for (unsigned long int j = i + 1; j < reducerThreadInfo->wordsAndListOfIndexes[index]->size(); j++) {
                if ((*reducerThreadInfo->wordsAndListOfIndexes[index])[i].first == (*reducerThreadInfo->wordsAndListOfIndexes[index])[j].first) {
                    for (unsigned long int k = 0; k < (*reducerThreadInfo->wordsAndListOfIndexes[index])[j].second.size(); k++) {
                        (*reducerThreadInfo->wordsAndListOfIndexes[index])[i].second.push_back((*reducerThreadInfo->wordsAndListOfIndexes[index])[j].second[k]);
                    }
                    (*reducerThreadInfo->wordsAndListOfIndexes[index]).erase((*reducerThreadInfo->wordsAndListOfIndexes[index]).begin() + j);
                    j--;
                }
            }
            // Also, we need to sort the indexes from each word in ascending order
            sort((*reducerThreadInfo->wordsAndListOfIndexes[index])[i].second.begin(), (*reducerThreadInfo->wordsAndListOfIndexes[index])[i].second.end());
        }

        // Sort the words in the vector - first by the length of the list of indexes and then lexicographically
        sort(reducerThreadInfo->wordsAndListOfIndexes[index]->begin(), reducerThreadInfo->wordsAndListOfIndexes[index]->end(), 
            [](const pair<string, vector<int>>& a, const pair<string, vector<int>>& b) {
                if (a.second.size() == b.second.size()) {
                    return a.first < b.first;
                }
                return a.second.size() > b.second.size();
            });

        pthread_mutex_lock(reducerThreadInfo->mutexReadingInput);
    }

    pthread_mutex_unlock(reducerThreadInfo->mutexReadingInput);

    // Before writing in the files, we need to wait for all the reducer threads to finish their work
    pthread_barrier_wait(reducerThreadInfo->barrierWritingInFiles);

    // Lock mutex for writing in the files
    pthread_mutex_lock(reducerThreadInfo->mutexWritingFiles);

    // Write the words in the files
    while(!reducerThreadInfo->alphabetForWritingInFiles->empty()) {
        char letter = reducerThreadInfo->alphabetForWritingInFiles->back();
        reducerThreadInfo->alphabetForWritingInFiles->pop_back();
        pthread_mutex_unlock(reducerThreadInfo->mutexWritingFiles);

        int index = letter - 'a';

        string filename = string(1, letter) + ".txt";
        ofstream outputFile(filename);

        for (const auto& word : *reducerThreadInfo->wordsAndListOfIndexes[index]) {
            outputFile << word.first << ":[";
            for (const auto& index : word.second) {
                if (index == word.second.back()) {
                    outputFile << index;
                    break;
                }
                outputFile << index << " ";
            }
            if (word == (*reducerThreadInfo->wordsAndListOfIndexes[index]).back()) {
                outputFile << "]";
                break;
            }
            outputFile << "]" << endl;
        }
        outputFile.close();
        pthread_mutex_lock(reducerThreadInfo->mutexWritingFiles);
    }
    pthread_mutex_unlock(reducerThreadInfo->mutexWritingFiles);
    
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
    for (int i = 1; i <= numberOfFiles; i++) {
        inputFile >> fileName;
        queueFileNames->push(make_pair(fileName, i));
    }
    
    // Closing the file
    inputFile.close();

    // Initialising the data for the mapper threads
    struct MapperThreadInfo mapperThreadInfo;
    mapperThreadInfo.queueFileNames = queueFileNames;
    mapperThreadInfo.outputList = new map<string, vector<int>>;
    pthread_mutex_t mutexQueue;
    pthread_mutex_init(&mutexQueue, NULL);
    if (pthread_mutex_init(&mutexQueue, NULL) != 0) {
        cout << "Error when initialising the mutex for the queue!" << endl;
        exit(-1);
    }
    mapperThreadInfo.mutexQueue = &mutexQueue;

    pthread_mutex_t mutexWritingOutput;
    pthread_mutex_init(&mutexWritingOutput, NULL);
    if (pthread_mutex_init(&mutexWritingOutput, NULL) != 0) {
        cout << "Error when initialising the mutex for writing to the output list!" << endl;
        exit(-1);
    }
    mapperThreadInfo.mutexWritingOutput = &mutexWritingOutput;

    // Creating the barrier for the mapper threads
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, numberOfMapperThreads + numberOfReducerThreads);
    mapperThreadInfo.barrier = &barrier;


    // Initialising the data for the reducer threads
    struct ReducerThreadInfo reducerThreadInfo;
    reducerThreadInfo.inputListFromMappers = mapperThreadInfo.outputList;
    reducerThreadInfo.finalList = new vector<pair<string, int>>;
    pthread_mutex_t mutexReadingInput;
    pthread_mutex_init(&mutexReadingInput, NULL);
    if (pthread_mutex_init(&mutexReadingInput, NULL) != 0) {
        cout << "Error when initialising the mutex for reading the input list!" << endl;
        exit(-1);
    }
    reducerThreadInfo.mutexReadingInput = &mutexReadingInput;

    pthread_mutex_t mutexWritingOutputReducer;
    pthread_mutex_init(&mutexWritingOutputReducer, NULL);
    if (pthread_mutex_init(&mutexWritingOutputReducer, NULL) != 0) {
        cout << "Error when initialising the mutex for writing to the final list!" << endl;
        exit(-1);
    }
    reducerThreadInfo.mutexWritingFinalList = &mutexWritingOutputReducer;

    pthread_mutex_t mutexWritingFiles;
    pthread_mutex_init(&mutexWritingFiles, NULL);
    if (pthread_mutex_init(&mutexWritingFiles, NULL) != 0) {
        cout << "Error when initialising the mutex for writing in the files!" << endl;
        exit(-1);
    }
    reducerThreadInfo.mutexWritingFiles = &mutexWritingFiles;

    // Creating the barrier for the reducer threads
    reducerThreadInfo.barrier = &barrier;

    // Creating the barrier for writing in files
    pthread_barrier_t barrierWritingInFiles;
    pthread_barrier_init(&barrierWritingInFiles, NULL, numberOfReducerThreads);
    reducerThreadInfo.barrierWritingInFiles = &barrierWritingInFiles;

    // Structures used for the reducer threads in various operations
    vector<pair<string, vector<int>>> wordsAndListOfIndexes[26];
    for (int i = 0; i < 26; i++) {
        reducerThreadInfo.wordsAndListOfIndexes[i] = &wordsAndListOfIndexes[i];
    }
    
    // Alphabet for words and list of indexes
    vector<char> alphabetForWordsAndListOfIndexes;
    for (char c = 'a'; c <= 'z'; c++) {
        alphabetForWordsAndListOfIndexes.push_back(c);
    }
    reducerThreadInfo.alphabetForWordsAndListOfIndexes = &alphabetForWordsAndListOfIndexes;

    // Alphabet for merging the words
    vector<char> alphabetForMergingWords;
    for (char c = 'z'; c >= 'a'; c--) {
        alphabetForMergingWords.push_back(c);
    }
    reducerThreadInfo.alphabetForMergingWords = &alphabetForMergingWords;

    // Alphabet for writing in files
    vector<char> alphabetForWritingInFiles;
    for (char c = 'a'; c <= 'z'; c++) {
        alphabetForWritingInFiles.push_back(c);
    }
    reducerThreadInfo.alphabetForWritingInFiles = &alphabetForWritingInFiles;

    // Creating the threads
    for (int i = 0; i < numberOfMapperThreads + numberOfReducerThreads; i++) {
        // Creating the mapper threads
        if (i < numberOfMapperThreads) {
            mapperThreadInfo.id = i;
            int ok = pthread_create(&mapperThreads[i], NULL, mapperFunc, &mapperThreadInfo);
            if (ok) {
                cout << "Error when creating the mapper thread!" << endl;
                exit(-1);
            }
        } else {
            // Creating the reducer threads
            int ok = pthread_create(&reducerThreads[i - numberOfMapperThreads], NULL, reducerFunc, &reducerThreadInfo);
            if (ok) {
                cout << "Error when creating the reducer thread!" << endl;
                exit(-1);
            }
        }
        
    }
    
    // Joining the threads
    for (int i = 0; i < numberOfMapperThreads + numberOfReducerThreads; i++) {
        if (i < numberOfMapperThreads) {
            int ok = pthread_join(mapperThreads[i], NULL);
            if (ok) {
                cout << "Error when joining the mapper thread!" << endl;
                exit(-1);
            }
        } else {
            int ok = pthread_join(reducerThreads[i - numberOfMapperThreads], NULL);
            if (ok) {
                cout << "Error when joining the reducer thread!" << endl;
                exit(-1);
            }
        }
    }

    // Destroying the mutexes
    pthread_mutex_destroy(&mutexQueue);
    pthread_mutex_destroy(&mutexWritingOutput);
    pthread_mutex_destroy(&mutexReadingInput);
    pthread_mutex_destroy(&mutexWritingOutputReducer);
    pthread_mutex_destroy(&mutexWritingFiles);

    // Destroying the barriers
    pthread_barrier_destroy(&barrier);
    pthread_barrier_destroy(&barrierWritingInFiles);
    
    return 0;
}