#ifndef THRESHOLD_ALERT_H
#define THRESHOLD_ALERT_H

#include <string>
#include <vector>
#include <deque>
#include <chrono>
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
    ALERT_RESOURCE_EXHAUSTION_PREVENTED
};

enum class MitigationPolicy {
    MITIGATE_NOTIFY,    // Proactive visual & report warning
    MITIGATE_OPTIMIZE,  // Attempt memory purge / priority reduction
    MITIGATE_TERMINATE  // Proactively terminate runaway leaking process to prevent system crash
};

struct ThresholdConfig {
    double cpu_threshold_pct = 80.0;            // Max sustained CPU % before alert
    double mem_warning_mb = 400.0;              // Early warning memory threshold (MB)
    double mem_critical_mb = 800.0;             // Critical exhaustion threshold (MB)
    int consecutive_growth_threshold = 4;       // Consecutive sample increases to flag leak
    double min_leak_growth_mb = 5.0;            // Minimum net growth in MB to trigger leak alert
    size_t history_window_size = 20;            // Historical sliding window length
    MitigationPolicy mitigation = MitigationPolicy::MITIGATE_NOTIFY;
};

struct MemorySnapshot {
    std::chrono::system_clock::time_point timestamp;
    long vm_rss_kb;
    long vm_size_kb;
    double cpu_usage;
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
    double growth_rate_mb_s;
    double est_seconds_to_exhaustion;
    std::string mitigation_applied;
};

class ThresholdAlertManager {
private:
    ThresholdConfig config;
    std::deque<MemorySnapshot> history;
    std::vector<AlertRecord> recorded_alerts;
    int consecutive_increases = 0;
    long initial_rss_kb = 0;
    bool initial_set = false;
    bool mitigation_executed = false;
    std::chrono::system_clock::time_point last_alert_time;

public:
    explicit ThresholdAlertManager(const ThresholdConfig &cfg = ThresholdConfig());

    void setConfig(const ThresholdConfig &cfg);
    const ThresholdConfig& getConfig() const;

    // Process a new sample: checks thresholds, runs leak detection, triggers prevention
    std::vector<AlertRecord> processSample(const Processinfo &proc);

    // Analyze current memory history for leak patterns
    LeakAnalysis analyzeMemoryLeak(double current_rss_mb) const;

    // Execute configured mitigation action (e.g. terminate leaking process before exhaustion)
    bool applyMitigation(const Processinfo &proc, const std::string &reason, AlertRecord &out_alert);

    // Retrieve all logged alert records
    const std::vector<AlertRecord>& getAlerts() const;

    // Formatted representations
    static std::string formatConsoleAlert(const AlertRecord &record);
    static std::string formatReportAlert(const AlertRecord &record);
    static std::string getAlertTypeName(AlertType type);

    // Reset tracking state
    void reset();
};

#endif
