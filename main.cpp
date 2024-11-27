#include <iostream>
#include <fstream>
#include <string>
#include <queue>
using namespace std;

int main(int argc, char **argv)
{
    // First things first, we check if the input format is correct
    if (argc != 4) {
        cout << "Wrong input! Please use the correct format." << endl;
    }

    // Openeing the input file
    ifstream inputFile(argv[3]);
    if (!inputFile.is_open()) {
        cout << "Error opening file!" << endl;
        return 1;
    }
    
    // Reading the number of files
    int numberOfFiles;
    inputFile >> numberOfFiles;

    // Reading the file names and storing them in a queue
    queue<string> fileNames;
    string fileName;
    for (int i = 0; i < numberOfFiles; i++) {
        inputFile >> fileName;
        fileNames.push(fileName);
    }
    
    // Closing the file
    inputFile.close();

    // Print the number of files and their names - just for checking purposes
    /*cout << "Number of files: " << numberOfFiles << endl;
    for (int i = 0; i < numberOfFiles; i++) {
        cout << "File " << fileNames.front() << endl;
        fileNames.pop();
    }*/

    return 0;
}