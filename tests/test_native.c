#include "render.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #condition, SDL_GetError()); exit(1); } } while (0)

int main(void)
{
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    const int sizes[4][2] = {{960, 544}, {1440, 900}, {1920, 1080}, {480, 272}};
    static World world;
    world_init(&world, 42);
    for (int size = 0; size < 4; ++size) {
        SDL_Window *window = NULL;
        SDL_Renderer *renderer = NULL;
        CHECK(SDL_CreateWindowAndRenderer("Terrarium visual check", sizes[size][0], sizes[size][1],
                                         SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer));
        CHECK(SDL_SetRenderLogicalPresentation(renderer, WORLD_W * 4, WORLD_H * 4,
                                              SDL_LOGICAL_PRESENTATION_INTEGER_SCALE));
        View view;
        CHECK(view_init(&view, renderer));
        Observation observation;
        observation_init(&observation);
        float x, y, window_x, window_y;
        observation_screen(&observation, world.creatures[0].x, world.creatures[0].y, &x, &y);
        CHECK(view_draw(&view, &world, &observation, 1));
        CHECK(SDL_RenderCoordinatesToWindow(renderer, x, y, &window_x, &window_y));
        SDL_Event click = {0};
        click.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
        click.button.windowID = SDL_GetWindowID(window);
        click.button.x = window_x; click.button.y = window_y;
        CHECK(SDL_ConvertEventToRenderCoordinates(renderer, &click));
        CHECK(fabsf(click.button.x - x) < 0.1f && fabsf(click.button.y - y) < 0.1f);
        uint32_t hit = observation_hit(&observation, &world, click.button.x, click.button.y);
        CHECK(hit == world.creatures[0].id);
        observation_select(&observation, &world, hit);
        for (int tick = 0; tick < 50; ++tick) {
            world_step(&world, SIM_DT);
            observation_step(&observation, &world, SIM_DT);
        }
        CHECK(view_draw(&view, &world, &observation, 1));
        SDL_Surface *capture = SDL_RenderReadPixels(renderer, NULL);
        CHECK(capture);
        int colored = 0;
        for (int row = 0; row < capture->h; row += 4) {
            for (int column = 0; column < capture->w; column += 4) {
                Uint8 red, green, blue, alpha;
                CHECK(SDL_ReadSurfacePixel(capture, column, row, &red, &green, &blue, &alpha));
                if (green > 70 && green > red + 15) ++colored;
            }
        }
        CHECK(colored > 2000);
        char filename[64];
        snprintf(filename, sizeof(filename), "render-native-%dx%d.bmp", sizes[size][0], sizes[size][1]);
        CHECK(SDL_SaveBMP(capture, filename));
        printf("Native %dx%d: renderer=%s output=%dx%d colored=%d focus=%u\n", sizes[size][0], sizes[size][1],
               SDL_GetRendererName(renderer), capture->w, capture->h, colored, observation.selected);
        SDL_DestroySurface(capture);
        CHECK(SDL_RenderPresent(renderer));
        if (size == 3) {
            CHECK(SDL_SetWindowFullscreen(window, true));
            CHECK(SDL_SyncWindow(window));
            CHECK(view_draw(&view, &world, &observation, 1));
            CHECK(SDL_RenderPresent(renderer));
            CHECK(SDL_SetWindowFullscreen(window, false));
            CHECK(SDL_SyncWindow(window));
        }
        view_destroy(&view);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
    }
    SDL_Quit();
    puts("Native rendering, input scaling and fullscreen checks passed.");
    return 0;
}