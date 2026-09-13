#include "logger.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>
#include <algorithm>
#include <iomanip>

using namespace std;
namespace fs = std::filesystem;

string const reports_dir = "reports";

// Handle the creation of the reports repository
void createReportsRepository()
{
    try
    {
        if (!fs::exists(reports_dir))
        {
            fs::create_directories(reports_dir);
            cout << "Reports directory created successfully!" << endl;
        }
    }
    catch (const exception &e)
    {
        cerr << "Error creating reports directory: " << e.what() << endl;
    }
}

static string sanitizeName(const string &name)
{
    string s = name;
    toLowercase(s);
    for (char &c : s)
    {
        if (!isalnum(c) && c != '-' && c != '_')
        {
            c = '_';
        }
    }
    // remove consecutive underscores
    string clean = "";
    bool prev_us = false;
    for (char c : s)
    {
        if (c == '_')
        {
            if (!prev_us) clean += c;
            prev_us = true;
        }
        else
        {
            clean += c;
            prev_us = false;
        }
    }
    while (!clean.empty() && clean.back() == '_') clean.pop_back();
    while (!clean.empty() && clean.front() == '_') clean.erase(clean.begin());
    return clean.empty() ? "app" : clean;
}

string findReportFile(const string &app_name)
{
    if (!fs::exists(reports_dir)) return "";

    string clean = sanitizeName(app_name);

    // 1. Direct match in reports/
    string direct = reports_dir + "/" + clean + ".report";
    if (fs::exists(direct)) return direct;

    // 2. Exact match with original name
    string exact = reports_dir + "/" + app_name + ".report";
    if (fs::exists(exact)) return exact;

    // 3. Search directory for partial/case-insensitive match
    for (const auto &entry : fs::recursive_directory_iterator(reports_dir))
    {
        if (entry.is_regular_file())
        {
            string fname = entry.path().filename().string();
            string stem = entry.path().stem().string();
            if (caseInsensitiveContains(fname, clean) || caseInsensitiveContains(stem, app_name))
            {
                return entry.path().string();
            }
        }
    }

    return "";
}

// Handles writing report of application processes to file
void saveReport(string app_name, Processinfo &details)
{
    try
    {
        createReportsRepository();

        if (app_name.empty())
        {
            app_name = details.name;
        }

        string clean_name = sanitizeName(app_name);
        string file_path = reports_dir + "/" + clean_name + ".report";

        bool file_exists = fs::exists(file_path);
        ofstream process_report(file_path, ios::app);

        if (!process_report.is_open())
        {
            throw runtime_error("Error: Unable to open the application report file: " + file_path);
        }

        if (!file_exists)
        {
            process_report << "Process Report for " << details.name << "\n";
            process_report << "-----------------------------------\n\n";
        }

        // Writing in "key: value" format
        process_report << "PID: " << details.pid << "\n";
        process_report << "Name: " << details.name << "\n";
        process_report << "State: " << get_state_char(details.state) << "\n";
        process_report << "PPID: " << details.ppid << "\n";
        process_report << "Threads: " << details.threads << "\n";
        process_report << "Owner: " << details.owner << "\n";
        process_report << "Executable Path: " << details.exec_path << "\n";
        process_report << "Open Files: " << details.open_files << "\n";
        process_report << "Priority: " << details.priority << "\n";
        process_report << "Log Date Time: " << details.log_date_time << "\n";
        process_report << "Log Time: " << details.log_time << "\n";
        process_report << "CPU Usage: " << fixed << setprecision(1) << details.cpu_usage << "%\n";
        process_report << "Resident Memory (VmRSS): " << details.vm_rss << " KB\n";
        process_report << "Virtual Memory (VmSize): " << details.vm_size << " KB\n";
        process_report << "Disk Usage: " << details.disk_usage << " KB\n";
        process_report << "Start Time: " << details.start_time << " seconds since boot\n";
        process_report << "Elapsed Time: " << details.elapsed_time << " seconds\n";
        process_report << "Network Usage: " << details.network_usage << " KB\n";
        process_report << "Network Connections: " << details.network_connections << "\n";
        process_report << "------------------------------------\n\n";

        process_report.close();
    }
    catch (const exception &e)
    {
        cerr << e.what() << endl;
    }
}

void saveAlertToReport(const string &app_name, const AlertRecord &alert)
{
    try
    {
        createReportsRepository();
        string clean_name = sanitizeName(app_name.empty() ? alert.app_name : app_name);
        string file_path = reports_dir + "/" + clean_name + ".report";

        ofstream process_report(file_path, ios::app);
        if (process_report.is_open())
        {
            process_report << ThresholdAlertManager::formatReportAlert(alert);
            process_report.close();
        }
    }
    catch (const exception &e)
    {
        cerr << "Error saving alert to report: " << e.what() << endl;
    }
}

bool parseReportSummary(const string &app_name, AppReportSummary &summary)
{
    string file_path = findReportFile(app_name);
    if (file_path.empty())
    {
        return false;
    }

    ifstream file(file_path);
    if (!file.is_open())
    {
        return false;
    }

    summary.app_name = app_name;
    string line;
    double cpu_sum = 0.0;
    int cpu_count = 0;
    long max_rss = 0;
    long max_elapsed = 0;

    while (getline(file, line))
    {
        trim(line);
        if (line.rfind("Name:", 0) == 0)
        {
            string val = line.substr(5);
            trim(val);
            if (summary.app_name == app_name && !val.empty())
            {
                summary.app_name = val;
            }
        }
        else if (line.rfind("Log Date Time:", 0) == 0)
        {
            string ts = line.substr(14);
            trim(ts);
            if (summary.first_monitored.empty())
            {
                summary.first_monitored = ts;
            }
            summary.last_active = ts;
            summary.samples_count++;
        }
        else if (line.rfind("Resident Memory (VmRSS):", 0) == 0)
        {
            string val = line.substr(24);
            stringstream ss(val);
            long rss = 0;
            ss >> rss;
            if (rss > max_rss) max_rss = rss;
        }
        else if (line.rfind("CPU Usage:", 0) == 0)
        {
            string val = line.substr(10);
            stringstream ss(val);
            double cpu = 0.0;
            ss >> cpu;
            cpu_sum += cpu;
            cpu_count++;
        }
        else if (line.rfind("Elapsed Time:", 0) == 0)
        {
            string val = line.substr(13);
            stringstream ss(val);
            long el = 0;
            ss >> el;
            if (el > max_elapsed) max_elapsed = el;
        }
        else if (line.rfind("Type:", 0) == 0)
        {
            string type = line.substr(5);
            trim(type);
            if (type == "HIGH_CPU_CONSUMPTION") summary.cpu_alerts++;
            else if (type == "MEMORY_THRESHOLD_WARNING") summary.mem_alerts++;
            else if (type == "SUSPECTED_MEMORY_LEAK" || type == "CONFIRMED_MEMORY_LEAK") summary.leak_alerts++;
            else if (type == "RESOURCE_EXHAUSTION_CRITICAL") summary.exhaustion_breaches++;
            else if (type == "RESOURCE_EXHAUSTION_PREVENTED") summary.preventions_executed++;
        }
        else if (line.rfind("Message:", 0) == 0)
        {
            string msg = line.substr(8);
            trim(msg);
            if (summary.recent_alerts.size() < 5)
            {
                summary.recent_alerts.push_back(msg);
            }
        }
    }
    file.close();

    summary.peak_memory_kb = max_rss;
    summary.avg_cpu_usage = (cpu_count > 0) ? (cpu_sum / cpu_count) : 0.0;
    summary.total_runtime_sec = max_elapsed;

    if (summary.first_monitored.empty()) summary.first_monitored = "N/A";
    if (summary.last_active.empty()) summary.last_active = "N/A";

    return true;
}

void displayReport(const string &app_name)
{
    AppReportSummary summary;
    if (!parseReportSummary(app_name, summary))
    {
        cout << "\nError: No report file found for '" << app_name << "' in '" << reports_dir << "/'." << endl;
        cout << "To generate a report, start monitoring with: appsentry monitor " << app_name << "\n" << endl;
        return;
    }

    cout << "\n========================================================\n";
    cout << " AppSentry - Application Usage Report\n";
    cout << "========================================================\n";
    cout << "Application: " << summary.app_name << "\n";
    cout << "Total Runtime: " << formatDuration(summary.total_runtime_sec) << "\n";
    cout << "Peak Memory Usage: " << formatKB(summary.peak_memory_kb) << "\n";
    cout << "Average CPU Usage: " << fixed << setprecision(1) << summary.avg_cpu_usage << "%\n";
    cout << "First Monitored: " << summary.first_monitored << "\n";
    cout << "Last Active: " << summary.last_active << "\n";
    cout << "--------------------------------------------------------\n";
    cout << "Resource Exhaustion & Memory Leak Prevention:\n";
    cout << "  Total Snapshots Logged: " << summary.samples_count << "\n";
    cout << "  Memory Leaks Detected: " << summary.leak_alerts << "\n";
    cout << "  Memory Threshold Warnings: " << summary.mem_alerts << "\n";
    cout << "  High CPU Warnings: " << summary.cpu_alerts << "\n";
    cout << "  Critical Exhaustion Breaches: " << summary.exhaustion_breaches << "\n";
    cout << "  Resource Exhaustion Events Prevented: " << summary.preventions_executed << "\n";

    if (!summary.recent_alerts.empty())
    {
        cout << "--------------------------------------------------------\n";
        cout << "Recent Diagnostic Alerts:\n";
        for (const auto &alert : summary.recent_alerts)
        {
            cout << "  * " << alert << "\n";
        }
    }
    cout << "========================================================\n\n";
}
