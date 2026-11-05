#ifndef TERRARIUM_WORLD_H
#define TERRARIUM_WORLD_H

#include <stdbool.h>
#include <stdint.h>

#define WORLD_W 120
#define WORLD_H 68
#define WORLD_CELLS (WORLD_W * WORLD_H)
#define MAX_CREATURES 96
#define SIM_DT 0.05f
#define DAY_SECONDS 90.0f
#define SEASON_SECONDS 270.0f

typedef enum { WATER, MEADOW, SHORE, ROCK } Terrain;
typedef enum { GRASS, REED, SHRUB } Plant;
typedef enum { RABBIT, SNAIL, FOX } Species;
typedef enum { WANDER, EAT, REST, FLEE, HUNT, DRINK, MIGRATE } Behavior;
typedef enum { SPRING, SUMMER, AUTUMN, WINTER } Season;
typedef enum { CLEAR, CLOUD, RAIN } Weather;

typedef struct {
    Terrain terrain;
    Plant plant;
    float moisture, biomass, danger;
    uint8_t variation;
} Cell;

typedef struct {
    uint32_t id;
    Species species;
    Behavior behavior;
    float x, y, previous_x, previous_y, heading;
    float energy, hydration, age, lifespan, cooldown, decision;
    float target_x, target_y, waypoint_x, waypoint_y;
    uint32_t prey;
    bool alive, emigrating;
} Creature;

typedef struct {
    Cell cells[WORLD_CELLS];
    Creature creatures[MAX_CREATURES];
    uint32_t rng, next_id;
    float time, weather_left, wetness, temperature, rain_amount, cloud_amount;
    Season season;
    Weather weather;
    unsigned births, grazings, hunts, migrations, arrivals, departures;
} World;

float world_random(World *world);
void world_init(World *world, uint32_t seed);
void world_step(World *world, float dt);
bool world_land(const World *world, int x, int y);
Creature *world_find(World *world, uint32_t id);
const Creature *world_find_const(const World *world, uint32_t id);
Creature *world_spawn(World *world, Species species, float x, float y);
int world_count(const World *world, Species species);
float world_activity(Species species, float time);
bool world_invariants(const World *world);

#endif