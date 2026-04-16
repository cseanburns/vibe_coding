#ifndef INPUT_H
#define INPUT_H

#include "game.h"

typedef struct {
    double key_hold_seconds;
    double left_seen;
    double right_seen;
    double up_seen;
    double down_seen;
} InputContext;

void input_init(InputContext *context);
void input_poll(InputContext *context, InputState *state, double now_seconds);

#endif
