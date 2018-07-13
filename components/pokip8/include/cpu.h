#ifndef __CPU_H__
#define __CPU_H__
#include "defs.h"

struct cpu
{
	uint8_t reg[8];
	uint16_t I;
	uint16_t pc;
	uint8_t delay_timer;
	uint8_t sound_timer;
	uint16_t stack[16];
	uint8_t sp;
	uint8_t memory[2048];
	uint8_t gfx[64 * 32];
};

extern struct cpu cpu;

void cpu_reset(void);
void cpu_step(void);

#endif


