#include "headers/util.h"
#include <iomanip>
#include <sstream>
#include <ctime>
#include <chrono>
#include <algorithm>

void toLowercase(string &s)
{
    int n = s.length();

    if (n == 0)
        return;

    for (int i = 0; i < n; i++)
    {
        s[i] = (char)tolower(s[i]);
    }
};

// Trim whitespaces of a string
void trim(string &s)
{
    size_t first = s.find_first_not_of(" \t\r\n");
    if (first == string::npos) {
        s = "";
        return;
    }
    size_t last = s.find_last_not_of(" \t\r\n");
    s = s.substr(first, last - first + 1);
}

string get_username_from_uid(int uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return string(pw->pw_name); // Return the username
    }
    return "Unknown"; // If UID is not found
}

string formatBytes(long bytes) {
    ostringstream oss;
    oss << fixed << setprecision(1);
    if (bytes >= 1024L * 1024L * 1024L) {
        oss << (double)bytes / (1024.0 * 1024.0 * 1024.0) << " GB";
    } else if (bytes >= 1024L * 1024L) {
        oss << (double)bytes / (1024.0 * 1024.0) << " MB";
    } else if (bytes >= 1024L) {
        oss << (double)bytes / 1024.0 << " KB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

string formatKB(long kb) {
    return formatBytes(kb * 1024L);
}

string formatDuration(long seconds) {
    if (seconds < 0) seconds = 0;
    long hours = seconds / 3600;
    long mins = (seconds % 3600) / 60;
    long secs = seconds % 60;

    ostringstream oss;
    if (hours > 0) {
        oss << hours << (hours == 1 ? " hour " : " hours ");
    }
    if (hours > 0 || mins > 0) {
        oss << mins << (mins == 1 ? " minute " : " minutes ");
    }
    oss << secs << (secs == 1 ? " second" : " seconds");
    return oss.str();
}

string getCurrentTimestamp() {
    auto now = chrono::system_clock::now();
    time_t now_c = chrono::system_clock::to_time_t(now);
    struct tm t;
#if defined(_WIN32)
    localtime_s(&t, &now_c);
#else
    localtime_r(&now_c, &t);
#endif
    char buf[64];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &t);
    return string(buf);
}

string toLowerCopy(const string &s) {
    string res = s;
    toLowercase(res);
    return res;
}

bool caseInsensitiveContains(const string &haystack, const string &needle) {
    string h = toLowerCopy(haystack);
    string n = toLowerCopy(needle);
    return h.find(n) != string::npos;
}