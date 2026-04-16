#include "input.h"

#include <ncurses.h>
#include <string.h>

static int key_still_held(double last_seen, double now_seconds, double hold_seconds) {
    return (now_seconds - last_seen) <= hold_seconds;
}

void input_init(InputContext *context) {
    memset(context, 0, sizeof(*context));
    context->key_hold_seconds = 0.12;
    context->left_seen = -1000.0;
    context->right_seen = -1000.0;
    context->up_seen = -1000.0;
    context->down_seen = -1000.0;
}

void input_poll(InputContext *context, InputState *state, double now_seconds) {
    int ch;

    memset(state, 0, sizeof(*state));

    while ((ch = getch()) != ERR) {
        switch (ch) {
            case KEY_LEFT:
                context->left_seen = now_seconds;
                break;
            case KEY_RIGHT:
                context->right_seen = now_seconds;
                break;
            case KEY_UP:
                context->up_seen = now_seconds;
                break;
            case KEY_DOWN:
                context->down_seen = now_seconds;
                break;
            case ' ':
                state->fire = 1;
                break;
            case 'b':
            case 'B':
                state->bomb = 1;
                break;
            case 'r':
            case 'R':
                state->restart = 1;
                break;
            case 'q':
            case 'Q':
                state->quit = 1;
                break;
            default:
                break;
        }
    }

    state->left = key_still_held(context->left_seen, now_seconds, context->key_hold_seconds);
    state->right = key_still_held(context->right_seen, now_seconds, context->key_hold_seconds);
    state->up = key_still_held(context->up_seen, now_seconds, context->key_hold_seconds);
    state->down = key_still_held(context->down_seen, now_seconds, context->key_hold_seconds);
}
