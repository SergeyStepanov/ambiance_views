#ifndef TERRARIUM_RENDER_H
#define TERRARIUM_RENDER_H
#include <SDL3/SDL.h>
#include "observation.h"
typedef struct {
    SDL_Renderer *renderer;
    SDL_Texture *atlas, *terrain;
    int palette_key;
} View;
bool view_init(View *view, SDL_Renderer *renderer);
bool view_resize(View *view, Observation *observation);
void view_destroy(View *view);
bool view_draw(View *view, const World *world, const Observation *observation, float interpolation);
#endif