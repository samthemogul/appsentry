#ifndef LAUNCHER_H
#define LAUNCHER_H

#include <string>

namespace Launcher {
    // Launch an application by name, bundle identifier, or executable path
    bool launchApplication(const std::string &app_name, std::string &feedback);
}

#endif
