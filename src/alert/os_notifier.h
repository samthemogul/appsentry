#ifndef OS_NOTIFIER_H
#define OS_NOTIFIER_H

#include <string>

namespace OSNotifier {
    enum class Urgency {
        LOW,
        NORMAL,
        CRITICAL
    };

    // Send a desktop notification to the native OS notification UI system
    // (macOS Notification Center, Linux notify-send/DBus, Windows Toast)
    // Dispatched asynchronously in background to ensure zero jitter on real-time loops
    bool sendNotification(const std::string &title,
                          const std::string &message,
                          Urgency urgency = Urgency::NORMAL,
                          const std::string &subtitle = "");
}

#endif
