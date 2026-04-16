#ifndef GAME_H
#define GAME_H

#define WORLD_W 600.0
#define GROUND_Y 21.0
#define MIN_TERM_W 60
#define MIN_TERM_H 24

#define MAX_ENEMIES 64
#define MAX_HUMANS 16
#define MAX_BULLETS 128
#define MAX_ENEMY_BULLETS 128

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
    E_MUTANT,
    E_BOMBER
} EnemyType;

typedef struct {
    double x;
    double y;
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

typedef struct {
    Enemy enemies[MAX_ENEMIES];
    Human humans[MAX_HUMANS];
    Bullet bullets[MAX_BULLETS];
    EnemyBullet enemy_bullets[MAX_ENEMY_BULLETS];
    Player player;
    int game_over;
    double spawn_timer;
    double wave_clear_timer;
    int wave_number;
    int wave_spawned;
    int wave_target;
} GameState;

void game_init(GameState *game);
void game_step(GameState *game, double dt, const InputState *input);

double game_wrap_x(double x);
double game_wrapped_dx(double from_x, double to_x);
double game_terrain_y(double x);

int game_humans_in_state(const GameState *game, HumanState state);
int game_active_human_count(const GameState *game);
int game_active_enemy_count(const GameState *game);

#endif
