#!/bin/bash
set -e

# Change to the root of the repository
cd "$(dirname "$0")/.."

echo "Building Cortex CLI..."
mkdir -p build
cd build

# Generate build system using CMake
cmake ..

# Build using parallel cores
cmake --build . --parallel $(nproc)

echo ""
echo "Build complete. Executable is ready at build/cortex"
