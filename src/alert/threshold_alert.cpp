#include "threshold_alert.h"
#include "os_notifier.h"
#include "../optimizer/optimizer.h"
#include "../shared/headers/util.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <numeric>

#if !defined(_WIN32)
#include <sys/time.h>
#include <signal.h>
#include <unistd.h>
#endif

using namespace std;

// Signal-safe real-time POSIX interval timer flag
static volatile sig_atomic_t g_posix_timer_tick = 0;

#if !defined(_WIN32)
static void posixTimerHandler(int sig) {
    (void)sig;
    g_posix_timer_tick = 1;
}
#endif

void ThresholdAlertManager::setupRealTimeTimer(int interval_sec, int interval_usec) {
#if !defined(_WIN32)
    struct sigaction sa{};
    sa.sa_handler = posixTimerHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, nullptr);

    struct itimerval itv{};
    itv.it_value.tv_sec = interval_sec;
    itv.it_value.tv_usec = interval_usec;
    itv.it_interval = itv.it_value;
    setitimer(ITIMER_REAL, &itv, nullptr);
#else
    (void)interval_sec; (void)interval_usec;
#endif
}

void ThresholdAlertManager::stopRealTimeTimer() {
#if !defined(_WIN32)
    struct itimerval itv{};
    setitimer(ITIMER_REAL, &itv, nullptr);
#endif
}

bool ThresholdAlertManager::isTimerTickFired() {
    return g_posix_timer_tick != 0;
}

void ThresholdAlertManager::resetTimerTick() {
    g_posix_timer_tick = 0;
}

ThresholdAlertManager::ThresholdAlertManager(const ThresholdConfig &cfg)
    : config(cfg), consecutive_memory_increases(0), consecutive_thread_increases(0),
      initial_rss_kb(0), initial_threads(0), initial_set(false), mitigation_executed(false)
{
    last_alert_time = chrono::system_clock::now() - chrono::seconds(60);
}

void ThresholdAlertManager::setConfig(const ThresholdConfig &cfg) {
    config = cfg;
}

const ThresholdConfig& ThresholdAlertManager::getConfig() const {
    return config;
}

void ThresholdAlertManager::reset() {
    history.clear();
    recorded_alerts.clear();
    consecutive_memory_increases = 0;
    consecutive_thread_increases = 0;
    initial_rss_kb = 0;
    initial_threads = 0;
    initial_set = false;
    mitigation_executed = false;
    resetTimerTick();
}

const vector<AlertRecord>& ThresholdAlertManager::getAlerts() const {
    return recorded_alerts;
}

string ThresholdAlertManager::getPosixSignalName(int signum) {
    switch (signum) {
        case SIGUSR1: return "SIGUSR1 (Real-time Alert)";
        case SIGUSR2: return "SIGUSR2 (Diagnostic Alert)";
        case SIGSTOP: return "SIGSTOP (Thread Freeze / Pause)";
        case SIGCONT: return "SIGCONT (Process Resume)";
        case SIGTERM: return "SIGTERM (Graceful Termination)";
        case SIGKILL: return "SIGKILL (Immediate Kill)";
        case SIGALRM: return "SIGALRM (Real-time Timer)";
        default: return "SIGNAL " + to_string(signum);
    }
}

string ThresholdAlertManager::getAlertTypeName(AlertType type) {
    switch (type) {
        case AlertType::ALERT_CPU_HIGH: return "HIGH_CPU_CONSUMPTION";
        case AlertType::ALERT_MEM_HIGH: return "MEMORY_THRESHOLD_WARNING";
        case AlertType::ALERT_LEAK_SUSPECTED: return "SUSPECTED_MEMORY_LEAK";
        case AlertType::ALERT_LEAK_CONFIRMED: return "CONFIRMED_MEMORY_LEAK";
        case AlertType::ALERT_RESOURCE_EXHAUSTION_IMMINENT: return "RESOURCE_EXHAUSTION_IMMINENT";
        case AlertType::ALERT_RESOURCE_EXHAUSTION_BREACHED: return "RESOURCE_EXHAUSTION_CRITICAL";
        case AlertType::ALERT_RESOURCE_EXHAUSTION_PREVENTED: return "RESOURCE_EXHAUSTION_PREVENTED";
        case AlertType::ALERT_THREAD_HIGH: return "THREAD_THRESHOLD_WARNING";
        case AlertType::ALERT_THREAD_LEAK_SUSPECTED: return "SUSPECTED_THREAD_LEAK";
        case AlertType::ALERT_THREAD_LEAK_CONFIRMED: return "CONFIRMED_THREAD_LEAK";
        case AlertType::ALERT_THREAD_EXHAUSTION_IMMINENT: return "THREAD_EXHAUSTION_IMMINENT";
        case AlertType::ALERT_THREAD_EXHAUSTION_BREACHED: return "THREAD_EXHAUSTION_CRITICAL";
        case AlertType::ALERT_THREAD_EXHAUSTION_PREVENTED: return "THREAD_EXHAUSTION_PREVENTED";
        case AlertType::ALERT_POSIX_SIGNAL_SENT: return "POSIX_SIGNAL_DISPATCHED";
        default: return "INFO";
    }
}

LeakAnalysis ThresholdAlertManager::analyzeMemoryLeak(double current_rss_mb) const {
    LeakAnalysis analysis;
    analysis.consecutive_growths = consecutive_memory_increases;

    if (history.size() < 2) {
        return analysis;
    }

    const auto &oldest = history.front();
    const auto &latest = history.back();

    auto duration_sec = chrono::duration_cast<chrono::seconds>(latest.timestamp - oldest.timestamp).count();
    if (duration_sec <= 0) duration_sec = 1;

    double oldest_mb = (double)oldest.vm_rss_kb / 1024.0;
    analysis.total_growth_mb = current_rss_mb - oldest_mb;
    analysis.growth_rate_mb_per_sec = analysis.total_growth_mb / (double)duration_sec;

    // Linear regression slope over history window
    double sum_t = 0.0, sum_m = 0.0, sum_tm = 0.0, sum_t2 = 0.0;
    int n = history.size();

    for (const auto &sample : history) {
        double t = chrono::duration_cast<chrono::milliseconds>(sample.timestamp - oldest.timestamp).count() / 1000.0;
        double m = (double)sample.vm_rss_kb / 1024.0;
        sum_t += t;
        sum_m += m;
        sum_tm += (t * m);
        sum_t2 += (t * t);
    }

    double denominator = (n * sum_t2 - sum_t * sum_t);
    double slope = 0.0;
    if (abs(denominator) > 1e-6) {
        slope = (n * sum_tm - sum_t * sum_m) / denominator;
    }

    // Determine leak signatures
    bool slope_positive = slope > 0.01;
    bool consecutive_hit = consecutive_memory_increases >= config.consecutive_growth_threshold;
    bool growth_hit = analysis.total_growth_mb >= config.min_leak_growth_mb;

    if (consecutive_hit && growth_hit && slope_positive) {
        analysis.leak_detected = true;
        if (consecutive_memory_increases >= config.consecutive_growth_threshold + 2 && slope > 0.05) {
            analysis.is_confirmed = true;
            analysis.confidence_percent = min(99.0, 60.0 + consecutive_memory_increases * 6.0);
        } else {
            analysis.confidence_percent = min(75.0, 40.0 + consecutive_memory_increases * 5.0);
        }

        if (slope > 0.001 && current_rss_mb < config.mem_critical_mb) {
            double remaining_mb = config.mem_critical_mb - current_rss_mb;
            analysis.est_seconds_to_exhaustion = remaining_mb / slope;
        }

        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Monotonic memory expansion across " << consecutive_memory_increases << " consecutive intervals (+"
            << analysis.total_growth_mb << " MB net, slope +" << (slope > 0 ? slope : analysis.growth_rate_mb_per_sec)
            << " MB/s).";
        if (analysis.est_seconds_to_exhaustion > 0) {
            oss << " Estimated exhaustion in " << formatDuration((long)analysis.est_seconds_to_exhaustion) << ".";
        }
        analysis.diagnostic_summary = oss.str();
    } else {
        analysis.diagnostic_summary = "Memory consumption within stable operating trajectory.";
    }

    return analysis;
}

ThreadAnalysis ThresholdAlertManager::analyzeThreadExhaustion(int current_threads) const {
    ThreadAnalysis analysis;
    analysis.consecutive_growths = consecutive_thread_increases;

    if (history.size() < 2) {
        return analysis;
    }

    const auto &oldest = history.front();
    const auto &latest = history.back();

    auto duration_sec = chrono::duration_cast<chrono::seconds>(latest.timestamp - oldest.timestamp).count();
    if (duration_sec <= 0) duration_sec = 1;

    analysis.total_thread_growth = current_threads - oldest.threads;
    analysis.thread_growth_rate_per_sec = (double)analysis.total_thread_growth / (double)duration_sec;

    // Linear regression slope on thread count
    double sum_t = 0.0, sum_th = 0.0, sum_t_th = 0.0, sum_t2 = 0.0;
    int n = history.size();

    for (const auto &sample : history) {
        double t = chrono::duration_cast<chrono::milliseconds>(sample.timestamp - oldest.timestamp).count() / 1000.0;
        double th = (double)sample.threads;
        sum_t += t;
        sum_th += th;
        sum_t_th += (t * th);
        sum_t2 += (t * t);
    }

    double denominator = (n * sum_t2 - sum_t * sum_t);
    double slope = 0.0;
    if (abs(denominator) > 1e-6) {
        slope = (n * sum_t_th - sum_t * sum_th) / denominator;
    }

    bool slope_positive = slope > 0.05;
    bool consecutive_hit = consecutive_thread_increases >= config.consecutive_thread_growth_threshold;
    bool growth_hit = analysis.total_thread_growth >= config.min_thread_leak_count;

    if (consecutive_hit && growth_hit && slope_positive) {
        analysis.thread_leak_detected = true;
        if (consecutive_thread_increases >= config.consecutive_thread_growth_threshold + 2 && slope > 0.2) {
            analysis.is_confirmed = true;
            analysis.confidence_percent = min(99.0, 60.0 + consecutive_thread_increases * 7.0);
        } else {
            analysis.confidence_percent = min(75.0, 40.0 + consecutive_thread_increases * 6.0);
        }

        if (slope > 0.01 && current_threads < config.thread_critical_count) {
            double remaining = config.thread_critical_count - current_threads;
            analysis.est_seconds_to_exhaustion = remaining / slope;
        }

        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Unbounded thread spawning detected under heavy workload: "
            << consecutive_thread_increases << " consecutive increases (+"
            << analysis.total_thread_growth << " threads net, rate +"
            << (slope > 0 ? slope : analysis.thread_growth_rate_per_sec) << " threads/s).";
        if (analysis.est_seconds_to_exhaustion > 0) {
            oss << " Estimated thread exhaustion in " << formatDuration((long)analysis.est_seconds_to_exhaustion) << ".";
        }
        analysis.diagnostic_summary = oss.str();
    } else {
        analysis.diagnostic_summary = "Thread pool activity within stable concurrency parameters.";
    }

    return analysis;
}

bool ThresholdAlertManager::dispatchPosixAlertSignal(int pid, int signum, const string &reason, AlertRecord &out_alert) {
    out_alert.timestamp = getCurrentTimestamp();
    out_alert.pid = pid;
    out_alert.type = AlertType::ALERT_POSIX_SIGNAL_SENT;
    out_alert.severity = AlertSeverity::SEV_WARNING;
    out_alert.posix_signal_sent = signum;
    out_alert.title = "REAL-TIME POSIX SIGNAL DISPATCHED: " + getPosixSignalName(signum);

    bool sent = Optimizer::sendSignal(pid, signum);
    ostringstream oss;
    oss << "Real-time POSIX signal " << getPosixSignalName(signum) << " dispatched to PID " << pid
        << ". Reason: " << reason << ". Status: " << (sent ? "SUCCESS" : "FAILED / NO PERMISSION");
    out_alert.message = oss.str();
    out_alert.mitigation_applied = "POSIX Signal " + to_string(signum);
    return sent;
}

bool ThresholdAlertManager::applyMitigation(const Processinfo &proc, const string &reason, AlertRecord &out_alert) {
    out_alert.timestamp = getCurrentTimestamp();
    out_alert.pid = proc.pid;
    out_alert.app_name = proc.name;
    out_alert.severity = AlertSeverity::SEV_CRITICAL;
    out_alert.current_rss_mb = (double)proc.vm_rss / 1024.0;
    out_alert.threshold_mb = config.mem_critical_mb;
    out_alert.current_threads = proc.threads;
    out_alert.threshold_threads = config.thread_critical_count;

    if (config.mitigation == MitigationPolicy::MITIGATE_TERMINATE) {
        out_alert.type = AlertType::ALERT_RESOURCE_EXHAUSTION_PREVENTED;
        bool killed = Optimizer::terminateProcess(proc.pid, false);
        out_alert.posix_signal_sent = SIGTERM;
        out_alert.title = "PROCESS TERMINATED VIA POSIX SIGTERM/SIGKILL";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Proactively terminated process '" << proc.name << "' (PID " << proc.pid
            << ") via POSIX signal to prevent host resource exhaustion. "
            << "Memory: " << out_alert.current_rss_mb << " MB, Threads: " << proc.threads
            << ". Reason: " << reason << ". Status: " << (killed ? "SUCCESS" : "SENT");
        out_alert.message = oss.str();
        out_alert.mitigation_applied = "POSIX Termination (SIGTERM/SIGKILL)";
        return killed;
    } else if (config.mitigation == MitigationPolicy::MITIGATE_PAUSE) {
        out_alert.type = AlertType::ALERT_THREAD_EXHAUSTION_PREVENTED;
        bool paused = Optimizer::pauseProcess(proc.pid);
        out_alert.posix_signal_sent = SIGSTOP;
        out_alert.title = "RUNAWAY PROCESS FROZEN VIA POSIX SIGSTOP";
        ostringstream oss;
        oss << "Dispatched POSIX SIGSTOP to pause process PID " << proc.pid
            << " (" << proc.threads << " threads) under heavy workload to prevent thread table exhaustion. Status: "
            << (paused ? "PAUSED" : "FAILED");
        out_alert.message = oss.str();
        out_alert.mitigation_applied = "POSIX SIGSTOP (Process Paused)";
        return paused;
    } else if (config.mitigation == MitigationPolicy::MITIGATE_SIGNAL) {
        int sig = config.posix_alert_signal > 0 ? config.posix_alert_signal : SIGUSR1;
        out_alert.type = AlertType::ALERT_POSIX_SIGNAL_SENT;
        out_alert.posix_signal_sent = sig;
        bool sent = Optimizer::sendSignal(proc.pid, sig);
        out_alert.title = "POSIX ALERT SIGNAL DISPATCHED (" + getPosixSignalName(sig) + ")";
        ostringstream oss;
        oss << "Dispatched POSIX signal " << getPosixSignalName(sig) << " to PID " << proc.pid
            << " notifying process of impending exhaustion. Reason: " << reason;
        out_alert.message = oss.str();
        out_alert.mitigation_applied = "POSIX Signal " + to_string(sig);
        return sent;
    } else if (config.mitigation == MitigationPolicy::MITIGATE_OPTIMIZE) {
        out_alert.type = AlertType::ALERT_RESOURCE_EXHAUSTION_PREVENTED;
        string purge_msg;
        Optimizer::purgeSystemMemory(purge_msg);
        Optimizer::reducePriority(proc.pid, 19);
        out_alert.title = "SYSTEM PURGE & PRIORITY THROTTLE APPLIED";
        ostringstream oss;
        oss << "Purged system memory caches and throttled scheduling priority for PID " << proc.pid
            << " to relieve workload pressure. " << purge_msg;
        out_alert.message = oss.str();
        out_alert.mitigation_applied = "Cache Purge & Renice";
    }

    if (config.enable_os_notifications) {
        string sub = "Process: " + proc.name + " (PID " + to_string(proc.pid) + ")";
        OSNotifier::sendNotification("AppSentry Mitigation: " + out_alert.title,
                                     out_alert.message,
                                     OSNotifier::Urgency::CRITICAL,
                                     sub);
    }

    return true;
}

vector<AlertRecord> ThresholdAlertManager::processSample(const Processinfo &proc) {
    vector<AlertRecord> generated_alerts;
    auto now = chrono::system_clock::now();
    double current_rss_mb = (double)proc.vm_rss / 1024.0;
    int current_threads = proc.threads;

    // Track consecutive memory and thread increases
    if (!history.empty()) {
        const auto &last_sample = history.back();
        // Memory tracking
        if (proc.vm_rss > last_sample.vm_rss_kb) {
            consecutive_memory_increases++;
        } else if (proc.vm_rss < last_sample.vm_rss_kb) {
            long dropped_kb = last_sample.vm_rss_kb - proc.vm_rss;
            if (dropped_kb > 1024) {
                consecutive_memory_increases = max(0, consecutive_memory_increases - 2);
            } else {
                consecutive_memory_increases = max(0, consecutive_memory_increases - 1);
            }
        }

        // Thread tracking
        if (proc.threads > last_sample.threads) {
            consecutive_thread_increases++;
        } else if (proc.threads < last_sample.threads) {
            consecutive_thread_increases = max(0, consecutive_thread_increases - 1);
        }
    }

    // Add to sliding history
    ResourceSnapshot snap{now, proc.vm_rss, proc.vm_size, proc.cpu_usage, proc.threads};
    history.push_back(snap);
    if (history.size() > config.history_window_size) {
        history.pop_front();
    }

    if (!initial_set) {
        initial_rss_kb = proc.vm_rss;
        initial_threads = proc.threads;
        initial_set = true;
    }

    // Run analyses
    LeakAnalysis leak = analyzeMemoryLeak(current_rss_mb);
    ThreadAnalysis thread_analysis = analyzeThreadExhaustion(current_threads);

    // 1. CPU Threshold Check
    if (proc.cpu_usage >= config.cpu_threshold_pct) {
        AlertRecord ar{};
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = AlertType::ALERT_CPU_HIGH;
        ar.severity = AlertSeverity::SEV_WARNING;
        ar.title = "CPU CONSUMPTION EXCEEDED THRESHOLD";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Process CPU consumption is " << proc.cpu_usage << "% (configured limit: "
            << config.cpu_threshold_pct << "%).";
        ar.message = oss.str();
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.current_threads = current_threads;
        ar.threshold_threads = config.thread_critical_count;
        ar.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        ar.est_seconds_to_exhaustion = leak.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Warning logged";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 2. Memory Leak Alerts
    if (leak.leak_detected) {
        AlertRecord ar{};
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = leak.is_confirmed ? AlertType::ALERT_LEAK_CONFIRMED : AlertType::ALERT_LEAK_SUSPECTED;
        ar.severity = leak.is_confirmed ? AlertSeverity::SEV_CRITICAL : AlertSeverity::SEV_WARNING;
        ar.title = leak.is_confirmed ? "CRITICAL: MEMORY LEAK CONFIRMED" : "WARNING: POTENTIAL MEMORY LEAK DETECTED";
        ar.message = leak.diagnostic_summary;
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.current_threads = current_threads;
        ar.threshold_threads = config.thread_critical_count;
        ar.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        ar.est_seconds_to_exhaustion = leak.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Proactive leak alert dispatched";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 3. Memory Warning Threshold
    if (current_rss_mb >= config.mem_warning_mb && current_rss_mb < config.mem_critical_mb) {
        AlertRecord ar{};
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = AlertType::ALERT_MEM_HIGH;
        ar.severity = AlertSeverity::SEV_WARNING;
        ar.title = "MEMORY WARNING THRESHOLD REACHED";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Resident memory reached " << current_rss_mb << " MB, exceeding warning threshold of "
            << config.mem_warning_mb << " MB.";
        ar.message = oss.str();
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_warning_mb;
        ar.current_threads = current_threads;
        ar.threshold_threads = config.thread_critical_count;
        ar.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        ar.est_seconds_to_exhaustion = leak.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Warning logged";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 4. Thread Warning Threshold under heavy workloads
    if (current_threads >= config.thread_warning_count && current_threads < config.thread_critical_count) {
        AlertRecord ar{};
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = AlertType::ALERT_THREAD_HIGH;
        ar.severity = AlertSeverity::SEV_WARNING;
        ar.title = "THREAD THRESHOLD WARNING UNDER HEAVY WORKLOAD";
        ostringstream oss;
        oss << "Active thread count reached " << current_threads << " threads (warning threshold: "
            << config.thread_warning_count << " threads).";
        ar.message = oss.str();
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.current_threads = current_threads;
        ar.threshold_threads = config.thread_warning_count;
        ar.growth_rate_mb_s = thread_analysis.thread_growth_rate_per_sec;
        ar.est_seconds_to_exhaustion = thread_analysis.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Thread warning logged";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 5. Thread Leak Alerts under heavy workloads
    if (thread_analysis.thread_leak_detected) {
        AlertRecord ar{};
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = thread_analysis.is_confirmed ? AlertType::ALERT_THREAD_LEAK_CONFIRMED : AlertType::ALERT_THREAD_LEAK_SUSPECTED;
        ar.severity = thread_analysis.is_confirmed ? AlertSeverity::SEV_CRITICAL : AlertSeverity::SEV_WARNING;
        ar.title = thread_analysis.is_confirmed ? "CRITICAL: THREAD EXHAUSTION LEAK CONFIRMED" : "WARNING: UNJOINED THREAD EXPANSION DETECTED";
        ar.message = thread_analysis.diagnostic_summary;
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.current_threads = current_threads;
        ar.threshold_threads = config.thread_critical_count;
        ar.growth_rate_mb_s = thread_analysis.thread_growth_rate_per_sec;
        ar.est_seconds_to_exhaustion = thread_analysis.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Proactive thread alert dispatched";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 6. Imminent Resource / Thread Exhaustion
    bool mem_imminent = (leak.est_seconds_to_exhaustion > 0 && leak.est_seconds_to_exhaustion <= 45.0 && current_rss_mb < config.mem_critical_mb);
    bool thread_imminent = (thread_analysis.est_seconds_to_exhaustion > 0 && thread_analysis.est_seconds_to_exhaustion <= 45.0 && current_threads < config.thread_critical_count);

    if (mem_imminent || thread_imminent) {
        AlertRecord ar{};
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = mem_imminent ? AlertType::ALERT_RESOURCE_EXHAUSTION_IMMINENT : AlertType::ALERT_THREAD_EXHAUSTION_IMMINENT;
        ar.severity = AlertSeverity::SEV_CRITICAL;
        ar.title = mem_imminent ? "IMMINENT MEMORY EXHAUSTION DETECTED" : "IMMINENT THREAD EXHAUSTION DETECTED";
        ostringstream oss;
        if (mem_imminent) {
            oss << "Projected memory limit (" << config.mem_critical_mb << " MB) breach in approximately "
                << formatDuration((long)leak.est_seconds_to_exhaustion) << "!";
        } else {
            oss << "Projected thread limit (" << config.thread_critical_count << " threads) exhaustion in approximately "
                << formatDuration((long)thread_analysis.est_seconds_to_exhaustion) << "!";
        }
        ar.message = oss.str();
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.current_threads = current_threads;
        ar.threshold_threads = config.thread_critical_count;
        ar.growth_rate_mb_s = mem_imminent ? leak.growth_rate_mb_per_sec : thread_analysis.thread_growth_rate_per_sec;
        ar.est_seconds_to_exhaustion = mem_imminent ? leak.est_seconds_to_exhaustion : thread_analysis.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Early warning dispatched";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 7. Dispatch Real-Time POSIX Alert Signal (e.g. SIGUSR1) if configured and new alerts fired
    if (!generated_alerts.empty() && config.posix_alert_signal > 0) {
        AlertRecord sig_record{};
        string reason = generated_alerts[0].title;
        if (dispatchPosixAlertSignal(proc.pid, config.posix_alert_signal, reason, sig_record)) {
            generated_alerts.push_back(sig_record);
            recorded_alerts.push_back(sig_record);
        }
    }

    // 8. Resource Exhaustion Breaches (Memory or Threads) & Automated Signal Mitigation
    bool mem_breach = current_rss_mb >= config.mem_critical_mb;
    bool thread_breach = current_threads >= config.thread_critical_count;

    if (mem_breach || thread_breach) {
        AlertRecord breach_alert{};
        breach_alert.timestamp = getCurrentTimestamp();
        breach_alert.pid = proc.pid;
        breach_alert.app_name = proc.name;
        breach_alert.type = mem_breach ? AlertType::ALERT_RESOURCE_EXHAUSTION_BREACHED : AlertType::ALERT_THREAD_EXHAUSTION_BREACHED;
        breach_alert.severity = AlertSeverity::SEV_CRITICAL;
        breach_alert.title = mem_breach ? "CRITICAL: MEMORY RESOURCE EXHAUSTION BREACHED" : "CRITICAL: THREAD EXHAUSTION BREACHED";
        ostringstream oss;
        if (mem_breach) {
            oss << "Process memory (" << fixed << setprecision(1) << current_rss_mb << " MB) breached critical limit ("
                << config.mem_critical_mb << " MB)! Process-level exhaustion active.";
        } else {
            oss << "Active threads (" << current_threads << ") breached critical limit ("
                << config.thread_critical_count << " threads)! Thread exhaustion active under heavy load.";
        }
        breach_alert.message = oss.str();
        breach_alert.current_rss_mb = current_rss_mb;
        breach_alert.threshold_mb = config.mem_critical_mb;
        breach_alert.current_threads = current_threads;
        breach_alert.threshold_threads = config.thread_critical_count;
        breach_alert.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        breach_alert.est_seconds_to_exhaustion = 0.0;
        breach_alert.mitigation_applied = "None";

        generated_alerts.push_back(breach_alert);
        recorded_alerts.push_back(breach_alert);

        // Automated mitigation via POSIX signals
        if (!mitigation_executed && config.mitigation != MitigationPolicy::MITIGATE_NOTIFY) {
            AlertRecord mitig_alert{};
            string reason = mem_breach ? "Memory exceeded " + to_string((int)config.mem_critical_mb) + " MB"
                                       : "Threads exceeded " + to_string(config.thread_critical_count);
            if (applyMitigation(proc, reason, mitig_alert)) {
                mitigation_executed = true;
                generated_alerts.push_back(mitig_alert);
                recorded_alerts.push_back(mitig_alert);
            }
        }
    }

    // 9. Dispatch Native OS Desktop UI Notifications (macOS / Linux / Windows)
    if (!generated_alerts.empty() && config.enable_os_notifications) {
        // Dispatch desktop notification for primary alert
        const auto &primary_alert = generated_alerts[0];
        OSNotifier::Urgency urg = (primary_alert.severity == AlertSeverity::SEV_CRITICAL) ?
                                  OSNotifier::Urgency::CRITICAL : OSNotifier::Urgency::NORMAL;
        string sub = "Process: " + proc.name + " (PID " + to_string(proc.pid) + ")";
        OSNotifier::sendNotification("AppSentry: " + primary_alert.title,
                                     primary_alert.message,
                                     urg,
                                     sub);
    }

    return generated_alerts;
}

string ThresholdAlertManager::formatConsoleAlert(const AlertRecord &record) {
    ostringstream oss;
    string color_start = "";
    string color_end = "\033[0m";

    if (record.severity == AlertSeverity::SEV_CRITICAL) {
        color_start = "\033[1;31m"; // Bold Red
    } else if (record.severity == AlertSeverity::SEV_WARNING) {
        color_start = "\033[1;33m"; // Bold Yellow
    } else {
        color_start = "\033[1;36m"; // Bold Cyan
    }

    oss << "\n" << color_start << "====================================================================\n";
    oss << " [ALERT] " << record.title << "\n";
    oss << " Time: " << record.timestamp << " | PID: " << record.pid << " | Process: " << record.app_name << "\n";
    if (record.threshold_mb > 0) {
        oss << " Memory: " << fixed << setprecision(1) << record.current_rss_mb << " MB (Limit: " << record.threshold_mb << " MB)\n";
    }
    if (record.current_threads > 0) {
        oss << " Threads: " << record.current_threads << " (Limit: " << record.threshold_threads << ")\n";
    }
    if (record.posix_signal_sent > 0) {
        oss << " POSIX Signal: " << getPosixSignalName(record.posix_signal_sent) << "\n";
    }
    oss << " Details: " << record.message << "\n";
    if (!record.mitigation_applied.empty() && record.mitigation_applied != "None") {
        oss << " Mitigation Action: " << record.mitigation_applied << "\n";
    }
    oss << "====================================================================" << color_end << "\n";
    return oss.str();
}

string ThresholdAlertManager::formatReportAlert(const AlertRecord &record) {
    ostringstream oss;
    oss << "[ALERT_EVENT]\n";
    oss << "Timestamp: " << record.timestamp << "\n";
    oss << "Type: " << getAlertTypeName(record.type) << "\n";
    oss << "Severity: " << (record.severity == AlertSeverity::SEV_CRITICAL ? "CRITICAL" : "WARNING") << "\n";
    oss << "PID: " << record.pid << "\n";
    oss << "Process: " << record.app_name << "\n";
    oss << "Memory_MB: " << fixed << setprecision(1) << record.current_rss_mb << "\n";
    oss << "Threshold_MB: " << record.threshold_mb << "\n";
    oss << "Threads: " << record.current_threads << "\n";
    oss << "Threshold_Threads: " << record.threshold_threads << "\n";
    if (record.posix_signal_sent > 0) {
        oss << "POSIX_Signal: " << getPosixSignalName(record.posix_signal_sent) << "\n";
    }
    oss << "Message: " << record.message << "\n";
    if (!record.mitigation_applied.empty()) {
        oss << "Mitigation: " << record.mitigation_applied << "\n";
    }
    oss << "------------------------------------\n";
    return oss.str();
}
