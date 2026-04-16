#include "render.h"

#include <math.h>
#include <ncurses.h>

static int g_use256_colors = 0;

static void draw_block(int sx, int sy, int w, int h, int pair, int screen_w, int screen_h) {
    int yy;
    int xx;

    if (sx >= screen_w || sy >= screen_h || sx + w <= 0 || sy + h <= 0) return;

    attron(COLOR_PAIR(pair));
    for (yy = 0; yy < h; ++yy) {
        int py = sy + yy;

        if (py < 0 || py >= screen_h) continue;
        for (xx = 0; xx < w; ++xx) {
            int px = sx + xx;

            if (px < 0 || px >= screen_w) continue;
            mvaddch(py, px, ' ');
        }
    }
    attroff(COLOR_PAIR(pair));
}

static void world_to_view(double wx, double wy, double center_x, int screen_center_x, int *sx, int *sy) {
    *sx = screen_center_x + (int)lround(game_wrapped_dx(center_x, wx));
    *sy = 1 + (int)lround(wy);
}

void render_init_graphics(void) {
    if (!has_colors()) return;

    start_color();
    use_default_colors();
    g_use256_colors = (COLORS >= 256);

    init_pair(20, g_use256_colors ? 226 : COLOR_YELLOW, g_use256_colors ? 226 : COLOR_YELLOW);
    init_pair(21, g_use256_colors ? 196 : COLOR_RED, g_use256_colors ? 196 : COLOR_RED);
    init_pair(22, g_use256_colors ? 46 : COLOR_GREEN, g_use256_colors ? 46 : COLOR_GREEN);
    init_pair(23, g_use256_colors ? 231 : COLOR_WHITE, g_use256_colors ? 231 : COLOR_WHITE);
    init_pair(24, g_use256_colors ? 210 : COLOR_MAGENTA, g_use256_colors ? 210 : COLOR_MAGENTA);
    init_pair(25, g_use256_colors ? 94 : COLOR_BLUE, g_use256_colors ? 94 : COLOR_BLUE);
    init_pair(27, COLOR_WHITE, -1);
}

void render_game(const GameState *game, int term_w, int term_h) {
    int i;
    int radar_origin = 7;
    int radar_width = term_w - radar_origin;
    int screen_center_x = term_w / 2;

    erase();

    if (term_w < MIN_TERM_W || term_h < MIN_TERM_H) {
        mvprintw(1, 2, "Terminal too small.");
        mvprintw(2, 2, "Need at least %dx%d, have %dx%d.", MIN_TERM_W, MIN_TERM_H, term_w, term_h);
        mvprintw(4, 2, "Resize the terminal, then press R to restart or Q to quit.");
        refresh();
        return;
    }

    mvprintw(0, 0, "Radar:");
    for (i = 0; i < radar_width; ++i) {
        mvaddch(0, radar_origin + i, '-');
    }

    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (game->enemies[i].active && radar_width > 0) {
            int rx = (int)((game->enemies[i].x / WORLD_W) * (radar_width - 1));

            if (rx >= 0 && rx < radar_width) {
                int marker = 'E';

                if (game->enemies[i].type == E_MUTANT) marker = 'M';
                else if (game->enemies[i].type == E_BOMBER) marker = 'B';
                mvaddch(0, radar_origin + rx, marker);
            }
        }
    }

    if (radar_width > 0) {
        int px = (int)((game->player.x / WORLD_W) * (radar_width - 1));

        if (px >= 0 && px < radar_width) {
            mvaddch(0, radar_origin + px, 'P');
        }
    }

    attron(COLOR_PAIR(25));
    for (i = 0; i < term_w; ++i) {
        int world_x = (int)game_wrap_x(game->player.x + (i - screen_center_x));
        int terrain_y = 1 + (int)lround(game_terrain_y(world_x));
        int y;

        for (y = terrain_y; y < term_h - 1; ++y) {
            mvaddch(y, i, ' ');
        }
    }
    attroff(COLOR_PAIR(25));

    for (i = 0; i < MAX_HUMANS; ++i) {
        int sx;
        int sy;
        int pair;

        if (game->humans[i].state == H_INACTIVE ||
            game->humans[i].state == H_RESCUED ||
            game->humans[i].state == H_LOST) {
            continue;
        }

        world_to_view(game->humans[i].x, game->humans[i].y, game->player.x, screen_center_x, &sx, &sy);
        pair = game->humans[i].state == H_FALLING ? 23 : 22;
        draw_block(sx, sy, 1, 1, pair, term_w, term_h);
    }

    for (i = 0; i < MAX_ENEMIES; ++i) {
        int sx;
        int sy;

        if (!game->enemies[i].active) continue;

        world_to_view(game->enemies[i].x, game->enemies[i].y, game->player.x, screen_center_x, &sx, &sy);
        draw_block(sx - 1, sy, 3, 1,
                   game->enemies[i].type == E_MUTANT ? 24 :
                   game->enemies[i].type == E_BOMBER ? 23 : 21,
                   term_w, term_h);
    }

    for (i = 0; i < MAX_BULLETS; ++i) {
        int sx;
        int sy;

        if (!game->bullets[i].active) continue;

        world_to_view(game->bullets[i].x, game->bullets[i].y, game->player.x, screen_center_x, &sx, &sy);
        draw_block(sx, sy, 1, 1, 23, term_w, term_h);
    }

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        int sx;
        int sy;

        if (!game->enemy_bullets[i].active) continue;

        world_to_view(game->enemy_bullets[i].x, game->enemy_bullets[i].y, game->player.x, screen_center_x, &sx, &sy);
        draw_block(sx, sy, 1, 1, 24, term_w, term_h);
    }

    if (game->player.active &&
        !(game->player.invulnerable_timer > 0.0 && ((int)(game->player.invulnerable_timer * 10.0) % 2 == 0))) {
        int sx;
        int sy;

        world_to_view(game->player.x, game->player.y, game->player.x, screen_center_x, &sx, &sy);
        draw_block(sx - 1, sy, 3, 1, 20, term_w, term_h);
    }

    attron(COLOR_PAIR(27));
    mvprintw((int)GROUND_Y + 2, 0,
             "Wave:%d  Score:%d  Next:%d  Bombs:%d  Lives:%d  Grounded:%d  Falling:%d  Lost:%d",
             game->wave_number,
             game->player.score,
             game->next_extra_life_score,
             game->player.bombs,
             game->player.lives,
             game_humans_in_state(game, H_GROUNDED),
             game_humans_in_state(game, H_FALLING),
             game_humans_in_state(game, H_LOST));
    attroff(COLOR_PAIR(27));

    if (game->wave_banner_timer > 0.0 && !game->game_over) {
        mvprintw((int)GROUND_Y / 2, term_w / 2 - 6, "WAVE %d", game->wave_number);
    }

    if (game->game_over) {
        mvprintw((int)GROUND_Y / 2, term_w / 2 - 8, "GAME OVER");
        mvprintw((int)GROUND_Y / 2 + 1, term_w / 2 - 18, "Press R to restart or Q to quit");
    } else if (!game->player.active) {
        mvprintw((int)GROUND_Y / 2, term_w / 2 - 7, "RESPAWNING");
    }

    refresh();
}
