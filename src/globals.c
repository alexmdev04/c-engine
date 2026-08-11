#include "src.c"

const i32 SX_WINDOW_WIDTH = 960;
const i32 SX_WINDOW_HEIGHT = 540;

bool SX_TryCreateWindow(
    SDL_Window** window, 
    SDL_Surface** surface,
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

    auto newSurface = SDL_GetWindowSurface(newWindow);

    if (!newSurface) {
        SDL_Log("Surface creation failed: %s\n", SDL_GetError());
        return false;
    }

    *window = newWindow;
    *surface = newSurface;
    return true;
}
