#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include "render.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static bool parse_unsigned(const char *text, unsigned long *value)
{
    char *end = NULL;
    errno = 0;
    *value = strtoul(text, &end, 10);
    return !errno && end != text && !*end && text[0] != '-';
}

int main(int argc, char **argv)
{
    uint32_t seed = (uint32_t)time(NULL) ^ (uint32_t)SDL_GetPerformanceCounter();
    unsigned long frames = 0;
    float advance = 0;
    bool headless = false, smoke = false;
    const char *snapshot = NULL;
    for (int argument = 1; argument < argc; ++argument) {
        if (!strcmp(argv[argument], "--headless")) headless = true;
        else if (!strcmp(argv[argument], "--smoke")) { smoke = true; frames = 180; }
        else if (!strcmp(argv[argument], "--help")) {
            puts("Living Terrarium\nUsage: ambiance_views [--seed N] [--frames N] [--advance SECONDS]\n                      [--snapshot FILE.bmp] [--headless] [--smoke]\nClick an animal to follow. Click empty ground or Escape for overview. F11: fullscreen.");
            return 0;
        } else if ((!strcmp(argv[argument], "--seed") || !strcmp(argv[argument], "--frames")
                    || !strcmp(argv[argument], "--advance")) && argument + 1 < argc) {
            const char *option = argv[argument++];
            unsigned long value;
            if (!parse_unsigned(argv[argument], &value) || value > UINT32_MAX) {
                fprintf(stderr, "Invalid number: %s\n", argv[argument]); return 1;
            }
            if (!strcmp(option, "--seed")) seed = (uint32_t)value;
            else if (!strcmp(option, "--frames")) frames = value;
            else if (value <= 86400) advance = (float)value;
            else { fputs("Advance must be <= 86400 seconds.\n", stderr); return 1; }
        } else if (!strcmp(argv[argument], "--snapshot") && argument + 1 < argc) snapshot = argv[++argument];
        else { fprintf(stderr, "Unknown or incomplete option: %s\n", argv[argument]); return 1; }
    }
    if (headless && !frames) frames = 1;
    int result = 1;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Surface *surface = NULL;
    View view = {0};
    World *world = calloc(1, sizeof(*world));
    if (!world) { fputs("Cannot allocate world.\n", stderr); return 1; }
    Observation observation;
    observation_init(&observation);
    world_init(world, seed);
    for (int tick = 0; tick < (int)(advance / SIM_DT); ++tick) world_step(world, SIM_DT);
    if (!SDL_Init(headless ? SDL_INIT_EVENTS : SDL_INIT_VIDEO)) goto cleanup;
    SDL_SetAppMetadata("Living Terrarium", "0.1.0", "org.ambianceviews.terrarium");
    if (headless) {
        surface = SDL_CreateSurface(WORLD_W * 4, WORLD_H * 4, SDL_PIXELFORMAT_RGBA32);
        if (surface) renderer = SDL_CreateSoftwareRenderer(surface);
    } else {
        if (!SDL_CreateWindowAndRenderer("Living Terrarium", 960, 544,
                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer)) goto cleanup;
        SDL_SetWindowMinimumSize(window, 160, 160);
    }
    if (!renderer || !view_init(&view, renderer) || !view_resize(&view, &observation)) goto cleanup;
    bool vsync = !headless && SDL_SetRenderVSync(renderer, 1);
    bool running = true, minimized = false, fullscreen = false;
    double previous = (double)SDL_GetTicksNS() / 1e9, accumulator = 0;
    unsigned long frame = 0;
    while (running) {
        Uint64 frame_start = SDL_GetTicksNS();
        if (!view_resize(&view, &observation)) goto cleanup;
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_EVENT_QUIT) running = false;
            else if (event.type == SDL_EVENT_WINDOW_MINIMIZED) { minimized = true; accumulator = 0; }
            else if (event.type == SDL_EVENT_WINDOW_RESTORED) {
                minimized = false; accumulator = 0; previous = (double)SDL_GetTicksNS() / 1e9;
            } else if (event.type == SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED || event.type == SDL_EVENT_WINDOW_RESIZED) {
                if (!view_resize(&view, &observation)) goto cleanup;
            } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN && event.button.button == SDL_BUTTON_LEFT) {
                if (SDL_ConvertEventToRenderCoordinates(renderer, &event)) {
                    if (event.button.x >= 0 && event.button.y >= 0 && event.button.x < observation.width && event.button.y < observation.height)
                        observation_select(&observation, world, observation_hit(&observation, world, event.button.x, event.button.y));
                }
            } else if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
                if (event.key.key == SDLK_ESCAPE) observation_select(&observation, world, 0);
                if (event.key.key == SDLK_F11 && window) {
                    fullscreen = !fullscreen;
                    if (!SDL_SetWindowFullscreen(window, fullscreen)) goto cleanup;
                }
            }
        }
        if (!running) break;
        double now = (double)SDL_GetTicksNS() / 1e9;
        double elapsed = now - previous;
        previous = now;
        if (elapsed > 0.25) elapsed = 0.25;
        if (headless) elapsed = SIM_DT;
        if (minimized) {
            SDL_Event wake;
            if (SDL_WaitEventTimeout(&wake, 100)) SDL_PushEvent(&wake);
            continue;
        }
        accumulator += elapsed;
        int updates = 0;
        while (accumulator >= SIM_DT && updates++ < 5) {
            world_step(world, SIM_DT);
            accumulator -= SIM_DT;
        }
        if (accumulator >= SIM_DT) accumulator = 0;
        if (smoke && frame == 15) {
            float screen_x, screen_y, window_x, window_y;
            observation_screen(&observation, world->creatures[0].x, world->creatures[0].y, &screen_x, &screen_y);
            if (!SDL_RenderCoordinatesToWindow(renderer, screen_x, screen_y, &window_x, &window_y)) goto cleanup;
            SDL_Event click = {0};
            click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
            click.button.windowID = window ? SDL_GetWindowID(window) : 0;
            click.button.button = SDL_BUTTON_LEFT;
            click.button.down = true;
            click.button.x = window_x;
            click.button.y = window_y;
            if (!SDL_PushEvent(&click)) goto cleanup;
        }
        if (smoke && frame == 90 && window) SDL_SetWindowSize(window, 1440, 900);
        if (!view_resize(&view, &observation)) goto cleanup;
        observation_step(&observation, world, (float)elapsed);
        if (!view_draw(&view, world, &observation, (float)(accumulator / SIM_DT))) goto cleanup;
        if (!SDL_RenderPresent(renderer)) goto cleanup;
        ++frame;
        if (frames && frame >= frames) running = false;
        if (!vsync && !headless && running) {
            Uint64 spent = SDL_GetTicksNS() - frame_start;
            if (spent < 16666667) SDL_DelayNS(16666667 - spent);
        }
    }
    if (snapshot) {
        if (!view_draw(&view, world, &observation, 1)) goto cleanup;
        SDL_Surface *capture = SDL_RenderReadPixels(renderer, NULL);
        if (!capture) goto cleanup;
        bool saved = SDL_SaveBMP(capture, snapshot);
        SDL_DestroySurface(capture);
        if (!saved) goto cleanup;
        if (!SDL_RenderPresent(renderer)) goto cleanup;
    }
    if (smoke) {
        if (!world_invariants(world) || !observation.selected || !observation.count) {
            fputs("Smoke test failed.\n", stderr); goto cleanup;
        }
        printf("Native smoke passed: %lu frames, follow #%u, trail %d, seed %u\n",
               frame, observation.selected, observation.count, seed);
    }
    result = 0;
cleanup:
    if (result) fprintf(stderr, "Terrarium: %s\n", SDL_GetError());
    view_destroy(&view);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(world);
    return result;
}