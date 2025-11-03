CC = gcc
CFLAGS = -O2
LIBS = -lAdvapi32
TARGET = StandbyRAMCleaner.exe
SRC = ../src/StandbyRAMCleaner.c
RC = ../src/version.rc
RESOBJ = version.o

# Default install dir (Program Files for x64)
ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
	DEFAULT_INSTALL_DIR := C:/Program\ Files/StandbyRAMCleaner
else
	DEFAULT_INSTALL_DIR := C:/Program\ Files\ \(x86\)/StandbyRAMCleaner
endif

INSTALL_DIR ?= $(DEFAULT_INSTALL_DIR)

# Convert to MSYS2 path for bash, if needed
ifdef CI
INSTALL_DIR_UNIX := $(shell echo "$(INSTALL_DIR)" | tr '\\' '/' | sed -e 's#C:#/c#')
else
INSTALL_DIR_UNIX := $(INSTALL_DIR)
endif

all: $(TARGET) install

$(RESOBJ): $(RC)
	@echo [INFO] Compiling resources ...
	windres $(RC) -O coff -o $(RESOBJ)

$(TARGET): $(SRC) $(RESOBJ)
	@echo [INFO] Compiling $(SRC) ...
	$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(RESOBJ) $(LIBS)

install: $(TARGET)
	@echo [INFO] Installing Standby RAM Cleaner Service ...
	@if [ ! -d "$(INSTALL_DIR_UNIX)" ]; then \
		echo [INFO] Creating directory "$(INSTALL_DIR_UNIX)" ... && \
		mkdir -p "$(INSTALL_DIR_UNIX)"; \
	fi
	@echo [INFO] Copying executable ...
	cp -f $(TARGET) "$(INSTALL_DIR_UNIX)/$(TARGET)"
	@echo [INFO] Installing service ...
	"$(INSTALL_DIR_UNIX)/$(TARGET)" /install
	@echo [INFO] Installation completed.

uninstall:
	@echo [INFO] Uninstalling Standby RAM Cleaner Service ...
	"$(INSTALL_DIR_UNIX)/$(TARGET)" /uninstall || echo "[WARN] Executable not found, skipping uninstall."
	@if [ -d "$(INSTALL_DIR_UNIX)" ]; then \
		echo [INFO] Removing directory "$(INSTALL_DIR_UNIX)" ... && \
		rm -rf "$(INSTALL_DIR_UNIX)"; \
	fi

clean: uninstall
	@echo [INFO] Cleaning build artifacts ...
	@rm -f $(RESOBJ) $(TARGET)
