#ifndef OPTIMIZER_H
#define OPTIMIZER_H

#include <string>
#include <vector>
#include "../shared/headers/process_info.h"

namespace Optimizer {
    // Terminate a single process by PID
    bool terminateProcess(int pid, bool force = false);

    // Terminate all processes matching app_name
    bool terminateByName(const std::string &app_name, bool force, int &killed_count, long &freed_kb);

    // Attempt OS-level system memory purge
    bool purgeSystemMemory(std::string &output_msg);

    // Lower process CPU scheduling priority (renice)
    bool reducePriority(int pid, int nice_value = 19);

    // Send arbitrary POSIX signal to a process
    bool sendSignal(int pid, int signum);

    // Pause/Freeze process via POSIX SIGSTOP (halts thread spawning under heavy workload)
    bool pauseProcess(int pid);

    // Resume process via POSIX SIGCONT
    bool resumeProcess(int pid);

    // Optimize application by freeing memory or stopping processes
    bool optimizeApplication(const std::string &app_name, bool force_kill, std::string &report_msg);
}

#endif
