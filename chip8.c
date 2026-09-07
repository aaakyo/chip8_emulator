#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>
#define SDL_MAIN_HANDLED
#include <SDL3/SDL.h>
#define SCALE 10

/*
##################################################
#                      SDL3                      #
##################################################
*/

/* SDL window and renderer*/

/*4KB memory, dont use 0x000 from 0x1FF. Most programs start at 0x200*/

uint8_t font_data[80] = {
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

int load_rom(const char *filename, uint8_t *memory)
{
    FILE *rom = fopen(filename, "rb");
    if (!rom)
    {
        printf("Failed to open ROM: %s\n", filename);
        return -1;
    }

    fseek(rom, 0, SEEK_END);
    long rom_size = ftell(rom);
    rewind(rom);

    if (rom_size > (4096 - 0x200))
    {
        printf("ROM too large to fit in memory\n");
        fclose(rom);
        return -1;
    }

    size_t bytes_read = fread(&memory[0x200], 1, rom_size, rom);
    fclose(rom);

    if (bytes_read != (size_t)rom_size)
    {
        printf("Failed to read entire ROM\n");
        return -1;
    }

    return 0;
}

void render_display(SDL_Renderer *renderer, uint8_t display[64][32])
{
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, SDL_ALPHA_OPAQUE);
    SDL_RenderClear(renderer);

    SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
    for (int x = 0; x < 64; x++)
    {
        for (int y = 0; y < 32; y++)
        {
            if (display[x][y])
            {
                SDL_FRect rect = {x * SCALE, y * SCALE, SCALE, SCALE};
                SDL_RenderFillRect(renderer, &rect);
            }
        }
    }

    SDL_RenderPresent(renderer);
}

int main()
{
    FILE *log = fopen("trace.log", "w");
    SetConsoleOutputCP(CP_UTF8);
    uint8_t memory[4096] = {0};
    memcpy(&memory[0x50], font_data, sizeof(font_data));
    uint16_t pc = 0x200;

    uint16_t stack[16] = {0};
    uint8_t sp = 0;

    /*registers: v0 to vF. vF is used as a flag by some instructions.
    I register is to store memory addresses so only the 12
    lowest rightmost bits are used.*/
    uint8_t V[16] = {0};
    uint16_t i = 0;

    /*Display is a 64x32 pixel monochrome display*/
    uint8_t display[64][32] = {0};

    /*Delay and sound registers*/
    uint8_t dt = 0;
    uint8_t st = 0;

    /* CHIP8's keypad */
    bool keypad[16] = {0};

    if (load_rom("6-keypad.ch8", memory) != 0)
    {
        return 1;
    }

    uint16_t stuck_inst = (memory[0x2E0] << 8) | memory[0x2E0 + 1];
    printf("Instruction at 0x2E0: %04X\n", stuck_inst);

    bool draw_flag = true;

    SDL_Init(SDL_INIT_VIDEO);
    SDL_Event event;

    static SDL_Window *window = NULL;
    static SDL_Renderer *renderer = NULL;

    if (!SDL_CreateWindowAndRenderer("CHIP8", 640, 320, SDL_WINDOW_MAXIMIZED, &window, &renderer))
    {
        SDL_Log("Couldn't create window/renderer: %s", SDL_GetError());
        return 1;
    }

    Uint64 last_timer_update = SDL_GetTicks();
    const int TIMER_INTERVAL_MS = 1000 / 60;

    Uint64 last_instruction_time = SDL_GetTicks();
    const int INSTRUCTION_INTERVAL_MS = 1000 / 600;

    bool running = true;
    while (running)
    {
        /*
        ##############################
        #       Poll SDL events      #
        ##############################
        */
        while (SDL_PollEvent(&event))
        {

            if (event.type == SDL_EVENT_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_EVENT_KEY_DOWN)
            {
                switch (event.key.key)
                {
                case SDLK_1:
                    keypad[0x1] = true;
                    break;
                case SDLK_2:
                    keypad[0x2] = true;
                    break;
                case SDLK_3:
                    keypad[0x3] = true;
                    break;
                case SDLK_4:
                    keypad[0xC] = true;
                    break;
                case SDLK_Q:
                    keypad[0x4] = true;
                    break;
                case SDLK_W:
                    keypad[0x5] = true;
                    break;
                case SDLK_E:
                    keypad[0x6] = true;
                    break;
                case SDLK_R:
                    keypad[0xD] = true;
                    break;
                case SDLK_A:
                    keypad[0x7] = true;
                    break;
                case SDLK_S:
                    keypad[0x8] = true;
                    break;
                case SDLK_D:
                    keypad[0x9] = true;
                    break;
                case SDLK_F:
                    keypad[0xE] = true;
                    break;
                case SDLK_Z:
                    keypad[0xA] = true;
                    break;
                case SDLK_X:
                    keypad[0x0] = true;
                    break;
                case SDLK_C:
                    keypad[0xB] = true;
                    break;
                case SDLK_V:
                    keypad[0xF] = true;
                    break;
                default:
                    break;
                }
            }
            else if (event.type == SDL_EVENT_KEY_UP)
            {
                switch (event.key.key)
                {
                case SDLK_1:
                    keypad[0x1] = false;
                    break;
                case SDLK_2:
                    keypad[0x2] = false;
                    break;
                case SDLK_3:
                    keypad[0x3] = false;
                    break;
                case SDLK_4:
                    keypad[0xC] = false;
                    break;
                case SDLK_Q:
                    keypad[0x4] = false;
                    break;
                case SDLK_W:
                    keypad[0x5] = false;
                    break;
                case SDLK_E:
                    keypad[0x6] = false;
                    break;
                case SDLK_R:
                    keypad[0xD] = false;
                    break;
                case SDLK_A:
                    keypad[0x7] = false;
                    break;
                case SDLK_S:
                    keypad[0x8] = false;
                    break;
                case SDLK_D:
                    keypad[0x9] = false;
                    break;
                case SDLK_F:
                    keypad[0xE] = false;
                    break;
                case SDLK_Z:
                    keypad[0xA] = false;
                    break;
                case SDLK_X:
                    keypad[0x0] = false;
                    break;
                case SDLK_C:
                    keypad[0xB] = false;
                    break;
                case SDLK_V:
                    keypad[0xF] = false;
                    break;
                default:
                    break;
                }
            }
        }

        Uint64 now = SDL_GetTicks();
        if (now - last_instruction_time >= INSTRUCTION_INTERVAL_MS)
        {

            /*
            ##############################
            # FETCH-DECODE-EXECUTE LOGIC #
            ##############################
            */

            uint16_t inst = (memory[pc] << 8) | memory[pc + 1];

            fprintf(log, "pc=%03X inst=%04X I=%03X\n", pc, inst, i);

            switch (inst & 0xF000)
            {
            case (0x0000):
                if ((inst & 0x00FF) == 0x00E0)
                {
                    memset(display, 0, sizeof(display));
                    pc += 2;
                    draw_flag = true;
                }
                else if ((inst & 0x00FF) == 0x00EE)
                {
                    sp--;
                    pc = stack[sp];
                }
                break;
            case (0x1000): // 1NNN
                pc = inst & 0x0FFF;
                break;
            case (0x2000): // 2NNN
                stack[sp] = pc + 2;
                sp += 1;
                pc = inst & 0x0FFF;
                break;
            case (0x6000): // 6XNN
                uint8_t value = inst & 0x00FF;
                uint8_t reg = (inst >> 8) & 0x000F;
                V[reg] = value;
                pc += 2;
                break;
            case (0x3000): // 3XNN, Skip next instruction if Vx = NN.
                uint8_t value3 = inst & 0x00FF;
                uint8_t reg3 = (inst >> 8) & 0x000F;
                if (V[reg3] == value3)
                {
                    pc += 4;
                }
                else
                {
                    pc += 2;
                }
                break;
            case (0x4000):
                uint8_t value4 = inst & 0x00FF;
                uint8_t reg4 = (inst >> 8) & 0x000F;
                if (V[reg4] != value4)
                {
                    pc += 4;
                }
                else
                {
                    pc += 2;
                }
                break;
            case (0x5000): // 5xy0
                uint8_t reg5x = (inst >> 8) & 0x000F;
                uint8_t reg5y = (inst >> 4) & 0x000F;
                if (V[reg5x] == V[reg5y])
                {
                    pc += 4;
                }
                else
                {
                    pc += 2;
                }
                break;
            case (0x7000):
                uint8_t value7 = inst & 0x00FF;
                uint8_t reg7 = (inst >> 8) & 0x000F;
                V[reg7] += value7;
                pc += 2;
                break;
            case (0x8000): // 8xy?
                uint8_t reg8x = (inst >> 8) & 0x000F;
                uint8_t reg8y = (inst >> 4) & 0x000F;
                switch (inst & 0x000F)
                {
                case (0x0):
                    V[reg8x] = V[reg8y];
                    break;
                case (0x1):
                    V[reg8x] = V[reg8x] | V[reg8y];
                    break;
                case (0x2):
                    V[reg8x] = V[reg8x] & V[reg8y];
                    break;
                case (0x3):
                    V[reg8x] = V[reg8x] ^ V[reg8y];
                    break;
                case (0x4):
                    uint16_t sum = V[reg8x] + V[reg8y];
                    V[0xF] = (sum > 255) ? 1 : 0;
                    V[reg8x] = sum & 0xFF;
                    break;
                case (0x5):
                    if (V[reg8x] > V[reg8y])
                    {
                        V[0xF] = 1;
                    }
                    else
                    {
                        V[0xF] = 0;
                    }
                    V[reg8x] = V[reg8x] - V[reg8y];
                    break;
                case (0x6):
                    if ((V[reg8x] & 0x1) == 1)
                    {
                        V[0xF] = 1;
                    }
                    else
                    {
                        V[0xF] = 0;
                    }
                    V[reg8x] = V[reg8x] >> 1;
                    break;
                case (0x7):
                    if (V[reg8y] > V[reg8x])
                    {
                        V[0xF] = 1;
                    }
                    else
                    {
                        V[0xF] = 0;
                    }
                    V[reg8x] = V[reg8y] - V[reg8x];
                    break;
                case (0xE):
                    if ((V[reg8x] & 0x80) == 1)
                    {
                        V[0xF] = 1;
                    }
                    else
                    {
                        V[0xF] = 0;
                    }
                    V[reg8x] = V[reg8x] << 1;
                    break;
                default:
                    break;
                }
                pc += 2;
                break;
            case (0x9000):
                uint8_t reg9x = (inst >> 8) & 0x000F;
                uint8_t reg9y = (inst >> 4) & 0x000F;
                if (V[reg9x] != V[reg9y])
                {
                    pc += 4;
                }
                else
                {
                    pc += 2;
                }
                break;
            case (0xA000): // ANNN
                uint16_t valueA = inst & 0x0FFF;
                i = valueA;
                pc += 2;
                break;
            case (0xB000): //BNNN
                pc = (inst & 0x0FFF) + V[0];
                break;
            case (0xC000): // CXNN
                uint8_t regC = (inst >> 8) & 0x000F;
                uint8_t valueC = inst & 0x00FF;
                V[regC] = rand() & valueC;
                pc += 2;
                break;
            case (0xD000): // DXYN
                // Extract variable
                uint8_t x = V[(inst >> 8) & 0x000F] % 64;
                uint8_t y = V[(inst >> 4) & 0x000F] % 32;
                uint8_t n = inst & 0x000F;

                // Reset collision flag
                V[0xF] = 0;

                for (int row = 0; row < n; row++)
                {
                    uint8_t sprite_byte = memory[i + row];

                    for (int col = 0; col < 8; col++)
                    {
                        uint8_t sprite_pixel = (sprite_byte >> (7 - col)) & 0x1;

                        int px = x + col;
                        int py = y + row;

                        if (px >= 64 || py >= 32)
                            continue;

                        if (sprite_pixel)
                        {
                            if (display[px][py] == 1)
                            {
                                V[0xF] = 1;
                            }
                            display[px][py] ^= 1;
                        }
                    }
                }
                draw_flag = true;
                pc += 2;
                break;
            case (0xE000): // EXNN
                uint8_t regE = (inst >> 8) & 0x000F;
                switch (inst & 0x00FF)
                {
                case 0x9E:
                    if (keypad[regE])
                    {
                        pc += 2;
                    }
                    pc += 2;
                    break;
                case 0xA1:
                    if (!keypad[regE])
                    {
                        pc += 2;
                    }
                    pc += 2;
                    break;
                default:
                    pc += 2;
                    break;
                }
                break;
            case (0xF000):
                uint8_t regF = (inst >> 8) & 0x000F;
                switch (inst & 0x00FF)
                {
                case 0x29:
                    i = 0x50 + (V[regF] * 5);
                    pc += 2;
                    break;
                case 0x33:
                    uint8_t val = V[regF];
                    memory[i] = val / 100;
                    memory[i + 1] = (val / 10) % 10;
                    memory[i + 2] = val % 10;
                    pc += 2;
                    break;
                case 0x55:
                    for (int idx = 0; idx <= regF; idx++)
                    {
                        memory[i + idx] = V[idx];
                    }
                    pc += 2;
                    break;
                case 0x65:
                    for (int idx = 0; idx <= regF; idx++)
                    {
                        V[idx] = memory[i + idx];
                    }
                    pc += 2;
                    break;
                case 0x0A:
                    bool key_found = false;
                    for (int idx = 0; idx < 16 && !key_found; idx++)
                    {
                        if (keypad[idx])
                        {
                            V[regF] = idx;
                            key_found = true;
                        }

                    }
                    if (key_found) pc += 2;
                    break;
                case 0x07:
                    V[regF] = dt;
                    pc += 2;
                    break;
                case 0x15:
                    dt = V[regF];
                    pc += 2;
                    break;
                case 0x18:
                    st = V[regF];
                    pc += 2;
                    break;
                case 0x1E:
                    i += V[regF];
                    pc = pc + 2;
                    break;
                default:
                    pc += 2;
                    break;
                }
                break;
            default:
                pc += 2;
                break;
            }

            last_instruction_time = now;
        }

        /*
        ##########################
        #      Timer update      #
        ##########################
        */

        Uint64 current_time = SDL_GetTicks();
        if (current_time - last_timer_update >= TIMER_INTERVAL_MS)
        {
            if (dt > 0)
                dt--;
            if (st > 0)
                st--;
            last_timer_update = current_time;
        }

        /*
        ##########################
        #        Rendering       #
        ##########################
        */

        if (draw_flag)
        {
            render_display(renderer, display);
            draw_flag = false;
        }
    }

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}
