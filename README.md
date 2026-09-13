# AppSentry - Application Usage Monitor & Optimizer

## Overview
**AppSentry** is a high-performance cross-platform command-line application that monitors desktop applications, tracking their CPU and memory usage, and logs how long they have been running. Users can generate detailed reports of app usage and optimize their system by freeing memory, stopping apps, or launching applications directly from the CLI.

AppSentry includes custom threshold-alerting logic and proactive memory leak detection algorithms to prevent process-level resource exhaustion during long-running tasks.

## Features
- **Monitor Applications**: Track real-time CPU and resident memory (RSS) usage of running applications.
- **Resource Exhaustion Prevention**: Custom threshold-alerting logic that proactively detects memory leaks and prevents system freeze or out-of-memory (OOM) crashes during long-running tasks.
- **Proactive Memory Leak Detection**: Sliding-window statistical analysis tracking monotonic expansion, growth velocity (MB/s), and time-to-exhaustion projections.
- **Automated & Interactive Mitigation**: Automatically terminates runaway leaking processes or purges caches (`--auto-kill`, `--auto-optimize`).
- **Log Usage Data**: Store app activity logs and diagnostic alert histories locally in a `reports/` folder.
- **Generate Reports**: View app usage history, performance metrics, and security/alert audits.
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
- Starts tracking the CPU and memory usage of an application.
- Emits proactive alerts upon detecting continuous monotonic memory expansion (leaks) or threshold breaches.
- Logs data and diagnostic alert events to `reports/<app_name>.report`.

**Options:**
- `-i, --interval <sec>`: Sampling interval in seconds (default: `2s`)
- `-d, --duration <sec>`: Total monitoring duration in seconds (default: continuous until Ctrl+C)
- `-m, --threshold-mem <MB>`: Critical memory threshold for resource exhaustion (default: `800 MB`)
- `-w, --threshold-warn <MB>`: Warning memory threshold (default: `400 MB`)
- `-c, --threshold-cpu <%>`: CPU warning threshold percentage (default: `80%`)
- `-l, --leak-growth <n>`: Consecutive interval increases required to flag a memory leak (default: `4`)
- `--auto-kill`: Proactively terminate the leaking process upon reaching exhaustion threshold to prevent system crash
- `--auto-optimize`: Automatically trigger system memory cache purge when threshold is reached
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
 │   ├── alert/                   # Threshold alerting & memory leak detection
 │   │   ├── threshold_alert.h
 │   │   ├── threshold_alert.cpp
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
