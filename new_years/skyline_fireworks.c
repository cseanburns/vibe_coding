// skyline_fireworks.c
// ncurses animation: skyline + lake + moving boats + looping fireworks

#define _XOPEN_SOURCE 700

#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <string.h>

/* ---------- utility ---------- */

static int clampi(int v, int lo, int hi) {
    return (v < lo) ? lo : (v > hi) ? hi : v;
}

static int randi(int lo, int hi) {
    return lo + rand() % (hi - lo + 1);
}

/* ---------- colors ---------- */

static void init_colors(void) {
    if (!has_colors()) return;

    start_color();
    use_default_colors();

    init_pair(1, COLOR_WHITE,   -1);
    init_pair(2, COLOR_YELLOW,  -1);
    init_pair(3, COLOR_CYAN,    -1);
    init_pair(4, COLOR_MAGENTA, -1);
    init_pair(5, COLOR_RED,     -1);
    init_pair(6, COLOR_GREEN,   -1);
    init_pair(7, COLOR_BLUE,    -1);
}

/* ---------- data types ---------- */

typedef struct {
    int x0, w, h;
    int is_tower;
    int light_rows;
    int light_cols;
    unsigned char *lights; /* row-major: light_rows * light_cols, 0=off,1=on */
} Building;

/* each building carries a static grid of window lights (on/off) */

typedef struct {
    float x;
    int y;
    float speed;
    int dir;
    int sprite;
} Boat;

typedef struct {
    int x, y, vy, fuse;
    short color;
    int active;
} Rocket;

typedef struct {
    int x, y, vx, vy, ttl;
    short color;
    int active;
} Particle;

/* ---------- skyline ---------- */

static void init_skyline_layout(Building *b, int n, int cols, int horizon_y) {
    int base_h = clampi(horizon_y / 2, 6, horizon_y - 3);

    for (int i = 0; i < n; i++) {
        int cx = (cols * (i + 1)) / (n + 1);
        int w  = clampi(cols / 14 + randi(-1, 3), 6, 16);
        int h  = clampi(base_h + randi(-3, 6), 6, (horizon_y * 2) / 3);

        b[i].is_tower = (i == n / 2);
        if (b[i].is_tower) {
            w = clampi(cols / 18, 6, 12);
            h = clampi(horizon_y - 6, 10, horizon_y - 3);
        }

        b[i].w  = w;
        b[i].h  = h;
        b[i].x0 = cx - w / 2;

        /* initialize static light grid for building interior */
        int interior_h = b[i].h - 2; /* exclude top and base lines */
        int interior_w = b[i].w - 2;
        if (interior_h < 1) interior_h = 0;
        if (interior_w < 1) interior_w = 0;
        b[i].light_rows = interior_h;
        b[i].light_cols = interior_w;
        if (interior_h > 0 && interior_w > 0) {
            b[i].lights = malloc(interior_h * interior_w);
            if (b[i].lights) {
                for (int r = 0; r < interior_h; r++)
                    for (int c = 0; c < interior_w; c++)
                        b[i].lights[r * interior_w + c] = (rand() % 100) < 55; /* ~55% on */
            }
        } else {
            b[i].lights = NULL;
        }
    }
}

static void draw_building(int base_y, const Building *b, int cols, int horizon_y) {
    int left   = clampi(b->x0, 0, cols - 1);
    int right  = clampi(b->x0 + b->w - 1, 0, cols - 1);
    int top_y  = clampi(base_y - b->h + 1, 0, horizon_y - 1);

    attron(COLOR_PAIR(1));
    for (int y = top_y; y <= base_y; y++) {
        mvaddch(y, left, '|');
        mvaddch(y, right, '|');
    }
    for (int x = left; x <= right; x++) {
        mvaddch(top_y, x, '-');
        mvaddch(base_y, x, '-');
    }
    attroff(COLOR_PAIR(1));

    /* draw window lights as a regular grid of squares using the building's static lights */
    if (b->lights && b->light_rows > 0 && b->light_cols > 0) {
        int interior_w = b->light_cols;
        int interior_h = b->light_rows;
        /* map interior grid to available interior area; one light per interior cell */
        for (int r = 0; r < interior_h; r++) {
            int y = top_y + 1 + r;
            if (y >= base_y) break;
            for (int c = 0; c < interior_w; c++) {
                int x = left + 1 + c;
                if (x >= right) break;
                if (b->lights[r * interior_w + c]) {
                    attron(COLOR_PAIR(2));
#ifdef ACS_CKBOARD
                    mvaddch(y, x, ACS_CKBOARD);
#else
                    mvaddch(y, x, 'O');
#endif
                    attroff(COLOR_PAIR(2));
                }
            }
        }
    }

    if (b->is_tower) {
        int cx = clampi(b->x0 + b->w / 2, 0, cols - 1);
        attron(COLOR_PAIR(1));
        for (int y = top_y - 1; y >= top_y - 4 && y >= 0; y--)
            mvaddch(y, cx, '|');
        mvaddch(clampi(top_y - 4, 0, horizon_y - 1), cx, '*');
        attroff(COLOR_PAIR(1));
    }
}

static void draw_skyline(const Building *b, int n, int cols, int horizon_y) {
    int base_y = horizon_y - 1;
    for (int i = 0; i < n; i++)
        draw_building(base_y, &b[i], cols, horizon_y);
}

/* ---------- lake + boats ---------- */

static void draw_lake(int rows, int cols, int horizon_y, int tick) {
    int orig_lake_rows = rows - (horizon_y + 1);
    int draw_lake_rows = orig_lake_rows / 2; /* reduce lake height by half */
    if (draw_lake_rows < 1) draw_lake_rows = 1;

    /* make lake flush to bottom */
    int y_start = rows - draw_lake_rows;
    if (y_start < horizon_y + 1) y_start = horizon_y + 1;

    for (int y = y_start; y < rows; y++) {
        for (int x = 0; x < cols; x++) {
            if ((x + y + tick) % 9 == 0) {
                attron(COLOR_PAIR(3));
                mvaddch(y, x, '~');
                attroff(COLOR_PAIR(3));
            }
        }
    }
}

static void draw_boat(int y, int x, int cols) {
    const char *hull = "\\____/";
    int len = strlen(hull);
    int x0 = x - len / 2;

    attron(COLOR_PAIR(1));
    for (int i = 0; i < len; i++) {
        int xx = x0 + i;
        if (xx >= 0 && xx < cols)
            mvaddch(y, xx, hull[i]);
    }
    attroff(COLOR_PAIR(1));
}

static void update_boats(Boat *boats, int n, int cols, int horizon_y, int rows) {
    int ybase = clampi(horizon_y + 2, 0, rows - 2);
    for (int i = 0; i < n; i++) {
        boats[i].x += boats[i].speed * boats[i].dir;

        if (boats[i].x > cols + 6) boats[i].x = -6;
        if (boats[i].x < -6)       boats[i].x = cols + 6;

        int y = ybase + (i % 2);
        if (y >= rows) y = rows - 1;
        draw_boat(y, (int)boats[i].x, cols);
    }
}

/* ---------- fireworks ---------- */

static short fw_color(void) {
    short c[] = {4,5,6,7,2};
    return c[rand() % 5];
}

static void spawn_rocket(Rocket *r, int cols, int horizon_y) {
    r->active = 1;
    r->x = randi(cols / 6, cols * 5 / 6);
    r->y = horizon_y - 1;
    r->vy = -1;
    r->fuse = randi(3, horizon_y / 3);
    r->color = fw_color();
}

static void explode(Rocket *r, Particle *p, int maxp) {
    for (int i = 0; i < 80; i++) {
        for (int j = 0; j < maxp; j++) {
            if (!p[j].active) {
                p[j] = (Particle){
                    .x = r->x,
                    .y = r->y,
                    .vx = randi(-3,3),
                    .vy = randi(-3,3),
                    .ttl = randi(12,30),
                    .color = r->color,
                    .active = 1
                };
                break;
            }
        }
    }
}

static void update_fireworks(Rocket *r, int nr, Particle *p, int np, int cols, int horizon_y) {
    for (int i = 0; i < nr; i++) {
        if (!r[i].active) continue;

        attron(COLOR_PAIR(r[i].color));
        mvaddch(r[i].y, r[i].x, '|');
        attroff(COLOR_PAIR(r[i].color));

        r[i].y += r[i].vy;
        if (r[i].y <= r[i].fuse) {
            explode(&r[i], p, np);
            r[i].active = 0;
        }
    }

    for (int i = 0; i < np; i++) {
        if (!p[i].active) continue;

        attron(COLOR_PAIR(p[i].color));
        mvaddch(p[i].y, p[i].x, '*');
        attroff(COLOR_PAIR(p[i].color));

        p[i].x += p[i].vx;
        p[i].y += p[i].vy;
        if (--p[i].ttl <= 0)
            p[i].active = 0;
    }

    if (rand() % 8 == 0) {
        for (int i = 0; i < nr; i++)
            if (!r[i].active) {
                spawn_rocket(&r[i], cols, horizon_y);
                break;
            }
    }
}

/* ---------- main ---------- */

int main(void) {
    srand(time(NULL));
    initscr();
    cbreak();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);

    init_colors();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    /* compute horizon so lake (half-size) sits flush to bottom and skyline shifts down */
    int base_horizon = rows * 3 / 5;
    int orig_lake_rows = rows - (base_horizon + 1);
    int draw_lake_rows = orig_lake_rows / 2;
    if (draw_lake_rows < 1) draw_lake_rows = 1;
    int horizon_y = rows - draw_lake_rows - 1;
    if (horizon_y < 1) horizon_y = rows / 2;

    const int NBUILD = 9;
    Building buildings[NBUILD];
    init_skyline_layout(buildings, NBUILD, cols, horizon_y);

    Boat boats[4];
    for (int i = 0; i < 4; i++)
        boats[i] = (Boat){ randi(0,cols), 0, 0.3f + i * 0.15f, 1, 0 };

    Rocket rockets[10] = {0};
    Particle particles[500] = {0};

    while (1) {
        int ch = getch();
        if (ch == 'q') break;

        erase();

        draw_skyline(buildings, NBUILD, cols, horizon_y);
        update_fireworks(rockets, 10, particles, 500, cols, horizon_y);
        draw_lake(rows, cols, horizon_y, rand());
        update_boats(boats, 4, cols, horizon_y, rows);

        refresh();

        struct timespec ts = {0, 80 * 1000 * 1000};
        nanosleep(&ts, NULL);
    }

    endwin();
    return 0;
}

