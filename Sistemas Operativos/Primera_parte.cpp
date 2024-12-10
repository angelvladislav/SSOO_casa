#include <iostream>
#include <fstream>
#include <string>
#include <cstring>
#include <sstream>
#include <expected>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

class SafeMap {
private:
    std::string_view sv_;

public:
    SafeMap(std::string_view sv) : sv_(sv) {}

    ~SafeMap() {
        if (!sv_.empty()) {
            munmap(const_cast<char*>(sv_.data()), sv_.size());
        }
    }

    std::string_view get() const {
        return sv_;
    }
};

std::expected<SafeMap, int> read_all(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd < 0) {
        return std::unexpected(errno);
    }

    struct stat st;
    if (fstat(fd, &st) != 0) {
        close(fd);
        return std::unexpected(errno);
    }

    void* mapped = mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    if (mapped == MAP_FAILED) {
        return std::unexpected(errno);
    }

    return SafeMap(std::string_view(static_cast<char*>(mapped), st.st_size));
}

void send_response(std::string_view header, std::string_view body = {}) {
    std::cout << header << "\n";
    if (!body.empty()) {
        std::cout << body;
    }
}

void print_help() {
    std::cout << "Usage: docserver [-v | --verbose] [-h | --help] ARCHIVO\n";
}

void verbose_log(const std::string& message, bool verbose) {
    if (verbose) {
        std::cerr << message << "\n";
    }
}

int main(int argc, char* argv[]) {
    bool verbose = false;
    std::string file_path;

    // Parse arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            print_help();
            return 0;
        } else if (arg == "-v" || arg == "--verbose") {
            verbose = true;
        } else {
            if (file_path.empty()) {
                file_path = arg;
            } else {
                std::cerr << "Error: Multiple files specified." << std::endl;
                return 1;
            }
        }
    }

    if (file_path.empty()) {
        std::cerr << "Error: No file specified." << std::endl;
        return 1;
    }

    // Read file
    auto result = read_all(file_path);
    if (!result) {
        int err = result.error();
        if (err == EACCES) {
            send_response("403 Forbidden\n");
        } else if (err == ENOENT) {
            send_response("404 Not Found\n");
        } else {
            std::cerr << "Error: " << std::strerror(err) << "\n";
            return 1;
        }
        return 0;
    }

    auto safe_map = std::move(result.value());
    std::string_view file_content = safe_map.get();

    std::ostringstream oss;
    oss << "Content-Length: " << file_content.size() << '\n';
    std::string header = oss.str();

    send_response(header, file_content);

    return 0;
}
