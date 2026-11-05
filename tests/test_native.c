#include "render.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #condition, SDL_GetError()); exit(1); } } while (0)

int main(void)
{
    CHECK(SDL_Init(SDL_INIT_VIDEO));
    const int sizes[][2] = {{960, 544}, {320, 960}, {640, 1080}, {160, 480},
                            {1440, 900}, {1920, 1080}, {480, 272}, {960, 544}};
    static World world;
    world_init(&world, 42);
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    CHECK(SDL_CreateWindowAndRenderer("Terrarium resize check", sizes[0][0], sizes[0][1],
                                     SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY, &window, &renderer));
    View view;
    CHECK(view_init(&view, renderer));
    Observation observation;
    observation_init(&observation);
    observation_select(&observation, &world, world.creatures[0].id);
    for (int size = 0; size < (int)(sizeof(sizes) / sizeof(sizes[0])); ++size) {
        CHECK(SDL_SetWindowSize(window, sizes[size][0], sizes[size][1]));
        CHECK(SDL_SyncWindow(window));
        SDL_Event pending;
        while (SDL_PollEvent(&pending)) {}
        CHECK(view_resize(&view, &observation));
        observation_step(&observation, &world, 10);
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
        int output_width, output_height;
        CHECK(SDL_GetRenderOutputSize(renderer, &output_width, &output_height));
        CHECK(capture->w == output_width && capture->h == output_height);
        float scale_x, scale_y;
        CHECK(SDL_GetRenderScale(renderer, &scale_x, &scale_y));
        CHECK(scale_x == scale_y && scale_x == floorf(scale_x));
        CHECK(fabsf(observation.width * scale_x - output_width) < 0.01f);
        CHECK(fabsf(observation.height * scale_y - output_height) < 0.01f);
        CHECK(observation.selected == world.creatures[0].id);
        int colored = 0;
        for (int row = 0; row < capture->h; row += 4) {
            for (int column = 0; column < capture->w; column += 4) {
                Uint8 red, green, blue, alpha;
                CHECK(SDL_ReadSurfacePixel(capture, column, row, &red, &green, &blue, &alpha));
                if (green > 70 && green > red + 15) ++colored;
            }
        }
        CHECK(colored > (capture->w / 4) * (capture->h / 4) / 5);
        for (int corner = 0; corner < 4; ++corner) {
            Uint8 red, green, blue, alpha;
            CHECK(SDL_ReadSurfacePixel(capture, corner % 2 ? capture->w - 1 : 0,
                corner / 2 ? capture->h - 1 : 0, &red, &green, &blue, &alpha));
            CHECK(red != 26 || green != 45 || blue != 37);
        }
        char filename[64];
        snprintf(filename, sizeof(filename), "render-native-%dx%d.bmp", sizes[size][0], sizes[size][1]);
        CHECK(SDL_SaveBMP(capture, filename));
        printf("Native %dx%d: renderer=%s output=%dx%d colored=%d focus=%u\n", sizes[size][0], sizes[size][1],
               SDL_GetRendererName(renderer), capture->w, capture->h, colored, observation.selected);
        SDL_DestroySurface(capture);
        CHECK(SDL_RenderPresent(renderer));
        if (size == 7) {
            CHECK(SDL_SetWindowFullscreen(window, true));
            CHECK(SDL_SyncWindow(window));
            CHECK(view_resize(&view, &observation));
            observation_step(&observation, &world, 0);
            CHECK(view_draw(&view, &world, &observation, 1));
            CHECK(SDL_RenderPresent(renderer));
            CHECK(SDL_SetWindowFullscreen(window, false));
            CHECK(SDL_SyncWindow(window));
            CHECK(view_resize(&view, &observation));
        }
    }
    view_destroy(&view);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    puts("Native rendering, input scaling and fullscreen checks passed.");
    return 0;
}