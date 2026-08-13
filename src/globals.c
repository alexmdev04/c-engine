#include "src.c"

const i32 SX_WINDOW_WIDTH = 960;
const i32 SX_WINDOW_HEIGHT = 540;
#define c_str const char* // immutable value, mutable pointer
#define c_str_arr const char* const* // immutable values, mutable pointers to values

bool SX_TryCreateWindow(
    SDL_Window** window,
    const char* title,
    i32 width,
    i32 height,
    SDL_WindowFlags flags
) {
    SDL_LogVerbose(0, "Creating window...\n");

    auto newWindow = SDL_CreateWindow(title, width, height, flags);

    if (!newWindow) {
        SDL_Log("Window creation failed: %s\n", SDL_GetError());
        return false;
    }

    SDL_LogVerbose(0, "Getting window surface...\n");

    *window = newWindow;
    return true;
}

#define SX_Log_Now(...) printf(__VA_ARGS__); fflush(stdout);

#define countof(arr) (sizeof(arr) / sizeof(typeof(arr[0])))

bool SX_FileExists(c_str path) {
    SDL_IOStream* io = SDL_IOFromFile(path, "rb");
    
    if (!io) {
        return false;
    }

    SDL_CloseIO(io);

    return true;
}

#ifdef _WIN32
    #define SX_SetCwd _chdir
#else
    #define SX_SetCwd chdir
#endif

#define SX_GetWindowSize(window) i32 windowW, windowH; SDL_GetWindowSize(window, &windowW, &windowH)
