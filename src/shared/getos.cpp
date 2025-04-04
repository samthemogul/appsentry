
#include <string>
#include "headers/getos.h"


#ifdef _WIN32
#include <windows.h>
#include <sstream>

std::string getOSName() {
    OSVERSIONINFOEX osvi;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (!GetVersionEx((OSVERSIONINFO*)&osvi)) {
        return "windows: (Unknown Version)";
    }

    std::ostringstream os;
    os << "windows: " << osvi.dwMajorVersion << "." << osvi.dwMinorVersion;
    return os.str();
}

#elif __linux__
#include <fstream>

std::string getOSName() {
    std::ifstream file("/etc/os-release");
    std::string line;

    while (std::getline(file, line)) {
        if (line.find("NAME=") == 0) {
            std::string name = line.substr(5);
            if (!name.empty() && name.front() == '"' && name.back() == '"') {
                name = name.substr(1, name.size() - 2);
            }
            return "linux:" + name;
        }
    }

    return "linux: (Unknown)";
}

#elif __APPLE__
#include <sys/utsname.h>

std::string getOSName() {
    struct utsname sysinfo;
    if (uname(&sysinfo) == 0) {
        return "macos:" + std::string("macOS ") + sysinfo.release;
    }
    return "macos: (Unknown)";
}

#else
std::string getOSName() {
    return "Unknown OS";
}
#endif