#ifndef TERRARIUM_OBSERVATION_H
#define TERRARIUM_OBSERVATION_H
#include "world.h"
#define TRAIL_CAPACITY 64
#define TRAIL_SECONDS 20.0f
typedef struct { float x, y, time; } TrailPoint;
typedef struct {
    uint32_t selected;
    TrailPoint trail[TRAIL_CAPACITY];
    int head, count;
    float last_sample, center_x, center_y, zoom;
    float width, height;
} Observation;
void observation_init(Observation *observation);
void observation_select(Observation *observation, const World *world, uint32_t id);
void observation_step(Observation *observation, const World *world, float elapsed);
uint32_t observation_hit(const Observation *observation, const World *world, float screen_x, float screen_y);
void observation_screen(const Observation *observation, float x, float y, float *screen_x, float *screen_y);
#endif