#!/usr/bin/env bash
# Universal QEMU launcher for the bare-metal calculator.
# Works from ANY working directory, on ANY machine, on ANY OS.

set -e

# Resolve repo root relative to THIS script, never the caller's cwd.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "${SCRIPT_DIR}/../.." && pwd)"

# Locate the kernel/ISO across every plausible build layout.
KERNEL_CANDIDATES=(
    "${REPO_ROOT}/Build/isofiles/boot/kernel.elf"
    "${REPO_ROOT}/build/isofiles/boot/kernel.elf"
    "${REPO_ROOT}/kernel.elf"
)
ISO_CANDIDATES=(
    "${REPO_ROOT}/Build/calculator.iso"
    "${REPO_ROOT}/build/calculator.iso"
    "${REPO_ROOT}/calculator.iso"
)

KERNEL_BIN=""
for candidate in "${KERNEL_CANDIDATES[@]}"; do
    if [ -f "${candidate}" ]; then
        KERNEL_BIN="${candidate}"
        break
    fi
done

ISO_IMAGE=""
for candidate in "${ISO_CANDIDATES[@]}"; do
    if [ -f "${candidate}" ]; then
        ISO_IMAGE="${candidate}"
        break
    fi
done

if [ -z "${KERNEL_BIN}" ] && [ -z "${ISO_IMAGE}" ]; then
    echo "Error: no kernel.elf or calculator.iso found under ${REPO_ROOT}."
    echo "Please build the project first."
    exit 1
fi

# Find a QEMU binary: PATH first, then well-known install locations.
find_qemu() {
    local qemu
    for qemu in qemu-system-x86_64 qemu-system-x86; do
        if command -v "${qemu}" >/dev/null 2>&1; then
            echo "${qemu}"
            return 0
        fi
    done
    for qemu in \
        "/usr/bin/qemu-system-x86_64" \
        "/opt/homebrew/bin/qemu-system-x86_64" \
        "/mnt/c/Program Files/qemu/qemu-system-x86_64.exe"; do
        if [ -x "${qemu}" ]; then
            echo "${qemu}"
            return 0
        fi
    done
    return 1
}

QEMU_BIN="$(find_qemu)" || {
    echo "Error: qemu-system-x86_64 not found on PATH or in standard locations."
    exit 1
}

echo "=== Launching QEMU ==="
echo "Kernel/ISO : ${KERNEL_BIN:-${ISO_IMAGE}}"
echo "QEMU       : ${QEMU_BIN}"
echo "Press Ctrl+A then X to exit the QEMU monitor."
echo
if [ -n "${KERNEL_BIN}" ]; then
    echo "For GDB debugging: gdb -ex \"symbol-file ${KERNEL_BIN}\" -x \"${SCRIPT_DIR}/GDBInit.gdb\""
    echo
fi

ARGS=(-m 512M -no-reboot -no-shutdown -d guest_errors,cpu_reset -s -S)
if [ -n "${ISO_IMAGE}" ]; then
    ARGS+=(-cdrom "${ISO_IMAGE}")
elif [ -n "${KERNEL_BIN}" ]; then
    ARGS+=(-kernel "${KERNEL_BIN}")
fi

"${QEMU_BIN}" "${ARGS[@]}"
