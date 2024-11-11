#include <iostream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <filesystem>
#include <pwd.h> // for getting user home directory

namespace fs = std::filesystem;

bool recursive = false;
bool caseInsensitive = false;

// Helper function to compare filenames with optional case insensitivity
bool compare_filenames(const std::string& file1, const std::string& file2) {
    if (caseInsensitive) {
        return strcasecmp(file1.c_str(), file2.c_str()) == 0;
    }
    return file1 == file2;
}

// Helper function to perform search
void find_file(const std::string& searchpath, const std::string& filename) {
    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(searchpath, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && compare_filenames(entry.path().filename().string(), filename)) {
                std::cout << getpid() << ": " << filename << ": " << entry.path() << std::endl;
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(searchpath, fs::directory_options::skip_permission_denied)) {
            if (entry.is_regular_file() && compare_filenames(entry.path().filename().string(), filename)) {
                std::cout << getpid() << ": " << filename << ": " << entry.path() << std::endl;
            }
        }
    }
}

int main(int argc, char* argv[]) {
    int opt;
    while ((opt = getopt(argc, argv, "Ri")) != -1) {
        switch (opt) {
            case 'R': recursive = true; break;
            case 'i': caseInsensitive = true; break;
            default:
                std::cerr << "Usage: " << argv[0] << " [-R] [-i] searchpath filename1 [filename2] ...\n";
                exit(EXIT_FAILURE);
        }
    }

    if (optind >= argc) {
        std::cerr << "Expected searchpath and filenames\n";
        exit(EXIT_FAILURE);
    }

    std::string searchpath = argv[optind++]; // Get the search path
    if (searchpath == "~") {
        const char* home = getenv("home");
        if (home == nullptr) {
            home = getpwuid(getuid())->pw_dir;
        }
        searchpath = home; // If it's "~", replace with actual home directory path
    }

    std::vector<std::string> filenames;
    for (int i = optind; i < argc; ++i) {
        filenames.push_back(argv[i]);
    }

    for (const auto& filename : filenames) {
        pid_t pid = fork();
        if (pid == 0) { // Child process
            find_file(searchpath, filename);
            exit(0); // Terminate child process
        }
    }

    // Wait for all children to complete
    while (wait(NULL) > 0); // Wait for all children to prevent zombies

    return 0;
}

