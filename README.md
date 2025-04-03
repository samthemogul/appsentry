# AppSentry - Application Usage Monitor & Optimizer

## Overview
**AppSentry** is a command-line application that monitors desktop applications, tracking their CPU and memory usage, and logs how long they have been running. Users can generate detailed reports of app usage and optimize their system by freeing memory, stopping apps, or launching applications directly from the CLI.

## Features
- **Monitor Applications**: Track real-time CPU and memory usage of running applications.
- **Log Usage Data**: Store app activity logs locally in a `reports/` folder.
- **Generate Reports**: View app usage history and performance metrics.
- **Optimize System**: Free memory or terminate running applications.
- **Launch Apps**: Start applications via the command line.

## Installation
### Prerequisites
- C++ compiler (GCC/Clang for Linux, MSVC/MinGW for Windows)
- Windows API (`<windows.h>`) or Linux process management libraries (`procfs`)

### Clone Repository
```bash
git clone https://github.com/yourusername/appsentry.git
cd appsentry
```

### Build
```bash
mkdir build && cd build
cmake ..
make
```
For Windows:
```powershell
mkdir build
cd build
cmake .. -G "MinGW Makefiles"
mingw32-make
```

## Usage
Run `appsentry` followed by a command:
```bash
appsentry <command> [options]
```

### Commands
#### Monitor an Application
```bash
appsentry monitor <app_name>
```
- Starts tracking the CPU and memory usage of an application.
- Logs data to `reports/<app_name>.report`.

#### View App Usage Report
```bash
appsentry report <app_name>
```
- Displays statistics on runtime, CPU, and memory consumption.

#### Optimize System
```bash
appsentry optimize <app_name>
```
- Attempts to free memory or stop a running application.

#### Launch an Application
```bash
appsentry launch <app_name>
```
- Starts an application from the CLI.

## File Storage Structure
```
📂 AppSentry/
 ├── 📂 reports/                  # Stores app reports
 │   ├── chrome.report            # Report for Google Chrome
 │   ├── vscode.report            # Report for VS Code
 │   ├── discord.report           # Report for Discord
 ├── src/                         # Source files
 │   ├── main.cpp                 # CLI handler
 │   ├── monitor.cpp/.h           # Tracks running apps & usage
 │   ├── logger.cpp/.h            # Handles report writing
 │   ├── optimizer.cpp/.h         # Frees memory, stops processes
 │   ├── launcher.cpp/.h          # Opens apps via CLI
 ├── appsentry.exe                 # Compiled binary
```

## Example Report Format (`reports/vscode.report`)
```
Application: Visual Studio Code
Total Runtime: 3 hours 42 minutes
Peak Memory Usage: 350MB
Average CPU Usage: 22%
First Monitored: 2025-04-01 14:32
Last Active: 2025-04-01 18:14
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

