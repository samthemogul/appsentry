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
unordered_map<string, string> getStatFields(const string &stat);
unordered_map<string, string> getStatusFields(const string &path);


#endif