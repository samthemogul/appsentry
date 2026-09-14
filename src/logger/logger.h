#ifndef LOGGER_H
#define LOGGER_H

#include "../shared/headers/process_info.h"
#include "../shared/headers/util.h"
#include "../shared/headers/constants.h"
#include "../alert/threshold_alert.h"
#include <string>
#include <vector>

struct AppReportSummary {
    std::string app_name;
    long total_runtime_sec = 0;
    long peak_memory_kb = 0;
    int peak_threads = 0;
    double avg_cpu_usage = 0.0;
    std::string first_monitored;
    std::string last_active;
    int samples_count = 0;
    int cpu_alerts = 0;
    int mem_alerts = 0;
    int leak_alerts = 0;
    int thread_alerts = 0;
    int exhaustion_breaches = 0;
    int preventions_executed = 0;
    int thread_exhaustion_prevented = 0;
    int posix_signals_dispatched = 0;
    std::vector<std::string> recent_alerts;
};

void createReportsRepository();
void saveReport(string app_name, Processinfo& details);
void saveAlertToReport(const string &app_name, const AlertRecord &alert);
string findReportFile(const string &app_name);
bool parseReportSummary(const string &app_name, AppReportSummary &summary);
void displayReport(const string &app_name);

#endif