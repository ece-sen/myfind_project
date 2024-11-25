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
                    std::cout << getpid() << ": " << filename << ": " << fs::absolute(entry.path()) << "\n";
                    found = true;
                }
            }
        } else {
            // non-recursive search only in the specified directory
            for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied)) {
                if (isMatchingFilename(entry.path().filename().string(), filename)) {
                    std::cout << getpid() << ": " << filename << ": " << fs::absolute(entry.path()) << "\n";
                    found = true;
                }
            }
        }

        if (!found) {
            std::cout << getpid() << ": " << filename << ": Not found in " << fs::absolute(directory) << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error accessing " << directory << ": " << e.what() << "\n";
    }

    // signal semaphore this child process has finished
    sem_post(&semaphore);
}

/*
 how requirement was achieved:
 - Creates a child process with 'fork()' for each filename.
 - Child process searches the directory (recursively if '-R' is enabled, case insensetive if '-i' is enabled).
 - Results printed to 'stdout': process ID, filename, file path, or "Not found".
 - Parent process waits for all children to finish using a semaphore.
 - Output is unsorted but readable, as each child prints results immediately.
 */
int main(int argc, char* argv[]) {
    int opt;
    bool optionError = false;

    // ensure correct synchronization between parent and child processes
    sem_init(&semaphore, 0, 0);

    // parse command-line options
    while ((opt = getopt(argc, argv, "Ri")) != EOF) {
        switch (opt) {
            case 'R':
                recursiveSearchEnabled = true; 
                break;
            case 'i':
                caseInsensetiveSearch = true; 
                break;
            default:
                optionError = true; // flag an error for invalid options
                break;
        }
    }

    // combined options like -Ri are not allowed (must be separate)
    if (optind > 1 && argv[optind - 1][0] == '-' && strlen(argv[optind - 1]) > 2) {
        std::cerr << "Error: Options -R and -i must be specified separately.\n";
        return EXIT_FAILURE;
    }

    // validate arguments and options
    if (optionError || optind >= argc) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    // get search path and filenames from the command-line
    std::string searchPath = argv[optind++];
    std::vector<std::string> filenames(argv + optind, argv + argc);

    // check if search path exists and is valid directory
    if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
        std::cerr << "Error: Invalid or non-existent directory: " << searchPath << "\n";
        return EXIT_FAILURE;
    }

    // create child process for each filename
    for (const auto& filename : filenames) {
        pid_t pid = fork();

        if (pid == 0) {
            searchForFile(searchPath, filename);
            return 0;
        } else if (pid < 0) {
            std::cerr << "Error: Failed to create process for " << filename << "\n";
        }
    }

// Parent process waits for all child processes to complete using semaphores
    for (size_t i = 0; i < filenames.size(); ++i) {
        int status;
        waitpid(-1, &status, 0); // Wait for any child process
    }

    // clean up after all processes have finished
    sem_destroy(&semaphore);

    return 0;
}