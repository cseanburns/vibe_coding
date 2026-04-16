#include "game.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static double clampd(double value, double min_value, double max_value) {
    if (value < min_value) return min_value;
    if (value > max_value) return max_value;
    return value;
}

double game_wrap_x(double x) {
    while (x < 0.0) x += WORLD_W;
    while (x >= WORLD_W) x -= WORLD_W;
    return x;
}

double game_wrapped_dx(double from_x, double to_x) {
    double dx = to_x - from_x;

    if (dx > WORLD_W / 2.0) dx -= WORLD_W;
    if (dx < -WORLD_W / 2.0) dx += WORLD_W;
    return dx;
}

double game_terrain_y(double x) {
    double wx = game_wrap_x(x);
    double ridge = sin(wx * 0.020) * 1.5;
    double swell = sin(wx * 0.047 + 1.3) * 1.1;
    double shelf = sin(wx * 0.009 - 0.8) * 0.8;
    double terrain = GROUND_Y - 1.4 + ridge + swell + shelf;

    return clampd(terrain, 17.0, GROUND_Y);
}

static double distance_sq_wrapped(double ax, double ay, double bx, double by) {
    double dx = game_wrapped_dx(ax, bx);
    double dy = by - ay;
    return dx * dx + dy * dy;
}

static double signum(double value) {
    if (value > 0.0) return 1.0;
    if (value < 0.0) return -1.0;
    return 0.0;
}

static void award_score(GameState *game, int points) {
    game->player.score += points;
    while (game->player.score >= game->next_extra_life_score) {
        game->player.lives++;
        game->next_extra_life_score += 10000;
    }
}

static int enemy_score_value(EnemyType type) {
    switch (type) {
        case E_MUTANT:
            return 200;
        case E_BOMBER:
            return 175;
        case E_LANDER:
        default:
            return 150;
    }
}

static void spawn_enemy_projectile(GameState *game, double x, double y, double vx, double vy, double ttl) {
    int i;

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!game->enemy_bullets[i].active) {
            game->enemy_bullets[i].active = 1;
            game->enemy_bullets[i].x = game_wrap_x(x);
            game->enemy_bullets[i].y = y;
            game->enemy_bullets[i].vx = vx;
            game->enemy_bullets[i].vy = vy;
            game->enemy_bullets[i].ttl = ttl;
            return;
        }
    }
}

int game_humans_in_state(const GameState *game, HumanState state) {
    int count = 0;
    int i;

    for (i = 0; i < MAX_HUMANS; ++i) {
        if (game->humans[i].state == state) {
            count++;
        }
    }
    return count;
}

int game_active_human_count(const GameState *game) {
    return game_humans_in_state(game, H_GROUNDED) +
           game_humans_in_state(game, H_FALLING) +
           game_humans_in_state(game, H_CARRIED_BY_PLAYER) +
           game_humans_in_state(game, H_CARRIED_BY_ENEMY);
}

int game_active_enemy_count(const GameState *game) {
    int count = 0;
    int i;

    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (game->enemies[i].active) {
            count++;
        }
    }
    return count;
}

static void reset_player_position(GameState *game) {
    game->player.x = WORLD_W / 2.0;
    game->player.y = 9.0;
    game->player.vx = 0.0;
    game->player.vy = 0.0;
    game->player.fire_timer = 0.0;
    game->player.active = 1;
    game->player.invulnerable_timer = 1.5;
}

static void release_player_human(GameState *game) {
    int human_index;

    if (game->player.carrying_human < 0) return;

    human_index = game->player.carrying_human;
    game->humans[human_index].state = H_FALLING;
    game->humans[human_index].vy = game->player.vy;
    game->humans[human_index].x = game_wrap_x(game->player.x);
    game->humans[human_index].y = game->player.y;
    game->player.carrying_human = -1;
}

static void start_wave(GameState *game, int wave) {
    game->wave_number = wave;
    game->wave_spawned = 0;
    game->wave_target = 4 + wave * 2;
    game->wave_kills = 0;
    if (game->wave_target > 24) game->wave_target = 24;
    game->spawn_timer = 0.5;
    game->wave_clear_timer = 1.0;
    game->wave_banner_timer = 1.8;
    if (wave > 1 && game->player.bombs < 9) {
        game->player.bombs++;
    }
}

void game_init(GameState *game) {
    int i;
    int initial_humans = 10;
    double spacing = WORLD_W / (initial_humans + 1);

    memset(game, 0, sizeof(*game));

    game->player.bombs = 3;
    game->player.lives = 3;
    game->player.score = 0;
    game->player.facing = 1;
    game->player.carrying_human = -1;
    game->player.respawn_timer = 0.0;
    game->next_extra_life_score = 10000;

    reset_player_position(game);

    for (i = 0; i < initial_humans; ++i) {
        game->humans[i].x = game_wrap_x(spacing * (i + 1) + (rand() % 9) - 4);
        game->humans[i].y = game_terrain_y(game->humans[i].x);
        game->humans[i].vy = 0.0;
        game->humans[i].state = H_GROUNDED;
    }

    game->game_over = 0;
    start_wave(game, 1);
}

static int spawn_enemy_type(GameState *game, EnemyType type, double x, double y, int dir) {
    int i;

    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (!game->enemies[i].active) {
            game->enemies[i].active = 1;
            game->enemies[i].type = type;
            game->enemies[i].x = game_wrap_x(x);
            game->enemies[i].y = y;
            game->enemies[i].fire_timer = type == E_MUTANT
                ? 0.45 + (rand() % 40) / 100.0
                : 0.75 + (rand() % 90) / 100.0;
            game->enemies[i].carrying = -1;
            game->enemies[i].dir = dir;
            return 1;
        }
    }
    return 0;
}

static void spawn_wave_enemy(GameState *game) {
    int side = rand() % 2;
    EnemyType type = E_LANDER;
    double x = side == 0 ? 4.0 : WORLD_W - 4.0;
    double y = 4.0 + rand() % 7;
    int dir = side == 0 ? 1 : -1;

    if (game->wave_number >= 3 && rand() % 100 < (game->wave_number - 2) * 8) {
        type = E_MUTANT;
        y = 3.0 + rand() % 6;
    }
    if (game->wave_number >= 5 && type == E_LANDER && rand() % 100 < 18) {
        type = E_BOMBER;
        y = 4.0 + rand() % 5;
    }

    if (spawn_enemy_type(game, type, x, y, dir)) {
        game->wave_spawned++;
    }
}

static void spawn_mutant_from_human(GameState *game, double x, double y, int dir) {
    spawn_enemy_type(game, E_MUTANT, x, clampd(y, 2.0, GROUND_Y - 4.0), dir);
}

static void fire_bullet(GameState *game) {
    int i;

    if (!game->player.active || game->player.fire_timer > 0.0) return;

    for (i = 0; i < MAX_BULLETS; ++i) {
        if (!game->bullets[i].active) {
            game->bullets[i].active = 1;
            game->bullets[i].x = game->player.x + (game->player.facing > 0 ? 2.0 : -2.0);
            game->bullets[i].y = game->player.y;
            game->bullets[i].vx = game->player.facing * 90.0;
            game->bullets[i].ttl = 1.2;
            game->player.fire_timer = 0.13;
            return;
        }
    }
}

static void use_bomb(GameState *game) {
    int i;

    if (!game->player.active || game->player.bombs <= 0) return;

    game->player.bombs--;
    for (i = 0; i < MAX_ENEMIES; ++i) {
        if (!game->enemies[i].active) continue;

        if (game->enemies[i].carrying >= 0) {
            int h = game->enemies[i].carrying;

            game->enemies[i].carrying = -1;
            game->humans[h].state = H_FALLING;
            game->humans[h].vy = 0.0;
        }
        game->enemies[i].active = 0;
        game->wave_kills++;
        award_score(game, 50);
    }

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        game->enemy_bullets[i].active = 0;
    }
}

static void enemy_fire(GameState *game, const Enemy *enemy) {
    double dx;
    double dy;
    double dist;

    if (!game->player.active) return;

    dx = game_wrapped_dx(enemy->x, game->player.x);
    dy = game->player.y - enemy->y;
    dist = sqrt(dx * dx + dy * dy);
    if (dist < 1.0) dist = 1.0;

    if (enemy->type == E_BOMBER) {
        spawn_enemy_projectile(game, enemy->x - 1.0, enemy->y + 1.0, -6.0, 22.0, 1.6);
        spawn_enemy_projectile(game, enemy->x, enemy->y + 1.0, 0.0, 24.0, 1.6);
        spawn_enemy_projectile(game, enemy->x + 1.0, enemy->y + 1.0, 6.0, 22.0, 1.6);
        return;
    }

    spawn_enemy_projectile(game, enemy->x, enemy->y, (dx / dist) * 34.0, (dy / dist) * 34.0, 2.0);
}

static void damage_player(GameState *game) {
    if (!game->player.active || game->player.invulnerable_timer > 0.0 || game->game_over) return;

    game->player.lives--;
    release_player_human(game);
    game->player.active = 0;
    game->player.vx = 0.0;
    game->player.vy = 0.0;

    if (game->player.lives <= 0) {
        game->game_over = 1;
        game->player.respawn_timer = 0.0;
        return;
    }

    game->player.respawn_timer = 1.5;
}

static void update_player(GameState *game, double dt, const InputState *input) {
    const double accel = 170.0;
    const double drag = 4.5;
    const double max_vx = 42.0;
    const double max_vy = 18.0;
    double thrust_x = 0.0;
    double thrust_y = 0.0;

    if (game->player.fire_timer > 0.0) game->player.fire_timer = fmax(0.0, game->player.fire_timer - dt);
    if (game->player.invulnerable_timer > 0.0) {
        game->player.invulnerable_timer = fmax(0.0, game->player.invulnerable_timer - dt);
    }

    if (!game->player.active) {
        if (game->player.respawn_timer > 0.0) {
            game->player.respawn_timer -= dt;
            if (game->player.respawn_timer <= 0.0) {
                reset_player_position(game);
            }
        }
        return;
    }

    thrust_x = (input->right ? 1.0 : 0.0) - (input->left ? 1.0 : 0.0);
    thrust_y = (input->down ? 1.0 : 0.0) - (input->up ? 1.0 : 0.0);

    if (thrust_x != 0.0) game->player.facing = thrust_x > 0.0 ? 1 : -1;

    game->player.vx += thrust_x * accel * dt;
    game->player.vy += thrust_y * accel * dt;

    game->player.vx -= game->player.vx * drag * dt;
    game->player.vy -= game->player.vy * drag * dt;

    game->player.vx = clampd(game->player.vx, -max_vx, max_vx);
    game->player.vy = clampd(game->player.vy, -max_vy, max_vy);

    game->player.x += game->player.vx * dt;
    game->player.y += game->player.vy * dt;

    game->player.x = game_wrap_x(game->player.x);

    if (game->player.y < 2.0) {
        game->player.y = 2.0;
        game->player.vy = 0.0;
    } else {
        double floor_y = game_terrain_y(game->player.x);

        if (game->player.y > floor_y) {
            if (game->player.vy > 10.0) {
                damage_player(game);
                return;
            }
            game->player.y = floor_y;
            if (game->player.vy > 0.0) game->player.vy = 0.0;
        }
    }

    if (input->fire) fire_bullet(game);
    if (input->bomb) use_bomb(game);

    if (game->player.carrying_human >= 0 &&
        game->player.y >= game_terrain_y(game->player.x) - 0.15) {
        int h = game->player.carrying_human;
        double floor_y = game_terrain_y(game->player.x);

        game->humans[h].state = H_GROUNDED;
        game->humans[h].x = game->player.x;
        game->humans[h].y = floor_y;
        game->humans[h].vy = 0.0;
        game->player.carrying_human = -1;
        award_score(game, 250);
    }
}

static void update_bullets(GameState *game, double dt) {
    int i;
    int e;

    for (i = 0; i < MAX_BULLETS; ++i) {
        if (!game->bullets[i].active) continue;

        game->bullets[i].x += game->bullets[i].vx * dt;
        game->bullets[i].x = game_wrap_x(game->bullets[i].x);
        game->bullets[i].ttl -= dt;
        if (game->bullets[i].ttl <= 0.0) {
            game->bullets[i].active = 0;
            continue;
        }

        for (e = 0; e < MAX_ENEMIES; ++e) {
            if (!game->enemies[e].active) continue;
            if (distance_sq_wrapped(game->bullets[i].x, game->bullets[i].y,
                                    game->enemies[e].x, game->enemies[e].y) > 9.0) {
                continue;
            }

            if (game->enemies[e].carrying >= 0) {
                int h = game->enemies[e].carrying;

                game->enemies[e].carrying = -1;
                game->humans[h].state = H_FALLING;
                game->humans[h].vy = 0.0;
                game->humans[h].x = game->enemies[e].x;
                game->humans[h].y = game->enemies[e].y + 1.0;
            }

            game->enemies[e].active = 0;
            game->bullets[i].active = 0;
            game->wave_kills++;
            award_score(game, enemy_score_value(game->enemies[e].type));
            break;
        }
    }
}

static void update_enemy_bullets(GameState *game, double dt) {
    int i;

    for (i = 0; i < MAX_ENEMY_BULLETS; ++i) {
        if (!game->enemy_bullets[i].active) continue;

        game->enemy_bullets[i].x += game->enemy_bullets[i].vx * dt;
        game->enemy_bullets[i].y += game->enemy_bullets[i].vy * dt;
        game->enemy_bullets[i].x = game_wrap_x(game->enemy_bullets[i].x);
        game->enemy_bullets[i].ttl -= dt;

        if (game->enemy_bullets[i].ttl <= 0.0 ||
            game->enemy_bullets[i].y < -8.0 || game->enemy_bullets[i].y > GROUND_Y + 8.0) {
            game->enemy_bullets[i].active = 0;
            continue;
        }

        if (game->player.active &&
            distance_sq_wrapped(game->enemy_bullets[i].x, game->enemy_bullets[i].y,
                                game->player.x, game->player.y) <= 4.0) {
            game->enemy_bullets[i].active = 0;
            damage_player(game);
        }
    }
}

static int closest_grounded_human(const GameState *game, double x) {
    int best_index = -1;
    double best_distance = 1e9;
    int i;

    for (i = 0; i < MAX_HUMANS; ++i) {
        double distance;

        if (game->humans[i].state != H_GROUNDED) continue;

        distance = fabs(game_wrapped_dx(x, game->humans[i].x));
        if (distance < best_distance) {
            best_distance = distance;
            best_index = i;
        }
    }

    return best_index;
}

static void update_enemies(GameState *game, double dt) {
    int i;

    for (i = 0; i < MAX_ENEMIES; ++i) {
        Enemy *enemy = &game->enemies[i];

        if (!enemy->active) continue;

            if (enemy->type == E_MUTANT) {
                double dx = game->player.active ? game_wrapped_dx(enemy->x, game->player.x) : enemy->dir * 8.0;
                double dy = game->player.active ? (game->player.y - enemy->y) : sin((enemy->x * 0.02) + i) * 2.0;

                enemy->dir = dx >= 0.0 ? 1 : -1;
                enemy->x += signum(dx) * 24.0 * dt;
                enemy->y += signum(dy) * 14.0 * dt;
            } else if (enemy->type == E_BOMBER) {
                enemy->x += enemy->dir * 22.0 * dt;
                enemy->y += sin((enemy->x * 0.04) + i) * 2.2 * dt;
        } else if (enemy->carrying >= 0) {
            int h = enemy->carrying;

            enemy->x += enemy->dir * 20.0 * dt;
            enemy->y -= 12.0 * dt;

            game->humans[h].state = H_CARRIED_BY_ENEMY;
            game->humans[h].x = game_wrap_x(enemy->x);
            game->humans[h].y = enemy->y + 1.0;
            game->humans[h].vy = 0.0;

            if (enemy->y < -2.0) {
                game->humans[h].state = H_LOST;
                spawn_mutant_from_human(game, enemy->x, 3.0, enemy->dir);
                enemy->carrying = -1;
                enemy->active = 0;
                continue;
            }
        } else {
            int target = closest_grounded_human(game, enemy->x);
            double cruise_y = 5.0 + (i % 5);

            if (target >= 0) {
                double dx = game_wrapped_dx(enemy->x, game->humans[target].x);
                double desired_y = fabs(dx) < 8.0 ? game->humans[target].y - 1.0 : cruise_y;
                double step_x = signum(dx) * 18.0 * dt;
                double step_y = signum(desired_y - enemy->y) * 10.0 * dt;

                enemy->dir = dx >= 0.0 ? 1 : -1;
                if (fabs(dx) < fabs(step_x)) {
                    enemy->x = game->humans[target].x;
                } else {
                    enemy->x += step_x;
                }

                if (fabs(desired_y - enemy->y) < fabs(step_y)) {
                    enemy->y = desired_y;
                } else {
                    enemy->y += step_y;
                }

                if (fabs(game_wrapped_dx(enemy->x, game->humans[target].x)) <= 1.5 &&
                    fabs(enemy->y - (game->humans[target].y - 1.0)) <= 1.5) {
                    enemy->carrying = target;
                    game->humans[target].state = H_CARRIED_BY_ENEMY;
                    game->humans[target].x = game_wrap_x(enemy->x);
                    game->humans[target].y = enemy->y + 1.0;
                }
            } else {
                enemy->x += enemy->dir * 16.0 * dt;
                enemy->y += sin((enemy->x * 0.03) + i) * 1.5 * dt;
            }
        }

        enemy->x = game_wrap_x(enemy->x);
        enemy->y = clampd(enemy->y, 2.0, game_terrain_y(enemy->x) - 1.0);
        enemy->fire_timer -= dt;
        if (enemy->fire_timer <= 0.0) {
            if (game->player.active &&
                distance_sq_wrapped(enemy->x, enemy->y, game->player.x, game->player.y) <= 130.0 * 130.0) {
                enemy_fire(game, enemy);
            }
            if (enemy->type == E_MUTANT) {
                enemy->fire_timer = 0.55 + (rand() % 35) / 100.0;
            } else if (enemy->type == E_BOMBER) {
                enemy->fire_timer = 0.40 + (rand() % 30) / 100.0;
            } else {
                enemy->fire_timer = 1.0 + (rand() % 90) / 100.0;
            }
        }

        if (game->player.active &&
            distance_sq_wrapped(enemy->x, enemy->y, game->player.x, game->player.y) <= 6.25) {
            damage_player(game);
        }
    }
}

static void update_humans(GameState *game, double dt) {
    int i;

    for (i = 0; i < MAX_HUMANS; ++i) {
        Human *human = &game->humans[i];

        if (human->state == H_FALLING) {
            double floor_y = game_terrain_y(human->x);

            human->vy += 26.0 * dt;
            human->y += human->vy * dt;

            if (game->player.active && game->player.carrying_human < 0 &&
                fabs(game_wrapped_dx(human->x, game->player.x)) <= 2.0 &&
                fabs(human->y - game->player.y) <= 1.5) {
                human->state = H_CARRIED_BY_PLAYER;
                human->vy = 0.0;
                game->player.carrying_human = i;
                continue;
            }

            if (human->y >= floor_y) {
                human->y = floor_y;
                if (human->vy > 13.0) {
                    human->state = H_LOST;
                } else {
                    human->state = H_GROUNDED;
                }
                human->vy = 0.0;
            }
        } else if (human->state == H_CARRIED_BY_PLAYER) {
            if (game->player.carrying_human != i || !game->player.active) {
                human->state = H_FALLING;
                human->vy = 0.0;
            } else {
                human->x = game_wrap_x(game->player.x);
                human->y = game->player.y - 1.0;
            }
        }
    }
}

void game_step(GameState *game, double dt, const InputState *input) {
    if (game->game_over) return;

    if (game->wave_banner_timer > 0.0) {
        game->wave_banner_timer -= dt;
        if (game->wave_banner_timer < 0.0) game->wave_banner_timer = 0.0;
    }

    update_player(game, dt, input);
    update_bullets(game, dt);
    update_enemy_bullets(game, dt);
    update_enemies(game, dt);
    update_humans(game, dt);

    if (game_active_human_count(game) <= 0) {
        game->game_over = 1;
        return;
    }

    if (game->wave_spawned < game->wave_target) {
        game->spawn_timer -= dt;
        if (game->spawn_timer <= 0.0) {
            double interval = 1.2 - game->wave_number * 0.04;

            spawn_wave_enemy(game);
            game->spawn_timer = clampd(interval, 0.35, 1.2);
        }
    } else if (game_active_enemy_count(game) == 0) {
        game->wave_clear_timer -= dt;
        if (game->wave_clear_timer <= 0.0 && game->wave_kills >= game->wave_target) {
            award_score(game, 500 * game->wave_number);
            start_wave(game, game->wave_number + 1);
        }
    }
}
