# Inversed Index using Map-Reduce Paradigm
## Pirleci Noela-Elena 335CA

### Idea of the project

The idea of the project consists of calculating the inversed index for a bunch of files, using the *Map-Reduce Paradigm*.

### Implementation

I start by checking the input given, because if the command for running the program it's wrong, also the parameters will be wrong. From the command, I extract the number of Mapper threads, the number of Reducer threads and the input file from which I will read the name of the files on which I'll operate. After getting these parameters, I open the input file and I put all the names of the files on a queue, that will help me later. After doing this, the **Mapper and Reducer** implementation starts. I create and join both the *Mapper* and *Reducer* threads in main, in the same loop(for every operation).

### Mapper Threads Structure and Implementation

For the Mapper threads I chose to create a structure looking like this:
```cpp
// Structure for the mapper threads
struct MapperThreadInfo {
    queue<pair<string, int>> *queueFileNames;
    map<string, vector<int>> *outputList;
    pthread_mutex_t *mutexQueue;
    pthread_mutex_t *mutexWritingOutput;
    pthread_barrier_t *barrier;
};
```
1. Using a *queue of pairs between strings and ints* to remember the name of the files from the given input file and the indexes for each file.
2. Using a *map* to store the result of the Mapper Threads.
3. Using a *mutex* for helping with reading and extracting from the queue of files.
4. Using a *mutex* to write to the result list.
5. Using a *barrier* to make sure that all the Mapper threads have finished their work before moving on to the next step.

After creating the Mapper threads, they start their work in the *mapperFunc*. To be honest, I didn't really know how to manage to jobs between the threads at first, so I thought that it'll be a good idea to give every thread a job and once he's done it, he takes another one, if there are more available. 

So, using the queue of files from the structure, while there are still files waiting to be processed from that queue, threads will get them and will *pop* them from the queue. Of course, being a shared resoruce(the queue), I used a mutex to read the files, because I wouldn't want 2 threads or more trying to access the same thing. 
After getting a file, it is opened and then the thread starts *his job*. 

In the first place, I create a vector of pairs between strings and ints for each file in which I store the words and the indexes of the files in which they appear. I read from the file word by word and I transform every word to lowercase and I remove the punctuation. After doing this, I check if the word I got now is on the list already: if it is, I don't add it again; if it isn't, I make a new pair consisting of the word and the index of the file and I add it to the list.

After finishing adding all the words to the list, I start creating the final list for the Mapper threads. Again, I use a mutex to be sure no more than one thread writes on the final list. Again, I check if the word I am trying to add is already on my final list: if it is, I only add the index of the file in which I also found the word; if it isn't, I create a new pair of the word and a vector of ints(in which I store the files indexes) and i add it to the structure. 

Finally, I introduced a *barrier*, because I need to make sure that all the Mapper threads will finish their work before I get to the next stage. 


### Reducer Threads Structure and Implementation

For the Reducer threads I chose to create a structure looking like this:
```cpp
// Structure for the reducer threads
struct ReducerThreadInfo {
    map<string, vector<int>>  *inputListFromMappers;
    pthread_mutex_t *mutexReadingInput;
    pthread_mutex_t *mutexWritingFiles;
    pthread_barrier_t *barrier;
    pthread_barrier_t *barrierWritingInFiles;
    vector<pair<string, vector<int>>> *wordsAndListOfIndexes[26];
    vector<char> *alphabetForWordsAndListOfIndexes;
    vector<char> *alphabetForMergingWords;
    vector<char> *alphabetForWritingInFiles;
};
```
1. Using a *map* to get the final list from the Mappers as input list for the Reducers.
2. Using a *mutex* for reading from the input list from the Mappers.
3. Using a *mutex* for writing the final results in the files.
4. Using a *barrier* to be sure that all the Mappers finished their work so that Reducers can start theirs.
5. Using a *barrier* to be sure that all the Reducers finished their work before starting the new *stage* of processing.
6. Using a *vector* for the final result.
7. Using three *vectors* to store partial results.

For the first part, I will go through the vector *alphabetForWordsAndListOfIndexes* in which I retain all the letters. For every pair from the list given from the mappers, I check if the first letter of the word is the same as the letter from the alphabet I have. If it is, I add the pair in the final vector. Here I encountered a problem when first trying to implement it. The first time, I was trying to push from the first place to the final vector, but the problem was that I was iterating though the vector too many times and the checker wasn't too happy about it :). So, that's why I decided to get an *alphabet* and check for every letter, because only if the letter was the same I had to iterate through the initial vector and search for new pairs; else, I would simply break the loop.
I also used mutexes here too because I was handling shared resources and I wouldn't want the threads to *fight* between them.

Now, to separate the stages of the Reducers jobs, I put another barrier so that all the Reducer threads finish indexing by first letter the words before merging the results. 

Again, I use a list with an alphabet for merging the words. As long as that list isn't empty, I try to merge the indexes from the same words. If two pairs in the vector have the same word, but different file indexes, I merge them into one pair. Also, I sort the vector of indexes in ascending order. I need to also sort the words in the vector for every letter: first after the length of the list of indexes and then lexicographically, using a comparator. Here, on this stage, I also used mutexes to be sure that only one thread does this job at a time. 

The final stage is represented by writing the final result in files. I use again a barrier to be sure that all Reducer threads finished their work before starting to write in the output files. Here, I simply create files for every letter of the alphabet and I iterate through the final list and add for every letter the corresponding pairs in each file. Again, using mutexes to be sure that no more than one thread writes in a file. 

Finally, I destroy the mutexes and the barriers I've used.


## Feedback

It was an interesting assignment, from which I learned better how mutexes and barriers work. The idea seemed pretty easy at the beginning, but when starting the implementation... It wasn't that easy :). Being the first time when I tried parallel programming it was a bit challenging, but not impossible. But, it is not that nice that there isn't a checker on Moodle so that you have at least an idea when talking about the score. Overall, it was an interesting homework.

