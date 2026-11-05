#include "observation.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)
int main(void)
{
    static World world, control;
    world_init(&world, 72);
    control = world;
    Observation observation;
    observation_init(&observation);
    uint32_t selected = world.creatures[0].id;
    observation_select(&observation, &world, selected);
    float x, y;
    observation_screen(&observation, world.creatures[0].x, world.creatures[0].y, &x, &y);
    CHECK(observation_hit(&observation, &world, x, y) == selected);
    CHECK(observation_hit(&observation, &world, -1, y) == 0);
    for (int tick = 0; tick < 800; ++tick) {
        world_step(&world, SIM_DT); world_step(&control, SIM_DT);
        observation_step(&observation, &world, SIM_DT);
        CHECK(observation.count <= TRAIL_CAPACITY);
    }
    CHECK(memcmp(&world, &control, sizeof(world)) == 0);
    CHECK(observation.count > 0);
    Creature *creature = world_find(&world, selected);
    if (creature) creature->alive = false;
    Creature *replacement = world_spawn(&world, RABBIT, world.creatures[0].x, world.creatures[0].y);
    CHECK(replacement && replacement->id != selected);
    observation_step(&observation, &world, SIM_DT);
    CHECK(observation.selected == 0);
    world.time += TRAIL_SECONDS;
    observation_step(&observation, &world, SIM_DT);
    CHECK(observation.count == 0);
    puts("Observation checks passed.");
    return 0;
}