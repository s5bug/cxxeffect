#!/usr/bin/env pwsh

# This changes us to the directory of build.ps1
# It should be in the project root
# This allows us to run build.ps1 from any directory, not just the project root
Set-Location -Path $PSScriptRoot;

# Create build folder in project root and enter it
New-Item -Path . -Name "build" -ItemType "directory" -ErrorAction SilentlyContinue | Out-Null
Set-Location -Path "build"

# Build project
cmake .. -DCMAKE_BUILD_TYPE=RelWithDebInfo
cmake --build .
