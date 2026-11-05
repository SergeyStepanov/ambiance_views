#include "world.h"
#include <math.h>
#include <stddef.h>
#include <string.h>

static float clamp(float value, float low, float high)
{
    return fmaxf(low, fminf(high, value));
}

static float distance_squared(float first_x, float first_y, float second_x, float second_y)
{
    float delta_x = first_x - second_x, delta_y = first_y - second_y;
    return delta_x * delta_x + delta_y * delta_y;
}

float world_random(World *world)
{
    uint32_t value = world->rng;
    value ^= value << 13;
    value ^= value >> 17;
    value ^= value << 5;
    world->rng = value;
    return (value >> 8) * (1.0f / 16777216.0f);
}

bool world_land(const World *world, int x, int y)
{
    if (x < 0 || x >= WORLD_W || y < 0 || y >= WORLD_H) return false;
    Terrain terrain = world->cells[y * WORLD_W + x].terrain;
    return terrain != WATER && terrain != ROCK;
}

Creature *world_find(World *world, uint32_t id)
{
    if (!id) return NULL;
    for (int index = 0; index < MAX_CREATURES; ++index)
        if (world->creatures[index].alive && world->creatures[index].id == id)
            return &world->creatures[index];
    return NULL;
}

const Creature *world_find_const(const World *world, uint32_t id)
{
    if (!id) return NULL;
    for (int index = 0; index < MAX_CREATURES; ++index)
        if (world->creatures[index].alive && world->creatures[index].id == id)
            return &world->creatures[index];
    return NULL;
}

int world_count(const World *world, Species species)
{
    int count = 0;
    for (int index = 0; index < MAX_CREATURES; ++index)
        if (world->creatures[index].alive && world->creatures[index].species == species) ++count;
    return count;
}

Creature *world_spawn(World *world, Species species, float x, float y)
{
    if (!world_land(world, (int)x, (int)y)) return NULL;
    for (int index = 0; index < MAX_CREATURES; ++index) {
        Creature *creature = &world->creatures[index];
        if (creature->alive) continue;
        memset(creature, 0, sizeof(*creature));
        creature->id = world->next_id++;
        creature->species = species;
        creature->x = creature->previous_x = creature->target_x = creature->waypoint_x = x;
        creature->y = creature->previous_y = creature->target_y = creature->waypoint_y = y;
        creature->energy = 0.65f + world_random(world) * 0.2f;
        creature->hydration = 0.9f;
        creature->lifespan = 750.0f + world_random(world) * 650.0f;
        creature->cooldown = 30.0f + world_random(world) * 50.0f;
        creature->heading = world_random(world) * 6.2831853f;
        creature->alive = true;
        return creature;
    }
    return NULL;
}

static float pond_shape(float x, float y, float offset)
{
    float center_x = 48.0f + offset, center_y = 32.0f;
    float delta_x = (x - center_x) / 21.0f, delta_y = (y - center_y) / 16.0f;
    return delta_x * delta_x + delta_y * delta_y
        + 0.10f * sinf(x * 0.36f) * cosf(y * 0.28f);
}

void world_init(World *world, uint32_t seed)
{
    memset(world, 0, sizeof(*world));
    world->rng = seed ? seed : 1;
    world->next_id = 1;
    world->weather_left = 35;
    world->wetness = 0.35f;
    world->temperature = 0.65f;
    float offset = (world_random(world) - 0.5f) * 12;
    for (int row = 0; row < WORLD_H; ++row) {
        for (int column = 0; column < WORLD_W; ++column) {
            Cell *cell = &world->cells[row * WORLD_W + column];
            float shape = pond_shape((float)column, (float)row, offset);
            cell->variation = (uint8_t)(world_random(world) * 255);
            cell->terrain = shape < 1 ? WATER : shape < 1.38f ? SHORE : MEADOW;
            float rock_shape = distance_squared((float)column / 1.7f, (float)row,
                                               97.0f / 1.7f, 48.0f);
            if (rock_shape < 23) cell->terrain = ROCK;
            cell->moisture = clamp(1.3f - shape * 0.32f, 0.12f, 1.0f);
            cell->plant = cell->terrain == SHORE ? REED : cell->variation > 224 ? SHRUB : GRASS;
            if (world_land(world, column, row))
                cell->biomass = clamp(0.12f + cell->moisture * 0.6f + world_random(world) * 0.22f, 0, 1);
        }
    }
    for (int index = 0; index < 22; ++index) {
        Species species = index < 12 ? RABBIT : index < 20 ? SNAIL : FOX;
        for (int attempt = 0; attempt < 1000; ++attempt) {
            float x = 4 + world_random(world) * (WORLD_W - 8);
            float y = 4 + world_random(world) * (WORLD_H - 8);
            if (!world_land(world, (int)x, (int)y)) continue;
            Cell *cell = &world->cells[(int)y * WORLD_W + (int)x];
            if (species == SNAIL && cell->moisture < 0.6f) continue;
            Creature *creature = world_spawn(world, species, x, y);
            if (creature) creature->age = world_random(world) * 180;
            break;
        }
    }
}

static bool line_clear(const World *world, float x, float y, float target_x, float target_y)
{
    int steps = (int)(sqrtf(distance_squared(x, y, target_x, target_y)) * 3) + 1;
    for (int step = 1; step <= steps; ++step) {
        float fraction = (float)step / (float)steps;
        if (!world_land(world, (int)(x + (target_x - x) * fraction),
                        (int)(y + (target_y - y) * fraction))) return false;
    }
    return true;
}

static void route(World *world, Creature *creature)
{
    if (line_clear(world, creature->x, creature->y, creature->target_x, creature->target_y)) {
        creature->waypoint_x = creature->target_x;
        creature->waypoint_y = creature->target_y;
        return;
    }
    int parent[WORLD_CELLS], queue[WORLD_CELLS];
    for (int index = 0; index < WORLD_CELLS; ++index) parent[index] = -1;
    int start = (int)creature->y * WORLD_W + (int)creature->x;
    int goal = (int)creature->target_y * WORLD_W + (int)creature->target_x;
    int head = 0, tail = 1;
    queue[0] = start;
    parent[start] = start;
    const int delta_x[4] = {1, -1, 0, 0}, delta_y[4] = {0, 0, 1, -1};
    while (head < tail && parent[goal] == -1) {
        int current = queue[head++];
        for (int direction = 0; direction < 4; ++direction) {
            int next_x = current % WORLD_W + delta_x[direction];
            int next_y = current / WORLD_W + delta_y[direction];
            if (!world_land(world, next_x, next_y)) continue;
            int next = next_y * WORLD_W + next_x;
            if (parent[next] != -1) continue;
            parent[next] = current;
            queue[tail++] = next;
        }
    }
    if (parent[goal] == -1) {
        creature->target_x = creature->waypoint_x = creature->x;
        creature->target_y = creature->waypoint_y = creature->y;
        return;
    }
    int next = goal;
    while (parent[next] != start && next != start) next = parent[next];
    creature->waypoint_x = (float)(next % WORLD_W) + 0.5f;
    creature->waypoint_y = (float)(next / WORLD_W) + 0.5f;
}

static const Creature *nearest(const World *world, const Creature *creature, bool predator, float radius)
{
    const Creature *found = NULL;
    float best = radius * radius;
    for (int index = 0; index < MAX_CREATURES; ++index) {
        const Creature *other = &world->creatures[index];
        if (!other->alive || other->id == creature->id) continue;
        if (predator ? other->species != FOX : other->species != RABBIT) continue;
        float distance = distance_squared(creature->x, creature->y, other->x, other->y);
        if (distance < best) { best = distance; found = other; }
    }
    return found;
}

float world_activity(Species species, float time)
{
    float night = (1 - cosf(time / DAY_SECONDS * 6.2831853f)) * 0.5f;
    return species == FOX ? 0.3f + night * 0.7f
         : species == SNAIL ? 0.5f + night * 0.35f : 1 - night * 0.75f;
}

static void choose_target(World *world, Creature *creature)
{
    creature->prey = 0;
    const Creature *predator = creature->species != FOX ? nearest(world, creature, true, 10) : NULL;
    if (predator) {
        creature->behavior = FLEE;
        float best = -1e9f;
        for (int attempt = 0; attempt < 24; ++attempt) {
            float x = clamp(creature->x + (world_random(world) - 0.5f) * 18, 0.5f, WORLD_W - 0.5f);
            float y = clamp(creature->y + (world_random(world) - 0.5f) * 18, 0.5f, WORLD_H - 0.5f);
            if (!world_land(world, (int)x, (int)y) || !line_clear(world, creature->x, creature->y, x, y)) continue;
            float score = distance_squared(x, y, predator->x, predator->y);
            if (score > best) { best = score; creature->target_x = x; creature->target_y = y; }
        }
        route(world, creature);
        return;
    }
    if (creature->emigrating) {
        creature->behavior = MIGRATE;
        route(world, creature);
        return;
    }
    if (creature->species == FOX && creature->energy < 0.78f) {
        const Creature *prey = nearest(world, creature, false, 24);
        if (prey) {
            creature->behavior = HUNT;
            creature->prey = prey->id;
            creature->target_x = prey->x;
            creature->target_y = prey->y;
            route(world, creature);
            return;
        }
    }
    Cell *here = &world->cells[(int)creature->y * WORLD_W + (int)creature->x];
    float activity = world_activity(creature->species, world->time);
    if (creature->hydration < 0.32f) creature->behavior = DRINK;
    else if (creature->species != FOX && (creature->energy < 0.58f
             || (creature->energy < 0.85f && world_random(world) < activity))) creature->behavior = EAT;
    else if (creature->energy > 0.58f && (creature->species == FOX || here->danger < 0.15f)
             && world_random(world) < 0.9f - activity * 0.65f) {
        creature->behavior = REST;
        creature->decision = 4 + world_random(world) * 7;
        return;
    } else creature->behavior = world_random(world) < 0.3f ? MIGRATE : WANDER;
    float best = -1e9f;
    int radius = creature->species == SNAIL ? 10 : 20;
    int start_x = (int)clamp(creature->x - radius, 0, WORLD_W - 1);
    int end_x = (int)clamp(creature->x + radius, 0, WORLD_W - 1);
    int start_y = (int)clamp(creature->y - radius, 0, WORLD_H - 1);
    int end_y = (int)clamp(creature->y + radius, 0, WORLD_H - 1);
    float preferred_y = world->season == WINTER ? 53 : world->season == SUMMER ? 18 : 34;
    for (int y = start_y; y <= end_y; ++y) {
        for (int x = start_x; x <= end_x; ++x) {
            if (!world_land(world, x, y)) continue;
            const Cell *cell = &world->cells[y * WORLD_W + x];
            float distance = sqrtf(distance_squared(creature->x, creature->y, x + 0.5f, y + 0.5f));
            float score = -distance * 0.025f - (creature->species == FOX ? 0 : cell->danger * 3);
            if (creature->behavior == DRINK) score += cell->moisture * 5;
            else if (creature->behavior == EAT) score += cell->biomass * 2;
            else if (creature->behavior == MIGRATE) score -= fabsf(y - preferred_y) * 0.055f;
            else score += world_random(world) * 1.2f;
            if (creature->species == SNAIL) score += cell->moisture;
            if (score > best) { best = score; creature->target_x = x + 0.5f; creature->target_y = y + 0.5f; }
        }
    }
    route(world, creature);
}

static void plant_step(World *world, float dt)
{
    world->season = (Season)((int)(world->time / SEASON_SECONDS) % 4);
    float year_phase = world->time / (SEASON_SECONDS * 4) * 6.2831853f;
    world->temperature = 0.55f + 0.4f * sinf(year_phase);
    world->weather_left -= dt;
    if (world->weather_left <= 0) {
        float roll = world_random(world);
        world->weather = roll < 0.27f ? RAIN : roll < 0.55f ? CLOUD : CLEAR;
        world->weather_left = 25 + world_random(world) * 45;
    }
    float weather_blend = 1 - expf(-dt * 0.3f);
    world->rain_amount += ((world->weather == RAIN ? 1.0f : 0.0f) - world->rain_amount) * weather_blend;
    world->cloud_amount += ((world->weather == CLEAR ? 0.0f : 1.0f) - world->cloud_amount) * weather_blend;
    world->wetness = clamp(world->wetness + dt * (world->rain_amount * 0.0088f - 0.0018f), 0, 1);
    for (int index = 0; index < WORLD_CELLS; ++index) {
        Cell *cell = &world->cells[index];
        cell->danger = fmaxf(0, cell->danger - dt * 0.024f);
        if (cell->terrain == WATER || cell->terrain == ROCK) continue;
        float water = clamp(cell->moisture * 0.8f + world->wetness * 0.4f, 0, 1);
        float growth = 0.0025f * water * (0.25f + world->temperature);
        cell->biomass = clamp(cell->biomass + dt * (growth * (1 - cell->biomass)
                             - (1 - world->temperature) * 0.00022f), 0, 1);
    }
}

static void mark_danger(World *world, const Creature *creature, float dt)
{
    int center_x = (int)creature->x, center_y = (int)creature->y;
    for (int y = center_y - 7; y <= center_y + 7; ++y) {
        for (int x = center_x - 7; x <= center_x + 7; ++x) {
            if (!world_land(world, x, y)) continue;
            float distance = sqrtf(distance_squared((float)x, (float)y, creature->x, creature->y));
            if (distance > 7) continue;
            Cell *cell = &world->cells[y * WORLD_W + x];
            cell->danger = clamp(cell->danger + dt * (1 - distance / 7) * 0.7f, 0, 1);
        }
    }
}

void world_step(World *world, float dt)
{
    if (!(dt > 0) || dt > 0.5f) return;
    int old_day = (int)(world->time / DAY_SECONDS);
    world->time += dt;
    plant_step(world, dt);
    uint32_t births_parent[MAX_CREATURES];
    int birth_count = 0;
    for (int index = 0; index < MAX_CREATURES; ++index) {
        Creature *creature = &world->creatures[index];
        if (!creature->alive) continue;
        creature->previous_x = creature->x;
        creature->previous_y = creature->y;
        creature->age += dt;
        creature->cooldown -= dt;
        creature->energy -= dt * (creature->species == FOX ? 0.003f : 0.0018f);
        creature->hydration -= dt * (creature->species == SNAIL ? 0.001f : 0.0018f);
        if (creature->age > creature->lifespan || creature->energy <= 0 || creature->hydration <= 0) {
            creature->alive = false;
            continue;
        }
        if (creature->species == FOX) mark_danger(world, creature, dt);
        creature->decision -= dt;
        if (creature->species != FOX && creature->behavior != FLEE && nearest(world, creature, true, 10))
            creature->decision = 0;
        if (creature->behavior == HUNT) {
            Creature *prey = world_find(world, creature->prey);
            if (!prey) creature->decision = 0;
            else if (distance_squared(prey->x, prey->y, creature->target_x, creature->target_y) > 1)
                creature->decision = 0;
        }
        if (creature->decision <= 0) {
            creature->decision = 0.8f + world_random(world) * 1.3f;
            choose_target(world, creature);
            if (creature->behavior == FLEE || creature->behavior == HUNT) creature->decision = 0.35f;
            if (creature->behavior == MIGRATE) ++world->migrations;
        }
        Cell *cell = &world->cells[(int)creature->y * WORLD_W + (int)creature->x];
        float to_goal = distance_squared(creature->x, creature->y, creature->target_x, creature->target_y);
        if (creature->emigrating && creature->behavior == MIGRATE && to_goal < 0.05f) {
            creature->alive = false;
            ++world->departures;
            continue;
        }
        bool stationary = creature->behavior == REST;
        if (creature->behavior == REST) creature->energy = clamp(creature->energy + dt * 0.0028f, 0, 1);
        if (creature->behavior == EAT && cell->biomass > 0.06f && cell->danger < 0.4f && to_goal < 1) {
            float bite = fminf(cell->biomass, dt * (creature->species == SNAIL ? 0.018f : 0.045f));
            cell->biomass -= bite;
            creature->energy = clamp(creature->energy + bite * 0.8f, 0, 1);
            creature->hydration = clamp(creature->hydration + bite * 0.15f, 0, 1);
            ++world->grazings;
            stationary = true;
        }
        if (creature->behavior == DRINK && cell->moisture > 0.72f) {
            creature->hydration = clamp(creature->hydration + dt * 0.15f, 0, 1);
            stationary = true;
            if (creature->hydration > 0.94f) creature->decision = 0;
        }
        if (creature->behavior == HUNT) {
            Creature *prey = world_find(world, creature->prey);
            if (prey && distance_squared(creature->x, creature->y, prey->x, prey->y) < 0.65f) {
                prey->alive = false;
                creature->energy = clamp(creature->energy + 0.55f, 0, 1);
                creature->hydration = clamp(creature->hydration + 0.25f, 0, 1);
                creature->behavior = REST;
                creature->decision = 8;
                ++world->hunts;
                stationary = true;
            }
        }
        if (!stationary) {
            float delta_x = creature->waypoint_x - creature->x;
            float delta_y = creature->waypoint_y - creature->y;
            float length = sqrtf(delta_x * delta_x + delta_y * delta_y);
            if (length < 0.2f && to_goal > 0.1f) {
                route(world, creature);
                delta_x = creature->waypoint_x - creature->x;
                delta_y = creature->waypoint_y - creature->y;
                length = sqrtf(delta_x * delta_x + delta_y * delta_y);
            }
            if (length > 0.01f) {
                float speed = creature->species == SNAIL ? 0.45f : creature->species == FOX ? 2.4f : 1.45f;
                if (creature->behavior == FLEE) speed *= 2.4f;
                if (creature->behavior == HUNT) speed *= 1.7f;
                float step = fminf(length, dt * speed);
                float x = creature->x + delta_x / length * step;
                float y = creature->y + delta_y / length * step;
                if (world_land(world, (int)x, (int)y) && line_clear(world, creature->x, creature->y, x, y)) {
                    creature->x = x; creature->y = y;
                    creature->heading = atan2f(delta_y, delta_x);
                    creature->energy -= dt * speed * 0.0005f;
                } else creature->decision = 0;
            }
        }
        if (creature->cooldown <= 0 && creature->energy > 0.78f && creature->hydration > 0.5f
            && creature->age > 45 && creature->behavior != FLEE && !creature->emigrating
            && (creature->species == FOX || cell->danger < 0.1f)) {
            int cap = creature->species == FOX ? 5 : creature->species == SNAIL ? 24 : 45;
            if (world_count(world, creature->species) < cap) {
                for (int partner_index = 0; partner_index < MAX_CREATURES; ++partner_index) {
                    Creature *partner = &world->creatures[partner_index];
                    if (!partner->alive || partner->id == creature->id || partner->species != creature->species
                        || partner->age < 45 || partner->energy < 0.72f || partner->cooldown > 0) continue;
                    if (distance_squared(creature->x, creature->y, partner->x, partner->y) > 16) continue;
                    births_parent[birth_count++] = creature->id;
                    creature->cooldown = partner->cooldown = 80 + world_random(world) * 60;
                    creature->energy -= 0.2f; partner->energy -= 0.15f;
                    break;
                }
            }
        }
        creature->energy = clamp(creature->energy, 0, 1);
    }
    for (int index = 0; index < birth_count; ++index) {
        Creature *parent = world_find(world, births_parent[index]);
        if (!parent) continue;
        int cap = parent->species == FOX ? 5 : parent->species == SNAIL ? 24 : 45;
        if (world_count(world, parent->species) < cap
            && world_spawn(world, parent->species, parent->x, parent->y)) ++world->births;
    }
    if ((int)(world->time / DAY_SECONDS) != old_day) {
        for (int index = 0; index < MAX_CREATURES; ++index) {
            Creature *creature = &world->creatures[index];
            if (!creature->alive || creature->energy > 0.28f || creature->emigrating
                || creature->behavior == FLEE || (creature->x > 14 && creature->x < WORLD_W - 14)) continue;
            const Cell *cell = &world->cells[(int)creature->y * WORLD_W + (int)creature->x];
            bool depleted = creature->species == FOX ? !nearest(world, creature, false, 20) : cell->biomass < 0.25f;
            if (!depleted) continue;
            creature->emigrating = true;
            creature->target_x = creature->x < WORLD_W * 0.5f ? 0.05f : WORLD_W - 0.05f;
            creature->target_y = creature->y;
            creature->decision = 0;
        }
        for (int species = RABBIT; species <= FOX; ++species) {
            int count = world_count(world, (Species)species);
            int threshold = species == FOX ? 2 : species == SNAIL ? 5 : 7;
            float habitat_food = 0;
            for (int index = 0; index < WORLD_CELLS; index += 17) habitat_food += world->cells[index].biomass;
            bool suitable = species == FOX ? world_count(world, RABBIT) > 5 : habitat_food > 100;
            if (count >= threshold || !suitable || world_random(world) > 0.65f) continue;
            float x = world_random(world) < 0.5f ? 0.5f : WORLD_W - 0.5f;
            float y = 2 + world_random(world) * (WORLD_H - 4);
            Creature *arriving = world_spawn(world, (Species)species, x, y);
            if (arriving) { arriving->behavior = MIGRATE; arriving->energy = 0.72f; ++world->arrivals; }
        }
    }
}

bool world_invariants(const World *world)
{
    if (!isfinite(world->time) || !isfinite(world->wetness) || world->wetness < 0 || world->wetness > 1
        || !isfinite(world->rain_amount) || world->rain_amount < 0 || world->rain_amount > 1
        || !isfinite(world->cloud_amount) || world->cloud_amount < 0 || world->cloud_amount > 1) return false;
    if (world_count(world, RABBIT) > 45 || world_count(world, SNAIL) > 24 || world_count(world, FOX) > 5) return false;
    for (int index = 0; index < WORLD_CELLS; ++index) {
        const Cell *cell = &world->cells[index];
        if (!isfinite(cell->biomass) || cell->biomass < 0 || cell->biomass > 1
            || !isfinite(cell->danger) || cell->danger < 0 || cell->danger > 1) return false;
    }
    for (int index = 0; index < MAX_CREATURES; ++index) {
        const Creature *creature = &world->creatures[index];
        if (!creature->alive) continue;
        if (!isfinite(creature->x) || !isfinite(creature->y) || !isfinite(creature->energy)
            || !isfinite(creature->hydration) || creature->energy < 0 || creature->energy > 1
            || creature->hydration < 0 || creature->hydration > 1 || !creature->id
            || !world_land(world, (int)creature->x, (int)creature->y)) return false;
        for (int other = index + 1; other < MAX_CREATURES; ++other)
            if (world->creatures[other].alive && world->creatures[other].id == creature->id) return false;
    }
    return true;
}