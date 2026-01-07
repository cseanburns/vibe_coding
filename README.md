# vibe_coding

A small collection of short, terminal-based "vibe" demos and small creative programs.

This repository contains two example projects:

- christmas/
  - A small ncurses demo binary `xmas` implemented in `xmas.c`.
  - Build: `make -C christmas`
  - Run: `./christmas/xmas`

- new_years/
  - An ncurses skyline + lake + fireworks animation (`skyline_fireworks.c`).
  - Build: `make -C new_years`
  - Run: `./new_years/skyline_fireworks` (press `q` to quit)

- sunset/
  - A terminal sunset animation using `ncurses` in `sunset/sunset.c`.
  - Build: `make -C sunset` or `cd sunset && make`
  - Run: `./sunset/sunset` (press `q` to quit). The program uses 256-color palettes when available and runs a full day->night->day cycle (approx 140s by default).

Requirements
- A Unix-like environment (Linux, macOS, WSL)
- A C compiler supporting C11 (e.g. `gcc` or `clang`)
- `ncurses` development libraries (install via your package manager: e.g. `libncurses-dev`)

Notes
- Each project includes a simple `Makefile` that compiles the program and links ncurses.
- The programs are intended to run in a terminal that supports ncurses/ANSI characters.

Contributing
- Small, focused demos are welcome. Open a PR with small additions or improvements.
- Keep binaries out of the repository; add code and build rules only.

License
- Not specified. Contact the repo owner for licensing preferences.
