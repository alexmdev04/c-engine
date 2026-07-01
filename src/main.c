#include "src.c"

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

bool X_SDLPoll(void) {
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

void X_PreloadAssets(void) {
    const char* imagePath = "hello.bmp";
    X_HelloSurface = SDL_LoadBMP(imagePath);
    if (!X_HelloSurface) {
        SDL_Log("Unable to load image %s: %s", imagePath, SDL_GetError());
        X_SafeAbort();
    }
}

bool X_RenderLoop(void) {
    SDL_FillSurfaceRect(
        X_MainSurface, nullptr, SDL_MapSurfaceRGB(X_MainSurface, 0x7f, 0x00, 0xff)
    );

    SDL_BlitSurfaceScaled(
        X_HelloSurface, nullptr, X_MainSurface, nullptr, SDL_SCALEMODE_NEAREST
    );

    SDL_UpdateWindowSurface(X_MainWindow);
    return true;
}

void X_Loop(void) {
    bool quit = false;
    while (!quit) {
        if (!X_SDLPoll()) {
            quit = true;
        }

        X_RenderLoop();
    }
}

void X_Init(void) {
    SDL_LogVerbose(0, "Initializing...\n");

    bool initSuccess = SDL_Init(SDL_INIT_VIDEO);

    if (!initSuccess) {
        SDL_Log("Initialization failed: %s \n", SDL_GetError());
        abort();
    }

    X_CreateWindow(&X_MainWindow, &X_MainSurface);
    X_PreloadAssets();

    X_Vk_Init();
}

int main(void) {
    printf("Here we go!\n");
    X_Init();
    SDL_LogVerbose(0, "Starting main loop...\n");
    X_Loop();
    X_Stop();
    return 0;
}