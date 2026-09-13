#include "launcher.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

#if defined(__APPLE__)
#include <sys/stat.h>
#elif defined(_WIN32)
#include <windows.h>
#include <shellapi.h>
#endif

using namespace std;
namespace fs = std::filesystem;

namespace Launcher {

bool launchApplication(const string &app_name, string &feedback) {
    if (app_name.empty()) {
        feedback = "Error: Application name cannot be empty.";
        return false;
    }

#if defined(__APPLE__)
    // 1. Try launching with macOS `open -a`
    string cmd = "open -a \"" + app_name + "\" 2>/dev/null";
    int ret = system(cmd.c_str());
    if (ret == 0) {
        feedback = "Successfully launched application '" + app_name + "' via macOS Application Services.";
        return true;
    }

    // 2. Check standard macOS Application folders
    vector<string> search_dirs = {
        "/Applications/",
        "/Applications/Utilities/",
        "/System/Applications/",
        "/System/Applications/Utilities/",
        string(getenv("HOME") ? getenv("HOME") : "") + "/Applications/"
    };

    for (const auto &dir : search_dirs) {
        string full_path = dir + app_name + ".app";
        if (fs::exists(full_path)) {
            string open_path = "open \"" + full_path + "\" 2>/dev/null";
            if (system(open_path.c_str()) == 0) {
                feedback = "Successfully launched application from: " + full_path;
                return true;
            }
        }
    }

    // 3. Fall back to launching executable directly in background if it exists
    string check_cmd = "command -v \"" + app_name + "\" >/dev/null 2>&1 || test -f \"" + app_name + "\"";
    if (system(check_cmd.c_str()) == 0) {
        string bg_cmd = "\"" + app_name + "\" >/dev/null 2>&1 &";
        if (system(bg_cmd.c_str()) == 0) {
            feedback = "Started background process for executable: " + app_name;
            return true;
        }
    }

    feedback = "Failed to launch '" + app_name + "'. Could not locate application bundle or executable.";
    return false;

#elif defined(_WIN32)
    HINSTANCE hInst = ShellExecuteA(NULL, "open", app_name.c_str(), NULL, NULL, SW_SHOWNORMAL);
    if ((intptr_t)hInst > 32) {
        feedback = "Successfully launched Windows application: " + app_name;
        return true;
    }

    string win_cmd = "start \"\" \"" + app_name + "\"";
    if (system(win_cmd.c_str()) == 0) {
        feedback = "Successfully started application: " + app_name;
        return true;
    }

    feedback = "Failed to launch '" + app_name + "'. ShellExecute code: " + to_string((intptr_t)hInst);
    return false;

#else
    // Linux
    string xdg_cmd = "gtk-launch \"" + app_name + "\" 2>/dev/null || xdg-open \"" + app_name + "\" 2>/dev/null";
    if (system(xdg_cmd.c_str()) == 0) {
        feedback = "Successfully launched desktop application: " + app_name;
        return true;
    }

    string check_cmd = "command -v \"" + app_name + "\" >/dev/null 2>&1 || test -f \"" + app_name + "\"";
    if (system(check_cmd.c_str()) == 0) {
        string linux_bg = "\"" + app_name + "\" >/dev/null 2>&1 &";
        if (system(linux_bg.c_str()) == 0) {
            feedback = "Started process: " + app_name;
            return true;
        }
    }

    feedback = "Failed to launch '" + app_name + "'. Application not found.";
    return false;
#endif
}

} // namespace Launcher
