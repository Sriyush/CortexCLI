#!/bin/bash
set -e

echo "--- Cortex CLI Uninstaller ---"

# Confirm uninstallation
read -p "Are you sure you want to uninstall Cortex CLI? (y/n): " confirm < /dev/tty
if [[ ! "$confirm" =~ ^[Yy]$ ]]; then
    echo "Uninstallation cancelled."
    exit 0
fi

# Remove symlink
if [ -L /usr/local/bin/cortex ]; then
    echo "Removing symlink /usr/local/bin/cortex..."
    sudo rm /usr/local/bin/cortex
fi

# Remove installation directory
INSTALL_DIR="$HOME/.cortex"
if [ -d "$INSTALL_DIR" ]; then
    echo "Removing installation directory $INSTALL_DIR..."
    rm -rf "$INSTALL_DIR"
fi

echo "--- Uninstallation Complete ---"
echo "Note: Dependencies like sqlite3, ZeroMQ, and Ollama were not removed."
