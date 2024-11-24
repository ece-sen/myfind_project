#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <filesystem>
#include <fstream>

namespace fs = std::filesystem;

// Global flags for enabling recursive search and case-insensitive matching
bool recursiveSearchEnabled = false;
bool caseInsensitiveSearch = false;

// Function to display program usage instructions
void printUsage(const char* programName) {
    std::cerr << "Usage: " << programName << " [-R] [-i] searchpath filename1 [filename2] ...\n"
              << "Options:\n"
              << "  -R  Search directories recursively\n"
              << "  -i  Perform case-insensitive filename matching\n";
}

// Helper function to compare filenames, with optional case-insensitive matching
bool isMatchingFilename(const std::string& file1, const std::string& file2) {
    if (caseInsensitiveSearch) {
        return strcasecmp(file1.c_str(), file2.c_str()) == 0; // Case-insensitive comparison
    }
    return file1 == file2; // Exact match
}

// Function to search for a file in the specified directory
void searchForFile(const std::string& directory, const std::string& filename, const std::string& outputFile) {
    bool found = false; // To track if the file is found
    std::ofstream outFile(outputFile);

    if (!outFile) {
        std::cerr << "Error: Unable to create or open output file: " << outputFile << "\n";
        return;
    }

    try {
        if (recursiveSearchEnabled) {
            // Perform recursive search
            for (const auto& entry : fs::recursive_directory_iterator(directory, fs::directory_options::skip_permission_denied)) {
                if (isMatchingFilename(entry.path().filename().string(), filename)) {
                    outFile << getpid() << ": " << filename << ": " << fs::absolute(entry.path()) << "\n";
                    found = true;
                }
            }
        } else {
            // Perform non-recursive search
            for (const auto& entry : fs::directory_iterator(directory, fs::directory_options::skip_permission_denied)) {
                if (isMatchingFilename(entry.path().filename().string(), filename)) {
                    outFile << getpid() << ": " << filename << ": " << fs::absolute(entry.path()) << "\n";
                    found = true;
                }
            }
        }

        if (!found) {
            outFile << getpid() << ": " << filename << ": Not found in " << fs::absolute(directory) << "\n";
        }
    } catch (const std::exception& e) {
        outFile << "Error accessing " << directory << ": " << e.what() << "\n";
    }

    outFile.close(); // Close the output file when done
}

int main(int argc, char* argv[]) {
    int opt;
    bool optionError = false;

    // Parse command-line options
    while ((opt = getopt(argc, argv, "Ri")) != EOF) {
        switch (opt) {
            case 'R':
                recursiveSearchEnabled = true;
                break;
            case 'i':
                caseInsensitiveSearch = true;
                break;
            default:
                optionError = true;
                break;
        }
    }

    // Ensure that -R and -i are not combined as a single option like -Ri
    if (optind > 1 && argv[optind - 1][0] == '-' && strlen(argv[optind - 1]) > 2) {
        std::cerr << "Error: Options -R and -i must be specified separately.\n";
        return EXIT_FAILURE;
    }
    
    // Validate arguments and options
    if (optionError || optind >= argc) {
        printUsage(argv[0]);
        return EXIT_FAILURE;
    }

    std::string searchPath = argv[optind++];
    std::vector<std::string> filenames(argv + optind, argv + argc);

    // Check if the search path exists and is a directory
    if (!fs::exists(searchPath) || !fs::is_directory(searchPath)) {
        std::cerr << "Error: Invalid or non-existent directory: " << searchPath << "\n";
        return EXIT_FAILURE;
    }

    // Temporary files for storing output from child processes
    std::vector<std::string> tempFiles;

    // Spawn a child process for each filename to search
    for (const auto& filename : filenames) {
        pid_t pid = fork();

        if (pid == 0) {
            // Child process: create a temporary file for output
            std::string tempFile = "/tmp/myfind_" + std::to_string(getpid()) + ".log";
            searchForFile(searchPath, filename, tempFile);
            return 0; // Exit child process
        } else if (pid < 0) {
            // Handle fork error
            std::cerr << "Error: Failed to create process for " << filename << "\n";
        } else {
            // Parent process: store the temporary file name
            tempFiles.push_back("/tmp/myfind_" + std::to_string(pid) + ".log");
        }
    }

    // Parent process waits for all child processes to finish
    while (wait(NULL) > 0);

    // Parent process reads and outputs the results from temporary files
    for (const auto& tempFile : tempFiles) {
        std::ifstream inFile(tempFile);
        if (inFile) {
            std::cout << inFile.rdbuf(); // Output the contents of the temporary file
        } else {
            std::cerr << "Error: Unable to read temporary file: " << tempFile << "\n";
        }
        inFile.close();
        fs::remove(tempFile); // Clean up temporary file
    }

    return 0;
}
