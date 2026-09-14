#include "os_notifier.h"
#include <iostream>
#include <sstream>
#include <cstdlib>
#include <thread>
#include <chrono>

using namespace std;

namespace OSNotifier {

static string escapeForShell(const string &str) {
    string res = "";
    for (char c : str) {
        if (c == '"') res += "\\\"";
        else if (c == '\\') res += "\\\\";
        else if (c == '\'') res += "'\\''";
        else if (c == '\n' || c == '\r') res += " ";
        else res += c;
    }
    return res;
}

bool sendNotification(const string &title,
                      const string &message,
                      Urgency urgency,
                      const string &subtitle)
{
    string safe_title = escapeForShell(title);
    string safe_msg = escapeForShell(message);
    string safe_sub = escapeForShell(subtitle);

#if defined(__APPLE__)
    // Native macOS Notification Center UI banner via osascript
    string sound_name = (urgency == Urgency::CRITICAL) ? "Basso" : "default";
    ostringstream oss;
    oss << "osascript -e 'display notification \"" << safe_msg << "\" with title \"" << safe_title << "\"";
    if (!safe_sub.empty()) {
        oss << " subtitle \"" << safe_sub << "\"";
    }
    oss << " sound name \"" << sound_name << "\"' >/dev/null 2>&1 &";

    int ret = system(oss.str().c_str());
    return ret == 0;

#elif defined(_WIN32)
    // Windows Native Balloon/Toast UI notification via PowerShell
    string icon_type = (urgency == Urgency::CRITICAL) ? "Error" : "Warning";
    ostringstream oss;
    oss << "start /B powershell -WindowStyle Hidden -Command \""
        << "[reflection.assembly]::loadwithpartialname('System.Windows.Forms') | Out-Null; "
        << "$n = New-Object System.Windows.Forms.NotifyIcon; "
        << "$n.Icon = [System.Drawing.SystemIcons]::" << icon_type << "; "
        << "$n.BalloonTipTitle = '" << safe_title << "'; "
        << "$n.BalloonTipText = '" << safe_msg << "'; "
        << "$n.Visible = $True; "
        << "$n.ShowBalloonTip(4000); "
        << "Start-Sleep -Seconds 4; $n.Dispose()\"";

    int ret = system(oss.str().c_str());
    return ret == 0;

#else
    // Linux Desktop Notification via libnotify / notify-send
    string urg_str = (urgency == Urgency::CRITICAL) ? "critical" :
                     (urgency == Urgency::LOW) ? "low" : "normal";

    ostringstream oss;
    oss << "(notify-send \"" << safe_title << "\" \"" << safe_msg
        << "\" --urgency=" << urg_str
        << " --app-name=\"AppSentry\""
        << " --icon=dialog-warning"
        << " 2>/dev/null || "
        << "kdialog --passivepopup \"" << safe_msg << "\" 5 \"" << safe_title << "\" 2>/dev/null || "
        << "zenity --notification --text=\"" << safe_title << ": " << safe_msg << "\" 2>/dev/null) &";

    int ret = system(oss.str().c_str());
    return ret == 0;
#endif
}

} // namespace OSNotifier
