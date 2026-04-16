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
- The current version uses a fixed-step simulation, ship inertia, wrapped horizontal world movement, wave progression, mutants, respawn logic, and a stricter human rescue/loss state machine.
- The program uses `ncurses`; ensure you have the development package installed (for example `libncurses-dev`).
