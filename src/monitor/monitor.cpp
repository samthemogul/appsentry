#include <iostream>
#include "monitor.h"
#include <vector>
#include <dirent.h>
#include <sys/types.h>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <ctime>

using namespace std;

unordered_map<string, string> getStatFields(const string &stat)
{
    unordered_map<string, string> result;

    // List of stat field names as per /proc/[pid]/stat man page
    vector<string> fields = {
        "pid", "comm", "state", "ppid", "pgrp", "session", "tty_nr",
        "tpgid", "flags", "minflt", "cminflt", "majflt", "cmajflt",
        "utime", "stime", "cutime", "cstime", "priority", "nice",
        "num_threads", "itrealvalue", "starttime", "vsize", "rss"
        // Add more fields if needed
    };

    // First, handle the tricky "comm" field which is enclosed in parentheses and may contain spaces
    size_t open_paren = stat.find('(');
    size_t close_paren = stat.find(')', open_paren);

    if (open_paren == string::npos || close_paren == string::npos)
    {
        cerr << "Invalid stat format\n";
        return result;
    }

    string before = stat.substr(0, open_paren - 1);                          // pid
    string comm = stat.substr(open_paren + 1, close_paren - open_paren - 1); // comm
    string after = stat.substr(close_paren + 2);                             // the rest

    vector<string> tokens;
    tokens.push_back(before);
    tokens.push_back(comm);

    stringstream ss(after);
    string token;
    while (ss >> token)
    {
        tokens.push_back(token);
    }

    for (size_t i = 0; i < fields.size() && i < tokens.size(); ++i)
    {
        result[fields[i]] = tokens[i];
    }

    return result;
}

unordered_map<string, string> getStatusFields(const string &path)
{
    unordered_map<string, string> keyValue;
    ifstream status_file(path, ios::in);

    if (!status_file.is_open())
    {
        cerr << "Failed to open status file: " << path << endl;
        return keyValue;
    }

    string line;
    while (getline(status_file, line))
    {
        stringstream ss(line);
        string key, value;

        // Parse the key-value pair (separated by ":")
        if (getline(ss, key, ':') && getline(ss, value))
        {
            trim(key);
            trim(value);
            keyValue[key] = value;
        }
    }

    status_file.close();
    return keyValue;
}

vector<Processinfo> getWindowsProcesses()
{
    try
    {
        vector<Processinfo> processes;
        return processes;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return {};
    }
}
vector<Processinfo> getIosProcesses()
{
    try
    {
        vector<Processinfo> processes;
        return processes;
    }
    catch (const std::exception &e)
    {
        std::cerr << e.what() << '\n';
        return {};
    }
}

vector<Processinfo> getLinuxProcesses()
{
    try
    {
        vector<Processinfo> processes;

        // Open the application processes directory in the OS
        DIR *dir = opendir("/proc");
        if (!dir)
            return processes;

        // Read the registered processes
        struct dirent *entry;

        while ((entry = readdir(dir)) != nullptr)
        {
            if (!isdigit(entry->d_name[0]))
                continue;

            int pid = std::stoi(entry->d_name);
            string name;
            ps state;
            int ppid;
            int threads;
            string owner;
            string exec_path;
            int open_files;
            int priority;
            time_t log_time = time(0);
            string log_date_time = ctime(&log_time);
            double cpu_usage = 25.5;
            long vm_rss = 123456;
            long vm_size = 789000;
            long disk_usage = 4000;
            long start_time = 1712100000;
            long elapsed_time = 7200;
            long network_usage = 15000;
            int network_connections = 3;

            // Open the stats files of the process
            string status_path = "/proc/" + to_string(pid) + "/status";
            string stat_path = "/proc/" + to_string(pid) + "/stat";

            ifstream stat_file(stat_path, ios::in);
            if (!stat_file.is_open())
            {
                throw runtime_error("Unable to open the process stat files");
            }

            string stat_line;
            auto status_map = getStatusFields(status_path);
            getline(stat_file, stat_line);
            auto statMap = getStatFields(stat_line);

            name = status_map["Name"];
            state = status_map["State"][1] == 'S' ? SLEEPING : status_map["State"][1] == 'R' ? RUNNING
                                                          : status_map["State"][1] == 'I'   ? IDLE
                                                                                           : TERMINATED;
            ppid = stoi((status_map["PPid"]));
            threads = stoi(status_map["Threads"]);
            owner = get_username_from_uid(stoi(status_map["Uid"]));
            exec_path = "/proc/pid/exe";
            open_files = 2;
            priority = stoi(statMap["priority"]);

            Processinfo process = {
                pid,                // Process ID
                name,               // Process Name
                state,              // Process State
                ppid,               // Parent Process ID
                threads,            // Number of Threads
                owner,              // Owner Name
                exec_path,          // Executable Path
                open_files,         // Open Files Count
                priority,           // Process Priority
                log_date_time,      // Log Time (Epoch)
                log_time,           // Log Date and Time (Formatted)
                cpu_usage,          // CPU Usage Percentage
                vm_rss,             // Virtual Memory Resident Set Size
                vm_size,            // Virtual Memory Size
                disk_usage,         // Disk Usage
                start_time,         // Start Time of Process
                elapsed_time,       // Elapsed Time
                network_usage,      // Network Usage
                network_connections // Number of Network Connections
            };

            processes.push_back(process);
        }
        return processes;
    }
    catch (const exception &e)
    {
        cerr << e.what() << '\n';
        return {};
    }
}