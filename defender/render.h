#ifndef RENDER_H
#define RENDER_H

#include "game.h"

void render_init_graphics(void);
void render_game(const GameState *game, int term_w, int term_h);

#endif
