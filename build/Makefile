CC = gcc
CFLAGS = -O2
LIBS = -lAdvapi32
TARGET = StandbyRAMCleaner.exe
SRC = ../src/StandbyRAMCleaner.c
RC = ../src/version.rc
RESOBJ = version.o

# Default install dir for CI (no spaces)
DEFAULT_INSTALL_DIR := /c/StandbyRAMCleaner

INSTALL_DIR ?= $(DEFAULT_INSTALL_DIR)

# Convert Windows-style path to MSYS2-compatible path
INSTALL_DIR_UNIX := $(shell echo $(INSTALL_DIR) | sed 's#^\([A-Za-z]\):#/\\L\1#; s#\\#/#g')

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
	"$(INSTALL_DIR_UNIX)/$(TARGET)" /uninstall
	@if [ -d "$(INSTALL_DIR_UNIX)" ]; then \
		echo [INFO] Removing directory "$(INSTALL_DIR_UNIX)" ... && \
		rm -rf "$(INSTALL_DIR_UNIX)"; \
	fi

clean: uninstall
	@echo [INFO] Cleaning build artifacts ...
	@rm -f $(RESOBJ) $(TARGET)
