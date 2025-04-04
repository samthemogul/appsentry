#include "headers/util.h"

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
    s = s.substr(s.find_first_not_of(" "), s.find_last_not_of(" ") + 1);
    return;
}

string get_username_from_uid(int uid) {
    struct passwd *pw = getpwuid(uid);
    if (pw) {
        return string(pw->pw_name); // Return the username
    }
    return "Unknown"; // If UID is not found
}