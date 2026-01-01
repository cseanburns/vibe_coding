// xmas.c - ncurses Christmas tree with snow + ornaments + sine garland
//
// Build: make
// Run:   ./xmas
// Quit:  press 'q'

#define _XOPEN_SOURCE 500

#include <ncurses.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <unistd.h>

typedef struct {
    int x, y;
    int speed;   // frames per fall step
    int phase;   // counter for speed
    char ch;
} Snowflake;

static int clampi(int v, int lo, int hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void init_colors(void) {
    start_color();
    use_default_colors();

    // Color pairs
    // 1: tree green
    init_pair(1, COLOR_GREEN, -1);

    // 2..6: ornaments
    init_pair(2, COLOR_RED, -1);
    init_pair(3, COLOR_YELLOW, -1);
    init_pair(4, COLOR_BLUE, -1);
    init_pair(5, COLOR_MAGENTA, -1);
    init_pair(6, COLOR_CYAN, -1);

    // 7: trunk
    init_pair(7, COLOR_YELLOW, -1);

    // 8: star
    init_pair(8, COLOR_YELLOW, -1);

    // 9: snow
    init_pair(9, COLOR_WHITE, -1);

    // 10: garland
    init_pair(10, COLOR_RED, -1);
}

static int ornament_color_for_cell(int x, int y) {
    // Deterministic-ish "random" based on coords
    // Returns color pair id from 2..6, or 0 for none.
    unsigned int h = (unsigned int)(x * 1103515245u + y * 12345u + (x << 16) + (y << 1));
    if ((h % 13u) != 0u) return 0; // ~1/13 cells get an ornament
    return 2 + (h % 5u); // 2..6
}

static void draw_tree(int rows, int cols, int frame) {
    // Tree dimensions relative to terminal
    int tree_height = rows * 2 / 3;
    tree_height = clampi(tree_height, 10, rows - 4);

    int tree_base_width = tree_height; // roughly triangular
    tree_base_width = clampi(tree_base_width, 12, cols - 4);

    int cx = cols / 2;
    int topy = (rows - tree_height) / 2;
    int trunk_h = clampi(tree_height / 6, 2, 5);
    int trunk_w = clampi(tree_base_width / 8, 2, 6);

    // Star at the top
    int star_y = topy - 1;
    if (star_y >= 0) {
        attron(COLOR_PAIR(8) | A_BOLD);
        // blink-ish every ~10 frames
        if ((frame / 10) % 2 == 0) attron(A_STANDOUT);
        mvaddch(star_y, cx, '*');
        if ((frame / 10) % 2 == 0) attroff(A_STANDOUT);
        attroff(COLOR_PAIR(8) | A_BOLD);
    }

    // Tree body
    for (int i = 0; i < tree_height; i++) {
        int y = topy + i;
        if (y < 0 || y >= rows) continue;

        // width grows with i
        int half = (i * tree_base_width) / (2 * tree_height);
        half = clampi(half, 1, cols / 2 - 2);

        int left = cx - half;
        int right = cx + half;

        for (int x = left; x <= right; x++) {
            if (x < 0 || x >= cols) continue;

            // Edges a bit lighter/bolder
            bool edge = (x == left || x == right);

            // Optional garland: a sine wave that "wraps" down the tree
            // This is the BASIC spirit: horizontal displacement based on sin().
            // We draw a garland character only sometimes to avoid filling the tree.
            double t = (double)i / 2.5;
            int wave_x = cx + (int)lround((half * 0.75) * sin(t + (frame * 0.08)));
            bool on_garland = (abs(x - wave_x) <= 0) && ((i % 2) == 0);

            int ornament_pair = ornament_color_for_cell(x, y);
            char ch = '^';

            if (on_garland) {
                attron(COLOR_PAIR(10) | A_BOLD);
                mvaddch(y, x, '~');
                attroff(COLOR_PAIR(10) | A_BOLD);
                continue;
            }

            if (ornament_pair != 0) {
                attron(COLOR_PAIR(ornament_pair) | A_BOLD);
                mvaddch(y, x, 'o');
                attroff(COLOR_PAIR(ornament_pair) | A_BOLD);
                continue;
            }

            attron(COLOR_PAIR(1) | (edge ? A_BOLD : 0));
            mvaddch(y, x, ch);
            attroff(COLOR_PAIR(1) | (edge ? A_BOLD : 0));
        }
    }

    // Trunk
    int trunk_top = topy + tree_height;
    int trunk_left = cx - trunk_w / 2;
    int trunk_right = trunk_left + trunk_w;

    attron(COLOR_PAIR(7));
    for (int y = trunk_top; y < trunk_top + trunk_h; y++) {
        if (y < 0 || y >= rows) continue;
        for (int x = trunk_left; x <= trunk_right; x++) {
            if (x < 0 || x >= cols) continue;
            mvaddch(y, x, '#');
        }
    }
    attroff(COLOR_PAIR(7));

    // Simple base "ground"
    attron(COLOR_PAIR(9));
    int ground_y = trunk_top + trunk_h;
    if (ground_y >= 0 && ground_y < rows) {
        for (int x = 0; x < cols; x++) {
            mvaddch(ground_y, x, '_');
        }
    }
    attroff(COLOR_PAIR(9));
}

static void snow_init(Snowflake *s, int n, int rows, int cols) {
    (void)rows;
    for (int i = 0; i < n; i++) {
        s[i].x = rand() % (cols > 0 ? cols : 1);
        s[i].y = rand() % (rows > 0 ? rows : 1);
        s[i].speed = 1 + (rand() % 4); // 1..4
        s[i].phase = rand() % s[i].speed;
        int r = rand() % 6;
        s[i].ch = (r == 0) ? '.' : (r <= 2 ? '*' : '+');
    }
}

static void snow_step(Snowflake *s, int n, int rows, int cols, int frame) {
    (void)frame;
    for (int i = 0; i < n; i++) {
        // drift a tiny bit
        int drift = (rand() % 3) - 1; // -1,0,1
        s[i].phase++;
        if (s[i].phase >= s[i].speed) {
            s[i].phase = 0;
            s[i].y += 1;
            s[i].x += drift;
        }

        if (s[i].x < 0) s[i].x = cols - 1;
        if (s[i].x >= cols) s[i].x = 0;

        if (s[i].y >= rows) {
            s[i].y = 0;
            s[i].x = rand() % (cols > 0 ? cols : 1);
            s[i].speed = 1 + (rand() % 4);
            s[i].phase = rand() % s[i].speed;
            int r = rand() % 6;
            s[i].ch = (r == 0) ? '.' : (r <= 2 ? '*' : '+');
        }
    }
}

static void snow_draw(const Snowflake *s, int n, int rows, int cols) {
    attron(COLOR_PAIR(9));
    for (int i = 0; i < n; i++) {
        int x = s[i].x;
        int y = s[i].y;
        if (x >= 0 && x < cols && y >= 0 && y < rows) {
            mvaddch(y, x, s[i].ch);
        }
    }
    attroff(COLOR_PAIR(9));
}

int main(void) {
    srand((unsigned int)time(NULL));

    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    nodelay(stdscr, TRUE);   // non-blocking getch
    curs_set(0);

    if (has_colors()) {
        init_colors();
    }

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int snow_n = (rows * cols) / 80; // density
    snow_n = clampi(snow_n, 60, 600);

    Snowflake *snow = (Snowflake *)calloc((size_t)snow_n, sizeof(Snowflake));
    if (!snow) {
        endwin();
        return 1;
    }

    snow_init(snow, snow_n, rows, cols);

    int frame = 0;

    while (1) {
        int ch = getch();
        if (ch == 'q' || ch == 'Q') break;

        // handle resize: recompute sizes + re-seed snow within bounds
        int new_rows, new_cols;
        getmaxyx(stdscr, new_rows, new_cols);
        if (new_rows != rows || new_cols != cols) {
            rows = new_rows;
            cols = new_cols;

            int new_snow_n = (rows * cols) / 80;
            new_snow_n = clampi(new_snow_n, 60, 600);

            Snowflake *new_snow = (Snowflake *)realloc(snow, (size_t)new_snow_n * sizeof(Snowflake));
            if (new_snow) {
                snow = new_snow;
                snow_n = new_snow_n;
                snow_init(snow, snow_n, rows, cols);
            } else {
                // keep old snow buffer if realloc failed
            }
        }

        erase();

        draw_tree(rows, cols, frame);
        snow_step(snow, snow_n, rows, cols, frame);
        snow_draw(snow, snow_n, rows, cols);

        // little footer
        attron(A_DIM);
        mvprintw(rows - 1, 0, "xmas: press 'q' to quit  |  %dx%d", rows, cols);
        attroff(A_DIM);

        refresh();

        // ~30 FPS
        usleep(33000);
        frame++;
    }

    free(snow);
    endwin();
    return 0;
}

