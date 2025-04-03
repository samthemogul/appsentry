#include <iostream>
#include "logger/logger.h"

using namespace std;


int main()
{
    createReportsRepository();
    Processinfo testProcess = {
        9999,             // pid
        "samprocess.app",  // name
        RUNNING,              // state
        8888,             // ppid
        5,                // threads
        "test_user",      // owner
        "/usr/bin/dummy", // exec_path
        10,               // open_files
        20,               // priority
        "2025-04-03 15:00:00", // log_date_time
        1712149800,       // log_time
        25.5,             // cpu_usage
        123456,           // vm_rss
        789000,           // vm_size
        4000,             // disk_usage
        1712100000,       // start_time
        7200,             // elapsed_time
        15000,            // network_usage
        3                 // network_connections
    };
    string app_name = "samprocess.app";
    saveReport(app_name, testProcess);
    
    return 0;
};