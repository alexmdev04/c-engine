#include "src.c"

typedef struct {
    SDL_Window* mainWindow;
    SDL_Surface* mainSurface;
    SDL_Surface* helloSurface;
    VkInstance vkInstance;
    
} App;

void SX_App_Stop(App* app) {
    SDL_Log("Stopping...\n");

    if (app->mainWindow) {
        SDL_DestroyWindow(app->mainWindow);
        app->mainWindow = nullptr;
        app->mainSurface = nullptr;
    }

    if (app->helloSurface) {
        SDL_DestroySurface(app->helloSurface);
        app->helloSurface = nullptr;
    }

    SDL_Quit();
}

void SX_App_SafeAbort(App* app) {
    SDL_Log("Safely aborting...\n");
    SX_App_Stop(app);
    abort();
}

void SX_App_CreateWindow(App* app) {
    if (!SX_TryCreateWindow(
        &app->mainWindow,
        &app->mainSurface,
        "swade",
        SX_WINDOW_WIDTH,
        SX_WINDOW_HEIGHT,
        0
    )) {
        SX_App_SafeAbort(app);
    }
}

bool SX_App_RenderLoop(App* app) {
    SDL_FillSurfaceRect(
        app->mainSurface,
        nullptr,
        SDL_MapSurfaceRGB(app->mainSurface, 0x7f, 0x00, 0xff)
    );

    SDL_BlitSurfaceScaled(
        app->helloSurface,
        nullptr,
        app->mainSurface,
        nullptr,
        SDL_SCALEMODE_NEAREST
    );

    SDL_UpdateWindowSurface(app->mainWindow);
    return true;
}

bool SX_App_SDLPoll(App* app) {
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

void SX_App_Update(App* app) {
    SDL_LogVerbose(0, "[SX_App] Starting main loop...\n");

    while (SX_App_SDLPoll(app)) {
        SX_App_RenderLoop(app);
    }
}

void SX_App_Temp_PreloadTestAssets(App* app) {
    const char* imagePath = "hello.bmp";
    app->helloSurface = SDL_LoadBMP(imagePath);
    if (!app->helloSurface) {
        SDL_Log("Unable to load image %s: %s", imagePath, SDL_GetError());
        SX_App_SafeAbort(app);
    }
}

App SX_App_Init(void) {
    App app = { };

    SDL_LogVerbose(0, "[SX_App] Initializing...\n");

    bool initSuccess = SDL_Init(SDL_INIT_VIDEO);

    if (!initSuccess) {
        SDL_Log("[SX_App] Initialization failed: %s \n", SDL_GetError());
        abort();
    }

    SX_App_CreateWindow(&app);
    SX_App_Temp_PreloadTestAssets(&app);

    app.vkInstance = SX_Vk_Init();

    return app;
}