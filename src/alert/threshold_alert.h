#ifndef THRESHOLD_ALERT_H
#define THRESHOLD_ALERT_H

#include <string>
#include <vector>
#include <deque>
#include <chrono>
#include <csignal>
#include "../shared/headers/process_info.h"

enum class AlertSeverity {
    SEV_INFO,
    SEV_WARNING,
    SEV_CRITICAL
};

enum class AlertType {
    ALERT_NONE,
    ALERT_CPU_HIGH,
    ALERT_MEM_HIGH,
    ALERT_LEAK_SUSPECTED,
    ALERT_LEAK_CONFIRMED,
    ALERT_RESOURCE_EXHAUSTION_IMMINENT,
    ALERT_RESOURCE_EXHAUSTION_BREACHED,
    ALERT_RESOURCE_EXHAUSTION_PREVENTED,
    ALERT_THREAD_HIGH,
    ALERT_THREAD_LEAK_SUSPECTED,
    ALERT_THREAD_LEAK_CONFIRMED,
    ALERT_THREAD_EXHAUSTION_IMMINENT,
    ALERT_THREAD_EXHAUSTION_BREACHED,
    ALERT_THREAD_EXHAUSTION_PREVENTED,
    ALERT_POSIX_SIGNAL_SENT
};

enum class MitigationPolicy {
    MITIGATE_NOTIFY,    // Proactive visual & report warning
    MITIGATE_SIGNAL,    // Dispatch POSIX alert signal (SIGUSR1/SIGUSR2) to target process
    MITIGATE_PAUSE,     // Dispatch POSIX SIGSTOP to freeze runaway thread spawning under load
    MITIGATE_OPTIMIZE,  // Attempt memory purge / priority reduction
    MITIGATE_TERMINATE  // Proactively terminate runaway leaking process (SIGTERM/SIGKILL)
};

struct ThresholdConfig {
    double cpu_threshold_pct = 80.0;            // Max sustained CPU % before alert
    double mem_warning_mb = 400.0;              // Early warning memory threshold (MB)
    double mem_critical_mb = 800.0;             // Critical exhaustion threshold (MB)
    int consecutive_growth_threshold = 4;       // Consecutive memory sample increases to flag leak
    double min_leak_growth_mb = 5.0;            // Minimum net memory growth in MB
    
    // Thread exhaustion configuration under heavy workloads
    int thread_warning_count = 50;              // Warning threshold for active thread count
    int thread_critical_count = 150;            // Critical thread exhaustion limit
    int consecutive_thread_growth_threshold = 4;// Consecutive thread increases indicating leak
    int min_thread_leak_count = 5;              // Minimum thread count increase to flag leak

    // POSIX Signal configuration
    int posix_alert_signal = SIGUSR1;           // POSIX signal dispatched on alert (0 = none, SIGUSR1 default)
    bool enable_posix_timer = true;             // Drive real-time sampling via POSIX SIGALRM interval timer
    bool enable_os_notifications = true;        // Dispatch native OS desktop UI notifications (macOS, Linux, Windows)

    size_t history_window_size = 20;            // Historical sliding window length
    MitigationPolicy mitigation = MitigationPolicy::MITIGATE_NOTIFY;
};

struct ResourceSnapshot {
    std::chrono::system_clock::time_point timestamp;
    long vm_rss_kb;
    long vm_size_kb;
    double cpu_usage;
    int threads;
};

struct LeakAnalysis {
    bool leak_detected = false;
    bool is_confirmed = false;
    int consecutive_growths = 0;
    double total_growth_mb = 0.0;
    double growth_rate_mb_per_sec = 0.0;
    double est_seconds_to_exhaustion = -1.0; // -1 if not trending toward limit
    double confidence_percent = 0.0;
    std::string diagnostic_summary;
};

struct ThreadAnalysis {
    bool thread_leak_detected = false;
    bool is_confirmed = false;
    int consecutive_growths = 0;
    int total_thread_growth = 0;
    double thread_growth_rate_per_sec = 0.0;
    double est_seconds_to_exhaustion = -1.0; // -1 if not trending toward limit
    double confidence_percent = 0.0;
    std::string diagnostic_summary;
};

struct AlertRecord {
    std::string timestamp;
    int pid;
    std::string app_name;
    AlertType type;
    AlertSeverity severity;
    std::string title;
    std::string message;
    double current_rss_mb;
    double threshold_mb;
    int current_threads;
    int threshold_threads;
    double growth_rate_mb_s;
    double est_seconds_to_exhaustion;
    std::string mitigation_applied;
    int posix_signal_sent = 0;
};

class ThresholdAlertManager {
private:
    ThresholdConfig config;
    std::deque<ResourceSnapshot> history;
    std::vector<AlertRecord> recorded_alerts;
    int consecutive_memory_increases = 0;
    int consecutive_thread_increases = 0;
    long initial_rss_kb = 0;
    int initial_threads = 0;
    bool initial_set = false;
    bool mitigation_executed = false;
    std::chrono::system_clock::time_point last_alert_time;

public:
    explicit ThresholdAlertManager(const ThresholdConfig &cfg = ThresholdConfig());

    void setConfig(const ThresholdConfig &cfg);
    const ThresholdConfig& getConfig() const;

    // Process a new sample: checks thresholds, runs memory leak and thread exhaustion detection
    std::vector<AlertRecord> processSample(const Processinfo &proc);

    // Analyze current memory history for leak patterns
    LeakAnalysis analyzeMemoryLeak(double current_rss_mb) const;

    // Analyze current thread history for thread exhaustion and thread leaks under heavy workloads
    ThreadAnalysis analyzeThreadExhaustion(int current_threads) const;

    // Dispatch POSIX alert signal (e.g. SIGUSR1/SIGUSR2) to target process
    bool dispatchPosixAlertSignal(int pid, int signum, const std::string &reason, AlertRecord &out_alert);

    // Execute configured mitigation action (POSIX signals: SIGSTOP, SIGTERM, SIGKILL, or cache purge)
    bool applyMitigation(const Processinfo &proc, const std::string &reason, AlertRecord &out_alert);

    // Retrieve all logged alert records
    const std::vector<AlertRecord>& getAlerts() const;

    // POSIX Real-Time Interval Timer (SIGALRM / ITIMER_REAL)
    static void setupRealTimeTimer(int interval_sec, int interval_usec = 0);
    static void stopRealTimeTimer();
    static bool isTimerTickFired();
    static void resetTimerTick();

    // Formatted representations
    static std::string formatConsoleAlert(const AlertRecord &record);
    static std::string formatReportAlert(const AlertRecord &record);
    static std::string getAlertTypeName(AlertType type);
    static std::string getPosixSignalName(int signum);

    // Reset tracking state
    void reset();
};

#endif
