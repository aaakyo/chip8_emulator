#include "chip8.h"
#include <string.h>
#include <stdlib.h>

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



void chip8_init(Chip8* c8)
{
    memset(c8->memory, 0, sizeof(c8->memory));
    memcpy(&c8->memory[0x50], font_data, sizeof(font_data));

    memset(c8->V, 0, sizeof(c8->V));
    c8->i = 0;
    c8->pc = 0x200;

    memset(c8->stack, 0, sizeof(c8->stack));
    c8->sp = 0;

    c8->dt = 0;
    c8->st = 0;

    memset(c8->keypad, 0, sizeof(c8->keypad));
    memset(c8->keypad_prev, 0, sizeof(c8->keypad_prev));
    memset(c8->display, 0, sizeof(c8->display));

    c8->draw_flag = true;
}

int chip8_load_rom(Chip8* c8, const char *filename)
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

    size_t bytes_read = fread(&c8->memory[0x200], 1, rom_size, rom);
    fclose(rom);

    if (bytes_read != (size_t)rom_size)
    {
        printf("Failed to read entire ROM\n");
        return -1;
    }

    return 0;
}
void chip8_cycle(Chip8* c8, FILE* log)
{
    uint16_t inst = (c8->memory[c8->pc] << 8) | c8->memory[c8->pc + 1];

            fprintf(log, "c8->pc=%03X inst=%04X c8->i=%03X\n", c8->pc, inst, c8->i);

            switch (inst & 0xF000)
            {
            case (0x0000):
                if ((inst & 0x00FF) == 0x00E0)
                {
                    memset(c8->display, 0, sizeof(c8->display));
                    c8->pc += 2;
                    c8->draw_flag = true;
                }
                else if ((inst & 0x00FF) == 0x00EE)
                {
                    c8->sp--;
                    c8->pc = c8->stack[c8->sp];
                }
                break;
            case (0x1000): // 1NNN
                c8->pc = inst & 0x0FFF;
                break;
            case (0x2000): // 2NNN
                c8->stack[c8->sp] = c8->pc + 2;
                c8->sp += 1;
                c8->pc = inst & 0x0FFF;
                break;
            case (0x6000): // 6XNN
                uint8_t value = inst & 0x00FF;
                uint8_t reg = (inst >> 8) & 0x000F;
                c8->V[reg] = value;
                c8->pc += 2;
                break;
            case (0x3000): // 3XNN, Skip next instruction if Vx = NN.
                uint8_t value3 = inst & 0x00FF;
                uint8_t reg3 = (inst >> 8) & 0x000F;
                if (c8->V[reg3] == value3)
                {
                    c8->pc += 4;
                }
                else
                {
                    c8->pc += 2;
                }
                break;
            case (0x4000):
                uint8_t value4 = inst & 0x00FF;
                uint8_t reg4 = (inst >> 8) & 0x000F;
                if (c8->V[reg4] != value4)
                {
                    c8->pc += 4;
                }
                else
                {
                    c8->pc += 2;
                }
                break;
            case (0x5000): // 5xy0
                uint8_t reg5x = (inst >> 8) & 0x000F;
                uint8_t reg5y = (inst >> 4) & 0x000F;
                if (c8->V[reg5x] == c8->V[reg5y])
                {
                    c8->pc += 4;
                }
                else
                {
                    c8->pc += 2;
                }
                break;
            case (0x7000):
                uint8_t value7 = inst & 0x00FF;
                uint8_t reg7 = (inst >> 8) & 0x000F;
                c8->V[reg7] += value7;
                c8->pc += 2;
                break;
            case (0x8000): // 8xy?
                uint8_t reg8x = (inst >> 8) & 0x000F;
                uint8_t reg8y = (inst >> 4) & 0x000F;
                switch (inst & 0x000F)
                {
                case (0x0):
                    c8->V[reg8x] = c8->V[reg8y];
                    break;
                case (0x1):
                    c8->V[reg8x] = c8->V[reg8x] | c8->V[reg8y];
                    break;
                case (0x2):
                    c8->V[reg8x] = c8->V[reg8x] & c8->V[reg8y];
                    break;
                case (0x3):
                    c8->V[reg8x] = c8->V[reg8x] ^ c8->V[reg8y];
                    break;
                case (0x4):
                    uint16_t sum = c8->V[reg8x] + c8->V[reg8y];
                    c8->V[0xF] = (sum > 255) ? 1 : 0;
                    c8->V[reg8x] = sum & 0xFF;
                    break;
                case (0x5):
                    if (c8->V[reg8x] > c8->V[reg8y])
                    {
                        c8->V[0xF] = 1;
                    }
                    else
                    {
                        c8->V[0xF] = 0;
                    }
                    c8->V[reg8x] = c8->V[reg8x] - c8->V[reg8y];
                    break;
                case (0x6):
                    if ((c8->V[reg8x] & 0x1) == 1)
                    {
                        c8->V[0xF] = 1;
                    }
                    else
                    {
                        c8->V[0xF] = 0;
                    }
                    c8->V[reg8x] = c8->V[reg8x] >> 1;
                    break;
                case (0x7):
                    if (c8->V[reg8y] > c8->V[reg8x])
                    {
                        c8->V[0xF] = 1;
                    }
                    else
                    {
                        c8->V[0xF] = 0;
                    }
                    c8->V[reg8x] = c8->V[reg8y] - c8->V[reg8x];
                    break;
                case (0xE):
                    if ((c8->V[reg8x] & 0x80) == 1)
                    {
                        c8->V[0xF] = 1;
                    }
                    else
                    {
                        c8->V[0xF] = 0;
                    }
                    c8->V[reg8x] = c8->V[reg8x] << 1;
                    break;
                default:
                    break;
                }
                c8->pc += 2;
                break;
            case (0x9000):
                uint8_t reg9x = (inst >> 8) & 0x000F;
                uint8_t reg9y = (inst >> 4) & 0x000F;
                if (c8->V[reg9x] != c8->V[reg9y])
                {
                    c8->pc += 4;
                }
                else
                {
                    c8->pc += 2;
                }
                break;
            case (0xA000): // ANNN
                uint16_t valueA = inst & 0x0FFF;
                c8->i = valueA;
                c8->pc += 2;
                break;
            case (0xB000): // BNNN
                c8->pc = (inst & 0x0FFF) + c8->V[0];
                break;
            case (0xC000): // CXNN
                uint8_t regC = (inst >> 8) & 0x000F;
                uint8_t valueC = inst & 0x00FF;
                c8->V[regC] = rand() & valueC;
                c8->pc += 2;
                break;
            case (0xD000): // DXYN
                // Extract variable
                uint8_t x = c8->V[(inst >> 8) & 0x000F] % 64;
                uint8_t y = c8->V[(inst >> 4) & 0x000F] % 32;
                uint8_t n = inst & 0x000F;

                // Reset collision flag
                c8->V[0xF] = 0;

                for (int row = 0; row < n; row++)
                {
                    uint8_t sprite_byte = c8->memory[c8->i + row];

                    for (int col = 0; col < 8; col++)
                    {
                        uint8_t sprite_pixel = (sprite_byte >> (7 - col)) & 0x1;

                        int px = x + col;
                        int py = y + row;

                        if (px >= 64 || py >= 32)
                            continue;

                        if (sprite_pixel)
                        {
                            if (c8->display[px][py] == 1)
                            {
                                c8->V[0xF] = 1;
                            }
                            c8->display[px][py] ^= 1;
                        }
                    }
                }
                c8->draw_flag = true;
                c8->pc += 2;
                break;
            case (0xE000): // EXNN
                uint8_t regE = (inst >> 8) & 0x000F;
                switch (inst & 0x00FF)
                {
                case 0x9E:
                    if (c8->keypad[c8->V[regE]])
                    {
                        c8->pc += 2;
                    }
                    c8->pc += 2;
                    break;
                case 0xA1:
                    if (!c8->keypad[c8->V[regE]])
                    {
                        c8->pc += 2;
                    }
                    c8->pc += 2;
                    break;
                default:
                    c8->pc += 2;
                    break;
                }
                break;
            case (0xF000):
                uint8_t regF = (inst >> 8) & 0x000F;
                switch (inst & 0x00FF)
                {
                case 0x29:
                    c8->i = 0x50 + (c8->V[regF] * 5);
                    c8->pc += 2;
                    break;
                case 0x33:
                    uint8_t val = c8->V[regF];
                    c8->memory[c8->i] = val / 100;
                    c8->memory[c8->i + 1] = (val / 10) % 10;
                    c8->memory[c8->i + 2] = val % 10;
                    c8->pc += 2;
                    break;
                case 0x55:
                    for (int idx = 0; idx <= regF; idx++)
                    {
                        c8->memory[c8->i + idx] = c8->V[idx];
                    }
                    c8->pc += 2;
                    break;
                case 0x65:
                    for (int idx = 0; idx <= regF; idx++)
                    {
                        c8->V[idx] = c8->memory[c8->i + idx];
                    }
                    c8->pc += 2;
                    break;
                case 0x0A:
                    bool key_found = false;
                    for (int idx = 0; idx < 16 && !key_found; idx++)
                    {
                        if (c8->keypad_prev[c8->V[idx]] && !c8->keypad[c8->V[idx]])
                        {
                            c8->V[regF] = idx;
                            key_found = true;
                        }
                    }
                    if (key_found)
                        c8->pc += 2;
                    break;
                case 0x07:
                    c8->V[regF] = c8->dt;
                    c8->pc += 2;
                    break;
                case 0x15:
                    c8->dt = c8->V[regF];
                    c8->pc += 2;
                    break;
                case 0x18:
                    c8->st = c8->V[regF];
                    c8->pc += 2;
                    break;
                case 0x1E:
                    c8->i += c8->V[regF];
                    c8->pc = c8->pc + 2;
                    break;
                default:
                    c8->pc += 2;
                    break;
                }
                break;
            default:
                c8->pc += 2;
                break;
            }
}