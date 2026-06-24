#!/usr/bin/env bash

NAME="engine"
LIBS="vulkan m"
ARCHIVES="SDL3"

RUN_IMMEDIATELY=true
ECHO_FLAGS=false
DEBUG_MODE=false

INCLUDE_DIRS="-Iinclude/SDL3 -Iinclude/external/cglm/include"
DEFAULT_FLAGS="-xc -std=gnu23 -Wall -Wextra -pedantic -Werror=return-type -Iinclude $INCLUDE_DIRS"
# DEFAULT_FLAGS="-xc -std=gnu23 -Wall -Wextra -pedantic -Werror=return-type -Iinclude -Iinclude/external -Llib -lSDL3 -Wl,-rpath,lib $INCLUDE_DIRS"

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

# Auto replace .clangd config file flags so they're are up to date
sed -i "2s|.*|    Add: [$(echo $DEFAULT_FLAGS | sed "s| |, |g")]|" .clangd

# Detect the current OS
unameOut="$(uname -s)"
case "$unameOut" in
    Linux*)     HOST_OS=LINUX;;
    Darwin*)    HOST_OS=MAC;;
    CYGWIN*)    HOST_OS=WINDOWS;;
    MINGW*)     HOST_OS=WINDOWS;;
    MSYS*)      HOST_OS=WINDOWS;;
    *)          HOST_OS="UNKNOWN:$unameOut"
esac

# Set default build target OS to the current OS 
TARGET_OS=$HOST_OS

# Handle command line options
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

# Get debug mode and platform specific compilation flags, and print the current target (e.g. Debug Linux)
if [[ $DEBUG_MODE == true ]]; then
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

# If requested, print the compilation flags
if [[ $ECHO_FLAGS == true ]]; then
    echo "Compiler flags: $FLAGS"
fi

# If on windows, append .exe to the executable name
if [[ $TARGET_OS == "WINDOWS" ]]; then
    NAME="$NAME".exe
fi

# Start compilation timer
TIME_START=$(date +%s%3N)

# Start compilation
clang -c $FLAGS src/main.c -o build/$NAME.o

# If compilation failed, print and exit
if [[ $? -ne 0 ]]; then
    echo "Build failed (compilation)."
    exit 1
fi

# Transform archives into clang input files
read -ra arr <<<" $ARCHIVES"
for a in "${arr[@]}"; 
    do ARCHIVES_AS_FLAGS="$ARCHIVES_AS_FLAGS lib/lib$a.a";
done

# Transform libraries into clang flags
LIBS_AS_FLAGS=$(echo " $LIBS" | sed 's/ / -l/g')

# Link archives and libraries
clang build/"$NAME.o" $ARCHIVES_AS_FLAGS -o build/"$NAME" $LIBS_AS_FLAGS

# Stop compilation timer
TIME_END=$(date +%s%3N)

# If compilation failed, print and exit
if [[ $? -ne 0 ]]; then
    echo "Build failed (linking)."
    exit 1
fi

# If on non-windows then mark the executable as executable
if [[ $TARGET_OS != "WINDOWS" ]]; then
    chmod +x build/$NAME
fi

# Print success and the compilation time
echo -e "Build successful (\e[38;5;154m$((TIME_END - TIME_START))ms\e[0m)."

# If not requested to run the executable then exit
if [[ $RUN_IMMEDIATELY == false ]]; then
    exit 0
fi

# Print that executable is running
echo -e "Running $NAME...\n"

# Start debugging
# if [ $DEBUG_MODE = true ]; then
#     valgrind --suppressions=nvidia1.supp --suppressions=nvidia2.supp --leak-check=full --show-reachable=yes --quiet "./$NAME"
# else
#     "./$NAME"
# fi

# Start executable
build/$NAME

# Print executable exit code
echo -e "\nExited $NAME with code: $?"
