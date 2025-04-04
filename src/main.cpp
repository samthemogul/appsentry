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

    if (os != "linux" && os != "windows" && os != "macos")
    {
        cerr << "Your Operating system architecture is not supported at this time." << endl;
        return 1;
    }

    // Save processess Statistics
    createReportsRepository();
    auto processes = (os == "linux") ? getLinuxProcesses() : (os == "windows") ? getWindowsProcesses()
                                                                               : getIosProcesses();

    for (auto &process : processes)
    {
        saveReport(process.name, process);
    }

    return 0;
};