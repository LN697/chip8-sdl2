#include "std_CommonIncludes.h"
#include "std_Chip8Includes.h"
#include "std_Display.h"

long int cycle = 0;

uint8_t	frame_buffer[DISP_HEIGHT * DISP_WIDTH] {};
uint8_t memory[4 * ONE_K] {0};

uint8_t delay_timer{};
uint8_t sound_timer{};

bool LoadROM(int argc, char **argv, uint8_t* memory) {
	if (argc < 2) {
        nDebug::LogInfo("Usage: <rom_name>");
		return false;
    }

    const char *rom_name = argv[1];
    FILE *rom = fopen(rom_name, "rb");
    if (!rom) {
        nDebug::LogInfo("Error: Unable to open ROM file");
		return false;
    }

    fseek(rom, 0, SEEK_END);
    const size_t rom_size = ftell(rom);
    rewind(rom);
    fread(&memory[ROM_ENTRYPOINT], 1, rom_size, rom);
    fclose(rom);
    nDebug::LogInfo("Successfully loaded ROM!");

	return true;
}

struct sRegister {
	uint16_t    PC{};
	uint8_t     V[0x10] {};
	uint16_t    I{};

	void PrintRegisters () {
		nDebug::LogValue("PC", PC);
		nDebug::LogValue("I", I);
		for (int i = 0; i < 0x10; ++i) {
			nDebug::LogValue(nDebug::ConvertToString("V[", i, "]"), V[i]);
		}
		nDebug::LogInfo("");
	}
};

class cCPU  {
	private:
		sRegister*  _reg{};
		uint8_t*    _mem{};
		uint8_t*    _delay{};
		uint8_t*    _sound{};
		uint8_t*    _disp{};
		uint16_t    instr{};
		uint16_t    NNN{};
		uint8_t     NN{};
		uint8_t     N{};
		uint8_t     X{};
		uint8_t     Y{};

		bool state = true;
		bool keypad[16]{};
		bool key_pressed = false;

		uint16_t 	stack[12];
    	uint16_t* 	stack_ptr;

		uint8_t X_coord, Y_coord, orig_X, randNum;
	public:
		cCPU () {};

		cCPU (sRegister &reg, uint8_t* mem, uint8_t &delay, uint8_t &sound, uint8_t* disp) : _reg(&reg), _mem(mem), _delay(&delay), _sound(&sound), _disp(disp) {
			stack_ptr = stack;
			srand(time(0));
		}

		void InitToRom () {
			_reg->PC = ROM_ENTRYPOINT;
		}

		void LoadFontToMem () {
			uint8_t font[80] = {
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

			for (int i = 0x50; i < 0xA0; ++i) {
				_mem[i] = font[i - 0x50];
			}
		}

		void DisplayFont () {
			for (int character = 0; character < 16; character++) {
				for (int row = 0; row < 5; row++) {
					unsigned char byte = _mem[0x50 + character * 5 + row];
					for (int bit = 7; bit >= 4; bit--) {
						if ((byte >> bit) & 1) {
							std::cout << "*";
						} else {
							std::cout << " ";
						}
					}
					std::cout << "\n";
				}
				std::cout << "\n";
			}
		}

		void Fetch () { // BIG_ENDIAN
			instr = _mem[_reg->PC] << 8;
			instr |= _mem[_reg->PC + 1];
			_reg->PC += 2;
		}

		void Decode () {
			NNN = instr & 0x0FFF;
			NN = NNN & 0x00FF;
			N = NN & 0x0F;
			X = (NNN >> 8) & 0x0F;
			Y = (NNN >> 4) & 0x0F;
		}

		void Execute () {
			switch (instr >> 12) {
				case (0x0):
					if (NNN == 0x00E0) {
						#ifdef DEBUG
							nDebug::LogInfo("Clearing Screen");
						#endif
						std::memset(&_disp[0], false, sizeof *_disp);
						break;
					}
					if (NNN == 0x00EE) {
						#ifdef DEBUG
							nDebug::LogInfo("Pop from stack");
						#endif
						_reg->PC = *--stack_ptr;
						break;
					}
					#ifdef DEBUG
						nDebug::LogInfo("Operation not implemented");
						nDebug::LogValue("Instruction", instr);
					#endif
					break;
				case (0x1):
					#ifdef DEBUG
						nDebug::LogInfo("Jumping to ", NNN);
					#endif
					_reg->PC = NNN;
					break;
				case (0x2):
					#ifdef DEBUG
						nDebug::LogInfo("Push to stack");
					#endif
					*stack_ptr++ = _reg->PC;
					_reg->PC = NNN;
					break;
				case (0x3):
					#ifdef DEBUG
						nDebug::LogInfo("If V[", X,"] == ", NN, " skip instruction");
					#endif
					if(_reg->V[X] == NN) {
						_reg->PC += 2;
					}
					break;
				case (0x4):
					#ifdef DEBUG
						nDebug::LogInfo("If V[", X,"] != ", NN, " skip instruction");
					#endif
					if(_reg->V[X] != NN) {
						_reg->PC += 2;
					}
					break;
				case (0x5):
					#ifdef DEBUG
						nDebug::LogInfo("If V[", X,"] == V[", Y, "] skip instruction");
					#endif
					if(_reg->V[X] == _reg->V[Y]) {
						_reg->PC += 2;
					}
					break;
				case (0x6):
					#ifdef DEBUG
						nDebug::LogInfo("Setting V[", X,"] to ", NN);
					#endif
					_reg->V[X] = NN;
					break;
				case (0x7):
					#ifdef DEBUG
						nDebug::LogInfo("Adding ", NN," to V[", X,"]");
					#endif
					_reg->V[X] += NN;
					break;
				case (0x8):
					switch (N) {
						case (0x0):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] = V[", Y,"]");
							#endif
							_reg->V[X] = _reg->V[Y];
							break;
						case (0x1):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] |= V[", Y,"]");
							#endif
							_reg->V[X] |= _reg->V[Y];
							break;
						case (0x2):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] &= V[", Y,"]");
							#endif
							_reg->V[X] &= _reg->V[Y];
							break;
						case (0x3):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] XOR= V[", Y,"]");
							#endif
							_reg->V[X] ^= _reg->V[Y];
							break;
						case (0x4):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] += V[", Y,"]");
							#endif
							_reg->V[X] += _reg->V[Y];
							if (_reg->V[X] < _reg->V[Y]) {
								_reg->V[0xF] = 0x1;
							}
							break;
						case (0x5):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] -= V[", Y,"]");
							#endif
							_reg->V[X] -= _reg->V[Y];
							if (_reg->V[X] > _reg->V[Y]) {
								_reg->V[0xF] = 0x1;
							} else {
								_reg->V[0xF] = 0x0;
							}
							break;
						case (0x6):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] to V[", Y,"] and shift right by 1");
							#endif
							_reg->V[X] = _reg->V[Y];
							_reg->V[0xF] = _reg->V[X] >> 7;
							_reg->V[X] <<= 1;
							break;
						case (0x7):
							#ifdef DEBUG
								nDebug::LogInfo("Set -V[", X,"] -= V[", Y,"]");
							#endif
							_reg->V[X] = _reg->V[Y] - _reg->V[X];
							if (_reg->V[Y] > _reg->V[X]) {
								_reg->V[0xF] = 0x1;
							} else {
								_reg->V[0xF] = 0x0;
							}
							break;
						case (0xE):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] to V[", Y,"] and shift left by 1");
							#endif
							_reg->V[X] = _reg->V[Y];
							_reg->V[0xF] = _reg->V[X] & 0x01;
							_reg->V[X] >>= 1;
							break;
					}	
					break;
				case (0x9):
					#ifdef DEBUG
						nDebug::LogInfo("If V[", X,"] != V[", Y, "] skip instruction");
					#endif
					if(_reg->V[X] != _reg->V[Y]) {
						_reg->PC += 2;
					}
					break;
				case (0xA):
					#ifdef DEBUG
						nDebug::LogInfo("Settting I to ", NNN);
					#endif
					_reg->I = NNN;
					break;
				case (0xC):
					#ifdef DEBUG
						nDebug::LogInfo("Store random value in V[", X, "] binary ANDed with ", NN);
					#endif
					randNum = rand() % 0xFF;
					randNum &= NN;
					_reg->V[X] = randNum;
					break;
				case (0xD):
					#ifdef DEBUG
						nDebug::LogInfo("Drawing sprites");
					#endif			
					X_coord = _reg->V[X] % DISP_WIDTH;
					Y_coord = _reg->V[Y] % DISP_HEIGHT;
					orig_X = X_coord;
					_reg->V[0xF] = 0;
					for (uint8_t i = 0; i < N; i++) {
						const uint8_t sprite_data = _mem[_reg->I + i];
						X_coord = orig_X;
						for (int8_t j = 7; j >= 0; j--) {
							uint8_t *pixel = &_disp[Y_coord * DISP_WIDTH + X_coord];
							const bool sprite_bit = (sprite_data & (1 << j));
							if (sprite_bit && *pixel) {
								_reg->V[0xF] = 1;
							}
							*pixel ^= sprite_bit;
							if (++X_coord >= DISP_WIDTH)   break;
						}
						if (++Y_coord >= DISP_HEIGHT)  break;
					}
					break;
				case (0xE):
					if (NN == 0x9E) {
						#ifdef DEBUG
							nDebug::LogInfo("Skip next instruction if key V[", X, "] is pressed");
						#endif
						if (keypad[_reg->V[X]]) {
							_reg->PC += 2;
						}
					} else if (NN == 0xA1) {
						#ifdef DEBUG
							nDebug::LogInfo("Skip next instruction if key V[", X, "] is not pressed");
						#endif
						if (!keypad[_reg->V[X]]) {
							_reg->PC += 2;
						}
					} else {
						#ifdef DEBUG
							nDebug::LogInfo("Operation not implemented");
							nDebug::LogValue("Instruction", instr);
						#endif
					}
					break;
				case (0xF):
					switch (NN) {
						case (0x1E):
							#ifdef DEBUG
								nDebug::LogInfo("Add V[", X,"] to I ", _reg->I);
							#endif
							_reg->I += _reg->V[X];
							if (_reg->I > 0x1000) {
								_reg->V[0xF] = 0x1;
							}
							break;
						case (0x0A):
							#ifdef DEBUG
								nDebug::LogInfo("Wait for key press and store in V[", X, "]");
							#endif
							
							for (int i = 0; i < 16; ++i) {
								if (keypad[i]) {
									_reg->V[X] = i;
									key_pressed = true;
									#ifdef DEBUG
										nDebug::LogInfo("Key pressed: ", i);
									#endif
									break;
								}
							}
							if (!key_pressed) {
								_reg->PC -= 2;
							}
							break;
						case (0x07):
							#ifdef DEBUG
								nDebug::LogInfo("Set V[", X,"] to delay timer value");
							#endif
							_reg->V[X] = *_delay;
							break;
						case (0x15):
							#ifdef DEBUG
								nDebug::LogInfo("Set delay timer to V[", X,"]");
							#endif
							*_delay = _reg->V[X];
							break;
						case (0x18):
							#ifdef DEBUG
								nDebug::LogInfo("Set sound timer to V[", X,"]");
							#endif
							*_sound = _reg->V[X];
							break;
						case (0x29):
							#ifdef DEBUG
								nDebug::LogInfo("Set I to the location of the sprite for digit V[", X,"]");
							#endif
							_reg->I = _reg->V[X] * 5 + 0x50;
							break;
						case (0x33):
							#ifdef DEBUG
								nDebug::LogInfo("Store BCD of V[", X,"] in memory locations I, I+1, I+2");
							#endif
							_mem[_reg->I] = _reg->V[X] / 100;
							_mem[_reg->I + 1] = (_reg->V[X] / 10) % 10;
							_mem[_reg->I + 2] = _reg->V[X] % 10;
							break;
						case (0x55):
							#ifdef DEBUG
								nDebug::LogInfo("Store registers V[0] to V[", X,"] in memory starting at I");
							#endif
							for (int i = 0; i <= X; ++i) {
								_mem[_reg->I + i] = _reg->V[i];
							}
							break;
						case (0x65):
							#ifdef DEBUG
								nDebug::LogInfo("Fill registers V[0] to V[", X,"] with values from memory starting at I");
							#endif
							for (int i = 0; i <= X; ++i) {
								_reg->V[i] = _mem[_reg->I + i];
							}
							break;
					}
					break;
				default:
					#ifdef DEBUG
						nDebug::LogInfo("Operation not implemented");
						nDebug::LogValue("Instruction", instr);
					#endif
					break;
			}
		}
		
		void HandleInputs() {
			const uint8_t key_map[16] = {SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_3, SDL_SCANCODE_4,
										  SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_E, SDL_SCANCODE_R,
										  SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_D, SDL_SCANCODE_F,
										  SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_C, SDL_SCANCODE_V};
			for (int i = 0; i < 16; ++i) {
				keypad[i] = (SDL_GetKeyboardState(NULL)[key_map[i]] != 0);
			}
		}
		void HandleTimers() {
			if (*_delay > 0) {
				--(*_delay);
			}
			if (*_sound > 0) {
				if (*_sound == 1) {
					nDebug::LogInfo("BEEP!");
				}
				--(*_sound);
			}
		}

		void Run () {
			GetState();
			if (!state) {
				return;
			}
			#ifdef DEBUG
				nDebug::LogInfo("Step: ", cycle);
			#endif

			key_pressed = false;
			Fetch();			
			Decode();
			Execute();
			cycle++;

			#ifdef DEBUG
				nDebug::LogInfo("");
			#endif
		}

		void PrintRegisters () {
			nDebug::LogValue("Instr", instr);
			nDebug::LogValue("NNN", NNN);
			nDebug::LogValue("NN", NN);
			nDebug::LogValue("N", N);
			nDebug::LogValue("X", X);
			nDebug::LogValue("Y", Y);
			nDebug::LogInfo("");

			_reg->PrintRegisters();
		}

		void MemDump () {
			for (int i = 0; i < 4 * ONE_K; ++i) {
				if ((i % 16) == 0) {
					std::cout << std::setfill('0') << std::setw(4) << i <<": ";
					for (int j = 0; j < 16; ++j) {
						std::cout << std::setfill('0') << std::setw(2) << std::hex << static_cast<int> (_mem[i + j]) << " ";
					}
					std::cout << std::endl;
				}
			}
		}

		// void DispDump() {
		// 	for (int y = 0; y < DISP_HEIGHT; ++y) {
		// 		for (int x = 0; x < DISP_WIDTH; ++x) {
		// 			if (_disp[y * DISP_WIDTH + x]) {
		// 				std::cout << "*";
		// 			} else {
		// 				std::cout << " ";
		// 			}
		// 		}
		// 		std::cout << std::endl;
		// 	}
		// }

		void SetState (bool state) {
			this->state = state;
		}
		bool GetState () {
			return state;
		}

};

int main(int argc, char **argv) {
    if (!LoadROM(argc, argv, memory)) {
		nDebug::LogInfo("Found an error while loading memory from ROM");

		return -1;
	}

	sRegister	reg;
	sSDL		sdl;

	cCPU cpu(reg, memory, delay_timer, sound_timer, frame_buffer);
	cSDL sdl_ctl(sdl.dispWindow, sdl.dispRenderer);

	cpu.InitToRom();

	sdl_ctl.InitSDL();
	bool quit = false;

    SDL_Event e;
	while (!quit) {
		while (SDL_PollEvent(&e) != 0) {
	        if (e.type == SDL_QUIT) {
	            quit = true;
	        }
			if (e.type == SDL_KEYDOWN) {
				if (e.key.keysym.sym == SDLK_ESCAPE) {
					quit = true;
				}
				if (e.key.keysym.sym == SDLK_SPACE) {
					cpu.SetState(!cpu.GetState());
					if (cpu.GetState()) {
						nDebug::LogInfo("Resuming CPU execution...");
					} else {
						nDebug::LogInfo("Pausing CPU execution...");
					}
				}
			}

	    }
		const uint64_t start_frame = SDL_GetPerformanceCounter();
		for (int i = 0; i < INST_PER_SEC; ++i) {
			cpu.Run();
		}
		const uint64_t end_frame = SDL_GetPerformanceCounter();
		const double elapsed = (double) ((end_frame - start_frame) / 1000) / SDL_GetPerformanceFrequency();
		if (elapsed < DELAY_MS) {
			SDL_Delay(DELAY_MS - elapsed);
		}
		sdl_ctl.UpdateFrame(frame_buffer);
		cpu.HandleInputs();
		cpu.HandleTimers();
	}

	sdl_ctl.QuitSDL();

	nDebug::LogInfo("Dumping Memory...");
	cpu.MemDump();

	return 0;
}
