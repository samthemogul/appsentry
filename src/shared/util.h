#ifndef UTIL_H
#define UTIL_H

#include <string>
#include <cctype>

using namespace std;



void toLowercase(string &s)
{
    int n = s.length();

    if (n == 0)
        return;

    for (int i = 0; i < n; i++)
    {
        s[i] = (char)tolower(s[i]);
    }
}


#endif