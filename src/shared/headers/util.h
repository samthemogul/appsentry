#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <cctype>
#include <pwd.h>

using namespace std;

void toLowercase(string &s);
void trim(string &s);
string get_username_from_uid(int uid);

string formatBytes(long bytes);
string formatKB(long kb);
string formatDuration(long seconds);
string getCurrentTimestamp();
string toLowerCopy(const string &s);
bool caseInsensitiveContains(const string &haystack, const string &needle);

#endif