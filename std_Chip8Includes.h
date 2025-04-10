#pragma once

#ifndef CHIP8Includes
#define CHIP8Includes

// #define DEBUG

#define DISP_HEIGHT 32
#define DISP_WIDTH  64
#define DISP_FACTOR 20

#define OUTLINES    true
#define DELAY_MS    16.67f
#define INST_PER_SEC 700

#define FG_COLOR    0xffffffff
#define BG_COLOR    0x000000ff
#define LERP_RATE   0.7f

#define ROM_ENTRYPOINT  0x200

#endif