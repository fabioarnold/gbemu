enum CPUState {
	CPU_STATE_FETCH,
	CPU_STATE_IDLE0,
	CPU_STATE_IDLE1,
	CPU_STATE_EXECUTE,

	CPU_STATE_MEMORY_LOAD,
	CPU_STATE_MEMORY_STORE,
	CPU_STATE_READ_PC,
	CPU_STATE_STALL,
	CPU_STATE_OP2
};

const char *cpu_state_names[] = {
	[CPU_STATE_FETCH]   = "fetch",
	[CPU_STATE_IDLE0]   = "idle0",
	[CPU_STATE_IDLE1]   = "idle1",
	[CPU_STATE_EXECUTE] = "execute",

	[CPU_STATE_MEMORY_LOAD]  = "mem load",
	[CPU_STATE_MEMORY_STORE] = "mem store",
	[CPU_STATE_READ_PC]      = "read pc",
	[CPU_STATE_STALL]        = "stall",
	[CPU_STATE_OP2]          = "op2"
};

struct Memory;

// SHARP LR35902
struct CPU {
	typedef void (CPU::*Instruction)();

	// registers (for little endian host)
	union {
		u16 AF; 
		struct {
			union {
				u8 F; // flag register
				struct {
					u8 F_unused : 4;
					u8 F_C : 1; // carry: carry or A is smaller with CP
					u8 F_H : 1; // half carry: carry in lower nibble
					u8 F_N : 1; // negative: substraction in last math op
					u8 F_Z : 1; // zero: math op produced 0 or match with CP
				};
			};
			u8 A;
		};
	};
	union { u16 BC; struct { u8 C, B; }; };
	union { u16 DE; struct { u8 E, D; }; };
	union { u16 HL; struct { u8 L, H; }; };
	union { u16 SP; struct { u8 P, S; }; }; // stack pointer
	u16 PC; // program counter

	u8 IME; // interrupt master enable (only set by ei and di instruction)

	u64 cycle_count;
	bool halted;

	Memory *memory;

	CPUState state;
	u16 address; // current address
	u8 bus; // current byte on bus
	Instruction instruction; // next instruction to execute
	bool condition;

	// debug
	bool DEBUG_not_implemented_error = false;
	bool DEBUG_illegal_instruction = false;
	int DEBUG_break_point = 0;

	void reset();
	void step();

	#include "cpu_instructions.h"
};
