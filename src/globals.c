#include "src.c"

const i32 X_WINDOW_WIDTH = 960;
const i32 X_WINDOW_HEIGHT = 540;

SDL_Window* X_MainWindow = { };
SDL_Surface* X_MainSurface = { };
SDL_Surface* X_HelloSurface = { };

VkInstance X_Vk_Instance = { };

void X_Stop(void) {
    SDL_Log("Stopping...\n");

    if (X_MainWindow) {
        SDL_DestroyWindow(X_MainWindow);
        X_MainWindow = nullptr;
        X_MainSurface = nullptr;
    }

    if (X_HelloSurface) {
        SDL_DestroySurface(X_HelloSurface);
        X_HelloSurface = nullptr;
    }

    SDL_Quit();
}

void X_SafeAbort(void) {
    SDL_Log("Safely aborting...\n");
    X_Stop();
    abort();
}
