#include "src.c"

typedef struct {
    f64 delta;
    f64 elapsed;
    u64 performanceCounter;
    u64 prevPerformanceCounter;
} Time;

void SX_Time_Update(Time* time) {
    time->prevPerformanceCounter = time->performanceCounter;
    time->performanceCounter = SDL_GetPerformanceCounter();

    u64 deltaPerfCount =
        ((time->performanceCounter - time->prevPerformanceCounter));

    f64 delta = deltaPerfCount / (f64)SDL_GetPerformanceFrequency();

    time->delta = delta;
    time->elapsed += delta;
}

void SX_Time_PrintFPS(Time* time) {
    printf(
        "\r[SX_App] FPS=%i, delta=%f, elapsed=%f",
        (i32)(1.0 / time->delta),
        time->delta,
        time->elapsed
    );
    fflush(stdout);
}

Time SX_Time_Init(void) {
    Time time = { .performanceCounter = SDL_GetPerformanceCounter() };
    return time;
}
