/* defender.c
 * Terminal Defender-like game using ncurses.
 * Controls: Arrow keys to thrust, Space to fire, B to use bomb, R to restart, Q to quit.
 */

#define _POSIX_C_SOURCE 199309L
#include <ncurses.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define WORLD_W 600.0
#define GROUND_Y 21.0
#define MIN_TERM_W 60
#define MIN_TERM_H 24

#define MAX_ENEMIES 64
#define MAX_HUMANS 16
#define MAX_BULLETS 128
#define MAX_ENEMY_BULLETS 128

#define SIM_HZ 60.0
#define FRAME_HZ 60.0
#define SIM_DT (1.0 / SIM_HZ)

typedef enum {
    H_INACTIVE = 0,
    H_GROUNDED,
    H_CARRIED_BY_ENEMY,
    H_FALLING,
    H_CARRIED_BY_PLAYER,
    H_RESCUED,
    H_LOST
} HumanState;

typedef enum {
    E_LANDER = 0,
    E_MUTANT
} EnemyType;

typedef struct {
    double x;
    double y;
    double vx;
    double fire_timer;
    EnemyType type;
    int active;
    int carrying;
    int dir;
} Enemy;

typedef struct {
    double x;
    double y;
    double vy;
    HumanState state;
} Human;

typedef struct {
    double x;
    double y;
    double vx;
    double ttl;
    int active;
} Bullet;

typedef struct {
    double x;
    double y;
    double vx;
    double vy;
    double ttl;
    int active;
} EnemyBullet;

typedef struct {
    double x;
    double y;
    double vx;
    double vy;
    double fire_timer;
    double respawn_timer;
    double invulnerable_timer;
    int active;
    int bombs;
    int lives;
    int score;
    int facing;
    int carrying_human;
} Player;

typedef struct {
    int left;
    int right;
    int up;
    int down;
    int fire;
    int bomb;
    int quit;
    int restart;
} InputState;

static Enemy enemies[MAX_ENEMIES];
static Human humans[MAX_HUMANS];
static Bullet bullets[MAX_BULLETS];
static EnemyBullet enemy_bullets[MAX_ENEMY_BULLETS];
static Player player;

static int g_use256_colors = 0;
static int game_over = 0;
static double spawn_timer = 0.0;
static double wave_clear_timer = 0.0;
static int wave_number = 1;
static int wave_spawned = 0;
static int wave_target = 0;

static double clampd(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

static double min_double(double a, double b) {
    return a < b ? a : b;
}

static double wrap_x(double x) {
    while (x < 0.0) x += WORLD_W;
    while (x >= WORLD_W) x -= WORLD_W;
    return x;
}

static double wrapped_dx(double from_x, double to_x) {
    double dx = to_x - from_x;

    if (dx > WORLD_W / 2.0) dx -= WORLD_W;
    if (dx < -WORLD_W / 2.0) dx += WORLD_W;
    return dx;
}

static double signum(double value) {
    if (value > 0.0) return 1.0;
    if (value < 0.0) return -1.0;
    return 0.0;
}

static double distance_sq_wrapped(double ax, double ay, double bx, double by) {
    double dx = wrapped_dx(ax, bx);
    double dy = by - ay;
    return dx * dx + dy * dy;
}

static int humans_in_state(HumanState state) {
    int count = 0;
    int i;

    for (i = 0; i < MAX_HUMANS; ++i) {
        if (humans[i].state == state) {
            count++;
        }
    }
    return count;
}

static int active_human_count(void) {
    return humans_in_state(H_GROUNDED) +
           humans_in_state(H_FALLING) +
           humans_in_state(H_CARRIED_BY_PLAYER) +
           humans_in_state(H_CARRIED_BY_ENEMY);
}

static int active_enemy_count(void) {
    int count = 0;
    int i;

    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) {
            count++;
        }
    }
    return count;
}

static void start_wave(int wave) {
    wave_number = wave;
    wave_spawned = 0;
    wave_target = 4 + wave * 2;
    if (wave_target > 24) wave_target = 24;
    spawn_timer = 0.5;
    wave_clear_timer = 1.0;
    if (wave > 1 && player.bombs < 9) {
        player.bombs++;
    }
}

static void init_graphics(void) {
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
    *sx = screen_center_x + (int)lround(wrapped_dx(center_x, wx));
    *sy = 1 + (int)lround(wy);
}

static void reset_player_position(void) {
    player.x = WORLD_W / 2.0;
    player.y = 9.0;
    player.vx = 0.0;
    player.vy = 0.0;
    player.fire_timer = 0.0;
    player.active = 1;
    player.invulnerable_timer = 1.5;
}

static void release_player_human(void) {
    int human_index;

    if (player.carrying_human < 0) return;

    human_index = player.carrying_human;
    humans[human_index].state = H_FALLING;
    humans[human_index].vy = player.vy;
    humans[human_index].x = wrap_x(player.x);
    humans[human_index].y = player.y;
    player.carrying_human = -1;
}

static void init_game(void) {
    int i;
    int initial_humans = 10;
    double spacing = WORLD_W / (initial_humans + 1);

    memset(enemies, 0, sizeof(enemies));
    memset(humans, 0, sizeof(humans));
    memset(bullets, 0, sizeof(bullets));
    memset(enemy_bullets, 0, sizeof(enemy_bullets));

    player.bombs = 3;
    player.lives = 3;
    player.score = 0;
    player.facing = 1;
    player.carrying_human = -1;
    player.respawn_timer = 0.0;

    reset_player_position();

    for (i = 0; i < initial_humans; ++i) {
        humans[i].x = wrap_x(spacing * (i + 1) + (rand() % 9) - 4);
        humans[i].y = GROUND_Y;
        humans[i].vy = 0.0;
        humans[i].state = H_GROUNDED;
    }

    game_over = 0;
    start_wave(1);
}

static int spawn_enemy_type(EnemyType type, double x, double y, int dir) {
    int i;

    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) {
            enemies[i].active = 1;
            enemies[i].type = type;
            enemies[i].x = wrap_x(x);
            enemies[i].y = y;
            enemies[i].vx = dir * (type == E_MUTANT ? 22.0 : 18.0);
            enemies[i].fire_timer = type == E_MUTANT
                ? 0.45 + (rand() % 40) / 100.0
                : 0.75 + (rand() % 90) / 100.0;
            enemies[i].carrying = -1;
            enemies[i].dir = dir;
            return 1;
        }
    }
    return 0;
}

static void spawn_wave_enemy(void) {
    int side = rand() % 2;
    EnemyType type = E_LANDER;
    double x = side == 0 ? 4.0 : WORLD_W - 4.0;
    double y = 4.0 + rand() % 7;
    int dir = side == 0 ? 1 : -1;

    if (wave_number >= 3 && rand() % 100 < (wave_number - 2) * 8) {
        type = E_MUTANT;
        y = 3.0 + rand() % 6;
    }

    if (spawn_enemy_type(type, x, y, dir)) {
        wave_spawned++;
    }
}

static void spawn_mutant_from_human(double x, double y, int dir) {
    spawn_enemy_type(E_MUTANT, x, clampd(y, 2.0, GROUND_Y - 4.0), dir);
}

static void fire_bullet(void) {
    int i;

    if (!player.active || player.fire_timer > 0.0) return;

    for (i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].active) {
            bullets[i].active = 1;
            bullets[i].x = player.x + (player.facing > 0 ? 2.0 : -2.0);
            bullets[i].y = player.y;
            bullets[i].vx = player.facing * 90.0;
            bullets[i].ttl = 1.2;
            player.fire_timer = 0.13;
            return;
        }
    }
}

static void use_bomb(void) {
    int i;

    if (!player.active || player.bombs <= 0) return;

    player.bombs--;
    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) continue;

        if (enemies[i].carrying >= 0) {
            int h = enemies[i].carrying;

            enemies[i].carrying = -1;
            humans[h].state = H_FALLING;
            humans[h].vy = 0.0;
        }
        enemies[i].active = 0;
        player.score += 50;
    }

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        enemy_bullets[i].active = 0;
    }
}

static void enemy_fire(const Enemy *enemy) {
    int i;
    double dx;
    double dy;
    double dist;

    if (!player.active) return;

    dx = player.x - enemy->x;
    dy = player.y - enemy->y;
    dist = sqrt(dx * dx + dy * dy);
    if (dist < 1.0) dist = 1.0;

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!enemy_bullets[i].active) {
            enemy_bullets[i].active = 1;
            enemy_bullets[i].x = enemy->x;
            enemy_bullets[i].y = enemy->y;
            enemy_bullets[i].vx = (dx / dist) * 34.0;
            enemy_bullets[i].vy = (dy / dist) * 34.0;
            enemy_bullets[i].ttl = 2.0;
            return;
        }
    }
}

static void damage_player(void) {
    if (!player.active || player.invulnerable_timer > 0.0 || game_over) return;

    player.lives--;
    release_player_human();
    player.active = 0;
    player.vx = 0.0;
    player.vy = 0.0;

    if (player.lives <= 0) {
        game_over = 1;
        player.respawn_timer = 0.0;
        return;
    }

    player.respawn_timer = 1.5;
}

static void update_player(double dt, const InputState *input) {
    const double accel = 170.0;
    const double drag = 4.5;
    const double max_vx = 42.0;
    const double max_vy = 18.0;
    double thrust_x = 0.0;
    double thrust_y = 0.0;

    if (player.fire_timer > 0.0) player.fire_timer = fmax(0.0, player.fire_timer - dt);
    if (player.invulnerable_timer > 0.0) player.invulnerable_timer = fmax(0.0, player.invulnerable_timer - dt);

    if (!player.active) {
        if (player.respawn_timer > 0.0) {
            player.respawn_timer -= dt;
            if (player.respawn_timer <= 0.0) {
                reset_player_position();
            }
        }
        return;
    }

    thrust_x = (input->right ? 1.0 : 0.0) - (input->left ? 1.0 : 0.0);
    thrust_y = (input->down ? 1.0 : 0.0) - (input->up ? 1.0 : 0.0);

    if (thrust_x != 0.0) player.facing = thrust_x > 0.0 ? 1 : -1;

    player.vx += thrust_x * accel * dt;
    player.vy += thrust_y * accel * dt;

    player.vx -= player.vx * drag * dt;
    player.vy -= player.vy * drag * dt;

    player.vx = clampd(player.vx, -max_vx, max_vx);
    player.vy = clampd(player.vy, -max_vy, max_vy);

    player.x += player.vx * dt;
    player.y += player.vy * dt;

    player.x = wrap_x(player.x);

    if (player.y < 2.0) {
        player.y = 2.0;
        player.vy = 0.0;
    } else if (player.y > GROUND_Y) {
        player.y = GROUND_Y;
        if (player.vy > 0.0) player.vy = 0.0;
    }

    if (input->fire) fire_bullet();
    if (input->bomb) use_bomb();

    if (player.carrying_human >= 0 && player.y >= GROUND_Y - 0.2) {
        int h = player.carrying_human;

        humans[h].state = H_GROUNDED;
        humans[h].x = player.x;
        humans[h].y = GROUND_Y;
        humans[h].vy = 0.0;
        player.carrying_human = -1;
        player.score += 250;
    }
}

static void update_bullets(double dt) {
    int i;
    int e;

    for (i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].active) continue;

        bullets[i].x += bullets[i].vx * dt;
        bullets[i].x = wrap_x(bullets[i].x);
        bullets[i].ttl -= dt;
        if (bullets[i].ttl <= 0.0) {
            bullets[i].active = 0;
            continue;
        }

        for (e = 0; e < MAX_ENEMIES; ++e) {
            if (!enemies[e].active) continue;
            if (distance_sq_wrapped(bullets[i].x, bullets[i].y, enemies[e].x, enemies[e].y) > 9.0) continue;

            if (enemies[e].carrying >= 0) {
                int h = enemies[e].carrying;

                enemies[e].carrying = -1;
                humans[h].state = H_FALLING;
                humans[h].vy = 0.0;
                humans[h].x = enemies[e].x;
                humans[h].y = enemies[e].y + 1.0;
            }

            enemies[e].active = 0;
            bullets[i].active = 0;
            player.score += 150;
            break;
        }
    }
}

static void update_enemy_bullets(double dt) {
    int i;

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!enemy_bullets[i].active) continue;

        enemy_bullets[i].x += enemy_bullets[i].vx * dt;
        enemy_bullets[i].y += enemy_bullets[i].vy * dt;
        enemy_bullets[i].x = wrap_x(enemy_bullets[i].x);
        enemy_bullets[i].ttl -= dt;

        if (enemy_bullets[i].ttl <= 0.0 ||
            enemy_bullets[i].y < -8.0 || enemy_bullets[i].y > GROUND_Y + 8.0) {
            enemy_bullets[i].active = 0;
            continue;
        }

        if (player.active &&
            distance_sq_wrapped(enemy_bullets[i].x, enemy_bullets[i].y, player.x, player.y) <= 4.0) {
            enemy_bullets[i].active = 0;
            damage_player();
        }
    }
}

static int closest_grounded_human(double x) {
    int best_index = -1;
    double best_distance = 1e9;
    int i;

    for (i = 0; i < MAX_HUMANS; ++i) {
        double distance;

        if (humans[i].state != H_GROUNDED) continue;

        distance = fabs(wrapped_dx(x, humans[i].x));
        if (distance < best_distance) {
            best_distance = distance;
            best_index = i;
        }
    }

    return best_index;
}

static void update_enemies(double dt) {
    int i;

    for (i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *enemy = &enemies[i];

        if (!enemy->active) continue;

        if (enemy->type == E_MUTANT) {
            double dx = player.active ? wrapped_dx(enemy->x, player.x) : enemy->dir * 8.0;
            double dy = player.active ? (player.y - enemy->y) : sin((enemy->x * 0.02) + i) * 2.0;

            enemy->dir = dx >= 0.0 ? 1 : -1;
            enemy->x += signum(dx) * 24.0 * dt;
            enemy->y += signum(dy) * 14.0 * dt;
        } else if (enemy->carrying >= 0) {
            int h = enemy->carrying;

            enemy->x += enemy->dir * 20.0 * dt;
            enemy->y -= 12.0 * dt;

            humans[h].state = H_CARRIED_BY_ENEMY;
            humans[h].x = wrap_x(enemy->x);
            humans[h].y = enemy->y + 1.0;
            humans[h].vy = 0.0;

            if (enemy->y < -2.0) {
                humans[h].state = H_LOST;
                spawn_mutant_from_human(enemy->x, 3.0, enemy->dir);
                enemy->carrying = -1;
                enemy->active = 0;
                continue;
            }
        } else {
            int target = closest_grounded_human(enemy->x);
            double cruise_y = 5.0 + (i % 5);

            if (target >= 0) {
                double dx = wrapped_dx(enemy->x, humans[target].x);
                double desired_y = fabs(dx) < 8.0 ? humans[target].y - 1.0 : cruise_y;
                double step_x = signum(dx) * 18.0 * dt;
                double step_y = signum(desired_y - enemy->y) * 10.0 * dt;

                enemy->dir = dx >= 0.0 ? 1 : -1;
                if (fabs(dx) < fabs(step_x)) {
                    enemy->x = humans[target].x;
                } else {
                    enemy->x += step_x;
                }

                if (fabs(desired_y - enemy->y) < fabs(step_y)) {
                    enemy->y = desired_y;
                } else {
                    enemy->y += step_y;
                }

                if (fabs(wrapped_dx(enemy->x, humans[target].x)) <= 1.5 &&
                    fabs(enemy->y - (humans[target].y - 1.0)) <= 1.5) {
                    enemy->carrying = target;
                    humans[target].state = H_CARRIED_BY_ENEMY;
                    humans[target].x = wrap_x(enemy->x);
                    humans[target].y = enemy->y + 1.0;
                }
            } else {
                enemy->x += enemy->dir * 16.0 * dt;
                enemy->y += sin((enemy->x * 0.03) + i) * 1.5 * dt;
            }
        }

        enemy->x = wrap_x(enemy->x);
        enemy->y = clampd(enemy->y, 2.0, GROUND_Y - 1.0);
        enemy->fire_timer -= dt;
        if (enemy->fire_timer <= 0.0) {
            if (player.active &&
                distance_sq_wrapped(enemy->x, enemy->y, player.x, player.y) <= 130.0 * 130.0) {
                enemy_fire(enemy);
            }
            enemy->fire_timer = enemy->type == E_MUTANT
                ? 0.55 + (rand() % 35) / 100.0
                : 1.0 + (rand() % 90) / 100.0;
        }

        if (player.active && distance_sq_wrapped(enemy->x, enemy->y, player.x, player.y) <= 6.25) {
            damage_player();
        }
    }
}

static void update_humans(double dt) {
    int i;

    for (i = 0; i < MAX_HUMANS; ++i) {
        Human *human = &humans[i];

        if (human->state == H_FALLING) {
            human->vy += 26.0 * dt;
            human->y += human->vy * dt;

            if (player.active && player.carrying_human < 0 &&
                fabs(wrapped_dx(human->x, player.x)) <= 2.0 &&
                fabs(human->y - player.y) <= 1.5) {
                human->state = H_CARRIED_BY_PLAYER;
                human->vy = 0.0;
                player.carrying_human = i;
                continue;
            }

            if (human->y >= GROUND_Y) {
                human->y = GROUND_Y;
                if (human->vy > 13.0) {
                    human->state = H_LOST;
                } else {
                    human->state = H_GROUNDED;
                }
                human->vy = 0.0;
            }
        } else if (human->state == H_CARRIED_BY_PLAYER) {
            if (player.carrying_human != i || !player.active) {
                human->state = H_FALLING;
                human->vy = 0.0;
            } else {
                human->x = wrap_x(player.x);
                human->y = player.y - 1.0;
            }
        }
    }
}

static void step_game(double dt, const InputState *input) {
    if (game_over) return;

    update_player(dt, input);
    update_bullets(dt);
    update_enemy_bullets(dt);
    update_enemies(dt);
    update_humans(dt);

    if (active_human_count() <= 0) {
        game_over = 1;
        return;
    }

    if (wave_spawned < wave_target) {
        spawn_timer -= dt;
        if (spawn_timer <= 0.0) {
            double interval = 1.2 - wave_number * 0.04;

            spawn_wave_enemy();
            spawn_timer = clampd(interval, 0.35, 1.2);
        }
    } else if (active_enemy_count() == 0) {
        wave_clear_timer -= dt;
        if (wave_clear_timer <= 0.0) {
            player.score += 500 * wave_number;
            start_wave(wave_number + 1);
        }
    }
}

static void render(int term_w, int term_h) {
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
        if (enemies[i].active && radar_width > 0) {
            int rx = (int)((enemies[i].x / WORLD_W) * (radar_width - 1));

            if (rx >= 0 && rx < radar_width) {
                mvaddch(0, radar_origin + rx, enemies[i].type == E_MUTANT ? 'M' : 'E');
            }
        }
    }

    if (radar_width > 0) {
        int px = (int)((player.x / WORLD_W) * (radar_width - 1));

        if (px >= 0 && px < radar_width) {
            mvaddch(0, radar_origin + px, 'P');
        }
    }

    attron(COLOR_PAIR(25));
    for (i = 0; i < term_w; ++i) {
        mvaddch(1 + (int)GROUND_Y, i, ' ');
    }
    attroff(COLOR_PAIR(25));

    for (i = 0; i < MAX_HUMANS; ++i) {
        int sx;
        int sy;
        int pair;

        if (humans[i].state == H_INACTIVE || humans[i].state == H_RESCUED || humans[i].state == H_LOST) continue;

        world_to_view(humans[i].x, humans[i].y, player.x, screen_center_x, &sx, &sy);
        pair = humans[i].state == H_FALLING ? 23 : 22;
        draw_block(sx, sy, 1, 1, pair, term_w, term_h);
    }

    for (i = 0; i < MAX_ENEMIES; ++i) {
        int sx;
        int sy;

        if (!enemies[i].active) continue;

        world_to_view(enemies[i].x, enemies[i].y, player.x, screen_center_x, &sx, &sy);
        draw_block(sx - 1, sy, 3, 1, enemies[i].type == E_MUTANT ? 24 : 21, term_w, term_h);
    }

    for (i = 0; i < MAX_BULLETS; ++i) {
        int sx;
        int sy;

        if (!bullets[i].active) continue;

        world_to_view(bullets[i].x, bullets[i].y, player.x, screen_center_x, &sx, &sy);
        draw_block(sx, sy, 1, 1, 23, term_w, term_h);
    }

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        int sx;
        int sy;

        if (!enemy_bullets[i].active) continue;

        world_to_view(enemy_bullets[i].x, enemy_bullets[i].y, player.x, screen_center_x, &sx, &sy);
        draw_block(sx, sy, 1, 1, 24, term_w, term_h);
    }

    if (player.active && !(player.invulnerable_timer > 0.0 && ((int)(player.invulnerable_timer * 10.0) % 2 == 0))) {
        int sx;
        int sy;

        world_to_view(player.x, player.y, player.x, screen_center_x, &sx, &sy);
        draw_block(sx - 1, sy, 3, 1, 20, term_w, term_h);
    }

    attron(COLOR_PAIR(27));
    mvprintw((int)GROUND_Y + 2, 0,
             "Wave:%d  Score:%d  Bombs:%d  Lives:%d  Grounded:%d  Falling:%d  Lost:%d",
             wave_number,
             player.score,
             player.bombs,
             player.lives,
             humans_in_state(H_GROUNDED),
             humans_in_state(H_FALLING),
             humans_in_state(H_LOST));
    attroff(COLOR_PAIR(27));

    if (game_over) {
        mvprintw((int)GROUND_Y / 2, term_w / 2 - 8, "GAME OVER");
        mvprintw((int)GROUND_Y / 2 + 1, term_w / 2 - 18, "Press R to restart or Q to quit");
    } else if (!player.active) {
        mvprintw((int)GROUND_Y / 2, term_w / 2 - 7, "RESPAWNING");
    }

    refresh();
}

static void read_input(InputState *input) {
    int ch;

    memset(input, 0, sizeof(*input));

    while ((ch = getch()) != ERR) {
        switch (ch) {
            case KEY_LEFT:
                input->left = 1;
                break;
            case KEY_RIGHT:
                input->right = 1;
                break;
            case KEY_UP:
                input->up = 1;
                break;
            case KEY_DOWN:
                input->down = 1;
                break;
            case ' ':
                input->fire = 1;
                break;
            case 'b':
            case 'B':
                input->bomb = 1;
                break;
            case 'r':
            case 'R':
                input->restart = 1;
                break;
            case 'q':
            case 'Q':
                input->quit = 1;
                break;
            default:
                break;
        }
    }
}

int main(void) {
    struct timespec last;
    struct timespec now;
    double accumulator = 0.0;

    srand((unsigned)time(NULL));

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);
    curs_set(0);
    init_graphics();

    clear();
    mvprintw(5, 5, "DEFENDER - terminal demo");
    mvprintw(7, 5, "Controls: Arrow keys to thrust, Space to fire, B to use bomb, R to restart, Q to quit");
    mvprintw(9, 5, "Goal: Stop abductions, catch falling humans, and bring them back to the ground.");
    mvprintw(11, 5, "Press any key to start...");
    refresh();
    nodelay(stdscr, FALSE);
    getch();
    nodelay(stdscr, TRUE);

    init_game();
    clock_gettime(CLOCK_MONOTONIC, &last);

    while (1) {
        InputState input;
        double frame_dt;

        read_input(&input);
        if (input.quit) break;
        if (input.restart) {
            init_game();
        }

        clock_gettime(CLOCK_MONOTONIC, &now);
        frame_dt = (now.tv_sec - last.tv_sec) + (now.tv_nsec - last.tv_nsec) / 1e9;
        last = now;

        if (frame_dt > 0.25) frame_dt = 0.25;
        accumulator += frame_dt;

        while (accumulator >= SIM_DT) {
            step_game(SIM_DT, &input);
            input.fire = 0;
            input.bomb = 0;
            accumulator -= SIM_DT;
        }

        {
            int term_h;
            int term_w;

            getmaxyx(stdscr, term_h, term_w);
            render(term_w, term_h);
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
