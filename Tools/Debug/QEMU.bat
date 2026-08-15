@echo off
setlocal enabledelayedexpansion
rem Universal Windows QEMU launcher: works from any directory on any PC.

rem Resolve repo root relative to THIS script, never the caller's cwd.
set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"

rem Locate the kernel/ISO across every plausible build layout.
set "KERNEL_BIN="
set "ISO_IMAGE="
for %%C in ("%REPO_ROOT%\Build\isofiles\boot\kernel.elf" "%REPO_ROOT%\build\isofiles\boot\kernel.elf") do (
    if exist "%%~C" set "KERNEL_BIN=%%~C"
)
for %%C in ("%REPO_ROOT%\Build\calculator.iso" "%REPO_ROOT%\build\calculator.iso") do (
    if exist "%%~C" set "ISO_IMAGE=%%~C"
)

if not defined KERNEL_BIN if not defined ISO_IMAGE (
    echo Error: no kernel.elf or calculator.iso found under %REPO_ROOT%.
    echo Please build the project first.
    exit /b 1
)

rem Find QEMU: PATH first, then the standard install locations.
set "QEMU_BIN="
where qemu-system-x86_64 >nul 2>nul && set "QEMU_BIN=qemu-system-x86_64"
if not defined QEMU_BIN if exist "C:\Program Files\qemu\qemu-system-x86_64.exe" set "QEMU_BIN=C:\Program Files\qemu\qemu-system-x86_64.exe"
if not defined QEMU_BIN if exist "C:\Program Files (x86)\qemu\qemu-system-x86_64.exe" set "QEMU_BIN=C:\Program Files (x86)\qemu\qemu-system-x86_64.exe"
if not defined QEMU_BIN (
    echo Error: qemu-system-x86_64 not found. Install QEMU and add it to PATH.
    exit /b 1
)

echo === Launching Bare Metal Calculator in QEMU ===
echo Kernel/ISO : %KERNEL_BIN%%ISO_IMAGE%
echo QEMU       : %QEMU_BIN%
echo Press Ctrl+Alt+G to toggle input grab; close the window to exit.

if defined ISO_IMAGE (
    "%QEMU_BIN%" -m 512M -cdrom "%ISO_IMAGE%" -serial stdio -no-reboot -no-shutdown -s -S
) else (
    "%QEMU_BIN%" -m 512M -kernel "%KERNEL_BIN%" -serial stdio -no-reboot -no-shutdown -s -S
)
