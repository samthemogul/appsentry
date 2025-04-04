#include <iostream>
#include <sstream>
#include "logger/logger.h"
#include "monitor/monitor.h"
#include "shared/headers/getos.h"

using namespace std;

int main()
{
    // Get current Operating System Architecture
    string os;
    stringstream ss(getOSName());
    getline(ss, os, ':');
    string ostype = "linux";

    // Save processess Statistics
    createReportsRepository();
    auto processes = (os == "linux") ? getLinuxProcesses() : (os == "windows") ? getWindowsProcesses()
                                                                               : getIosProcesses();

    for (auto &entry : processes)
    {
        saveReport(entry.name, entry);
    }

    return 0;
};