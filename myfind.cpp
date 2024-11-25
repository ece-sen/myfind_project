#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <filesystem>
#include <semaphore.h>

namespace fs = std::filesystem;

// global variables
bool recursiveSearchEnabled = false;
bool caseInsensetiveSearch = false;

// track number of active child processes
sem_t semaphore;

// display how to properly search
void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " [-R] [-i] searchpath filename1 [filename2] ...\n"
              << "Options:\n"
              << "  -R  Search directories recursively\n"
              << "  -i  Perform case-insensitive filename matching\n";
}

// compare filenames, optionally case-insensitive 
bool isMatchingFilename(const std::string& file1, const std::string& file2) {
    if (caseInsensetiveSearch) {
        // compare filenames ignoring case
        return strcasecmp(file1.c_str(), file2.c_str()) == 0;
    }
    // case-sensitive comparison
    return file1 == file2;
}

// search for file in directory
void searchForFile(const std::string& directory, const std::string& filename) {
    bool found = false;

    try {
        if (recursiveSearchEnabled) {
            // recursive search through all subdirectories
            for (const auto& entry : fs::recursive_directory_iterator(directory, fs::directory_options::skip_permission_denied)) {
                if (isMatchingFilename(entry.path().filename().string(), filename)) {
                    sem_wait(&semaphore); 
                    std::cout << getpid() << ": " << filename << ": " << fs::absolute(entry.path()) << "\n";
                    sem_post(&semaphore); 
                    found = true;
                }
            }
        } else {
            // non-recursive search only in the specified directory
            for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied)) {
                if (isMatchingFilename(entry.path().filename().string(), filename)) {
                    sem_wait(&semaphore);
                    std::cout << getpid() << ": " << filename << ": " << fs::absolute(entry.path()) << "\n";
                    sem_post(&semaphore);
                    found = true;
                }
            }
        }

        if (!found) {
            sem_wait(&semaphore);
            std::cout << getpid() << ": " << filename << ": Not found in " << fs::absolute(directory) << "\n";
            sem_post(&semaphore);
        }
    } catch (const std::exception& e) {
        sem_wait(&semaphore);
        std::cerr << "Error accessing " << directory << ": " << e.what() << "\n";
        sem_post(&semaphore);
    }
}

int main(int argc, char* argv[]) {
    int opt;
    bool optionError = false;

    // ensure correct synchronization between parent and child processes
    sem_init(&semaphore, 0, 1);

    // to check if -R or -i are already set
    bool doubleR = false;
    bool doubleI = false;

    // parse command-line options
    while ((opt = getopt(argc, argv, "Ri")) != EOF) {
        switch (opt) {
            case 'R':
                 if (doubleR) { // check if -R was already set
                    optionError = true;
                    std::cerr << "Error: Option -R is specified multiple times.\n";
                }
                recursiveSearchEnabled = true;
                doubleR = true;
                break;
            case 'i':
                 if (doubleI) { // check if -i was already set
                    optionError = true;
                    std::cerr << "Error: Option -i is specified multiple times.\n";
                }
                caseInsensetiveSearch = true;
                doubleI = true;
                break;
            default:
                optionError = true; // flag an error for invalid options
                break;
        }
    }

    // combined options like -Ri are not allowed (must be separate)
    if (optind > 1 && argv[optind - 1][0] == '-' && strlen(argv[optind - 1]) > 2) {
        std::cerr << "Error: Options -R and -i must be written separately.\n";
        return EXIT_FAILURE;
    }

    // validate arguments and options
    if (optionError || optind >= argc) {
        printUsage(argv[0]);
        sem_destroy(&semaphore);
        return EXIT_FAILURE;
    }

    // get search path and filenames from the command-line
    std::string searchPath = argv[optind++];
    std::vector<std::string> filenames(argv + optind, argv + argc);

    // check if search path exists and is valid directory
    if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
        std::cerr << "Error: Invalid or non-existent directory: " << searchPath << "\n";
        sem_destroy(&semaphore);
        return EXIT_FAILURE;
    }

    // create child process for each filename
    for (const auto& filename : filenames) {
        pid_t pid = fork();

        if (pid == 0) {
            searchForFile(searchPath, filename);
            return 0;
        } else if (pid < 0) {
            sem_wait(&semaphore);
            std::cerr << "Error: Failed to create process for " << filename << "\n";
            sem_post(&semaphore);
        }
    }

    for (size_t i = 0; i < filenames.size(); ++i) {
        int status;
        waitpid(-1, &status, 0); // wait for any child process
    }

    // clean up after all processes have finished
    sem_destroy(&semaphore);

    return 0;
}