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
for cmd in cmake g++ make sqlite3 curl; do
    if ! command -v $cmd &> /dev/null; then
        echo "Error: $cmd is not installed. Please install it first."
        exit 1
    fi
done

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
