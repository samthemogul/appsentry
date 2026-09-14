#include <iostream>
#include <sstream>
#include <vector>
#include <string>
#include <iomanip>
#include <chrono>
#include <thread>
#include <csignal>
#include <atomic>
#include <algorithm>

#include "logger/logger.h"
#include "monitor/monitor.h"
#include "optimizer/optimizer.h"
#include "launcher/launcher.h"
#include "alert/threshold_alert.h"
#include "shared/headers/getos.h"
#include "shared/headers/util.h"

using namespace std;

// Atomic flag for graceful Ctrl+C handling
static atomic<bool> g_running(true);

static void signalHandler(int signum) {
    (void)signum;
    g_running = false;
}

static void printUsage(const string &prog_name) {
    cout << "\n=========================================================================\n";
    cout << " AppSentry - Application Usage Monitor, Leak Detector & Optimizer\n";
    cout << "=========================================================================\n";
    cout << "Usage:\n";
    cout << "  " << prog_name << " <command> [arguments] [options]\n\n";
    cout << "Commands:\n";
    cout << "  monitor <app_name|pid>  Track real-time CPU, memory, & threads; detect leaks;\n";
    cout << "                          dispatch POSIX signals & prevent resource exhaustion.\n";
    cout << "  report <app_name>       View aggregated usage statistics, leak history, & alerts.\n";
    cout << "  optimize <app_name>     Free memory or stop running processes.\n";
    cout << "  launch <app_name>       Start an application from the CLI.\n";
    cout << "  list [filter]           List currently active processes and resource consumption.\n";
    cout << "  help                    Display this help message.\n\n";
    cout << "Monitor Options:\n";
    cout << "  -i, --interval <sec>    Sampling interval in seconds (default: 2s, POSIX timer driven)\n";
    cout << "  -d, --duration <sec>    Total monitoring time in seconds (default: continuous)\n";
    cout << "  -m, --threshold-mem <MB> Memory exhaustion critical limit in MB (default: 800 MB)\n";
    cout << "  -w, --threshold-warn <MB> Early warning memory threshold in MB (default: 400 MB)\n";
    cout << "  -t, --threshold-threads <n> Critical thread exhaustion limit (default: 150 threads)\n";
    cout << "  --threshold-threads-warn <n> Thread warning threshold under load (default: 50 threads)\n";
    cout << "  -c, --threshold-cpu <%> CPU warning threshold percentage (default: 80%)\n";
    cout << "  -l, --leak-growth <n>   Consecutive memory sample increases indicating leak (default: 4)\n";
    cout << "  --thread-growth <n>     Consecutive thread increases indicating leak (default: 4)\n";
    cout << "  --signal-alert <SIG>    POSIX signal dispatched on alert (default: SIGUSR1, e.g. SIGUSR2)\n";
    cout << "  --no-signal             Disable POSIX alert signal dispatching\n";
    cout << "  --pause-on-exhaustion   Dispatch POSIX SIGSTOP to pause runaway thread spawning\n";
    cout << "  --auto-kill             Proactively terminate process on exhaustion breach (SIGTERM/SIGKILL)\n";
    cout << "  --auto-optimize         Automatically purge memory caches when threshold breached\n";
    cout << "  --once                  Record a single snapshot to report and exit\n\n";
    cout << "Examples:\n";
    cout << "  " << prog_name << " monitor chrome --threshold-mem 1000 --auto-kill\n";
    cout << "  " << prog_name << " monitor worker_app --threshold-threads 80 --pause-on-exhaustion\n";
    cout << "  " << prog_name << " monitor 12345 --interval 1 --signal-alert SIGUSR1\n";
    cout << "  " << prog_name << " report chrome\n";
    cout << "  " << prog_name << " optimize discord --kill\n";
    cout << "  " << prog_name << " launch Safari\n";
    cout << "  " << prog_name << " list code\n";
    cout << "=========================================================================\n" << endl;
}

static int handleMonitor(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Error: 'monitor' command requires an application name or PID.\n";
        cerr << "Usage: appsentry monitor <app_name|pid> [options]\n";
        return 1;
    }

    string target_name = argv[2];
    int interval_sec = 2;
    int duration_sec = 0; // 0 = continuous
    bool single_shot = false;

    ThresholdConfig config;

    // Parse options
    for (int i = 3; i < argc; ++i) {
        string arg = argv[i];
        if ((arg == "-i" || arg == "--interval") && i + 1 < argc) {
            interval_sec = max(1, stoi(argv[++i]));
        } else if ((arg == "-d" || arg == "--duration") && i + 1 < argc) {
            duration_sec = max(1, stoi(argv[++i]));
        } else if ((arg == "-m" || arg == "--threshold-mem") && i + 1 < argc) {
            config.mem_critical_mb = stod(argv[++i]);
            config.mem_warning_mb = min(config.mem_warning_mb, config.mem_critical_mb * 0.75);
        } else if ((arg == "-w" || arg == "--threshold-warn") && i + 1 < argc) {
            config.mem_warning_mb = stod(argv[++i]);
        } else if ((arg == "-t" || arg == "--threshold-threads") && i + 1 < argc) {
            config.thread_critical_count = stoi(argv[++i]);
            config.thread_warning_count = min(config.thread_warning_count, (int)(config.thread_critical_count * 0.6));
        } else if (arg == "--threshold-threads-warn" && i + 1 < argc) {
            config.thread_warning_count = stoi(argv[++i]);
        } else if ((arg == "-c" || arg == "--threshold-cpu") && i + 1 < argc) {
            config.cpu_threshold_pct = stod(argv[++i]);
        } else if ((arg == "-l" || arg == "--leak-growth") && i + 1 < argc) {
            config.consecutive_growth_threshold = max(2, stoi(argv[++i]));
        } else if (arg == "--thread-growth" && i + 1 < argc) {
            config.consecutive_thread_growth_threshold = max(2, stoi(argv[++i]));
        } else if (arg == "--signal-alert" && i + 1 < argc) {
            string sig_str = argv[++i];
            if (sig_str == "SIGUSR2") config.posix_alert_signal = SIGUSR2;
            else if (sig_str == "SIGUSR1") config.posix_alert_signal = SIGUSR1;
            else if (sig_str == "SIGSTOP") config.posix_alert_signal = SIGSTOP;
            else config.posix_alert_signal = stoi(sig_str);
        } else if (arg == "--no-signal") {
            config.posix_alert_signal = 0;
        } else if (arg == "--pause-on-exhaustion") {
            config.mitigation = MitigationPolicy::MITIGATE_PAUSE;
        } else if (arg == "--auto-kill" || arg == "--auto-terminate") {
            config.mitigation = MitigationPolicy::MITIGATE_TERMINATE;
        } else if (arg == "--auto-optimize") {
            config.mitigation = MitigationPolicy::MITIGATE_OPTIMIZE;
        } else if (arg == "--once") {
            single_shot = true;
        } else {
            cerr << "Warning: Unknown option '" << arg << "' ignored.\n";
        }
    }

    createReportsRepository();

    // Locate matching process
    vector<Processinfo> matches = findProcessesByName(target_name);
    if (matches.empty()) {
        cerr << "\nError: No running process found matching '" << target_name << "'.\n";
        cerr << "Hint: Use 'appsentry list " << target_name << "' to verify running processes.\n\n";
        return 1;
    }

    // If multiple matches, select the one with highest RSS
    Processinfo target_proc = matches[0];
    for (const auto &p : matches) {
        if (p.vm_rss > target_proc.vm_rss) {
            target_proc = p;
        }
    }

    if (matches.size() > 1) {
        cout << "Notice: Found " << matches.size() << " processes matching '" << target_name << "'.\n";
        cout << "Monitoring primary instance PID " << target_proc.pid << " ('" << target_proc.name << "').\n";
    }

    // Set up signal handler for clean exit
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);

    ThresholdAlertManager alertManager(config);

    // Initialize real-time POSIX interval timer
    ThresholdAlertManager::setupRealTimeTimer(interval_sec, 0);

    cout << "\n=========================================================================\n";
    cout << " AppSentry Monitor: " << target_proc.name << " (PID: " << target_proc.pid << ")\n";
    cout << "=========================================================================\n";
    cout << " Real-Time Thresholds & POSIX Alerting Config:\n";
    cout << "   - Memory Limits: " << config.mem_warning_mb << " MB (Warning) | " << config.mem_critical_mb << " MB (Critical)\n";
    cout << "   - Thread Limits: " << config.thread_warning_count << " threads (Warning) | " << config.thread_critical_count << " threads (Critical)\n";
    cout << "   - CPU Limit: " << config.cpu_threshold_pct << "%\n";
    cout << "   - Leak Sensitivity: >= " << config.consecutive_growth_threshold << " mem increases | >= "
         << config.consecutive_thread_growth_threshold << " thread increases\n";
    cout << "   - POSIX Alert Signal: " << (config.posix_alert_signal > 0 ? ThresholdAlertManager::getPosixSignalName(config.posix_alert_signal) : "DISABLED") << "\n";
    cout << "   - Prevention Policy: "
         << (config.mitigation == MitigationPolicy::MITIGATE_TERMINATE ? "AUTO-TERMINATE (POSIX SIGTERM/SIGKILL)" :
             config.mitigation == MitigationPolicy::MITIGATE_PAUSE     ? "AUTO-PAUSE (POSIX SIGSTOP freeze)" :
             config.mitigation == MitigationPolicy::MITIGATE_OPTIMIZE  ? "AUTO-OPTIMIZE (Purge cache on breach)" :
                                                                        "NOTIFY ONLY") << "\n";
    cout << " Sampling: " << interval_sec << "s (Real-Time POSIX SIGALRM Timer) | Report: reports/" << target_proc.name << ".report\n";
    cout << " Press Ctrl+C to stop monitoring and generate session summary.\n";
    cout << "-------------------------------------------------------------------------\n";

    long start_time = time(nullptr);
    long peak_memory_kb = target_proc.vm_rss;
    long initial_memory_kb = target_proc.vm_rss;
    int peak_threads = target_proc.threads;
    int initial_threads = target_proc.threads;
    double cpu_sum = 0.0;
    int samples_count = 0;
    bool mitigation_triggered = false;

    // Monitoring loop driven by POSIX real-time timer
    while (g_running) {
        Processinfo current_proc;
        if (!getProcessInfoByPid(target_proc.pid, current_proc)) {
            cout << "\n[INFO] Target process (PID " << target_proc.pid << ") has terminated or exited.\n";
            break;
        }

        samples_count++;
        cpu_sum += current_proc.cpu_usage;
        if (current_proc.vm_rss > peak_memory_kb) {
            peak_memory_kb = current_proc.vm_rss;
        }
        if (current_proc.threads > peak_threads) {
            peak_threads = current_proc.threads;
        }

        // Process real-time threshold-alerting, memory leaks, and thread exhaustion
        vector<AlertRecord> alerts = alertManager.processSample(current_proc);

        // Save snapshot to report
        saveReport(target_proc.name, current_proc);

        // Log and print alerts
        for (const auto &alert : alerts) {
            saveAlertToReport(target_proc.name, alert);
            cout << ThresholdAlertManager::formatConsoleAlert(alert);
            if (alert.type == AlertType::ALERT_RESOURCE_EXHAUSTION_PREVENTED ||
                alert.type == AlertType::ALERT_THREAD_EXHAUSTION_PREVENTED) {
                mitigation_triggered = true;
            }
        }

        // Status line
        LeakAnalysis leak_info = alertManager.analyzeMemoryLeak((double)current_proc.vm_rss / 1024.0);
        ThreadAnalysis thread_info = alertManager.analyzeThreadExhaustion(current_proc.threads);

        string status_badge = "NORMAL";
        if (mitigation_triggered) {
            status_badge = "MITIGATED (POSIX SIGNAL)";
        } else if (thread_info.is_confirmed) {
            status_badge = "THREAD LEAK CONFIRMED";
        } else if (leak_info.is_confirmed) {
            status_badge = "MEM LEAK CONFIRMED";
        } else if (thread_info.thread_leak_detected) {
            status_badge = "THREAD LEAK SUSPECTED";
        } else if (leak_info.leak_detected) {
            status_badge = "MEM LEAK SUSPECTED";
        } else if (current_proc.threads >= config.thread_warning_count) {
            status_badge = "HIGH THREAD LOAD";
        } else if ((double)current_proc.vm_rss / 1024.0 >= config.mem_warning_mb) {
            status_badge = "HIGH MEMORY";
        }

        cout << "[" << getCurrentTimestamp() << "] "
             << "PID: " << current_proc.pid << " | "
             << "CPU: " << fixed << setprecision(1) << setw(4) << current_proc.cpu_usage << "% | "
             << "RSS: " << setw(7) << formatKB(current_proc.vm_rss) << " (" << formatKB(peak_memory_kb) << ") | "
             << "Threads: " << setw(2) << current_proc.threads << " (" << peak_threads << ") | "
             << "Status: " << status_badge;

        if (thread_info.est_seconds_to_exhaustion > 0) {
            cout << " | Thread Exhaustion in ~" << formatDuration((long)thread_info.est_seconds_to_exhaustion);
        } else if (leak_info.est_seconds_to_exhaustion > 0) {
            cout << " | Mem Exhaustion in ~" << formatDuration((long)leak_info.est_seconds_to_exhaustion);
        }
        cout << endl;

        if (mitigation_triggered) {
            cout << "\n[RESOURCE EXHAUSTION PREVENTED] Real-time POSIX signal mitigation executed.\n";
            break;
        }

        if (single_shot) {
            break;
        }

        if (duration_sec > 0 && (time(nullptr) - start_time) >= duration_sec) {
            cout << "\n[INFO] Monitoring duration completed (" << duration_sec << "s).\n";
            break;
        }

        // Wait for next real-time POSIX timer tick
        while (g_running && !ThresholdAlertManager::isTimerTickFired()) {
            this_thread::sleep_for(chrono::milliseconds(50));
        }
        ThresholdAlertManager::resetTimerTick();
    }

    ThresholdAlertManager::stopRealTimeTimer();

    long total_runtime = time(nullptr) - start_time;
    double avg_cpu = (samples_count > 0) ? (cpu_sum / samples_count) : 0.0;
    long net_mem_growth = peak_memory_kb - initial_memory_kb;
    int net_thread_growth = peak_threads - initial_threads;

    cout << "\n=========================================================================\n";
    cout << " Monitoring Session Summary\n";
    cout << "=========================================================================\n";
    cout << " Application: " << target_proc.name << " (PID: " << target_proc.pid << ")\n";
    cout << " Session Duration: " << formatDuration(total_runtime) << "\n";
    cout << " Samples Collected: " << samples_count << "\n";
    cout << " Memory (RSS): Initial " << formatKB(initial_memory_kb) << " -> Peak " << formatKB(peak_memory_kb)
         << " (" << (net_mem_growth >= 0 ? "+" : "") << formatKB(net_mem_growth) << ")\n";
    cout << " Active Threads: Initial " << initial_threads << " -> Peak " << peak_threads
         << " (" << (net_thread_growth >= 0 ? "+" : "") << net_thread_growth << " threads)\n";
    cout << " Average CPU Usage: " << fixed << setprecision(1) << avg_cpu << "%\n";
    cout << " Total Alerts Logged: " << alertManager.getAlerts().size() << "\n";
    cout << " Resource Exhaustion Prevented: " << (mitigation_triggered ? "YES (POSIX Mitigation)" : "NO") << "\n";
    cout << " Full Report File: " << findReportFile(target_proc.name) << "\n";
    cout << "=========================================================================\n\n";

    return 0;
}


static int handleReport(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Error: 'report' command requires an application name.\n";
        cerr << "Usage: appsentry report <app_name>\n";
        return 1;
    }
    string app_name = argv[2];
    displayReport(app_name);
    return 0;
}

static int handleOptimize(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Error: 'optimize' command requires an application name.\n";
        cerr << "Usage: appsentry optimize <app_name> [--kill] [--force] [--purge]\n";
        return 1;
    }

    string app_name = argv[2];
    bool force = false;

    for (int i = 3; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--force" || arg == "-f") {
            force = true;
        } else if (arg == "--purge") {
            string purge_out;
            Optimizer::purgeSystemMemory(purge_out);
            cout << purge_out << endl;
            return 0;
        }
    }

    string report;
    Optimizer::optimizeApplication(app_name, force, report);
    cout << "\n" << report << "\n";
    return 0;
}

static int handleLaunch(int argc, char *argv[]) {
    if (argc < 3) {
        cerr << "Error: 'launch' command requires an application name.\n";
        cerr << "Usage: appsentry launch <app_name>\n";
        return 1;
    }

    string app_name = argv[2];
    string feedback;
    bool success = Launcher::launchApplication(app_name, feedback);
    cout << "\n" << feedback << "\n\n";
    return success ? 0 : 1;
}

static int handleList(int argc, char *argv[]) {
    string filter = (argc >= 3) ? argv[2] : "";
    vector<Processinfo> procs = filter.empty() ? getAllProcesses() : findProcessesByName(filter);

    // Sort by Resident Memory (descending)
    sort(procs.begin(), procs.end(), [](const Processinfo &a, const Processinfo &b) {
        return a.vm_rss > b.vm_rss;
    });

    cout << "\n" << left
         << setw(8) << "PID"
         << setw(26) << "NAME"
         << setw(10) << "CPU%"
         << setw(16) << "MEMORY (RSS)"
         << setw(10) << "THREADS"
         << setw(12) << "STATE"
         << setw(16) << "OWNER" << "\n";
    cout << string(98, '-') << "\n";

    size_t count = min((size_t)30, procs.size());
    for (size_t i = 0; i < count; ++i) {
        const auto &p = procs[i];
        string short_name = p.name;
        if (short_name.length() > 24) {
            short_name = short_name.substr(0, 21) + "...";
        }
        cout << left
             << setw(8) << p.pid
             << setw(26) << short_name
             << setw(10) << fixed << setprecision(1) << p.cpu_usage
             << setw(16) << formatKB(p.vm_rss)
             << setw(10) << p.threads
             << setw(12) << get_state_char(p.state)
             << setw(16) << p.owner << "\n";
    }

    cout << string(98, '-') << "\n";
    cout << "Total active processes: " << procs.size();
    if (!filter.empty()) {
        cout << " (filtered by '" << filter << "')";
    }
    cout << "\n\n";
    return 0;
}

int main(int argc, char *argv[])
{
    // OS Verification
    string os;
    stringstream ss(getOSName());
    getline(ss, os, ':');

    if (os != "linux" && os != "windows" && os != "macos")
    {
        cerr << "Your Operating system architecture (" << os << ") is not supported at this time." << endl;
        return 1;
    }

    if (argc < 2)
    {
        printUsage(argv[0]);
        return 0;
    }

    string command = argv[1];
    toLowercase(command);

    if (command == "monitor")
    {
        return handleMonitor(argc, argv);
    }
    else if (command == "report")
    {
        return handleReport(argc, argv);
    }
    else if (command == "optimize")
    {
        return handleOptimize(argc, argv);
    }
    else if (command == "launch")
    {
        return handleLaunch(argc, argv);
    }
    else if (command == "list" || command == "ps")
    {
        return handleList(argc, argv);
    }
    else if (command == "help" || command == "--help" || command == "-h")
    {
        printUsage(argv[0]);
        return 0;
    }
    else
    {
        cerr << "Unknown command: '" << argv[1] << "'. Run '" << argv[0] << " help' for available commands.\n";
        return 1;
    }
}