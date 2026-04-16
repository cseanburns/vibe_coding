#define _POSIX_C_SOURCE 199309L

#include "game.h"
#include "input.h"
#include "render.h"

#include <ncurses.h>
#include <stdlib.h>
#include <time.h>

#define SIM_HZ 60.0
#define FRAME_HZ 60.0
#define SIM_DT (1.0 / SIM_HZ)

static double min_double(double a, double b) {
    return a < b ? a : b;
}

static double monotonic_seconds(void) {
    struct timespec now;

    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec + now.tv_nsec / 1e9;
}

int main(void) {
    GameState game;
    InputContext input_context;
    double last_seconds;
    double accumulator = 0.0;

    srand((unsigned)time(NULL));

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    render_init_graphics();

    clear();
    mvprintw(5, 5, "DEFENDER - terminal demo");
    mvprintw(7, 5, "Controls: Arrow keys to thrust, Space to fire, B to use bomb, R to restart, Q to quit");
    mvprintw(9, 5, "Goal: Stop abductions, catch falling humans, and bring them back to the ground.");
    mvprintw(11, 5, "Press any key to start...");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);

    game_init(&game);
    input_init(&input_context);
    last_seconds = monotonic_seconds();

    while (1) {
        InputState input;
        double now_seconds = monotonic_seconds();
        double frame_dt = now_seconds - last_seconds;

        last_seconds = now_seconds;
        if (frame_dt > 0.25) frame_dt = 0.25;

        input_poll(&input_context, &input, now_seconds);
        if (input.quit) break;
        if (input.restart) {
            game_init(&game);
            input_init(&input_context);
        }

        accumulator += frame_dt;
        while (accumulator >= SIM_DT) {
            game_step(&game, SIM_DT, &input);
            input.fire = 0;
            input.bomb = 0;
            input.restart = 0;
            accumulator -= SIM_DT;
        }

        {
            int term_h;
            int term_w;

            getmaxyx(stdscr, term_h, term_w);
            render_game(&game, term_w, term_h);
        }

        if (accumulator < SIM_DT) {
            struct timespec sleep_for;
            double remain = (1.0 / FRAME_HZ) - accumulator;

            if (remain > 0.0) {
                sleep_for.tv_sec = 0;
                sleep_for.tv_nsec = (long)(min_double(remain, 1.0 / FRAME_HZ) * 1e9);
                nanosleep(&sleep_for, NULL);
            }
        }
    }

    endwin();
    return 0;
}
