#!/usr/bin/env bash

NAME="engine"
LIBS="vulkan SDL3"

RUN_IMMEDIATELY=true
ECHO_FLAGS=false
DEBUG_MODE=false

INCLUDE_DIRS="-Iinclude/external/cglm/include"
DEFAULT_FLAGS="-xc -std=gnu23 -Wall -Wextra -pedantic -Werror=return-type -Iinclude -Iinclude/external $INCLUDE_DIRS"

DEBUG_FLAGS="-O0 -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -fstack-protector-strong"
LINUX_DEBUG_FLAGS="-fPIE -pie"
WINDOWS_DEBUG_FLAGS=""
# DEBUG_FLAGS="-O0 -g -fsanitize=undefined -fno-omit-frame-pointer -fstack-protector-strong"

RELEASE_FLAGS="-O2 -DNDEBUG -fPIE -pie"
LINUX_RELEASE_FLAGS=""
WINDOWS_RELEASE_FLAGS=""

COLOR_GREEN="\e[38;5;154m"
COLOR_YELLOW="\e[38;5;229m"
COLOR_RESET="\e[0m"

sed -i "2s|.*|    Add: [$(echo $DEFAULT_FLAGS | sed "s| |, |g")]|" .clangd

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     HOST_OS=LINUX;;
    Darwin*)    HOST_OS=MAC;;
    CYGWIN*)    HOST_OS=WINDOWS;;
    MINGW*)     HOST_OS=WINDOWS;;
    MSYS*)      HOST_OS=WINDOWS;;
    *)          HOST_OS="UNKNOWN:${unameOut}"
esac

TARGET_OS=$HOST_OS

while [[ $# -gt 0 ]]; do
    case "$1" in
        --debug)
            DEBUG_MODE=true
            shift
            ;;
        --release)
            DEBUG_MODE=false
            shift
            ;;
        --linux)
            TARGET_OS=LINUX
            shift
            ;;
        --windows)
            TARGET_OS=WINDOWS
            shift
            ;;
        --no-run)
            RUN_IMMEDIATELY=false
            shift
            ;;
        --echo-flags)
            ECHO_FLAGS=true
            shift
            ;;
        --help)
            echo -e "Usage: $0 [--debug] [--no-run] [--echo-flags]$COLOR_YELLOW"
            echo -e "  $COLOR_GREEN default         $COLOR_YELLOW Builds the executable in release mode for the current OS, then runs it if the build was successful."
            echo -e "                       Default compiler flags:"
            echo -e "                          $(echo "$DEFAULT_FLAGS" | sed 's/ /\n                          /g')\n"
            echo -e "  $COLOR_GREEN--debug          $COLOR_YELLOW Compiles with the following additional flags;"
            echo -e "                      $(echo "$DEBUG_FLAGS" | sed 's/ /\n                      /g')\n"
            echo -e "  $COLOR_GREEN--release        $COLOR_YELLOW Compiles with the following additional flags;"
            echo -e "                      $(echo "$RELEASE_FLAGS" | sed 's/ /\n                      /g')\n"
            echo -e "  $COLOR_GREEN--linux          $COLOR_YELLOW Builds for Linux. Compiles with the following additional flags;"
            echo -e "                      $(echo "Release: $LINUX_RELEASE_FLAGS" | sed 's/ /\n                      /g')\n"
            echo -e "                      $(echo "Debug: $LINUX_DEBUG_FLAGS" | sed 's/ /\n                      /g')\n"
            echo -e "  $COLOR_GREEN--windows        $COLOR_YELLOW Builds for Windows.\n" # Compiles with the following additional flags;"
            # echo -e "                      $(echo "Release: $WINDOWS_RELEASE_FLAGS" | sed 's/ /\n                      /g')\n"
            # echo -e "                      $(echo "Debug: $WINDOWS_DEBUG_FLAGS" | sed 's/ /\n                      /g')\n"
            echo -e "  $COLOR_GREEN--no-run         $COLOR_YELLOW Disables running the executable on a successful build.\n"
            echo -e "  $COLOR_GREEN--echo-flags     $COLOR_YELLOW Prints all the compiler flags that will be used.\n"
            echo -e "\e[0m"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done


if [ $DEBUG_MODE = true ]; then
    echo -e "Building $NAME in \e[38;5;197mDEBUG\e[0m mode for $TARGET_OS..."
    if [ $TARGET_OS=="WINDOWS" ]; then
        FLAGS="$FLAGS $WINDOWS_DEBUG_FLAGS"
    else
        FLAGS="$FLAGS $LINUX_DEBUG_FLAGS"
    fi
else
    echo -e "Building $NAME in \e[38;5;154mRELEASE\e[0m mode for $TARGET_OS..."
    if [ $TARGET_OS=="WINDOWS" ]; then
        FLAGS="$DEFAULT_FLAGS $WINDOWS_RELEASE_FLAGS"
    else
        FLAGS="$DEFAULT_FLAGS $LINUX_RELEASE_FLAGS"
    fi
fi

FLAGS="$FLAGS$(echo " $LIBS" | sed 's/ / -l/g')"

if [ $ECHO_FLAGS = true ]; then
    echo "Compiler flags: $FLAGS"
fi

TIME_START=$(date +%s%3N)

if [ $TARGET_OS=="WINDOWS" ]; then
    NAME="$NAME".exe
fi

clang $FLAGS src/main.c -o build/$NAME

if [ $? -ne 0 ]; then
    echo "Build failed."
    exit 1
fi

if [ $TARGET_OS!="WINDOWS" ]; then
    chmod +x build/$NAME
fi

TIME_END=$(date +%s%3N)

echo -e "Build successful (\e[38;5;154m$((TIME_END - TIME_START))ms\e[0m)."

if [ $RUN_IMMEDIATELY = false ]; then
    exit 0
fi

echo -e "Running $NAME...\n"

# if [ $DEBUG_MODE = true ]; then
#     valgrind --suppressions=nvidia1.supp --suppressions=nvidia2.supp --leak-check=full --show-reachable=yes --quiet "./$NAME"
# else
#     "./$NAME"
# fi

build/$NAME

echo -e "\nExited $NAME with code: $?"
