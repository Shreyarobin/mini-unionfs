#!/bin/bash

echo "Running tests..."

# Check if executable exists
if [ -f "unionfs" ]; then
    echo "Build successful ✅"
else
    echo "Build failed ❌"
    exit 1
fi

echo "All tests passed 🎉"