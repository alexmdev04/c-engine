#include "src.c"

typedef struct {
    // SDL
    SDL_Window* mainWindow;
    u32 windowWidth;
    u32 windowHeight;

    // Vulkan
    Vk vk;

    // Other
    // c_str exeName;
    Time time;
} App;

void SX_App_Stop(App* app) {
    SDL_Log("Stopping...\n");

    SX_Vk_Stop(&app->vk);

    if (app->mainWindow) {
        SDL_DestroyWindow(app->mainWindow);
        app->mainWindow = nullptr;
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
        "swade",
        SX_WINDOW_WIDTH,
        SX_WINDOW_HEIGHT,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE
    )) {
        SX_App_SafeAbort(app);
    }
}

bool SX_App_RenderLoop(App* app) {
    SX_Vk_Render(&app->vk, app->mainWindow);
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
            case SDL_EVENT_WINDOW_RESIZED: {
                app->windowWidth = event.window.data1;
                app->windowHeight = event.window.data2;
                break;
            }
        }
    }

    return true;
}

void SX_App_Update(App* app) {
    SDL_LogVerbose(0, "[SX_App] Starting main loop...\n");

    while (SX_App_SDLPoll(app)) {
        SX_Time_Update(&app->time);
        SX_Time_PrintFPS(&app->time);

        SX_App_RenderLoop(app);
    }
}

App SX_App_Init(i32 argc, c_str_arr argv) {
    App app = { };

    SDL_LogVerbose(0, "[SX_App] Initializing...\n");

    bool initSuccess = SDL_Init(SDL_INIT_VIDEO);

    if (!initSuccess) {
        SDL_Log("[SX_App] SDL initialization failed: %s \n", SDL_GetError());
        abort();
    }

    SX_App_CreateWindow(&app);

    c_str cwd = SDL_GetCurrentDirectory();
    
    c_str basePath = SDL_GetBasePath();
    SX_Log_Now("cwd='%s', exePath='%s'\n", cwd, basePath);
    
    if (strcmp(cwd, basePath) != 0) {
        SX_SetCwd(basePath);
        SX_Log_Now("Set cwd to: %s\n", basePath);
    }

    if (!SX_Vk_TryInit(&app.vk, app.mainWindow)) {
        SX_Log_Now("[SK_App] Vulkan initialization failed.\n");
        abort();
    }

    app.time = SX_Time_Init();

    return app;
}