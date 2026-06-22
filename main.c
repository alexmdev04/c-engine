#define MAIN
#include "external.h"
#include "types.c"

const i32 X_WINDOW_WIDTH = 960;
const i32 X_WINDOW_HEIGHT = 540;

SDL_Window* MainWindow = { };
SDL_Surface* MainSurface = { };
SDL_Surface* HelloSurface = { };

void X_Init(void) {
    SDL_LogVerbose(0, "Initializing...\n");

    bool initSuccess = SDL_Init(SDL_INIT_VIDEO);

    if (!initSuccess) {
        SDL_Log("Initialization failed: %s \n", SDL_GetError());
        abort();
    }
}

void X_Stop() {
    SDL_Log("Stopping...\n");

    if (MainWindow) {
        SDL_DestroyWindow(MainWindow);
        MainWindow = nullptr;
        MainSurface = nullptr;
    }

    if (HelloSurface) {
        SDL_DestroySurface(HelloSurface);
        HelloSurface = nullptr;
    }

    SDL_Quit();
}

void X_SafeAbort() {
    SDL_Log("Safely aborting...\n");
    X_Stop();
    abort();
}

void X_CreateWindow(SDL_Window** window, SDL_Surface** surface) {
    SDL_LogVerbose(0, "Creating window...\n");

    auto newWindow =
        SDL_CreateWindow("engine", X_WINDOW_WIDTH, X_WINDOW_HEIGHT, 0);

    if (!newWindow) {
        SDL_Log("Window creation failed: %s\n", SDL_GetError());
        X_SafeAbort();
    }

    SDL_LogVerbose(0, "Getting window surface...\n");

    auto newSurface = SDL_GetWindowSurface(newWindow);

    if (!surface) {
        SDL_Log("Surface creation failed: %s\n", SDL_GetError());
        X_SafeAbort();
    }

    *window = newWindow;
    *surface = newSurface;
}

bool X_SDLPoll() {
    SDL_Event event = { };

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            printf("%u", event.type);
            case SDL_EVENT_QUIT: {
                SDL_LogVerbose(0, "SDL_EVENT_QUIT caught.\n");
                return false;
            }
        }
    }

    return true;
}

void X_PreloadAssets() {
    const char* imagePath = "hello.bmp";
    HelloSurface = SDL_LoadBMP(imagePath);
    if (!HelloSurface) {
        SDL_Log("Unable to load image %s: %s", imagePath, SDL_GetError());
        X_SafeAbort();
    }
}

bool X_RenderLoop() {
    SDL_FillSurfaceRect(
        MainSurface, nullptr, SDL_MapSurfaceRGB(MainSurface, 0x7f, 0x00, 0xff)
    );

    SDL_BlitSurfaceScaled(
        HelloSurface, nullptr, MainSurface, nullptr, SDL_SCALEMODE_NEAREST
    );

    SDL_UpdateWindowSurface(MainWindow);
    return true;
}

void X_Loop() {
    bool quit = false;
    while (!quit) {
        if (!X_SDLPoll()) {
            quit = true;
        }

        X_RenderLoop();
    }
}

int main(void) {
    printf("Here we go!\n");
    X_Init();
    X_CreateWindow(&MainWindow, &MainSurface);
    SDL_LogVerbose(0, "Starting main loop...\n");
    X_PreloadAssets();
    X_Loop();
    X_Stop();
    return 0;
}