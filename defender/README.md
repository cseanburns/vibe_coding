# Defender (terminal demo)

Small Defender-inspired terminal game using `ncurses`.

Build and run:

```bash
make
./defender
```

Controls:
- Arrow keys: thrust the ship
- Space: fire laser
- B: use a smart bomb
- R: restart the game
- Q: quit

Notes:
- The current version uses a fixed-step simulation, ship inertia, wrapped horizontal world movement, terrain-aware landings, wave progression, bombers and mutants, respawn logic, and a stricter human rescue/loss state machine.
- The source is now split into `main.c`, `game.c`, `input.c`, and `render.c` so game rules, input handling, and ncurses drawing are separated.
- Movement input is handled as a short-lived held state instead of a single-frame key snapshot, which makes terminal key repeat feel less choppy.
- The program uses `ncurses`; ensure you have the development package installed (for example `libncurses-dev`).
