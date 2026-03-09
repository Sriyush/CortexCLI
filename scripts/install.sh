#!/bin/bash
set -e

echo "--- Cortex CLI Installer ---"

# Detect OS
OS="$(uname)"
if [ "$OS" != "Linux" ]; then
    echo "Error: Only Linux is supported at this time."
    exit 1
fi

# Dependencies check
for cmd in cmake g++ make curl; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: $cmd is not installed. Please install it first."
        exit 1
    fi
done

# Interactive check for sqlite3
if ! command -v sqlite3 &> /dev/null; then
    echo "Warning: sqlite3 is not installed. It is required for Cortex memory management."
    read -p "Would you like to install sqlite3 now? (y/n): " choice
    case "$choice" in 
      y|Y ) 
        echo "Attempting to install sqlite3..."
        if [ -f /etc/debian_version ]; then
            sudo apt-get update && sudo apt-get install -y sqlite3 libsqlite3-dev
        else
            echo "Non-Debian system detected. Please install sqlite3 and libsqlite3-dev manually."
            exit 1
        fi
        ;;
      * ) 
        echo "Aborting installation as sqlite3 is required."
        exit 1
        ;;
    esac
fi

# Create installation directory
INSTALL_DIR="$HOME/.cortex"
mkdir -p "$INSTALL_DIR"
cd "$INSTALL_DIR"

# Simulating download (In a real scenario, we'd fetch from GitHub)
# For now, we assume the user is running this from the repo root
# or we'd clone the repo.
REPO_URL="https://github.com/Sriyush/CortexCLI.git"
if [ ! -d "CortexCLI" ]; then
    echo "Cloning CortexCLI..."
    git clone "$REPO_URL"
else
    echo "CortexCLI already exists, updating..."
    cd CortexCLI && git pull && cd ..
fi

cd CortexCLI
mkdir -p build
cd build
echo "Building Cortex..."
cmake ..
make -j$(nproc)

echo "Setting up symlink..."
sudo ln -sf "$INSTALL_DIR/CortexCLI/build/cortex" /usr/local/bin/cortex

echo "Success! Cortex CLI installed."
echo "Run 'cortex --help' to get started."
