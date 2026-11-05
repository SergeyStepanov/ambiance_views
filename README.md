# Living Terrarium

A passive, top-down pixel-art ecosystem for the desktop, written in C11 and SDL 3.
Every launch creates a fresh world. There are no scores, world-editing controls,
victory conditions or loss screens. All art is original bitmap data embedded in
the executable; no network connection or external asset files are needed.

## Made by AI

Code, tests, documentation and original pixel art were created by GitHub Copilot,
without manual coding or drawing by the "meatbags". Humans supplied the idea,
direction and feedback.

## Build and Run

Requirements: CMake 3.20+, a C11 compiler, and SDL 3.2+ with its CMake development
package. The application has been built and run on Linux with SDL 3.4.14 and GCC 15.
SDL_ttf, SDL_image and a separate game engine are not required.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/ambiance_views
```

The resizable window starts at 960x544 and can shrink to 160x160. The camera
adapts its visible area to the actual window dimensions and fills the window
without letterboxing or stretching the art. Narrow portrait windows show a
vertical portion of the world; widening the same window expands the view again.
The 480x272 world uses nearest sampling and integer pixel scaling. Following a
creature, mouse selection and weather overlays adapt to the visible area too.

- Click a creature to follow it; its recent path fades over 20 simulation seconds.
- Click empty ground or press Escape to return to the overview.
- F11 toggles fullscreen. Close the window to exit.

These interactions change the observation only, not the world. Minimized windows
pause the simulation and use event waiting rather than spinning. No world is saved.

## The Ecosystem

The initial populations are 12 rabbits, 8 snails and 2 foxes. Grass, reeds and
shrubs have biomass that grows faster in moist habitat. Grazers deplete local
food. Foxes hunt moving rabbits; nearby grazers flee immediately, including while
resting, and remember dangerous patches until the local threat field fades.

Creatures search for food and wet ground, rest, grow older and reproduce with a
nearby healthy partner. Reproduction costs energy and has a cooldown. Young
creatures grow visibly. Foxes are more active at night; rabbits favor daylight;
snails prefer moist terrain. Seasonal habitat preferences change migration
destinations. Water and rocks are impassable, and routing can go around them.

Rain and cloud cover fade in and out and rain affects moisture. A simulated day
takes about 90 real seconds, a season 270 seconds, and a year 18 minutes. Seasonal
palette, growth and temperature changes are gradual. Noisy weather transitions
occur every 25-70 seconds. The simulation advances at 20 Hz independently of
rendering; movement is interpolated between steps.

Population growth is bounded at 45 rabbits, 24 snails and 5 foxes, with 96 storage
slots. New arrivals may enter visibly at map edges if local populations are low
and there is food; hungry creatures near an edge may leave depleted habitat.
Local extinction is possible and is not a loss condition. This is a deliberately
compact ecological model, not a claim of biological accuracy or guaranteed
permanent equilibrium.

## Verification

```sh
ctest --test-dir build --output-on-failure
./build/test_simulation --extended
./build/test_native
./build/ambiance_views --seed 42 --smoke --snapshot build/live.bmp
```

The default tests do not need a graphical session. They check ecological
relationships, obstacle routing, connected land, daily activity, reproduction,
weather, emigration, same-seed reproducibility, stable creature IDs, trail expiry,
observation independence and nonblank rendered pixels. The extended test checks
20 seeds over approximately four simulated years each. Native tests require a
display and repeatedly resize the same window through eight sizes, including
320x960, 640x1080 and 160x480 portrait views and a return to 960x544. They verify
full-window pixel coverage, preserved selection, mouse-coordinate conversion
and fullscreen.
To include the native test in CTest, configure with `-DTERRARIUM_NATIVE_TESTS=ON`.

```sh
cmake -S . -B build-asan -DCMAKE_BUILD_TYPE=Debug -DTERRARIUM_SANITIZERS=ON
cmake --build build-asan -j
ctest --test-dir build-asan --output-on-failure
```

AddressSanitizer and UndefinedBehaviorSanitizer are supported on GCC/Clang.
The seed guarantees reproducibility on the same build; cross-platform bitwise
floating-point equivalence is not promised.

Diagnostic options are available from the terminal only, not as world controls:

```sh
./build/ambiance_views --help
./build/ambiance_views --headless --seed 42 --advance 810 --snapshot build/winter.bmp
```

`--frames N` exits after N frames. `--advance N` warms up a world by N simulation
seconds, up to 86400. `--headless` uses an SDL software renderer without a window,
and exits after one frame by default. BMP captures are taken before presenting.

## Source Layout

- `src/world.c`: seeded terrain, ecology, needs, behavior, routing and lifecycle.
- `src/observation.c`: selection, camera and bounded timestamped trails; no world mutations.
- `src/assets.c`: original palette-based animal and plant bitmap atlas.
- `src/render.c`: cached terrain, plants, sprites, trails, water and weather.
- `src/main.c`: SDL lifetime, events, fixed-step timing, CLI and diagnostics.
- `tests/`: simulation, observation, software rendering and native-window checks.

Targets use ordinary CMake; `cmake --install build --prefix ~/.local` installs
the executable. Linux is the tested platform. Other SDL desktop platforms have
not been validated. Audio, persistence and installable platform bundles are
outside this first version.