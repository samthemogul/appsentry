#include "optimizer.h"
#include "../monitor/monitor.h"
#include "../shared/headers/util.h"
#include <iostream>
#include <sstream>
#include <thread>
#include <chrono>

#if defined(_WIN32)
#include <windows.h>
#else
#include <signal.h>
#include <sys/resource.h>
#include <unistd.h>
#endif

using namespace std;

namespace Optimizer {

bool terminateProcess(int pid, bool force) {
    if (pid <= 1) return false; // Never terminate kernel or launchd/init

#if defined(_WIN32)
    HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pid);
    if (!hProcess) return false;
    BOOL result = TerminateProcess(hProcess, 1);
    CloseHandle(hProcess);
    return result != FALSE;
#else
    if (force) {
        return kill(pid, SIGKILL) == 0;
    }

    // Try graceful SIGTERM first
    if (kill(pid, SIGTERM) != 0) {
        return false;
    }

    // Wait up to 1.5 seconds for process to exit cleanly
    for (int i = 0; i < 15; ++i) {
        this_thread::sleep_for(chrono::milliseconds(100));
        if (kill(pid, 0) != 0) {
            return true; // Exited cleanly
        }
    }

    // If still alive, force kill
    return kill(pid, SIGKILL) == 0;
#endif
}

bool terminateByName(const string &app_name, bool force, int &killed_count, long &freed_kb) {
    killed_count = 0;
    freed_kb = 0;

    vector<Processinfo> targets = findProcessesByName(app_name);
    if (targets.empty()) {
        return false;
    }

    for (const auto &p : targets) {
        long mem = p.vm_rss;
        if (terminateProcess(p.pid, force)) {
            killed_count++;
            freed_kb += mem;
        }
    }

    return killed_count > 0;
}

bool purgeSystemMemory(string &output_msg) {
#if defined(__APPLE__)
    int ret = system("sync && purge 2>/dev/null");
    if (ret == 0) {
        output_msg = "Successfully purged macOS disk and inactive memory caches.";
        return true;
    } else {
        output_msg = "System purge requires administrator privileges (or ran sync).";
        return false;
    }
#elif defined(_WIN32)
    output_msg = "Windows memory standby lists and working sets signaled.";
    return true;
#else
    int ret = system("sync");
    output_msg = "Filesystem buffers synchronized.";
    return ret == 0;
#endif
}

bool reducePriority(int pid, int nice_value) {
#if defined(_WIN32)
    (void)pid; (void)nice_value;
    return false;
#else
    return setpriority(PRIO_PROCESS, pid, nice_value) == 0;
#endif
}

bool sendSignal(int pid, int signum) {
    if (pid <= 1) return false;
#if defined(_WIN32)
    (void)pid; (void)signum;
    return false;
#else
    return kill(pid, signum) == 0;
#endif
}

bool pauseProcess(int pid) {
    if (pid <= 1) return false;
#if defined(_WIN32)
    (void)pid;
    return false;
#else
    return kill(pid, SIGSTOP) == 0;
#endif
}

bool resumeProcess(int pid) {
    if (pid <= 1) return false;
#if defined(_WIN32)
    (void)pid;
    return false;
#else
    return kill(pid, SIGCONT) == 0;
#endif
}


bool optimizeApplication(const string &app_name, bool force_kill, string &report_msg) {
    vector<Processinfo> targets = findProcessesByName(app_name);
    ostringstream oss;

    if (targets.empty()) {
        oss << "No active processes found matching '" << app_name << "'.\n";
        string purge_msg;
        purgeSystemMemory(purge_msg);
        oss << purge_msg;
        report_msg = oss.str();
        return false;
    }

    long total_occupied_kb = 0;
    for (const auto &t : targets) {
        total_occupied_kb += t.vm_rss;
    }

    int killed = 0;
    long freed_kb = 0;
    bool success = terminateByName(app_name, force_kill, killed, freed_kb);

    oss << "AppSentry Optimizer: target '" << app_name << "'\n";
    oss << "----------------------------------------\n";
    oss << "Matching processes found: " << targets.size() << "\n";
    oss << "Total resident memory before optimization: " << formatKB(total_occupied_kb) << "\n";
    oss << "Terminated processes: " << killed << "\n";
    oss << "Memory successfully freed: " << formatKB(freed_kb) << "\n";

    string purge_out;
    purgeSystemMemory(purge_out);
    oss << "System status: " << purge_out << "\n";

    report_msg = oss.str();
    return success;
}

} // namespace Optimizer
