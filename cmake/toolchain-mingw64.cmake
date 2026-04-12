# ── MinGW-w64 Cross-Compilation Toolchain (WSL → Windows x64) ──────────────
#
# Usage (from WSL):
#   cmake -B build/windows \
#         -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-mingw64.cmake \
#         ..
#   cmake --build build/windows
#
# Prerequisites (install in WSL):
#   sudo apt install mingw-w64 g++-mingw-w64-x86-64
# ───────────────────────────────────────────────────────────────────────────

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

# Cross-compiler binaries
set(CMAKE_C_COMPILER   x86_64-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
set(CMAKE_RC_COMPILER  x86_64-w64-mingw32-windres)

# Target environment search paths
set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# Adjust search behaviour
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
