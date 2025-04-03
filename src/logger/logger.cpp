#include "logger.h"
#include <iostream>
#include <filesystem>
#include <fstream>

using namespace std;

namespace fs = filesystem;

// Handle the creation of the reports repository
void createReportsRepository()
{
    string reports_path = "reports";

    // check if the directory exists, if not create it
    if (!fs::exists(reports_path))
    {
        fs::create_directory(reports_path);
        cout << "Reports directory created successfully!" << endl;
    }
    else
    {
        cout << "Existing Reports directory found." << endl;
    }
}


// Handles writing report of application processes to file
void saveReport(string app_name, Processinfo& details)
{
    try {

        // Validate process name
        toLowercase(app_name);
        toLowercase(details.name);
        if(app_name != details.name) {
            throw "Invalid process name identifier";
        }

        // check if the current application process has a folder to its name or a folder containing it's staring letters(e.g chrome, chrome.app)

    } catch (exception e) {

    }
}
