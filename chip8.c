#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <windows.h>

/*4KB memory, dont use 0x000 from 0x1FF. Most programs start at 0x200*/

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
            printf("%c", display[x][y] ? '#' : ' ');
        }
        printf("\n");
    }
    printf("\n");
}

int main()
{

    uint8_t memory[4096] = {0};
    uint16_t pc = 0x200;

    /*registers: v0 to vF. vF is used as a flag by some instructions.
    I register is to store memory addresses so only the 12
    lowest rightmost bits are used.*/
    uint8_t V[16];
    uint16_t i;

    /*Display is a 64x32 pixel monochrome display*/
    uint8_t display[64][32];

    /*Delay and sound registers*/
    uint8_t dt;
    uint8_t st;

    if (load_rom("IBM_Logo.ch8", memory) != 0)
    {
        return 1;
    }

    bool draw_flag = true;

    while (true)
    {

        uint16_t inst = (memory[pc] << 8) | memory[pc + 1];

        switch (inst & 0xF000)
        {
        case (0x0000):
            memset(display, 0, sizeof(display));
            pc += 2;
            draw_flag = true;
            break;
        case (0x1000): // 1NNN
            pc = inst & 0x0FFF;
            break;
        case (0x6000): // 6XNN
            uint8_t value = inst & 0x00FF;
            uint8_t reg = (inst >> 8) & 0x000F;
            V[reg] = value;
            pc += 2;
            break;
        case (0x7000):
            uint8_t value7 = inst & 0x00FF;
            uint8_t reg7 = (inst >> 8) & 0x000F;
            V[reg7] += value7;
            pc += 2;
            break;
        case (0xA000): // ANNN
            uint16_t valueA = inst & 0x0FFF;
            i = valueA;
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
        default:
            pc += 2;
        }

        

       if (draw_flag) {
        print_display(display);
        draw_flag = false;
       }

        Sleep(2);
    }
}
