#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <cctype>
#include <pwd.h>

using namespace std;

void toLowercase(string &s);
void trim(string &s);
string get_username_from_uid(int uid);


#endif