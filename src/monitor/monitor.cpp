#include <iostream>
#include "monitor.h"
#include <vector>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <ctime>
#include <cstring>
#include <algorithm>
#include <unistd.h>

#if defined(__APPLE__)
#include <libproc.h>
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <pwd.h>
#elif defined(_WIN32)
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#else
#include <dirent.h>
#include <sys/types.h>
#endif

using namespace std;

unordered_map<string, string> getStatFields(const string &stat)
{
    unordered_map<string, string> result;

    vector<string> fields = {
        "pid", "comm", "state", "ppid", "pgrp", "session", "tty_nr",
        "tpgid", "flags", "minflt", "cminflt", "majflt", "cmajflt",
        "utime", "stime", "cutime", "cstime", "priority", "nice",
        "num_threads", "itrealvalue", "starttime", "vsize", "rss"
    };

    size_t open_paren = stat.find('(');
    size_t close_paren = stat.find(')', open_paren);

    if (open_paren == string::npos || close_paren == string::npos)
    {
        return result;
    }

    string before = stat.substr(0, open_paren - 1);
    string comm = stat.substr(open_paren + 1, close_paren - open_paren - 1);
    string after = stat.substr(close_paren + 2);

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
        return keyValue;
    }

    string line;
    while (getline(status_file, line))
    {
        stringstream ss(line);
        string key, value;

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

bool getProcessInfoByPid(int pid, Processinfo &out_info)
{
#if defined(__APPLE__)
    struct proc_taskallinfo tai;
    int ret = proc_pidinfo(pid, PROC_PIDTASKALLINFO, 0, &tai, sizeof(tai));
    if (ret <= 0)
    {
        return false;
    }

    char name[1024] = {0};
    int name_len = proc_name(pid, name, sizeof(name));
    if (name_len <= 0)
    {
        strncpy(name, tai.pbsd.pbi_comm, sizeof(name) - 1);
    }

    char exec_path[PROC_PIDPATHINFO_MAXSIZE] = {0};
    proc_pidpath(pid, exec_path, sizeof(exec_path));

    time_t now = time(nullptr);
    time_t start_time = tai.pbsd.pbi_start_tvsec;
    long elapsed = (start_time > 0 && now >= start_time) ? (now - start_time) : 0;

    double cpu = 0.0;
    uint64_t total_cpu_ns = tai.ptinfo.pti_total_user + tai.ptinfo.pti_total_system;
    if (elapsed > 0)
    {
        cpu = ((double)total_cpu_ns / 1e9) / (double)elapsed * 100.0;
        if (cpu > 1000.0) cpu = 100.0;
    }

    int fds_bytes = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, NULL, 0);
    int open_files = (fds_bytes > 0) ? (fds_bytes / (int)sizeof(struct proc_fdinfo)) : 0;

    ps state = RUNNING;
    switch (tai.pbsd.pbi_status)
    {
        case SIDL: state = IDLE; break;
        case SRUN: state = RUNNING; break;
        case SSLEEP: state = SLEEPING; break;
        case SZOMB: state = TERMINATED; break;
        default: state = RUNNING; break;
    }

    out_info.pid = pid;
    out_info.name = string(name);
    out_info.state = state;
    out_info.ppid = tai.pbsd.pbi_ppid;
    out_info.threads = tai.ptinfo.pti_threadnum;
    out_info.owner = get_username_from_uid(tai.pbsd.pbi_uid);
    out_info.exec_path = (strlen(exec_path) > 0) ? string(exec_path) : string(name);
    out_info.open_files = open_files;
    out_info.priority = tai.pbsd.pbi_nice;
    out_info.log_time = now;
    out_info.log_date_time = getCurrentTimestamp();
    out_info.cpu_usage = cpu;
    out_info.vm_rss = tai.ptinfo.pti_resident_size / 1024;
    out_info.vm_size = tai.ptinfo.pti_virtual_size / 1024;
    out_info.disk_usage = 0;
    out_info.start_time = start_time;
    out_info.elapsed_time = elapsed;
    out_info.network_usage = 0;
    out_info.network_connections = 0;

    return true;
#elif defined(_WIN32)
    HANDLE hProcess = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!hProcess) return false;

    char procName[MAX_PATH] = "<unknown>";
    GetModuleBaseNameA(hProcess, NULL, procName, sizeof(procName));

    PROCESS_MEMORY_COUNTERS_EX pmc;
    long rss = 0, vsize = 0;
    if (GetProcessMemoryInfo(hProcess, (PROCESS_MEMORY_COUNTERS*)&pmc, sizeof(pmc)))
    {
        rss = pmc.WorkingSetSize / 1024;
        vsize = pmc.PrivateUsage / 1024;
    }

    FILETIME ftCreation, ftExit, ftKernel, ftUser;
    double cpu = 0.0;
    long elapsed = 0;
    time_t now = time(nullptr);
    time_t start_time = now;
    if (GetProcessTimes(hProcess, &ftCreation, &ftExit, &ftKernel, &ftUser))
    {
        ULARGE_INTEGER uUser, uKernel;
        uUser.LowPart = ftUser.dwLowDateTime; uUser.HighPart = ftUser.dwHighDateTime;
        uKernel.LowPart = ftKernel.dwLowDateTime; uKernel.HighPart = ftKernel.dwHighDateTime;
        uint64_t total_time = uUser.QuadPart + uKernel.QuadPart;
        cpu = (double)total_time / 100000.0;
    }
    CloseHandle(hProcess);

    out_info.pid = pid;
    out_info.name = string(procName);
    out_info.state = RUNNING;
    out_info.ppid = 0;
    out_info.threads = 1;
    out_info.owner = "WindowsUser";
    out_info.exec_path = procName;
    out_info.open_files = 0;
    out_info.priority = 0;
    out_info.log_time = now;
    out_info.log_date_time = getCurrentTimestamp();
    out_info.cpu_usage = cpu;
    out_info.vm_rss = rss;
    out_info.vm_size = vsize;
    out_info.disk_usage = 0;
    out_info.start_time = start_time;
    out_info.elapsed_time = elapsed;
    out_info.network_usage = 0;
    out_info.network_connections = 0;
    return true;
#else
    // Linux implementation
    string status_path = "/proc/" + to_string(pid) + "/status";
    string stat_path = "/proc/" + to_string(pid) + "/stat";
    auto status_map = getStatusFields(status_path);
    if (status_map.empty()) return false;

    ifstream stat_file(stat_path, ios::in);
    unordered_map<string, string> stat_map;
    if (stat_file.is_open())
    {
        string stat_line;
        getline(stat_file, stat_line);
        stat_map = getStatFields(stat_line);
    }

    time_t now = time(nullptr);
    out_info.pid = pid;
    out_info.name = status_map["Name"];
    out_info.state = (status_map["State"].find('R') != string::npos) ? RUNNING :
                     (status_map["State"].find('S') != string::npos) ? SLEEPING :
                     (status_map["State"].find('I') != string::npos) ? IDLE : TERMINATED;
    out_info.ppid = status_map.count("PPid") ? stoi(status_map["PPid"]) : 0;
    out_info.threads = status_map.count("Threads") ? stoi(status_map["Threads"]) : 1;
    out_info.owner = status_map.count("Uid") ? get_username_from_uid(stoi(status_map["Uid"])) : "Unknown";

    char link_path[1024] = {0};
    string exe_link = "/proc/" + to_string(pid) + "/exe";
    ssize_t len = readlink(exe_link.c_str(), link_path, sizeof(link_path) - 1);
    out_info.exec_path = (len > 0) ? string(link_path) : out_info.name;

    out_info.open_files = 0;
    out_info.priority = stat_map.count("priority") ? stoi(stat_map["priority"]) : 0;
    out_info.log_time = now;
    out_info.log_date_time = getCurrentTimestamp();

    long rss = 0, vsize = 0;
    if (status_map.count("VmRSS"))
    {
        stringstream ss(status_map["VmRSS"]);
        ss >> rss;
    }
    if (status_map.count("VmSize"))
    {
        stringstream ss(status_map["VmSize"]);
        ss >> vsize;
    }
    out_info.vm_rss = rss;
    out_info.vm_size = vsize;

    long start_ticks = stat_map.count("starttime") ? stol(stat_map["starttime"]) : 0;
    long clk_tck = sysconf(_SC_CLK_TCK);
    out_info.start_time = (clk_tck > 0) ? (start_ticks / clk_tck) : 0;
    out_info.elapsed_time = (out_info.start_time > 0 && now >= out_info.start_time) ? (now - out_info.start_time) : 0;
    out_info.cpu_usage = 0.0;
    out_info.disk_usage = 0;
    out_info.network_usage = 0;
    out_info.network_connections = 0;
    return true;
#endif
}

vector<Processinfo> getMacProcesses()
{
    vector<Processinfo> processes;
#if defined(__APPLE__)
    int bytes = proc_listpids(PROC_ALL_PIDS, 0, NULL, 0);
    if (bytes <= 0) return processes;

    int num_pids = bytes / sizeof(pid_t);
    vector<pid_t> pids(num_pids * 2);
    bytes = proc_listpids(PROC_ALL_PIDS, 0, pids.data(), pids.size() * sizeof(pid_t));
    num_pids = bytes / sizeof(pid_t);

    for (int i = 0; i < num_pids; ++i)
    {
        pid_t pid = pids[i];
        if (pid <= 0) continue;

        Processinfo proc;
        if (getProcessInfoByPid(pid, proc))
        {
            if (!proc.name.empty())
            {
                processes.push_back(proc);
            }
        }
    }
#endif
    return processes;
}

vector<Processinfo> getIosProcesses()
{
    return getMacProcesses();
}

vector<Processinfo> getWindowsProcesses()
{
    vector<Processinfo> processes;
#if defined(_WIN32)
    DWORD pids[2048], bytesReturned;
    if (EnumProcesses(pids, sizeof(pids), &bytesReturned))
    {
        int count = bytesReturned / sizeof(DWORD);
        for (int i = 0; i < count; ++i)
        {
            if (pids[i] == 0) continue;
            Processinfo proc;
            if (getProcessInfoByPid(pids[i], proc))
            {
                processes.push_back(proc);
            }
        }
    }
#endif
    return processes;
}

vector<Processinfo> getLinuxProcesses()
{
    vector<Processinfo> processes;
#if !defined(__APPLE__) && !defined(_WIN32)
    DIR *dir = opendir("/proc");
    if (!dir) return processes;

    struct dirent *entry;
    while ((entry = readdir(dir)) != nullptr)
    {
        if (!isdigit(entry->d_name[0])) continue;

        int pid = std::stoi(entry->d_name);
        Processinfo proc;
        if (getProcessInfoByPid(pid, proc))
        {
            processes.push_back(proc);
        }
    }
    closedir(dir);
#endif
    return processes;
}

vector<Processinfo> getAllProcesses()
{
#if defined(__APPLE__)
    return getMacProcesses();
#elif defined(_WIN32)
    return getWindowsProcesses();
#else
    return getLinuxProcesses();
#endif
}

vector<Processinfo> findProcessesByName(const string &name)
{
    vector<Processinfo> all = getAllProcesses();
    vector<Processinfo> matched;

    bool is_num = !name.empty() && all_of(name.begin(), name.end(), ::isdigit);
    int target_pid = is_num ? stoi(name) : -1;

    for (const auto &p : all)
    {
        if (is_num && p.pid == target_pid)
        {
            matched.push_back(p);
            continue;
        }
        if (caseInsensitiveContains(p.name, name) ||
            caseInsensitiveContains(p.exec_path, name))
        {
            matched.push_back(p);
        }
    }
    return matched;
}