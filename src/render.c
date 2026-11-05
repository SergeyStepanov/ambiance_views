#include "render.h"
#include "assets.h"
#include <math.h>
#include <string.h>

static SDL_Color mix(SDL_Color first, SDL_Color second, float fraction)
{
    return (SDL_Color){(Uint8)(first.r + (second.r - first.r) * fraction),
        (Uint8)(first.g + (second.g - first.g) * fraction),
        (Uint8)(first.b + (second.b - first.b) * fraction), 255};
}

static SDL_Color land_color(float time)
{
    const SDL_Color palette[4] = {{91, 154, 107, 255}, {109, 161, 100, 255},
                                  {145, 156, 106, 255}, {147, 177, 170, 255}};
    float phase = time / SEASON_SECONDS;
    int season = (int)phase % 4;
    float fraction = phase - floorf(phase);
    fraction = fraction * fraction * (3 - 2 * fraction);
    return mix(palette[season], palette[(season + 1) % 4], fraction);
}

static bool terrain_build(View *view, const World *world)
{
    SDL_Surface *surface = SDL_CreateSurface(WORLD_W * 4, WORLD_H * 4, SDL_PIXELFORMAT_RGBA32);
    if (!surface) return false;
    SDL_Color meadow = land_color(world->time);
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            const Cell *cell = &world->cells[y * WORLD_W + x];
            SDL_Color base = meadow;
            int noise = (cell->variation % 9) - 4;
            if (cell->terrain == WATER) {
                bool edge = world_land(world, x - 1, y) || world_land(world, x + 1, y)
                         || world_land(world, x, y - 1) || world_land(world, x, y + 1);
                base = edge ? (SDL_Color){80, 167, 160, 255} : (SDL_Color){45, 137, 149, 255};
            } else if (cell->terrain == SHORE) base = mix(meadow, (SDL_Color){113, 173, 135, 255}, 0.55f);
            else if (cell->terrain == ROCK) base = (SDL_Color){144, 161, 158, 255};
            base.r = (Uint8)(base.r + noise); base.g = (Uint8)(base.g + noise); base.b = (Uint8)(base.b + noise);
            SDL_Rect tile = {x * 4, y * 4, 4, 4};
            SDL_FillSurfaceRect(surface, &tile, SDL_MapSurfaceRGBA(surface, base.r, base.g, base.b, 255));
            SDL_Rect grain = {x * 4 + cell->variation % 4, y * 4 + (cell->variation / 4) % 4, 1, 1};
            SDL_FillSurfaceRect(surface, &grain, SDL_MapSurfaceRGBA(surface, base.r + 9, base.g + 9, base.b + 9, 255));
            if (cell->terrain == ROCK && cell->variation % 3 == 0) {
                SDL_Rect crack = {x * 4, y * 4 + 3, 3, 1};
                SDL_FillSurfaceRect(surface, &crack, SDL_MapSurfaceRGBA(surface, 103, 126, 126, 255));
            }
        }
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(view->renderer, surface);
    SDL_DestroySurface(surface);
    if (!texture) return false;
    SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_NEAREST);
    SDL_DestroyTexture(view->terrain);
    view->terrain = texture;
    view->palette_key = (int)(world->time / 15);
    return true;
}

bool view_init(View *view, SDL_Renderer *renderer)
{
    memset(view, 0, sizeof(*view));
    view->renderer = renderer;
    view->palette_key = -1;
    view->atlas = assets_create(renderer);
    return view->atlas && SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
}

void view_destroy(View *view)
{
    SDL_DestroyTexture(view->atlas);
    SDL_DestroyTexture(view->terrain);
    memset(view, 0, sizeof(*view));
}

static void rectangle(SDL_Renderer *renderer, float x, float y, float width, float height, SDL_Color color)
{
    SDL_SetRenderDrawColor(renderer, color.r, color.g, color.b, color.a);
    SDL_FRect rect = {roundf(x), roundf(y), fmaxf(1, roundf(width)), fmaxf(1, roundf(height))};
    SDL_RenderFillRect(renderer, &rect);
}

static void sprite(View *view, const Observation *observation, float x, float y,
                   int row, int frame, float size, SDL_FlipMode flip)
{
    float screen_x, screen_y;
    observation_screen(observation, x, y, &screen_x, &screen_y);
    float extent = roundf(size * observation->zoom);
    SDL_FRect source = {(float)(frame * 16), (float)(row * 16), 16, 16};
    SDL_FRect destination = {screen_x - extent / 2, screen_y - extent / 2, extent, extent};
    SDL_RenderTextureRotated(view->renderer, view->atlas, &source, &destination, 0, NULL, flip);
}

bool view_draw(View *view, const World *world, const Observation *observation, float interpolation)
{
    SDL_Renderer *renderer = view->renderer;
    if (view->palette_key != (int)(world->time / 15) && !terrain_build(view, world)) return false;
    SDL_SetRenderDrawColor(renderer, 26, 45, 37, 255);
    if (!SDL_RenderClear(renderer)) return false;
    float origin_x, origin_y;
    observation_screen(observation, 0, 0, &origin_x, &origin_y);
    SDL_FRect land = {origin_x, origin_y, WORLD_W * 4 * observation->zoom, WORLD_H * 4 * observation->zoom};
    if (!SDL_RenderTexture(renderer, view->terrain, NULL, &land)) return false;
    float time = world->time;
    float winter = (1 - sinf(time / (SEASON_SECONDS * 4) * 6.2831853f)) * 0.5f;
    for (int y = 0; y < WORLD_H; ++y) {
        for (int x = 0; x < WORLD_W; ++x) {
            const Cell *cell = &world->cells[y * WORLD_W + x];
            float screen_x, screen_y;
            observation_screen(observation, x + 0.5f, y + 0.5f, &screen_x, &screen_y);
            if (screen_x < -16 || screen_y < -16 || screen_x > WORLD_W * 4 + 16 || screen_y > WORLD_H * 4 + 16) continue;
            if (cell->terrain == WATER) {
                if (cell->variation % 9 == 0) {
                    float shimmer = sinf(time * 0.8f + x * 0.7f + y * 0.4f);
                    if (shimmer > -0.1f)
                        rectangle(renderer, screen_x, screen_y, (2 + shimmer * 3) * observation->zoom, observation->zoom,
                                  (SDL_Color){137, 213, 204, (Uint8)(60 + shimmer * 65)});
                }
                continue;
            }
            if (cell->terrain == ROCK || cell->biomass < 0.18f || cell->variation % 7 != 0) continue;
            float size = cell->plant == GRASS ? 6 + cell->biomass * 5 : 9 + cell->biomass * 9;
            int plant = cell->plant;
            float sway = sinf(time * 1.7f + cell->variation) * 0.18f;
            SDL_SetTextureColorMod(view->atlas, (Uint8)(230 + winter * 25), (Uint8)(255 - winter * 25),
                                   (Uint8)(218 + winter * 37));
            sprite(view, observation, x + 0.5f + sway, y + 0.1f, 3, plant, size, SDL_FLIP_NONE);
        }
    }
    SDL_SetTextureColorMod(view->atlas, 255, 255, 255);
    for (int index = 0; index < observation->count; ++index) {
        int slot = (observation->head - observation->count + index + TRAIL_CAPACITY) % TRAIL_CAPACITY;
        TrailPoint point = observation->trail[slot];
        float age = fmaxf(0, time - point.time);
        float alpha = fmaxf(0, 1 - age / TRAIL_SECONDS);
        float x, y;
        observation_screen(observation, point.x, point.y, &x, &y);
        rectangle(renderer, x - 1, y - 1, 2, 2, (SDL_Color){246, 231, 158, (Uint8)(alpha * 160)});
    }
    int order[MAX_CREATURES], count = 0;
    for (int index = 0; index < MAX_CREATURES; ++index) {
        if (!world->creatures[index].alive) continue;
        int slot = count++;
        while (slot > 0 && world->creatures[order[slot - 1]].y > world->creatures[index].y) {
            order[slot] = order[slot - 1]; --slot;
        }
        order[slot] = index;
    }
    for (int index = 0; index < count; ++index) {
        const Creature *creature = &world->creatures[order[index]];
        float x = creature->previous_x + (creature->x - creature->previous_x) * interpolation;
        float y = creature->previous_y + (creature->y - creature->previous_y) * interpolation;
        float screen_x, screen_y;
        observation_screen(observation, x, y, &screen_x, &screen_y);
        float size = creature->species == SNAIL ? 11 : creature->species == FOX ? 17 : 14;
        float age_scale = creature->age < 45 ? 0.65f + creature->age / 45 * 0.35f : 1;
        size *= age_scale;
        rectangle(renderer, screen_x - size * observation->zoom * 0.28f, screen_y + 3 * observation->zoom,
                  size * observation->zoom * 0.6f, 2 * observation->zoom, (SDL_Color){32, 75, 52, 72});
        int frame = (int)(time * (creature->species == SNAIL ? 3 : 7) + creature->id) % 4;
        if (creature->behavior == REST) frame = 5 + (int)(time * 0.4f) % 2;
        else if (creature->behavior == EAT || creature->behavior == DRINK) frame = (int)(time * 3) % 2 ? 4 : 0;
        if (creature->behavior == FLEE || creature->behavior == HUNT) frame = (int)(time * 14) % 4;
        sprite(view, observation, x, y, creature->species, frame, size,
               cosf(creature->heading) < 0 ? SDL_FLIP_HORIZONTAL : SDL_FLIP_NONE);
        if (creature->id == observation->selected) {
            SDL_SetRenderDrawColor(renderer, 250, 238, 173, 230);
            float radius = size * observation->zoom * 0.5f + 2;
            SDL_RenderLine(renderer, screen_x - radius, screen_y - radius, screen_x - radius + 3, screen_y - radius);
            SDL_RenderLine(renderer, screen_x - radius, screen_y - radius, screen_x - radius, screen_y - radius + 3);
            SDL_RenderLine(renderer, screen_x + radius - 3, screen_y + radius, screen_x + radius, screen_y + radius);
            SDL_RenderLine(renderer, screen_x + radius, screen_y + radius - 3, screen_x + radius, screen_y + radius);
        }
    }
    if (world->cloud_amount > 0.01f) {
        for (int band = 0; band < 3; ++band) {
            float x = fmodf(time * 2 + band * 175, 700) - 120;
            SDL_Vertex vertices[6] = {
                {{x, 0}, {0.16f, 0.25f, 0.22f, 0.10f}, {0, 0}},
                {{x + 100, 0}, {0.16f, 0.25f, 0.22f, 0.10f}, {0, 0}},
                {{x - 25, WORLD_H * 4}, {0.16f, 0.25f, 0.22f, 0.10f}, {0, 0}},
                {{x + 100, 0}, {0.16f, 0.25f, 0.22f, 0.10f}, {0, 0}},
                {{x + 75, WORLD_H * 4}, {0.16f, 0.25f, 0.22f, 0.10f}, {0, 0}},
                {{x - 25, WORLD_H * 4}, {0.16f, 0.25f, 0.22f, 0.10f}, {0, 0}}
            };
            for (int vertex = 0; vertex < 6; ++vertex) vertices[vertex].color.a *= world->cloud_amount;
            SDL_RenderGeometry(renderer, NULL, vertices, 6, NULL, 0);
        }
    }
    if (world->rain_amount > 0.01f) {
        SDL_SetRenderDrawColor(renderer, 197, 226, 222, (Uint8)(105 * world->rain_amount));
        for (int drop = 0; drop < 90; ++drop) {
            float x = fmodf(drop * 73.0f + time * 11, WORLD_W * 4);
            float y = fmodf(drop * 41.0f + time * 130, WORLD_H * 4);
            SDL_RenderLine(renderer, roundf(x), roundf(y), roundf(x - 1), roundf(y + 3));
        }
    }
    float night = (1 - cosf(time / DAY_SECONDS * 6.2831853f)) * 0.5f;
    night = powf(night, 3);
    rectangle(renderer, 0, 0, WORLD_W * 4, WORLD_H * 4, (SDL_Color){30, 50, 64, (Uint8)(night * 105)});
    return SDL_FlushRenderer(renderer);
}