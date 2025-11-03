CC = gcc
CFLAGS = -O2
LIBS = -lAdvapi32
TARGET = StandbyRAMCleaner.exe
SRC = ../src/StandbyRAMCleaner.c
RC = ../src/version.rc
RESOBJ = version.o

ifeq ($(OS),Windows_NT)
    ifeq ($(PROCESSOR_ARCHITECTURE),AMD64)
        DEFAULT_INSTALL_DIR := C:/Program Files/StandbyRAMCleaner
    else
        DEFAULT_INSTALL_DIR := C:/Program Files (x86)/StandbyRAMCleaner
    endif
else
    DEFAULT_INSTALL_DIR := ./StandbyRAMCleaner
endif

INSTALL_DIR ?= $(DEFAULT_INSTALL_DIR)

all: $(TARGET) install

$(RESOBJ): $(RC)
	@echo [INFO] Compiling resources ...
	windres $(RC) -O coff -o $(RESOBJ)

$(TARGET): $(SRC) $(RESOBJ)
	@echo [INFO] Compiling $(SRC) ...
	@$(CC) $(CFLAGS) -o $(TARGET) $(SRC) $(RESOBJ) $(LIBS)

install: $(TARGET)
	@echo [INFO] Installing Standby RAM Cleaner Service ...
	@if not exist "$(INSTALL_DIR)" ( \
		echo [INFO] Creating directory "$(INSTALL_DIR)" ... && \
		mkdir "$(INSTALL_DIR)" \
	)
	@echo [INFO] Copying executable ...
	@copy /Y $(TARGET) "$(INSTALL_DIR)/$(TARGET)" >nul
	@echo [INFO] Running service installation ...
	@"$(INSTALL_DIR)/$(TARGET)" /install
	@echo [INFO] Installation completed.

uninstall:
	@echo [INFO] Uninstalling Standby RAM Cleaner Service ...
	@@"$(INSTALL_DIR)/$(TARGET)" /uninstall
	@if exist "$(INSTALL_DIR)" ( \
		echo [INFO] Removing directory "$(INSTALL_DIR)" ... && \
		rmdir /S /Q "$(INSTALL_DIR)" \
	)

clean: uninstall
	@echo [INFO] Cleaning build artifacts ...
	@del /f "$(RESOBJ)" >nul 2>&1 || exit 0
	@del /f "$(TARGET)" >nul 2>&1 || exit 0
