#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

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

void print_display(uint8_t display[64][32])
{
    system("cls"); // Clear the console (Windows)
    for (int y = 0; y < 32; y++)
    {
        for (int x = 0; x < 64; x++)
        {
            printf("%s", display[x][y] ? "\u2588\u2588" : "  ");
        }
        printf("\n");
    }
    printf("\n");
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
    uint8_t dt;
    uint8_t st;

    if (load_rom("test_opcode.ch8", memory) != 0)
    {
        return 1;
    }

    uint16_t stuck_inst = (memory[0x2E0] << 8) | memory[0x2E0 + 1];
    printf("Instruction at 0x2E0: %04X\n", stuck_inst);

    bool draw_flag = true;

    while (true)
    {

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
        case (0xF000):
            uint8_t regF = (inst >> 8) & 0x000F;
            switch (inst & 0x00FF)
            {
            case 0x29:
                i = 0x50 + (V[regF] * 5);
                break;
            case 0x33:
                uint8_t val = V[regF];
                memory[i] = val / 100;
                memory[i+1] = (val / 10) % 10;
                memory[i+2] = val % 10;
                break;
            case 0x55:
                for(int idx = 0; idx <= regF; idx++) {
                    memory[i + idx] = V[idx];
                }
                break;
            case 0x65:
                for(int idx = 0; idx <= regF; idx++) {
                    V[idx] = memory[i + idx];
                }
                break;
            default:
                break;
            }
            pc += 2;
            break;
        default:
            pc += 2;
            break;
        }

        if (draw_flag)
        {
            print_display(display);
            draw_flag = false;
        }

        Sleep(2);
    }
}
