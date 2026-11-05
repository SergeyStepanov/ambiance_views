#include "render.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "%s:%d: %s (%s)\n", __FILE__, __LINE__, #condition, SDL_GetError()); exit(1); } } while (0)
int main(void)
{
    CHECK(SDL_Init(0));
    SDL_Surface *surface = SDL_CreateSurface(WORLD_W * 4, WORLD_H * 4, SDL_PIXELFORMAT_RGBA32);
    CHECK(surface);
    SDL_Renderer *renderer = SDL_CreateSoftwareRenderer(surface);
    CHECK(renderer);
    View view;
    CHECK(view_init(&view, renderer));
    static World world;
    world_init(&world, 42);
    Observation observation;
    observation_init(&observation);
    CHECK(view_draw(&view, &world, &observation, 1));
    int water = 0, vegetation = 0, white = 0, fox = 0;
    for (int y = 0; y < surface->h; ++y) {
        for (int x = 0; x < surface->w; ++x) {
            Uint8 red, green, blue, alpha;
            CHECK(SDL_ReadSurfacePixel(surface, x, y, &red, &green, &blue, &alpha));
            if (blue > red + 35 && green > red + 35) ++water;
            if (green > red + 25 && green > blue + 15) ++vegetation;
            if (red > 220 && green > 220 && blue > 190) ++white;
            if (red > 170 && red > green + 55 && blue < 100) ++fox;
        }
    }
    CHECK(water > 10000 && vegetation > 40000 && white > 50 && fox > 10);
    CHECK(SDL_SaveBMP(surface, "render-spring.bmp"));
    observation_select(&observation, &world, world.creatures[0].id);
    for (int frame = 0; frame < 100; ++frame) {
        world_step(&world, SIM_DT);
        observation_step(&observation, &world, SIM_DT);
    }
    world.weather = RAIN;
    world.rain_amount = world.cloud_amount = 1;
    CHECK(view_draw(&view, &world, &observation, 1));
    CHECK(SDL_SaveBMP(surface, "render-follow-rain.bmp"));
    world.time = SEASON_SECONDS * 3;
    observation_init(&observation);
    CHECK(view_draw(&view, &world, &observation, 1));
    CHECK(SDL_SaveBMP(surface, "render-winter.bmp"));
    view_destroy(&view);
    SDL_DestroyRenderer(renderer);
    SDL_DestroySurface(surface);
    SDL_Quit();
    printf("Render checks passed: water=%d plants=%d rabbit=%d fox=%d\n", water, vegetation, white, fox);
    return 0;
}