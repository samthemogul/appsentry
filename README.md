# AppSentry - Application Usage Monitor & Optimizer

## Overview
**AppSentry** is a high-performance cross-platform command-line application that monitors desktop applications, tracking their CPU and memory usage, and logs how long they have been running. Users can generate detailed reports of app usage and optimize their system by freeing memory, stopping apps, or launching applications directly from the CLI.

AppSentry implements real-time threshold-alerting logic using POSIX signals, enabling automated detection of memory leaks and thread exhaustion under heavy workloads. Alerts are piped in real time to the native OS notification system (macOS Notification Center, Linux Desktop Notifications via `libnotify`/`notify-send`, and Windows Toast Notifications).

## Features
- **Monitor Applications**: Track real-time CPU, resident memory (RSS), and active thread counts of running applications.
- **POSIX Signal Alerting**: Real-time asynchronous sampling driven by POSIX interval timers (`SIGALRM`/`setitimer`) and inter-process alert dispatching (`SIGUSR1`/`SIGUSR2`).
- **Automated Memory Leak Detection**: Sliding-window statistical analysis tracking monotonic expansion, growth velocity (MB/s), and time-to-exhaustion projections.
- **Thread Exhaustion Detection**: Proactively detects unbounded thread spawning, unjoined threads, and thread pool exhaustion under heavy workloads before system PID table exhaustion occurs.
- **Cross-Platform Native OS Desktop UI Notifications**: Real-time visual UI banner alerts dispatched directly into macOS Notification Center (`osascript`), Linux desktop (`notify-send`/`libnotify`/KDE `kdialog`), and Windows Toast/Action Center (PowerShell balloon/toast) with severity-mapped notification sounds and urgency levels. Dispatched asynchronously in background subshells to avoid interrupting real-time monitoring loops.
- **Signal-Based Automated Mitigation**: Automatically pauses runaway processes via POSIX `SIGSTOP` or terminates them via `SIGTERM`/`SIGKILL` (`--pause-on-exhaustion`, `--auto-kill`, `--auto-optimize`).
- **Log Usage Data**: Store app activity logs, thread statistics, and diagnostic alert histories locally in a `reports/` folder.
- **Generate Reports**: View app usage history, peak threads, leak diagnostics, and POSIX signal audits.
- **Optimize System**: Free memory or terminate running applications via graceful `SIGTERM` or force `SIGKILL`.
- **Launch Apps**: Start applications via the command line across macOS, Linux, and Windows.
- **Process Inspection**: Inspect active processes and memory consumption with `appsentry list`.

## Installation
### Prerequisites
- C++17 compatible compiler (Clang/GCC for macOS/Linux, MSVC/MinGW for Windows)
- Process management libraries: macOS (`<libproc.h>`), Linux (`/proc`), or Windows (`<psapi.h>`)

### Clone Repository
```bash
git clone https://github.com/samthemogul/appsentry.git
cd appsentry
```

### Build
```bash
make
```

## Usage
Run `appsentry` followed by a command:
```bash
appsentry <command> [options]
```

### Commands

#### 1. Monitor an Application & Prevent Resource Exhaustion
```bash
appsentry monitor <app_name|pid> [options]
```
- Starts tracking CPU, memory, and thread counts driven by real-time POSIX interval timers (`SIGALRM`).
- Emits real-time POSIX signals (`SIGUSR1`) upon detecting memory leaks or thread exhaustion under heavy workloads.
- Dispatches visual UI banner alerts to the native desktop OS notification system (macOS Notification Center, Linux `libnotify`, Windows Toast).
- Proactively halts or terminates runaway processes (`SIGSTOP`/`SIGTERM`) before process-level exhaustion causes system failure.
- Logs data and diagnostic alert events to `reports/<app_name>.report`.

**Options:**
- `-i, --interval <sec>`: Sampling interval in seconds (default: `2s`, POSIX timer driven)
- `-d, --duration <sec>`: Total monitoring duration in seconds (default: continuous until Ctrl+C)
- `-m, --threshold-mem <MB>`: Critical memory threshold for resource exhaustion (default: `800 MB`)
- `-w, --threshold-warn <MB>`: Warning memory threshold (default: `400 MB`)
- `-t, --threshold-threads <n>`: Critical thread exhaustion limit (default: `150 threads`)
- `--threshold-threads-warn <n>`: Warning thread threshold under heavy workload (default: `50 threads`)
- `-c, --threshold-cpu <%>`: CPU warning threshold percentage (default: `80%`)
- `-l, --leak-growth <n>`: Consecutive interval memory increases required to flag a leak (default: `4`)
- `--thread-growth <n>`: Consecutive interval thread increases required to flag thread exhaustion (default: `4`)
- `--signal-alert <SIG>`: POSIX signal dispatched to process on alert (default: `SIGUSR1`, e.g. `SIGUSR2`)
- `--pause-on-exhaustion`: Dispatch POSIX `SIGSTOP` to freeze runaway thread spawning under load
- `--auto-kill`: Proactively terminate the leaking process on exhaustion breach (`SIGTERM`/`SIGKILL`)
- `--auto-optimize`: Automatically trigger system memory cache purge when threshold is reached
- `--no-ui-notify`: Suppress desktop OS UI notification banners (keep terminal logging and POSIX signals)
- `--once`: Record a single snapshot to the report and exit

```bash
# Proactively monitor Chrome with a 1GB limit and auto-termination policy:
appsentry monitor chrome --threshold-mem 1000 --auto-kill

# Monitor a specific PID with 1-second sampling and leak sensitivity:
appsentry monitor 54606 --interval 1 --leak-growth 3
```

#### 2. View App Usage Report
```bash
appsentry report <app_name>
```
- Displays statistics on runtime, CPU, peak memory consumption, and alert history.

#### 3. Optimize System
```bash
appsentry optimize <app_name> [--kill] [--force] [--purge]
```
- Attempts to free memory or stop a running application, reporting reclaimed memory.

#### 4. Launch an Application
```bash
appsentry launch <app_name>
```
- Starts an application from the CLI across macOS (`open -a`), Linux (`gtk-launch`/`xdg-open`), or Windows (`start`).

#### 5. List Active Processes
```bash
appsentry list [filter]
```
- Lists running processes sorted by resident memory with PID, CPU%, memory, thread count, state, and owner.

## File Storage Structure
```
📂 AppSentry/
 ├── 📂 reports/                  # Stores app reports & alert audits
 │   ├── chrome.report            # Report for Google Chrome
 │   ├── vscode.report            # Report for VS Code
 │   ├── discord.report           # Report for Discord
 ├── src/                         # Source files
 │   ├── main.cpp                 # CLI handler & command routing
 │   ├── monitor/                 # Tracks running apps & usage
 │   │   ├── monitor.h
 │   │   ├── monitor.cpp
 │   ├── logger/                  # Handles report writing & summary parsing
 │   │   ├── logger.h
 │   │   ├── logger.cpp
 │   ├── alert/                   # Threshold alerting, leak detection & OS UI notifier
 │   │   ├── threshold_alert.h
 │   │   ├── threshold_alert.cpp
 │   │   ├── os_notifier.h
 │   │   ├── os_notifier.cpp
 │   ├── optimizer/               # Frees memory, stops processes, purges cache
 │   │   ├── optimizer.h
 │   │   ├── optimizer.cpp
 │   ├── launcher/                # Opens apps via CLI
 │   │   ├── launcher.h
 │   │   ├── launcher.cpp
 │   ├── shared/                  # Common utilities and data structures
 │   │   ├── headers/
 │   │   │   ├── constants.h
 │   │   │   ├── getos.h
 │   │   │   ├── process_info.h
 │   │   │   ├── util.h
 │   │   ├── getos.cpp
 │   │   ├── process_info.cpp
 │   │   ├── util.cpp
 ├── appsentry                    # Compiled binary
 ├── Makefile
 ├── README.md
```

## Example Report Format (`reports/vscode.report`)
```
========================================================
 AppSentry - Application Usage Report
========================================================
Application: Visual Studio Code
Total Runtime: 3 hours 42 minutes
Peak Memory Usage: 350MB
Average CPU Usage: 22%
First Monitored: 2025-04-01 14:32:00
Last Active: 2025-04-01 18:14:00
--------------------------------------------------------
Resource Exhaustion & Memory Leak Prevention:
  Total Snapshots Logged: 120
  Memory Leaks Detected: 1
  Memory Threshold Warnings: 2
  High CPU Warnings: 0
  Critical Exhaustion Breaches: 1
  Resource Exhaustion Events Prevented: 1
--------------------------------------------------------
Recent Diagnostic Alerts:
  * Resident memory is 350.0 MB, exceeding critical threshold of 300.0 MB.
  * Monotonic memory expansion across 6 consecutive intervals (+50.0 MB net, slope +8.3 MB/s).
  * Proactively terminated leaking process to prevent system resource exhaustion.
========================================================
```

## Contributing
1. Fork the repository.
2. Create a new branch (`git checkout -b feature-branch`).
3. Commit changes (`git commit -m "Added new feature"`).
4. Push to the branch (`git push origin feature-branch`).
5. Create a Pull Request.

## License
MIT License

## Author
[Samuel Emeka](https://github.com/samthemogul)
