#include "logger.h"
#include <iostream>
#include <filesystem>
#include <fstream>
#include <sys/stat.h>
#include <stdexcept>
#include <sys/stat.h>
#include <unistd.h>

using namespace std;

namespace fs = filesystem;

string const reports_dir = "reports";

// Handle the creation of the reports repository
void createReportsRepository()
{

    // check if the directory exists, if not create it
    if (!fs::exists(reports_dir))
    {
        mkdir(reports_dir.c_str(), 0777);
        cout << "Reports directory created successfully!" << endl;
    }
    else
    {
        cout << "Existing Reports directory found." << endl;
    }
    return;
}

// Handles writing report of application processes to file
void saveReport(string app_name, Processinfo &details)
{
    try
    {

        // Validate process name
        toLowercase(app_name);
        toLowercase(details.name);
        if (app_name != details.name)
        {
            throw runtime_error("Invalid process name identifier!");
        }

        // check if the current application process has a folder in /reports to its name or a folder containing it's staring letters(e.g chrome, chrome.app)
        string app_folder;

        for (const auto &entry : fs::directory_iterator(reports_dir))
        {
            if (fs::is_directory(entry))
            {
                string folder = entry.path().filename().string();
                if (folder == app_name || app_name.find(folder) != string::npos || folder.find(app_name) != string::npos)
                {
                    app_folder = folder;
                    break;
                }
            }
        }
        if (app_folder.length() == 0)
        {
            app_folder = app_name;
        }

        // create the reports file for the application process in the appropraite folder(create if it doesnt exist)
        string folder_path = reports_dir + '/' + app_folder;
        struct stat dir_stat;

        if (stat(folder_path.c_str(), &dir_stat) != 0)
        {
            if (mkdir(folder_path.c_str(), 0777) != 0)
            {
                throw runtime_error("Error creating the directory.");
            };
        }
        else if (!(dir_stat.st_mode & S_IWUSR))
        {
            throw runtime_error("Error: No write permissions for the directory.");
        }

        string file_path = folder_path + '/' + app_name + ".report";

        // Check if the file exists
        struct stat buffer;
        bool file_exists = (stat(file_path.c_str(), &buffer) == 0);

        // Check if the user has write permission in the directory
        if (access(folder_path.c_str(), W_OK) != 0)
        {
            throw runtime_error("Error: No write permissions for the directory.");
        }

        ofstream process_report(file_path, ios::app);

        if (!process_report.is_open())
        {
            throw runtime_error("Error: Unable to open the application report file.");
        }

        if (!file_exists)
        {
            process_report << "Process Report for " << app_name << "\n";
            process_report << "-----------------------------------\n\n\n";
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
        process_report << "CPU Usage: " << details.cpu_usage << "%\n";
        process_report << "Resident Memory (VmRSS): " << details.vm_rss << " KB\n";
        process_report << "Virtual Memory (VmSize): " << details.vm_size << " KB\n";
        process_report << "Disk Usage: " << details.disk_usage << " KB\n";
        process_report << "Start Time: " << details.start_time << " seconds since boot\n";
        process_report << "Elapsed Time: " << details.elapsed_time << " seconds\n";
        process_report << "Network Usage: " << details.network_usage << " KB\n";
        process_report << "Network Connections: " << details.network_connections << "\n";

        process_report << "------------------------------------\n\n";
        process_report.close();
        return;
    }
    catch (exception &e)
    {
        cerr << e.what() << endl;
        return;
    }
}
