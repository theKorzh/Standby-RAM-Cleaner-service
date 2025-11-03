# Standby RAM Cleaner Service

**Standby RAM Cleaner Service** is a Windows service that monitors system memory and purges the Standby List when free memory drops below a specified threshold.

This service is fully standalone, requires no Python or other interpreters, and supports installation to a custom directory (default is Program Files based on system architecture).

---

## Features

- Monitors free system memory.
- Automatically purges the Standby List when available memory falls below a configurable threshold.
- Registry-configurable settings:
  - `HKEY_LOCAL_MACHINE\SOFTWARE\MemoryCleaner\MinFreeMB` (default: 2048 MB)
  - `HKEY_LOCAL_MACHINE\SOFTWARE\MemoryCleaner\CheckIntervalSec` (default: 10 seconds)
- Automatic service installation and uninstallation.
- Integrated version information in the executable.

---

## Repository Structure

StandbyRAMCleaner/
├── src/
│ ├── StandbyRAMCleaner.c # Service source code with GPLv3 header
│ └── version.rc # Version information
├── build/
│ └── Makefile # Makefile for build, install, clean, uninstall
├── .github/
│ └── workflows/
│ └── build.yml # GitHub Actions workflow
├── README.md # This documentation
└── LICENSE # Full GPLv3 license text


---

## Build Instructions

### Requirements

- Windows 7/8/10/11  
- Compiler: MinGW-w64 or MSVC  
- Make (MinGW/MSYS2 includes `make`)  

### Build with MinGW

1. Open CMD or MSYS2 shell.
2. Navigate to the project folder:

```cmd
cd StandbyRAMCleaner

    Build the executable and install the service:

cd build
make clean
make INSTALL_DIR=C:/StandbyRAMCleaner all

Build with MSVC

    Open "Developer Command Prompt for Visual Studio".

    Navigate to src folder:

cd StandbyRAMCleaner\src

    Compile the executable:

cl StandbyRAMCleaner.c version.rc /link Advapi32.lib

Install & Uninstall Service
Default Installation (Program Files)

cd build
make           # build and install
make uninstall # stop service and uninstall
make clean     # clean build artifacts

Custom Installation Path

make INSTALL_DIR="D:/Tools/StandbyRAMCleaner" all

Service Command-Line Options

    /install — install and start the service.

    /uninstall — stop and remove the service.

Example:

StandbyRAMCleaner.exe /install
StandbyRAMCleaner.exe /uninstall

Registry Settings

    MinFreeMB — minimum free memory in MB to trigger Standby List purge.

    CheckIntervalSec — memory check interval in seconds.

Registry path: HKEY_LOCAL_MACHINE\SOFTWARE\MemoryCleaner
License

This project is licensed under the GNU General Public License v3 (GPLv3) — see the LICENSE file for details.
