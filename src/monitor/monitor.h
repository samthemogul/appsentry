#ifndef MONITOR_H
#define MONITOR_H

// standard headers
#include <vector>
#include <unordered_map>

// custom header
#include "../shared/headers/process_info.h"
#include "../shared/headers/util.h"

vector<Processinfo> getLinuxProcesses();
vector<Processinfo> getWindowsProcesses();
vector<Processinfo> getIosProcesses();
vector<Processinfo> getMacProcesses();
vector<Processinfo> getAllProcesses();
vector<Processinfo> findProcessesByName(const string &name);
bool getProcessInfoByPid(int pid, Processinfo &out_info);

unordered_map<string, string> getStatFields(const string &stat);
unordered_map<string, string> getStatusFields(const string &path);



#endif