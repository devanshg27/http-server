#include "fileutil.h"
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

bool isDirectory(const std::string& path) {
    struct stat sb;
    if (stat(path.data(), &sb) == -1) {
        perror("stat");
        exit(EXIT_FAILURE);
    }
    return S_ISDIR(sb.st_mode);
}
