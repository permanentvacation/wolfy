# WOLFY 3D

A Wolfenstein-3D-style raycasting shooter written in C with SDL2. No asset files:
textures, sprites, sound effects and music are all generated procedurally at start-up.

## Build & run

    sudo apt install libsdl2-dev   # or: brew install sdl2
    make
    ./wolfy

## Controls

| Key | Action |
| --- | --- |
| W/A/S/D or arrows | move / turn (A/D strafe, arrow keys turn) |
| Mouse | look (click to capture) |
| Left click / Ctrl | fire |
| Space / E | open doors, use the EXIT sign |
| 1 / 2 / 3 | knife / pistol / machine gun (find it on floor 2+) |
| Shift | run |
| Tab | automap |
| M | toggle music |
| Esc | pause (Q while paused: back to title) |
| F11 / Alt+Enter | fullscreen |

## Game

* 5 generated floors with rooms, sliding doors, pillars, treasure, ammo and health.
* Guards (brown), SS officers (blue, tougher) and a boss guarding the last exit.
* Enemies path-find to you, open doors, and wake up at gunshots.
* Extra life every 20,000 points. 3 lives; dying restarts the floor.

## Sound

Pistol/machine gun/knife, enemy barks and death cries, doors, pickups, fanfares
(all synthesised), positional pan + distance falloff, plus a driving march for gameplay
and a slow theme for the title screen. Everything goes through SDL's audio callback
(no SDL_mixer needed).

## Testing without a display

    ./wolfy --selftest                     # every generated level is fully reachable
    ./wolfy --demo 600                     # autopilot bot plays the whole game headless
    ./wolfy --demo 120 shot_ 15 3          # ...saving a BMP every 15 frames, starting on floor 3
    ./wolfy --wav out.wav 1 30             # export 30s of the gameplay music
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./wolfy   # run the real loop headless
