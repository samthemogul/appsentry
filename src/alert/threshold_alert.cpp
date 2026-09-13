#include "threshold_alert.h"
#include "../optimizer/optimizer.h"
#include "../shared/headers/util.h"
#include <iostream>
#include <iomanip>
#include <sstream>
#include <cmath>
#include <numeric>

using namespace std;

ThresholdAlertManager::ThresholdAlertManager(const ThresholdConfig &cfg)
    : config(cfg), consecutive_increases(0), initial_rss_kb(0), initial_set(false), mitigation_executed(false)
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
    consecutive_increases = 0;
    initial_rss_kb = 0;
    initial_set = false;
    mitigation_executed = false;
}

const vector<AlertRecord>& ThresholdAlertManager::getAlerts() const {
    return recorded_alerts;
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
        default: return "INFO";
    }
}

LeakAnalysis ThresholdAlertManager::analyzeMemoryLeak(double current_rss_mb) const {
    LeakAnalysis analysis;
    analysis.consecutive_growths = consecutive_increases;

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
    bool consecutive_hit = consecutive_increases >= config.consecutive_growth_threshold;
    bool growth_hit = analysis.total_growth_mb >= config.min_leak_growth_mb;

    if (consecutive_hit && growth_hit && slope_positive) {
        analysis.leak_detected = true;
        if (consecutive_increases >= config.consecutive_growth_threshold + 2 && slope > 0.05) {
            analysis.is_confirmed = true;
            analysis.confidence_percent = min(99.0, 60.0 + consecutive_increases * 6.0);
        } else {
            analysis.confidence_percent = min(75.0, 40.0 + consecutive_increases * 5.0);
        }

        // Time to resource exhaustion
        if (slope > 0.001 && current_rss_mb < config.mem_critical_mb) {
            double remaining_mb = config.mem_critical_mb - current_rss_mb;
            analysis.est_seconds_to_exhaustion = remaining_mb / slope;
        }

        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Monotonic memory expansion across " << consecutive_increases << " consecutive intervals (+"
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

bool ThresholdAlertManager::applyMitigation(const Processinfo &proc, const string &reason, AlertRecord &out_alert) {
    out_alert.timestamp = getCurrentTimestamp();
    out_alert.pid = proc.pid;
    out_alert.app_name = proc.name;
    out_alert.type = AlertType::ALERT_RESOURCE_EXHAUSTION_PREVENTED;
    out_alert.severity = AlertSeverity::SEV_CRITICAL;
    out_alert.current_rss_mb = (double)proc.vm_rss / 1024.0;
    out_alert.threshold_mb = config.mem_critical_mb;

    if (config.mitigation == MitigationPolicy::MITIGATE_TERMINATE) {
        bool killed = Optimizer::terminateProcess(proc.pid, false);
        out_alert.title = "PROCESS TERMINATED TO PREVENT SYSTEM EXHAUSTION";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Proactively terminated leaking process '" << proc.name << "' (PID " << proc.pid
            << ") at " << out_alert.current_rss_mb << " MB (limit: " << config.mem_critical_mb << " MB). "
            << "Reason: " << reason << ". Termination status: " << (killed ? "SUCCESS" : "SENT");
        out_alert.message = oss.str();
        out_alert.mitigation_applied = "Proactive Termination (SIGTERM/SIGKILL)";
        return killed;
    } else if (config.mitigation == MitigationPolicy::MITIGATE_OPTIMIZE) {
        string purge_msg;
        Optimizer::purgeSystemMemory(purge_msg);
        Optimizer::reducePriority(proc.pid, 19);
        out_alert.title = "SYSTEM PURGE & PRIORITY THROTTLE APPLIED";
        ostringstream oss;
        oss << "Attempted memory purge and throttled priority for PID " << proc.pid
            << " to mitigate memory pressure. " << purge_msg;
        out_alert.message = oss.str();
        out_alert.mitigation_applied = "Memory Cache Purge & Process Priority Throttle";
        return true;
    }

    return false;
}

vector<AlertRecord> ThresholdAlertManager::processSample(const Processinfo &proc) {
    vector<AlertRecord> generated_alerts;
    auto now = chrono::system_clock::now();
    double current_rss_mb = (double)proc.vm_rss / 1024.0;

    // Track consecutive memory growth
    if (!history.empty()) {
        const auto &last_sample = history.back();
        if (proc.vm_rss > last_sample.vm_rss_kb) {
            consecutive_increases++;
        } else if (proc.vm_rss < last_sample.vm_rss_kb) {
            long dropped_kb = last_sample.vm_rss_kb - proc.vm_rss;
            if (dropped_kb > 1024) { // Reclaimed more than 1MB
                consecutive_increases = max(0, consecutive_increases - 2);
            } else {
                consecutive_increases = max(0, consecutive_increases - 1);
            }
        }
    }

    // Add to history
    MemorySnapshot snap{now, proc.vm_rss, proc.vm_size, proc.cpu_usage};
    history.push_back(snap);
    if (history.size() > config.history_window_size) {
        history.pop_front();
    }

    if (!initial_set) {
        initial_rss_kb = proc.vm_rss;
        initial_set = true;
    }

    // 1. Run leak analysis
    LeakAnalysis leak = analyzeMemoryLeak(current_rss_mb);

    // 2. Evaluate CPU threshold
    if (proc.cpu_usage >= config.cpu_threshold_pct) {
        AlertRecord ar;
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = AlertType::ALERT_CPU_HIGH;
        ar.severity = AlertSeverity::SEV_WARNING;
        ar.title = "CPU THRESHOLD EXCEEDED";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Process CPU consumption reached " << proc.cpu_usage << "% (configured threshold: "
            << config.cpu_threshold_pct << "%).";
        ar.message = oss.str();
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        ar.est_seconds_to_exhaustion = leak.est_seconds_to_exhaustion;
        ar.mitigation_applied = "None";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 3. Evaluate Proactive Memory Leak Alerts
    if (leak.leak_detected) {
        AlertRecord ar;
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = leak.is_confirmed ? AlertType::ALERT_LEAK_CONFIRMED : AlertType::ALERT_LEAK_SUSPECTED;
        ar.severity = leak.is_confirmed ? AlertSeverity::SEV_CRITICAL : AlertSeverity::SEV_WARNING;
        ar.title = leak.is_confirmed ? "CRITICAL: MEMORY LEAK CONFIRMED" : "WARNING: POTENTIAL MEMORY LEAK DETECTED";
        ar.message = leak.diagnostic_summary;
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        ar.est_seconds_to_exhaustion = leak.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Proactive Monitoring Alert";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 4. Evaluate Memory Warning Threshold
    if (current_rss_mb >= config.mem_warning_mb && current_rss_mb < config.mem_critical_mb) {
        AlertRecord ar;
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = AlertType::ALERT_MEM_HIGH;
        ar.severity = AlertSeverity::SEV_WARNING;
        ar.title = "MEMORY USAGE WARNING THRESHOLD REACHED";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Resident memory is " << current_rss_mb << " MB, exceeding warning threshold of "
            << config.mem_warning_mb << " MB.";
        ar.message = oss.str();
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_warning_mb;
        ar.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        ar.est_seconds_to_exhaustion = leak.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Warning logged";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 5. Evaluate Imminent Resource Exhaustion (early proactive warning based on trajectory)
    if (leak.est_seconds_to_exhaustion > 0 && leak.est_seconds_to_exhaustion <= 45.0 && current_rss_mb < config.mem_critical_mb) {
        AlertRecord ar;
        ar.timestamp = getCurrentTimestamp();
        ar.pid = proc.pid;
        ar.app_name = proc.name;
        ar.type = AlertType::ALERT_RESOURCE_EXHAUSTION_IMMINENT;
        ar.severity = AlertSeverity::SEV_CRITICAL;
        ar.title = "IMMINENT RESOURCE EXHAUSTION DETECTED";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Process is on trajectory to breach critical limit (" << config.mem_critical_mb
            << " MB) in approximately " << formatDuration((long)leak.est_seconds_to_exhaustion)
            << "! Proactive action recommended.";
        ar.message = oss.str();
        ar.current_rss_mb = current_rss_mb;
        ar.threshold_mb = config.mem_critical_mb;
        ar.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        ar.est_seconds_to_exhaustion = leak.est_seconds_to_exhaustion;
        ar.mitigation_applied = "Proactive alert dispatched";
        generated_alerts.push_back(ar);
        recorded_alerts.push_back(ar);
    }

    // 6. Evaluate Resource Exhaustion Breached & Execute Mitigation
    if (current_rss_mb >= config.mem_critical_mb) {
        AlertRecord breach_alert;
        breach_alert.timestamp = getCurrentTimestamp();
        breach_alert.pid = proc.pid;
        breach_alert.app_name = proc.name;
        breach_alert.type = AlertType::ALERT_RESOURCE_EXHAUSTION_BREACHED;
        breach_alert.severity = AlertSeverity::SEV_CRITICAL;
        breach_alert.title = "CRITICAL: RESOURCE EXHAUSTION THRESHOLD BREACHED";
        ostringstream oss;
        oss << fixed << setprecision(1);
        oss << "Process memory (" << current_rss_mb << " MB) has breached critical limit of "
            << config.mem_critical_mb << " MB! Process-level resource exhaustion active.";
        breach_alert.message = oss.str();
        breach_alert.current_rss_mb = current_rss_mb;
        breach_alert.threshold_mb = config.mem_critical_mb;
        breach_alert.growth_rate_mb_s = leak.growth_rate_mb_per_sec;
        breach_alert.est_seconds_to_exhaustion = 0.0;
        breach_alert.mitigation_applied = "None";

        generated_alerts.push_back(breach_alert);
        recorded_alerts.push_back(breach_alert);

        // Proactive prevention: execute mitigation if enabled and not yet executed
        if (!mitigation_executed && config.mitigation != MitigationPolicy::MITIGATE_NOTIFY) {
            AlertRecord mitig_alert;
            if (applyMitigation(proc, "Exceeded critical memory threshold of " + to_string((int)config.mem_critical_mb) + " MB", mitig_alert)) {
                mitigation_executed = true;
                generated_alerts.push_back(mitig_alert);
                recorded_alerts.push_back(mitig_alert);
            }
        }
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
    oss << " Memory: " << fixed << setprecision(1) << record.current_rss_mb << " MB (Limit: " << record.threshold_mb << " MB)\n";
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
    oss << "Message: " << record.message << "\n";
    if (!record.mitigation_applied.empty()) {
        oss << "Mitigation: " << record.mitigation_applied << "\n";
    }
    oss << "------------------------------------\n";
    return oss.str();
}
