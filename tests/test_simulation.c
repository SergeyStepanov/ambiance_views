#include "world.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition) do { if (!(condition)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); exit(1); } } while (0)

static void fixture(World *world)
{
    world_init(world, 42);
    memset(world->creatures, 0, sizeof(world->creatures));
    for (int index = 0; index < WORLD_CELLS; ++index) {
        world->cells[index].terrain = MEADOW;
        world->cells[index].biomass = 0.1f;
        world->cells[index].moisture = 0.4f;
    }
}

static void ecological_tests(void)
{
    static World world;
    fixture(&world);
    world.cells[10 * WORLD_W + 10].moisture = 0.95f;
    world.cells[10 * WORLD_W + 11].moisture = 0.1f;
    world_step(&world, SIM_DT);
    CHECK(world.cells[10 * WORLD_W + 10].biomass > world.cells[10 * WORLD_W + 11].biomass);

    fixture(&world);
    Creature *rabbit = world_spawn(&world, RABBIT, 10.5f, 10.5f);
    CHECK(rabbit);
    rabbit->energy = 0.5f;
    world.cells[10 * WORLD_W + 15].biomass = 1;
    world_step(&world, SIM_DT);
    CHECK(rabbit->behavior == EAT && rabbit->target_x > rabbit->x);
    float food_target = rabbit->target_x;
    Creature *fox = world_spawn(&world, FOX, 14.5f, 10.5f);
    CHECK(fox);
    fox->decision = 10;
    rabbit->decision = 0;
    world_step(&world, SIM_DT);
    CHECK(rabbit->behavior == FLEE);
    CHECK(rabbit->target_x < food_target);
    CHECK(world.cells[10 * WORLD_W + 14].danger > 0);
    fox->alive = false;
    rabbit->decision = 0;
    world_step(&world, SIM_DT);
    CHECK(rabbit->behavior == EAT);
    float danger = world.cells[10 * WORLD_W + 14].danger;
    world_step(&world, SIM_DT);
    CHECK(world.cells[10 * WORLD_W + 14].danger < danger);

    fixture(&world);
    rabbit = world_spawn(&world, RABBIT, 10.5f, 10.5f);
    rabbit->energy = 0.5f;
    world.cells[10 * WORLD_W + 10].biomass = 1;
    world_step(&world, SIM_DT);
    CHECK(rabbit->energy > 0.5f);
    CHECK(world.cells[10 * WORLD_W + 10].biomass < 1);
    CHECK(world.grazings > 0);
    rabbit->behavior = REST;
    rabbit->decision = 10;
    float energy = rabbit->energy;
    world_step(&world, SIM_DT);
    CHECK(rabbit->energy > energy);

    fixture(&world);
    rabbit = world_spawn(&world, RABBIT, 10.5f, 10.5f);
    Creature *partner = world_spawn(&world, RABBIT, 11.5f, 10.5f);
    rabbit->age = partner->age = 60;
    rabbit->energy = partner->energy = 0.95f;
    rabbit->cooldown = partner->cooldown = 0;
    rabbit->behavior = partner->behavior = REST;
    rabbit->decision = partner->decision = 10;
    world_step(&world, SIM_DT);
    CHECK(world.births == 1 && world_count(&world, RABBIT) == 3);
    CHECK(rabbit->cooldown > 0 && partner->cooldown > 0);
    world_step(&world, SIM_DT);
    CHECK(world.births == 1);

    CHECK(world_activity(RABBIT, 0) > world_activity(RABBIT, DAY_SECONDS / 2));
    CHECK(world_activity(FOX, 0) < world_activity(FOX, DAY_SECONDS / 2));
    fixture(&world);
    Creature *first_fox = world_spawn(&world, FOX, 10.5f, 10.5f);
    Creature *second_fox = world_spawn(&world, FOX, 11.5f, 10.5f);
    first_fox->age = second_fox->age = 60;
    first_fox->energy = second_fox->energy = 0.95f;
    first_fox->cooldown = second_fox->cooldown = 0;
    first_fox->decision = second_fox->decision = 10;
    world.cells[10 * WORLD_W + 10].danger = 1;
    world_step(&world, SIM_DT);
    CHECK(world.births == 1 && world_count(&world, FOX) == 3);

    fixture(&world);
    rabbit = world_spawn(&world, RABBIT, 10.5f, 10.5f);
    rabbit->behavior = REST;
    rabbit->decision = 9;
    fox = world_spawn(&world, FOX, 14.5f, 10.5f);
    fox->decision = 10;
    world_step(&world, SIM_DT);
    CHECK(rabbit->behavior == FLEE);

    fixture(&world);
    world.weather = RAIN;
    float previous_wetness = world.wetness;
    world_step(&world, SIM_DT);
    CHECK(world.rain_amount > 0 && world.rain_amount < 1);
    for (int tick = 0; tick < 200; ++tick) world_step(&world, SIM_DT);
    CHECK(world.rain_amount > 0.9f && world.wetness > previous_wetness);

    fixture(&world);
    rabbit = world_spawn(&world, RABBIT, 10.5f, 10.5f);
    rabbit->energy = 0.5f;
    for (int y = 5; y <= 15; ++y) world.cells[y * WORLD_W + 12].terrain = WATER;
    world.cells[10 * WORLD_W + 15].biomass = 1;
    for (int tick = 0; tick < 300; ++tick) {
        world_step(&world, SIM_DT);
        CHECK(world_land(&world, (int)rabbit->x, (int)rabbit->y));
    }
    CHECK(rabbit->x > 12);

    fixture(&world);
    rabbit = world_spawn(&world, RABBIT, 0.5f, 10.5f);
    rabbit->energy = 0.22f;
    world.time = DAY_SECONDS - SIM_DT * 0.5f;
    world_step(&world, SIM_DT);
    CHECK(rabbit->emigrating);
    for (int tick = 0; tick < 40 && rabbit->alive; ++tick) world_step(&world, SIM_DT);
    CHECK(world.departures == 1);
}

static void check_connected(const World *world)
{
    bool visited[WORLD_CELLS] = {false};
    int queue[WORLD_CELLS], head = 0, tail = 1;
    queue[0] = 0; visited[0] = true;
    const int delta_x[4] = {1, -1, 0, 0}, delta_y[4] = {0, 0, 1, -1};
    while (head < tail) {
        int current = queue[head++];
        for (int direction = 0; direction < 4; ++direction) {
            int x = current % WORLD_W + delta_x[direction], y = current / WORLD_W + delta_y[direction];
            if (!world_land(world, x, y)) continue;
            int next = y * WORLD_W + x;
            if (!visited[next]) { visited[next] = true; queue[tail++] = next; }
        }
    }
    for (int y = 0; y < WORLD_H; ++y)
        for (int x = 0; x < WORLD_W; ++x)
            CHECK(!world_land(world, x, y) || visited[y * WORLD_W + x]);
}

int main(int argc, char **argv)
{
    ecological_tests();
    static World first, second;
    world_init(&first, 91); world_init(&second, 91);
    for (int tick = 0; tick < 600; ++tick) {
        world_step(&first, SIM_DT); world_step(&second, SIM_DT);
    }
    CHECK(memcmp(&first, &second, sizeof(first)) == 0);
    bool extended = argc > 1 && strcmp(argv[1], "--extended") == 0;
    int seeds = extended ? 20 : 3;
    int ticks = extended ? 86400 : 3600;
    for (int seed = 1; seed <= seeds; ++seed) {
        world_init(&first, (uint32_t)seed);
        check_connected(&first);
        for (int tick = 0; tick < ticks; ++tick) {
            world_step(&first, SIM_DT);
            if (tick % 100 == 0) CHECK(world_invariants(&first));
        }
        CHECK(world_invariants(&first));
        printf("seed %d: rabbits=%d snails=%d foxes=%d births=%u grazing=%u hunts=%u migration=%u arrivals=%u departures=%u\n",
               seed, world_count(&first, RABBIT), world_count(&first, SNAIL), world_count(&first, FOX),
               first.births, first.grazings, first.hunts, first.migrations, first.arrivals, first.departures);
        fflush(stdout);
    }
    puts("Simulation checks passed.");
    return 0;
}