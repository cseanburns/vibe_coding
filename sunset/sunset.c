/* sunset/sunset.c
 * Terminal sunset animation using ncurses
 * Builds with: gcc -o sunset sunset.c -lncurses
 */

#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <string.h>

#define CYCLE_SECONDS 140.0 /* full day->night->day cycle */
#define FPS 20
#define STAR_COUNT 150

typedef struct { int x, y; int bright; } Star;

static Star stars[STAR_COUNT];

static int g_use256 = 0;
static int g_sky_day, g_sky_sunset, g_sky_dusk, g_sky_night;
static int g_fg_text;

static void init_colors() {
    if (!has_colors()) return;
    start_color();
    use_default_colors();

    g_use256 = (COLORS >= 256);
    g_sky_day = g_use256 ? 33 : COLOR_BLUE;      /* light blue */
    g_sky_sunset = g_use256 ? 208 : COLOR_RED;  /* orange */
    g_sky_dusk = g_use256 ? 90 : COLOR_MAGENTA; /* purple */
    g_sky_night = g_use256 ? 16 : COLOR_BLACK;  /* dark */
    int sun_col = g_use256 ? 226 : COLOR_YELLOW;  /* bright yellow */
    int mountain_day = g_use256 ? 34 : COLOR_GREEN;
    int mountain_dusk = g_use256 ? 22 : COLOR_GREEN;
    int star_col = g_use256 ? 15 : COLOR_WHITE;
    int highlight = g_use256 ? 87 : COLOR_CYAN;

    init_pair(1, g_sky_day, g_sky_day);       /* day sky */
    init_pair(2, g_sky_sunset, g_sky_sunset); /* sunset */
    init_pair(3, g_sky_dusk, g_sky_dusk);     /* dusk */
    init_pair(4, g_sky_night, g_sky_night);   /* night */
    init_pair(5, sun_col, sun_col);       /* sun */
    init_pair(6, mountain_day, mountain_day); /* mountains day */
    init_pair(7, mountain_dusk, mountain_dusk); /* mountains dusk */
    init_pair(8, star_col, g_sky_night);    /* stars (fg on night bg) */
    init_pair(9, highlight, highlight);   /* highlights */

    g_fg_text = g_use256 ? 231 : COLOR_WHITE;
    init_pair(10, g_fg_text, g_sky_day); /* footer default, bg will be updated */
}

static void seed_stars(int w, int h) {
    srand((unsigned)time(NULL));
    for (int i = 0; i < STAR_COUNT; ++i) {
        stars[i].x = rand() % w;
        stars[i].y = rand() % (h/2);
        stars[i].bright = rand() % 100;
    }
}

static void draw_sky(WINDOW *buf, int w, int h, double darkness) {
    int sky_mode;
    if (darkness < 0.25) sky_mode = 1;          /* day */
    else if (darkness < 0.5) sky_mode = 2;      /* sunset */
    else if (darkness < 0.8) sky_mode = 3;      /* dusk */
    else sky_mode = 4;                          /* night */

    wbkgd(buf, COLOR_PAIR(sky_mode));
    werase(buf);
}

static void draw_mountains(WINDOW *buf, int w, int h, double darkness) {
    int horizon = h - (h/4);
    int peaks = 7;
    int peak_w = w / peaks;
    int base_color = (darkness < 0.5) ? 6 : 7;
    wattron(buf, COLOR_PAIR(base_color));
    for (int p = 0; p < peaks; ++p) {
        int peak_x = p * peak_w + peak_w/2;
        int peak_h = (h/6) + (p%2==0 ? 3 : 0);
        for (int x = p*peak_w; x < (p+1)*peak_w && x < w; ++x) {
            int dx = abs(x - peak_x);
            int ytop = horizon - peak_h + (dx * peak_h) / (peak_w/2 + 1);
            if (ytop < 0) ytop = 0;
            for (int y = ytop; y < h; ++y) {
                mvwaddch(buf, y, x, ' ');
            }
        }
    }
    wattroff(buf, COLOR_PAIR(base_color));
}

static void draw_sun(WINDOW *buf, int w, int h, double angle, double darkness) {
    /* angle from 0..2pi, sun follows an arc; sun visible when above horizon */
    int cx = w/2 + (int)((w/3)*cos(angle));
    int cy = h/2 - (int)((h/3)*sin(angle));
    int r = 2 + (int)(1.5 * (1.0 - darkness));
    wattron(buf, COLOR_PAIR(5));
    for (int dy = -r; dy <= r; ++dy) {
        for (int dx = -r; dx <= r; ++dx) {
            if (dx*dx + dy*dy <= r*r) {
                int x = cx + dx;
                int y = cy + dy;
                if (x >= 0 && x < w && y >= 0 && y < h) mvwaddch(buf, y, x, ' ');
            }
        }
    }
    wattroff(buf, COLOR_PAIR(5));
}

static void draw_stars(WINDOW *buf, int w, int h, double darkness, double twinkle) {
    if (darkness < 0.4) return;
    wattron(buf, COLOR_PAIR(8));
    for (int i = 0; i < STAR_COUNT; ++i) {
        double vis = (stars[i].bright/100.0);
        if (vis + (twinkle*0.5) > (darkness - 0.3)) {
            int x = stars[i].x;
            int y = stars[i].y;
            if (x>=0 && x<w && y>=0 && y<h) mvwaddch(buf, y, x, (vis>0.7)?'*':'.');
        }
    }
    wattroff(buf, COLOR_PAIR(8));
}

static void draw_moon(WINDOW *buf, int w, int h, double angle, double darkness) {
    if (darkness < 0.6) return;
    int cx = w/3 + (int)((w/3)*cos(angle + M_PI/3));
    int cy = h/3 - (int)((h/4)*sin(angle + M_PI/3));
    wattron(buf, COLOR_PAIR(4));
    mvwaddch(buf, cy, cx, 'o');
    wattroff(buf, COLOR_PAIR(4));
}

int main(void) {
    initscr();
    noecho();
    curs_set(0);
    nodelay(stdscr, TRUE);
    keypad(stdscr, TRUE);

    init_colors();

    int w, h;
    getmaxyx(stdscr, h, w);
    seed_stars(w, h);
    WINDOW *buf = newwin(h, w, 0, 0);

    struct timespec start_ts;
    clock_gettime(CLOCK_MONOTONIC, &start_ts);

    while (1) {
        struct timespec now_ts;
        clock_gettime(CLOCK_MONOTONIC, &now_ts);
        double elapsed = (now_ts.tv_sec - start_ts.tv_sec) +
                         (now_ts.tv_nsec - start_ts.tv_nsec)/1e9;
        double t = fmod(elapsed, CYCLE_SECONDS) / CYCLE_SECONDS; /* 0..1 */

        /* angle for sun/moon travel around arc */
        double angle = M_PI * (1.0 - 2.0*t); /* goes from +pi to -pi */

        /* darkness: 0 (day) .. 1 (night) based on vertical position of sun */
        double sun_y_norm = (sin(angle) + 1.0) / 2.0; /* 0..1; 1 means sun high */
        double darkness = 1.0 - sun_y_norm; /* inverted: 0 day -> 1 night */

        /* small twinkle factor */
        double twinkle = (sin(elapsed*3.1415) + 1.0)/2.0;

        int nh, nw;
        getmaxyx(stdscr, nh, nw);
        if (nh != h || nw != w) {
            h = nh; w = nw;
            delwin(buf);
            buf = newwin(h, w, 0, 0);
            seed_stars(w, h);
        }

        draw_sky(buf, w, h, darkness);
        draw_sun(buf, w, h, angle, darkness);
        draw_moon(buf, w, h, angle, darkness);
        draw_stars(buf, w, h, darkness, twinkle);
        draw_mountains(buf, w, h, darkness);

        /* footer text: pick contrasting background based on sky */
        int sky_mode;
        if (darkness < 0.25) sky_mode = 1;
        else if (darkness < 0.5) sky_mode = 2;
        else if (darkness < 0.8) sky_mode = 3;
        else sky_mode = 4;
        int bg_color;
        switch (sky_mode) {
            case 1: bg_color = g_sky_day; break;
            case 2: bg_color = g_sky_sunset; break;
            case 3: bg_color = g_sky_dusk; break;
            default: bg_color = g_sky_night; break;
        }
        init_pair(10, g_fg_text, bg_color);
        wattron(buf, COLOR_PAIR(10) | A_BOLD);
        mvwprintw(buf, h-1, 1, "Press 'q' to quit. Cycle: %.0fs", CYCLE_SECONDS);
        wattroff(buf, COLOR_PAIR(10) | A_BOLD);

        overwrite(buf, stdscr);
        wrefresh(stdscr);

        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        usleep((useconds_t)(1e6 / FPS));
    }

    delwin(buf);
    endwin();
    return 0;
}
