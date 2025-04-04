#ifndef PROCESS_INFO_H
#define PROCESS_INFO_H

#include <string>
#include <ctime>
using namespace std;



typedef enum ProcessState
{
    RUNNING,
    SLEEPING,
    TERMINATED,
    IDLE
} ps;

string get_state_char(ProcessState state);



// Information about an application process
struct Processinfo
{
    // GENERIC
    int pid;      // Process ID
    string name;  // Process name
    ps state;     // Process state (e.g., 'RUNNING' ,'SLEEPING', 'TERMINATED'.)
    int ppid;     // Parent process ID
    int threads;  // Number of threads in use by the process
    string owner; // The current user of the process
    string exec_path;
    int open_files;
    int priority;
    string log_date_time;
    time_t log_time;

    // NETWORK
    double cpu_usage; // CPU usage percentage
    long vm_rss;      // Resident memory (RAM) in KB
    long vm_size;     // Total virtual memory in KB
    long disk_usage;  // Disk I/O usage (requires external monitoring)

    // TIME
    long start_time;   // Start time in seconds since boot
    long elapsed_time; // Time active since last start in seconds

    // NETWORK
    long network_usage;
    int network_connections;
};

#endif