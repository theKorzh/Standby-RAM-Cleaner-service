# Standby RAM Cleaner Service

**Standby RAM Cleaner Service** is a Windows service that monitors system memory and purges the Standby List when zeroed memory drops below a specified threshold.

---
## Background

In some cases (in my case — almost always), Windows does not release Standby Memory even when the amount of free memory reaches zero. This can be quite frustrating, especially when you have 64 GB of RAM, but applications crash due to “out of memory” errors because 40 GB is occupied by Standby Memory. There are plenty of reports about this issue across the internet, but all the more or less official responses from Microsoft boil down to “run sfc /scannow” or “try a clean installation.” Needless to say, that doesn’t fix the problem.
Unofficial advice usually comes down to “use RAMMap from Sysinternals.” RAMMap is indeed an excellent tool, but having to launch it every few minutes can get tiresome.

I couldn’t find an existing solution to this problem, so I decided to create a small system service that periodically checks the amount of unused memory and, when it runs out, clears the Standby Memory.

### WARNING!

A significant part of the code was written with the help of ChatGPT and may contain errors. I don’t claim to be a good programmer—or a programmer at all. Please keep that in mind.
---
## Features

- Monitors zeroed system memory.
- Automatically purges the Standby List when available memory falls below a configurable threshold.
- Registry-configurable settings:
  - `HKEY_LOCAL_MACHINE\SOFTWARE\MemoryCleaner\MinFreeMB` (default: 2048 MB)
  - `HKEY_LOCAL_MACHINE\SOFTWARE\MemoryCleaner\CheckIntervalSec` (default: 10 seconds)
- Automatic service installation and uninstallation.

---

## Build Instructions

### Requirements

- Windows 7/8/10/11  
- Compiler: MinGW-w64  
- Make (MinGW/MSYS2 includes `make`)  

### Build with MinGW

1. Open CMD or MSYS2 shell.
2. Navigate to the project folder:

```cmd
cd build
make clean
make INSTALL_DIR=C:/StandbyRAMCleaner all
```

## Install & Uninstall Service
#### Default Installation (Program Files) with MinGW make

```cmd
cd build
make           # build and install
make uninstall # stop service and uninstall
make clean     # clean build artifacts
```

#### Custom Installation Path with MinGW make

```cmd
make INSTALL_DIR="D:/Tools/StandbyRAMCleaner" all
```

## Service Command-Line Options

    /install — install and start the service.

    /uninstall — stop and remove the service.

Example:

StandbyRAMCleaner.exe /install
StandbyRAMCleaner.exe /uninstall

## Registry Settings

    MinFreeMB — minimum free memory in MB to trigger Standby List purge.

    CheckIntervalSec — memory check interval in seconds.

Registry path: HKEY_LOCAL_MACHINE\SOFTWARE\MemoryCleaner
## License

This project is licensed under the GNU General Public License v3 (GPLv3) — see the LICENSE file for details.
