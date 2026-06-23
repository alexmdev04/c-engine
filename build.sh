#!/usr/bin/env bash

NAME="engine"
LIBS="vulkan SDL3"

RUN_IMMEDIATELY=true
ECHO_FLAGS=false
DEBUG_MODE=false

DEFAULT_FLAGS="-xc -std=gnu23 -Wall -Wextra -pedantic -Werror=return-type -Iinclude -Iinclude/external -Iinclude/external/cglm/include"
DEBUG_FLAGS="-O0 -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -fstack-protector-strong"
# DEBUG_FLAGS="-O0 -g -fsanitize=undefined -fno-omit-frame-pointer -fstack-protector-strong"
RELEASE_FLAGS="-O2 -DNDEBUG -fPIE -pie"

COLOR_GREEN="\e[38;5;154m"
COLOR_YELLOW="\e[38;5;229m"
COLOR_RESET="\e[0m"

sed -i "2s|.*|    Add: [$(echo $DEFAULT_FLAGS | sed "s| |, |g")]|" .clangd

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
            echo -e "  $COLOR_GREEN default         $COLOR_YELLOW Builds the executable in release mode, then runs it if the build was successful."
            echo -e "                       Default compiler flags:"
            echo -e "                          $(echo "$DEFAULT_FLAGS" | sed 's/ /\n                          /g')\n"
            echo -e "  $COLOR_GREEN--release        $COLOR_YELLOW Compiles with the following additional flags;"
            echo -e "                      $(echo "$RELEASE_FLAGS" | sed 's/ /\n                      /g')\n"
            echo -e "  $COLOR_GREEN--debug          $COLOR_YELLOW Compiles with the following additional flags;"
            echo -e "                      $(echo "$DEBUG_FLAGS" | sed 's/ /\n                      /g')\n"
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
    echo -e "Building $NAME in \e[38;5;197mDEBUG\e[0m mode..."
    FLAGS="$DEFAULT_FLAGS $DEBUG_FLAGS"
else
    echo -e "Building $NAME in \e[38;5;154mRELEASE\e[0m mode..."
    FLAGS="$DEFAULT_FLAGS $RELEASE_FLAGS"
fi

FLAGS="$FLAGS$(echo " $LIBS" | sed 's/ / -l/g')"

if [ $ECHO_FLAGS = true ]; then
    echo "Compiler flags: $FLAGS"
fi

TIME_START=$(date +%s%3N)

clang $FLAGS src/main.c -o build/$NAME

if [ $? -ne 0 ]; then
    echo "Build failed."
    exit 1
fi

chmod +x build/$NAME

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
