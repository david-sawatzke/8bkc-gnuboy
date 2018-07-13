#include "defs.h"
#include "regs.h"
#include "hw.h"
#include "cpu.h"
#include "mem.h"
#include "fastmem.h"
#include "cpuregs.h"
#include "cpucore.h"
#include "lcdc.h"
#include "debug.h"
#include <string.h>

#include "esp_attr.h"

#ifdef USE_ASM
#include "asm.h"
#endif

const static uint8_t chip8_fontset[80] =
{ 
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

struct cpu cpu;

void cpu_reset()
{
	memset(&cpu, 0, sizeof(cpu));
	memcpy(cpu.memory, chip_8_fontset, sizeof chip_8_fontset);
	cpu.pc = 0x200;
	// TODO Maybe load rom into ram here?
}

#define reg cpu.reg
#define i cpu.i
#define pc cpu.pc
#define delay_timer cpu.delay_timer
#define sound_timer cpu.sound_timer
#define stack cpu.stack
#define sp cpu.sp
#define memory cpu.memory
#define gfx cpu.gfx

void cpu_step(void)
{
    uint16_t opcode = memory[pc] << 8 | memory [pc + 1];
    // In numerical order. Yes, this giant else-if chain is probably not optimal for performance
    if (opcode == 0x00E0) {
        // Clear display
		// TODO Add mutex to inhibit drawing in this time
		memset(gfx, 0, sizeof gfx);
        pc += 2;
    } else if (opcode == 0x00EE) {
        // Return
        sp--;
        pc = stack[sp];
    } else if ((opcode & 0xF000) == 0x1000) {
        // Jump
        cpu.pc = opcode & 0x0FFF;
    } else if ((opcode & 0xF000) == 0x2000) {
        // Call
        stack[sp] = pc;
        sp++;
        pc = opcode & 0x0FFF;
    } else if ((opcode & 0xF000) == 0x3000) {
        // Skip if equal
        if (reg[(opcode & 0x0F00) >> (4 * 2)] == (opcode & 0xFF))
            pc += 2;
        pc += 2;
    } else if ((opcode & 0xF000) == 0x4000) {
        // Skip if not equal
        if (reg[(opcode & 0x0F00) >> (4 * 2)] != (opcode & 0xFF))
            pc += 2;
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x5000) {
        // Skip if registers equal
        if (reg[(opcode & 0x0F00) >> (4 * 2)] == reg[(opcode & 0x00F0) >> (4 * 1)])
            pc += 2;
        pc += 2;
    } else if ((opcode & 0xF00) == 0x6000) {
        // Set to const
        reg[(opcode & 0x0F00) >> (4 * 2)] = opcode & 0xFF;
        pc += 2;
    } else if ((opcode & 0xF00) == 0x7000) {
        // Add const
        reg[(opcode & 0x0F00) >> (4 * 2)] += opcode & 0xFF;
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x8000){
        // Mov
        reg[(opcode & 0x0F00) >> (4 * 2)] = reg[(opcode & 0x00F0) >> 4];
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x8001){
        // Or
        reg[(opcode & 0x0F00) >> (4 * 2)] |= reg[(opcode & 0x00F0) >> 4];
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x8002){
        // And
        reg[(opcode & 0x0F00) >> (4 * 2)] &= reg[(opcode & 0x00F0) >> 4];
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x8003){
        // XOr
        reg[(opcode & 0x0F00) >> (4 * 2)] ^= reg[(opcode & 0x00F0) >> 4];
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x8004){
        // Add
        if(reg[(opcode & 0x00F0) >> 4] > (0xFF - reg[(opcode & 0x0F00) >> (4 * 2)]))
            reg[0xF] = 1;
        else 
            reg[0xF] = 0;
        reg[(opcode & 0x0F00) >> (4 * 2)] += reg[(opcode & 0x00F0) >> 4];
        pc +=2;
    } else if ((opcode & 0xF00F) == 0x8005){
        // Sub
        if(reg[(opcode & 0x00F0) >> 4] < reg[(opcode & 0x0F00) >> (4 * 2)])
            reg[0xF] = 1;
        else 
            reg[0xF] = 0;
        reg[(opcode & 0x0F00) >> (4 * 2)] -= reg[(opcode & 0x00F0) >> 4];
        pc +=2;
    } else if ((opcode & 0xF00F) == 0x8006){
        // Shift
        reg[0xF] = reg[(opcode & 0x00F0) >> 4] & 0x1;
        reg[(opcode & 0x0F00) >> (4 * 2)]  =reg[(opcode & 0x00F0) >> 4] >> 1;
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x8007){
        // Sub
        if(reg[(opcode & 0x00F0) >> 4] > reg[(opcode & 0x0F00) >> (4 * 2)])
            reg[0xF] = 1;
        else 
            reg[0xF] = 0;
        reg[(opcode & 0x0F00) >> (4 * 2)] = reg[(opcode & 0x00F0) >> 4] - reg[(opcode & 0x0F00) >> (4 * 2)];
        pc +=2;
    } else if ((opcode & 0xF00F) == 0x800E){
        // Shift
        reg[0xF] = reg[(opcode & 0x00F0) >> 4] >> 7;
        reg[(opcode & 0x0F00) >> (4 * 2)]  = reg[(opcode & 0x00F0) >> 4] <<= 1;
        pc += 2;
    } else if ((opcode & 0xF00F) == 0x9000) {
        // Skip if registers equal
        if (reg[(opcode & 0x0F00) >> (4 * 2)] != reg[(opcode & 0x00F0) >> (4 * 1)])
            pc += 2;
        pc += 2;
    } else if ((opcode & 0xF000) == 0xA000) {
        I = opcode & 0x0FFF;
        pc += 2;
    } else if ((opcode & 0xF000) == 0xB000) {
        // Jump
        pc = (opcode & 0x0FFF) + reg[0];
    } else if ((opcode & 0xF000) == 0xC000) {
        // Rand
        // TODO
        pc += 2;
    } else if ((opcode & 0xF000) == 0xD000) {
        // Disp
        uint8_t x =  reg[(opcode & 0x0F00) >> (2 * 4)];
        uint8_t y =  reg[(opcode & 0x00F0) >> 4];
        uint8_t height =  (opcode & 0x000F);
        uint8_t pixel;
        reg[0xF] = 0;
        for (uint8_t yline = 0; yline < height; yline++) {
            pixel = memory[I + yline];
            for(uint8_t xline = 0; xline < 8; xline ++) {
                if ((pixel & (0x80 >> xline)) != 0) {
                    if ((gfx[x + xline + ((y + yline) * 64)] ^= 0xFFFF) == 0) {
                        reg[0xF] = 1;
                    }
                }
            }
        }
        pc += 2;
        redrawScreen = 1;
    } else if ((opcode & 0xF0FF) == 0xE09E) {
        // Key Read
        // TODO
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xE0A1) {
        // Key Read
        // TODO
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF007) {
        // Timer read
        reg[(opcode & 0x0F00) >> (4 * 2)] = delay_timer;
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF00A) {
        // Key Read (blocking)
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF015) {
        // Timer set
        delay_timer = reg[(opcode & 0x0F00) >> (4 * 2)];
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF018) {
        // Timer set
        sound_timer = reg[(opcode & 0x0F00) >> (4 * 2)];
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF01E) {
        // Timer set
        I += reg[(opcode & 0x0F00) >> (4 * 2)];
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF029) {
        // Sprite font read
        // Each sprite is 5 Bytes big
        I = reg[(opcode & 0x0F00) >> (4 * 2)] * 5;
        // TODO (implementation and display blocking)
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF033) {
        memory[I] = reg[(opcode & 0x0F00) >> 8] / 100;
        memory[I + 1] = (reg[(opcode & 0x0F00) >> 8] / 10) % 10;
        memory[I + 2] = (reg[(opcode & 0x0F00) >> 8] % 100) % 10;
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF055) {
        // Read memory to registers
        // TODO
        pc += 2;
    } else if ((opcode & 0xF0FF) == 0xF065) {
        // Write memory to registers
        // TODO
        pc += 2;
    } else {
        // TODO, opcode not found
    }
	// TODO  decrease externally
    if (delay_timer > 0)
        delay_timer--;
    if (sound_timer > 0) {
        // BEEP
        sound_timer--;
    }
}
#endif /* ASM_CPU_STEP */
