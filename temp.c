#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <SDL2/SDL.h>

// #include <iostream>
typedef struct {
    SDL_Window *window;
    SDL_Renderer *renderer;
} sdl_t;

typedef struct {
    uint32_t window_width; 
    uint32_t window_height;
    uint32_t fg_color;
    uint32_t bg_color;
    uint32_t scale_factor;
    bool pixel_outlines;
} config_t;

typedef enum {
    QUIT,
    RUNNING,
    PAUSED,
} emulator_state_t;

typedef struct {
    uint16_t opcode;
    uint16_t NNN;       // 12 bit address/constant
    uint8_t NN;         // 8 bit constant
    uint8_t N;          // 4 bit constant
    uint8_t X;          // 4 bit register identifier
    uint8_t Y;          // 4 bit register identifier
} instruction_t;

typedef struct {
    emulator_state_t state;
    uint8_t ram[4096];
    bool display[64 * 32];  // Emulate original CHIP8 resolution pixels
    uint16_t stack[12];     // Subroutine stack
    uint16_t *stack_ptr;
    uint8_t V[16];          // Data registers V0-VF
    uint16_t I;             // Index register
    uint16_t PC;            // Program counter
    uint8_t delay_timer;
    uint8_t sound_timer;
    bool keypad[16];        // Hexadecimal keypad 0x0-0xF
    const char *rom_name;   // Currently running ROM
    instruction_t inst;     // Currently executing instruction
} chip8_t;

bool init_sdl(sdl_t *sdl, const config_t config) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER) != 0) {
        SDL_Log("Could not initialize SDL subsystems! %s\n", SDL_GetError());
        return false;
    }

    sdl->window = SDL_CreateWindow("CHIP8 Emulator", SDL_WINDOWPOS_CENTERED,
                                    SDL_WINDOWPOS_CENTERED,
                                    config.window_width * config.scale_factor,
                                    config.window_height * config.scale_factor,
                                    0);
    if (!sdl->window) {
        SDL_Log("Could not create SDL window %s\n", SDL_GetError());
        return false;
    }

    sdl->renderer = SDL_CreateRenderer(sdl->window, -1, SDL_RENDERER_ACCELERATED);
    if (!sdl->renderer) {
        SDL_Log("Could not create SDL renderer %s\n", SDL_GetError());
        return false;
    }

    return true;
}

bool set_config_from_args(config_t *config, const int argc, char **argv) {
    // Set defaults
    *config = (config_t) {
        .window_width = 64,
        .window_height = 32,
        .fg_color = 0xFFFFFFFF,
        .bg_color = 0x000000FF,
        .scale_factor = 20,
        .pixel_outlines = true,
    };

    for (int i = 1; i < argc; i++) {
        (void) argv[i];
        // ...
    }

    return true;
}

bool init_chip8(chip8_t *chip8,const char rom_name[]) {
    const uint32_t entry_point = 0x200; // CHIP8 ROM entry point
    const uint8_t font[] = {
        0xF0, 0x90, 0x90, 0x90, 0xF0, // 0
        0x20, 0x60, 0x20, 0x20, 0x70, // 1
        0xF0, 0x10, 0xF0, 0x80, 0xF0, // 2
        0xF0, 0x10, 0xF0, 0x10, 0xF0, // 3
        0x90, 0x90, 0xF0, 0x10, 0x10, // 4
        0xF0, 0x80, 0xF0, 0x10, 0xF0, // 5
        0xF0, 0x80, 0xF0, 0x90, 0xF0, // 6
        0xF0, 0x10, 0x20, 0x40, 0x40, // 7
        0xF0, 0x90, 0xF0, 0x90, 0xF0, // 8
        0xF0, 0x90, 0xF0, 0x10, 0xF0, // 9
        0xF0, 0x90, 0xF0, 0x90, 0x90, // A
        0xE0, 0x90, 0xE0, 0x90, 0xE0, // B
        0xF0, 0x80, 0x80, 0x80, 0xF0, // C
        0xE0, 0x90, 0x90, 0x90, 0xE0, // D
        0xF0, 0x80, 0xF0, 0x80, 0xF0, // E
        0xF0, 0x80, 0xF0, 0x80, 0x80  // F
    };

    // Load font
    memcpy(&chip8->ram[0], font, sizeof(font));

    // Load ROM
    FILE *rom = fopen(rom_name, "rb");
    if (!rom) {
        SDL_Log("ROM file %s is invalid or does not exist\n", rom_name);

        return false;
    }

    // Get/check ROM size
    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    const size_t max_size = sizeof chip8->ram - entry_point;
    rewind(rom);

    if (rom_size > max_size) {
        SDL_Log("ROM file %s is too big! ROM size: %zu, Max size allowed: %zu\n",
            rom_name, rom_size, max_size);

        return false;
    }

    // Load ROM into CHIP8 memory
    if (fread(&chip8->ram[entry_point], rom_size, 1, rom) != 1) {
        SDL_Log("Could not read ROM file %s into CHIP8 memory\n",
            rom_name);

        return false;
    }

    fclose(rom);

    // Set CHIP8 machine defaults
    chip8->state = RUNNING;
    chip8->PC = entry_point;
    chip8->rom_name = rom_name;
    chip8->stack_ptr = &chip8->stack[0];

    return true;
}

void final_cleanup(const sdl_t sdl) {
    SDL_DestroyRenderer(sdl.renderer);
    SDL_DestroyWindow(sdl.window);
    SDL_Quit(); // Shutdown SDL subsystems
}

void clear_screen(const sdl_t sdl, const config_t config) {
    const uint8_t r = (config.bg_color >> 24) & 0xFF;
    const uint8_t g = (config.bg_color >> 16) & 0xFF;
    const uint8_t b = (config.bg_color >> 8) & 0xFF;
    const uint8_t a = (config.bg_color >> 0) & 0xFF;

    SDL_SetRenderDrawColor(sdl.renderer, r, g, b, a);
    SDL_RenderClear(sdl.renderer);
}

void update_screen(const sdl_t sdl, const config_t config ,const chip8_t chip8) {
    SDL_Rect rect = {.x = 0, .y = 0, .w = config.scale_factor, .h = config.scale_factor};

    const uint8_t bg_r = (config.bg_color >> 24) & 0xFF;
    const uint8_t bg_g = (config.bg_color >> 16) & 0xFF;
    const uint8_t bg_b = (config.bg_color >> 8) & 0xFF;
    const uint8_t bg_a = (config.bg_color >> 0) & 0xFF;

    const uint8_t fg_r = (config.fg_color >> 24) & 0xFF;
    const uint8_t fg_g = (config.fg_color >> 16) & 0xFF;
    const uint8_t fg_b = (config.fg_color >> 8) & 0xFF;
    const uint8_t fg_a = (config.fg_color >> 0) & 0xFF;

    for (uint32_t i = 0; i < sizeof chip8.display; i++) {
        rect.x = (i % config.window_width) * config.scale_factor;
        rect.y = (i / config.window_width) * config.scale_factor;

        if (chip8.display[i]) {
            SDL_SetRenderDrawColor(sdl.renderer, fg_r, fg_g, fg_b, fg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);

            if (config.pixel_outlines) {
                SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
                SDL_RenderDrawRect(sdl.renderer, &rect);
            }
        } else {
            SDL_SetRenderDrawColor(sdl.renderer, bg_r, bg_g, bg_b, bg_a);
            SDL_RenderFillRect(sdl.renderer, &rect);
        }
    }

    SDL_RenderPresent(sdl.renderer);
}

void handle_input(chip8_t *chip8) {
    SDL_Event event;

    while (SDL_PollEvent(&event)) {
        switch (event.type) {
            case SDL_QUIT:
                // Exit window; End program
                chip8->state = QUIT;
                return;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        chip8->state = QUIT;
                        return;
                    case SDLK_SPACE:
                        if (chip8->state == RUNNING) {
                            chip8->state = PAUSED;
                            puts("===PAUSED===");
                        }
                        else {
                            chip8->state = RUNNING;
                        }
                        return;

                    default:
                        break;
                }

            case SDL_KEYUP:
                break;

            default:
                break;
        }
    }

}

#ifdef DEBUG
    void print_debug_info(chip8_t *chip8) {
        printf("Address: 0x%04X, Opcode: 0x%04X, Desc: ",
                chip8->PC-2, chip8->inst.opcode);
        
        switch ((chip8->inst.opcode >> 12) & 0x0F) {
            case 0x00:
                if (chip8->inst.NN == 0xE0) {
                    // 0x00E0: Clear the screen
                    printf("Clear screen\n");

                } else if (chip8->inst.NN == 0xEE) {
                    // 0x00EE: Return from subroutine
                    printf("Return from subroutine to address 0x%04X\n",
                            *(chip8->stack_ptr - 1));
                } else {
                    printf("Unimplemented Opcode\n");
                }
                break;
            
            case 0x01:
                // 0x1NNN: Jump to address NNN
                printf("Jump to address NNN (0x%02X)\n",
                        chip8->inst.NNN);
                break;

            case 0x02:
                // 0x2NNN: Call subroutine at NNN
                *chip8->stack_ptr++ = chip8->PC;
                chip8->PC = chip8->inst.NNN;
                break;

            case 0x06:
                // 0x6XNN: Set VX to NN
                printf("Set register V%X to NN (0x%02X)\n",
                        chip8->inst.X, chip8->inst.NN);
                break;
        
            case 0x07:
                // 0x7XNN: Add NN to VX
                printf("Add NN (0x%02X) to register V%X (0x%02X). Result: 0x%02X\n",
                        chip8->inst.NN, chip8->inst.X, chip8->V[chip8->inst.X],
                        chip8->V[chip8->inst.X] + chip8->inst.NN);
                break;

            case 0x0A:
                // 0xANNN: Set index register I to NNNN
                printf("Set I to NNN (0x%04X)\n",
                        chip8->inst.NNN);
                break;

            case 0x0D:
                // 0xDXYN: Draw N-height sprite at coords X, Y; Read from memory location I;
                printf("Draw N (%u) height sprite at coords V%X (0x%02X), V%X (0x%02X) "
                        "from memory location I (0x%04X). Set VF = 1 if any pixels are turned off.\n",
                        chip8->inst.N, chip8->inst.X, chip8->V[chip8->inst.X], chip8->inst.Y, 
                        chip8->V[chip8->inst.Y], chip8->I);
                break;

            default:
                printf("Unimplemented Opcode\n");
                break;
        }
    }
#endif

void DispDump(chip8_t *chip8, const config_t config) {
	for (int y = 0; y < config.window_height; ++y) {
		for (int x = 0; x < config.window_width; ++x) {
			if (chip8->display[y * config.window_width + x]) {
				printf("*");
			} else {
				printf(" ");
			}
		}
		printf("\n");
	}
}

void emulate_instruction(chip8_t *chip8, const config_t config) {
    chip8->inst.opcode = (chip8->ram[chip8->PC] << 8) | chip8->ram[chip8->PC + 1];
    chip8->PC += 2;

    // Fill out current instruction format
    chip8->inst.NNN = chip8->inst.opcode & 0x0FFF;
    chip8->inst.NN = chip8->inst.opcode & 0x0FF;
    chip8->inst.N = chip8->inst.opcode & 0x0F;
    chip8->inst.X = (chip8->inst.opcode >> 8) & 0x0F;
    chip8->inst.Y = (chip8->inst.opcode >> 4) & 0x0F;

#ifdef DEBUG
    print_debug_info(chip8);
#endif

    // Emulate opcode
    switch ((chip8->inst.opcode >> 12) & 0x0F) {
        case 0x00:
            if (chip8->inst.NN == 0xE0) {
                // 0x00E0: Clear the screen
                memset(&chip8->display[0], false, sizeof chip8->display);

            } else if (chip8->inst.NN == 0xEE) {
                // 0x00EE: Return from subroutine
                chip8->PC = *--chip8->stack_ptr;
            }
            break;

        case 0x01:
            // 0x1NNN: Jump to address NNN
            chip8->PC = chip8->inst.NNN;
            break;

        case 0x02:
            // 0x2NNN: Call subroutine at NNN
            *chip8->stack_ptr++ = chip8->PC;
            chip8->PC = chip8->inst.NNN;
            break;
        
        case 0x06:
            // 0x6XNN: Set VX to NN
            chip8->V[chip8->inst.X] = chip8->inst.NN;
            break;

        case 0x07:
            // 0x7XNN: Add NN to VX
            chip8->V[chip8->inst.X] += chip8->inst.NN;
            break;
        
        case 0x0A:
            // 0xANNN: Set index register I to NNNN
            chip8->I = chip8->inst.NNN;
            break;
        
        case 0x0D:
            // 0xDXYN: Draw N-height sprite at coords X, Y; Read from memory location I;
            uint8_t X_coord = chip8->V[chip8->inst.X] % config.window_width;
            uint8_t Y_coord = chip8->V[chip8->inst.Y] % config.window_height;

            const uint8_t orig_X = X_coord;

            chip8->V[0xF] = 0; // Initializa carry flag to 0

            for (uint8_t i = 0; i < chip8->inst.N; i++) {
                const uint8_t sprite_data = chip8->ram[chip8->I + i];
                X_coord = orig_X;

                for (int8_t j = 7; j >= 0; j--) {
                    bool *pixel = &chip8->display[Y_coord * config.window_width + X_coord];
                    const bool sprite_bit = (sprite_data & (1 << j));

                    if (sprite_bit && *pixel) {
                        chip8->V[0xF] = 1;
                    }

                    *pixel ^= sprite_bit;

                    if (++X_coord >= config.window_width)   break;
                }
                if (++Y_coord >= config.window_height)  break;
            }

            break;

        default:
            break;
    }
}

int main(int argc, char **argv) {
    // Default usage message for args
    if (argc < 2) {
        fprintf(stderr,"Usage: %s <rom_name>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    // Initialize emulator configuration/options
    config_t config = {0};
    if (!set_config_from_args(&config, argc, argv)) exit(EXIT_FAILURE);

    // Initialize SDL
    sdl_t sdl = {0};
    if (!init_sdl(&sdl, config))    exit(EXIT_FAILURE);

    // Initialize CHIP8 machine
    chip8_t chip8 = {0};
    const char *rom_name = argv[1];
    if(!init_chip8(&chip8, rom_name)) exit(EXIT_FAILURE);

    // Initial screen clear
    clear_screen(sdl, config);

    // Main emulator loop
    while (chip8.state != QUIT) {
        // Hadle user input
        handle_input(&chip8);

        if (chip8.state == PAUSED)  continue;

        // get_time()
        // Emulate CHIP8 instructions
        emulate_instruction(&chip8, config);

        // get_time() elapsed since lat get_time()

        // Delay approx 60 Hz
        // SDL_Delay(16 - time elapsed for emulation);
        SDL_Delay(16);

        // Update with changes
        update_screen(sdl, config, chip8);
    }

    // Final cleanup
    final_cleanup(sdl);

    exit(EXIT_SUCCESS);
}