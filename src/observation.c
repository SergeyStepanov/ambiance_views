#include "observation.h"
#include <math.h>
#include <string.h>

void observation_init(Observation *observation)
{
    memset(observation, 0, sizeof(*observation));
    observation->center_x = WORLD_W * 0.5f;
    observation->center_y = WORLD_H * 0.5f;
    observation->zoom = 1;
    observation->width = WORLD_W * 4;
    observation->height = WORLD_H * 4;
}

void observation_select(Observation *observation, const World *world, uint32_t id)
{
    if (!world_find_const(world, id)) { observation->selected = 0; return; }
    observation->selected = id;
    observation->head = observation->count = 0;
    observation->last_sample = world->time - 1;
}

void observation_step(Observation *observation, const World *world, float elapsed)
{
    while (observation->count > 0) {
        int oldest = (observation->head - observation->count + TRAIL_CAPACITY) % TRAIL_CAPACITY;
        if (world->time - observation->trail[oldest].time < TRAIL_SECONDS) break;
        --observation->count;
    }
    const Creature *creature = world_find_const(world, observation->selected);
    if (!creature) observation->selected = 0;
    float target_zoom = creature ? 1.5f : 1;
    float blend = 1 - expf(-elapsed * 3);
    observation->zoom += (target_zoom - observation->zoom) * blend;
    float margin_x = observation->width / (8 * observation->zoom);
    float margin_y = observation->height / (8 * observation->zoom);
    float target_x = creature ? fmaxf(margin_x, fminf(WORLD_W - margin_x, creature->x)) : WORLD_W * 0.5f;
    float target_y = creature ? fmaxf(margin_y, fminf(WORLD_H - margin_y, creature->y)) : WORLD_H * 0.5f;
    observation->center_x += (target_x - observation->center_x) * blend;
    observation->center_y += (target_y - observation->center_y) * blend;
    observation->center_x = fmaxf(margin_x, fminf(WORLD_W - margin_x, observation->center_x));
    observation->center_y = fmaxf(margin_y, fminf(WORLD_H - margin_y, observation->center_y));
    if (creature && world->time - observation->last_sample >= 0.4f) {
        observation->trail[observation->head] = (TrailPoint){creature->x, creature->y, world->time};
        observation->head = (observation->head + 1) % TRAIL_CAPACITY;
        if (observation->count < TRAIL_CAPACITY) ++observation->count;
        observation->last_sample = world->time;
    }
}

void observation_screen(const Observation *observation, float x, float y, float *screen_x, float *screen_y)
{
    *screen_x = roundf((x - observation->center_x) * 4 * observation->zoom + observation->width * 0.5f);
    *screen_y = roundf((y - observation->center_y) * 4 * observation->zoom + observation->height * 0.5f);
}

uint32_t observation_hit(const Observation *observation, const World *world, float screen_x, float screen_y)
{
    if (screen_x < 0 || screen_y < 0 || screen_x >= observation->width || screen_y >= observation->height) return 0;
    float best = 8 * observation->zoom;
    best *= best;
    uint32_t found = 0;
    for (int index = 0; index < MAX_CREATURES; ++index) {
        const Creature *creature = &world->creatures[index];
        if (!creature->alive) continue;
        float x, y;
        observation_screen(observation, creature->x, creature->y, &x, &y);
        float delta_x = screen_x - x, delta_y = screen_y - y;
        float distance = delta_x * delta_x + delta_y * delta_y;
        if (distance <= best) { best = distance; found = creature->id; }
    }
    return found;
}