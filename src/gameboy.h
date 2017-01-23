/*
TODO:
DMA within 160 cycles (not instant)
8x16 sprites
the ppu window
sprite/bg priorities
lcd stat interrupts
input interrupt
audio
serial
memory bank controllers
external ram (sram)
halt/stop behavior
lcd disable
*/

const int CPU_FREQ_HZ  = 4<<20; // 4 MiHz
const int RAM_FREQ_HZ  = 1<<20; // 1 MiHz
const int PPU_FREQ_HZ  = 4<<20; // 4 MiHz
const int VRAM_FREQ_HZ = 2<<20; // 2 MiHz

const int LCD_WIDTH  = 160;
const int LCD_HEIGHT = 144;

const int OBJ_COUNT    = 40;
const int OBJ_PER_LINE = 10;

// timings
const int OAM_SEARCH_CYCLES = 4*20;
const int PIXEL_TRANSFER_CYCLES = 4*43; // or more
const int HBLANK_CYCLES = 4*51; // or less (pixel transfer might cut in)
const int VBLANK_LINES = 10;
const int VBLANK_CYCLES = VBLANK_LINES
	* (OAM_SEARCH_CYCLES + PIXEL_TRANSFER_CYCLES + HBLANK_CYCLES);
const int VSYNC_CYCLES = (LCD_HEIGHT + VBLANK_LINES)
	* (OAM_SEARCH_CYCLES + PIXEL_TRANSFER_CYCLES + HBLANK_CYCLES);
const float VSYNC_HZ = (float)CPU_FREQ_HZ / (float)VSYNC_CYCLES;

// addresses
const int ADR_CART_BANK0          = 0x0000;
const int ADR_CART_BANK1          = 0x4000;
const int ADR_VRAM                = 0x8000;
const int ADR_RAM_EXTERNAL        = 0xA000;
const int ADR_RAM_INTERNAL_BANK0  = 0xC000;
const int ADR_RAM_INTERNAL_BANK1  = 0xD000; // CGB: switchable (1-7)
const int ADR_RAM_INTERNAL_MIRROR = 0xE000;
const int ADR_OAM                 = 0xFE00;
const int ADR_EMPTY               = 0xFEA0;
const int ADR_UNUSABLE            = 0xFEA0;
const int ADR_IO                  = 0xFF00;
const int ADR_HRAM                = 0xFF80;
const int ADR_IE                  = 0xFFFF;

// sizes
const int SIZE_BOOT_ROM = 0x100;  // DMG
const int SIZE_ROM_BANK = 0x4000;
const int SIZE_VRAM     = 0x2000;
const int SIZE_RAM      = 0x2000;
const int SIZE_OAM      = 0xA0;
const int SIZE_IO       = 0x80;
const int SIZE_HRAM     = 0x7F;

// object attribute memory
struct OAM {
	struct OBJ {
		u8 y; // minus 16
		u8 x; // minus 8
		u8 tile;
		union {
			u8 attrib;
			struct {
				u8 ATTRIB_cgb_palette : 3;
				u8 ATTRIB_cgb_vram_bank : 1;
				u8 ATTRIB_palette : 1;
				u8 ATTRIB_flip_x : 1;
				u8 ATTRIB_flip_y : 1;
				// 0=OBJ above BG, 1 OBJ behind (but above pixel value 0)
				u8 ATTRIB_priority : 1;
			};
		};
	} objs[OBJ_COUNT];
};

// button input
const int REG_INPUT = 0x00;

// serial data transfer
const int REG_SB = 0x01;
const int REG_SC = 0x02;

// timer
const int REG_DIV  = 0x04;
const int REG_TIMA = 0x05;
const int REG_TMA  = 0x06;
const int REG_TAC  = 0x07;

// LCD
const int REG_LCDC = 0x40;

// DMA
const int REG_DMA = 0x46; // source address XX00-XX9F

// interrupt
const int REG_IF = 0x0F; // individual interrupt requests
const int REG_IE = 0xFF; // enable individual interrupts

// interrupt jump addresses
const int IRQ_ADR_VBLANK  = 0x40;
const int IRQ_ADR_LCDSTAT = 0x48;
const int IRQ_ADR_TIMER   = 0x50;
const int IRQ_ADR_SERIAL  = 0x58;
const int IRQ_ADR_JOYPAD  = 0x60;

// input/output registers
struct IO {
	// FF00
	union {
		u8 INPUT;
		struct { // select
			u8 INPUT_select_unused0    : 4;
			u8 INPUT_select_directions : 1; // 0=Selected
			u8 INPUT_select_buttons    : 1; // 0=Selected
			u8 INPUT_select_unused1    : 2;
		};
		struct { // directions
			u8 INPUT_right : 1; // 0=Pressed
			u8 INPUT_left  : 1; // 0=Pressed
			u8 INPUT_up    : 1; // 0=Pressed
			u8 INPUT_down  : 1; // 0=Pressed
			u8 INPUT_direction_unused : 4;
		};
		struct { // buttons
			u8 INPUT_a      : 1; // 0=Pressed
			u8 INPUT_b      : 1; // 0=Pressed
			u8 INPUT_select : 1; // 0=Pressed
			u8 INPUT_start  : 1; // 0=Pressed
			u8 INPUT_button_unused : 4;
		};
	};

	// FF01
	u8 SB;
	// FF02
	u8 SC;

	// FF03
	u8 FF03;

	// FF04
	u8 DIV;  // 1<<14 Hz, reset on write
	// FF05
	u8 TIMA; // controlled by TAC, reset on overflow
	// FF06
	u8 TMA;  // reset value for TIMA
	// FF07
	union {
		u8 TAC;
		struct {
			u8 TAC_clock  : 2; // 1<<12, 1<<18, 1<<16, 1<<14 Hz
			u8 TAC_stop   : 1; // 0=Stop, 1=Start
			u8 TAC_unused : 5;
		};
	};

	// FF08-FF0E
	u8 FF08_FF0E[0x07];

	// FF0F - IF - Interrupt Flag (R/W)
	union {
		u8 IF;
		struct {
			u8 IF_vblank   : 1; // 1=Request
			u8 IF_lcd_stat : 1; // 1=Request
			u8 IF_timer    : 1; // 1=Request
			u8 IF_serial   : 1; // 1=Request
			u8 IF_joypad   : 1; // 1=Request
			u8 IF_unused   : 3;
		};
	};

	// FF10-FF3F
	u8 FF10_FF3F[0x30];

	// FF40 - LCDC - LCD Control (R/W)
	union {
		u8 LCDC;
		struct {
			u8 LCDC_bg_enable       : 1; // 0=Off, 1=On
			u8 LCDC_obj_enable      : 1; // 0=Off, 1=On
			u8 LCDC_obj_size        : 1; // 0=8x8, 1=8x16
			u8 LCDC_bg_tile_map     : 1; // 0=9800-9BFF, 1=9C00-9FFF
			u8 LCDC_tile_data       : 1; // 0=8800-97FF, 1=8000-8FFF
			u8 LCDC_window_enable   : 1; // 0=Off, 1=On
			u8 LCDC_window_tile_map : 1; // 0=9800-9BFF, 1=9C00-9FFF
			u8 LCDC_enable          : 1; // 0=Off, 1=On
		}; // if !LCDC_tile_data tile index is signed
	};
	// FF41 - STAT - LCDC Status (R/W)
	union {
		u8 STAT;
		struct {
			// (R) 0: HBLANK, 1: VBLANK, 2: OAM-RAM, 3: transfer data to LCD
			u8 STAT_mode                      : 2;
			u8 STAT_coincide                  : 1; // (R) 0:LYC<>LY, 1:LYC==LY
			u8 STAT_hblank_interrupt_enable   : 1;
			u8 STAT_vblank_interrupt_enable   : 1;
			u8 STAT_oam_interrupt_enable      : 1;
			u8 STAT_coincide_interrupt_enable : 1;
			u8 STAT_unused                    : 1;
		};
	};
	// FF42 - SCY - Scroll Y (R/W)
	u8 SCY;
	// FF43 - SCX - Scroll X (R/W)
	u8 SCX;
	// FF44 - LY - LCDC Y-Coordinate (R)
	u8 LY;
	// FF45 - LYC - LY Compare (R/W)
	u8 LYC;
	// FF46 - DMA - DMA Transfer and Start Address (W)
	u8 DMA;
	// FF47 - BGP - BG Palette Data (R/W) - Non CGB Mode Only
	u8 BGP;
	// FF48 - OBP0 - Object Palette 0 Data (R/W) - Non CGB Mode Only
	u8 OBP0;
	// FF49 - OBP1 - Object Palette 1 Data (R/W) - Non CGB Mode Only
	u8 OBP1;
	// FF4A - WY - Window Y Position (R/W)
	u8 WY;
	// FF4B - WX - Window X Position minus 7 (R/W)
	u8 WX;

	// FF4C FF4D FF4E
	u8 FF4C_FF4E[3];

	// FF4F - VBK - CGB Mode Only - VRAM Bank
	u8 VBK; // 0: bank 0, 1: bank 1

	// FF50
	u8 BOOT; // 0: boot rom On, 1: boot rom Off

	// FF51-FF7F
	u8 FF51_FF7F[0x2F];

	// FF80-FFFE
	u8 FF80_FFFE[0x7F]; // hram

	// FFFF - IE - Interrupt Enable (R/W)
	union {
		u8 IE;
		struct {
			u8 IE_vblank   : 1; // 1=Enable
			u8 IE_lcd_stat : 1; // 1=Enable
			u8 IE_timer    : 1; // 1=Enable
			u8 IE_serial   : 1; // 1=Enable
			u8 IE_joypad   : 1; // 1=Enable
			u8 IE_unused   : 3;
		};
	};
};

struct GameBoy;
struct Memory {
	u8 boot_rom[SIZE_BOOT_ROM]; // 0x0000
	u8 *rom = nullptr; // 32kB, 64kB, 128kB, 256kB, 512kB and so on
	size_t rom_size;
	u8 *rom_bank0 = nullptr;
	u8 *rom_bank1 = nullptr;
	u8 vram[SIZE_VRAM]; // 0x8000
	u8  ram[SIZE_RAM];  // 0xC000
	OAM oam;            // 0xFE00
	IO io;              // 0xFF00
	u8 hram[SIZE_HRAM]; // 0xFF80

	void init();

	u8 load8(u16 address);
	void store8(u16 address, u8 value);

//private:
	GameBoy *gb;
	u8 *map(u16 address);
};

void Memory::init() {
	memset(ram,  0, sizeof(ram));
	memset(vram, 0, sizeof(vram));
	memset(&oam, 0, sizeof(oam));
	memset(&io,  0, sizeof(io));
	memset(hram, 0, sizeof(hram));
}

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



	// instructions

	#define CPU_INSTRUCTION(NAME, BODY) void NAME() { BODY }

	void nop() {}
	void ill() { DEBUG_illegal_instruction = true; }
	void halt() { halted = true; } // TODO: halt bug of DMG
	void stop_delay() { halted = true; }
	void stop() { state = CPU_STATE_READ_PC; instruction = &CPU::stop_delay; }



	#define CPU_INSTRUCTIONS_CB(NAME, WRITE_BACK, BODY) \
		CPU_INSTRUCTION(NAME ## _a, int reg=A; BODY A=reg;) \
		CPU_INSTRUCTION(NAME ## _b, int reg=B; BODY B=reg;) \
		CPU_INSTRUCTION(NAME ## _c, int reg=C; BODY C=reg;) \
		CPU_INSTRUCTION(NAME ## _d, int reg=D; BODY D=reg;) \
		CPU_INSTRUCTION(NAME ## _e, int reg=E; BODY E=reg;) \
		CPU_INSTRUCTION(NAME ## _h, int reg=H; BODY H=reg;) \
		CPU_INSTRUCTION(NAME ## _l, int reg=L; BODY L=reg;) \
		CPU_INSTRUCTION(NAME ## _hl_delay, int reg=bus; BODY \
			bus = reg; state = WRITE_BACK; instruction = &CPU::nop;) \
		CPU_INSTRUCTION(NAME ## _hl, address = HL; \
			state = CPU_STATE_MEMORY_LOAD; \
			instruction = &CPU::NAME ## _hl_delay;)

	#define CPU_INSTRUCTION_CB_ALU(NAME, BODY) \
		CPU_INSTRUCTIONS_CB(NAME, CPU_STATE_MEMORY_STORE, BODY \
			F_N = F_H = 0; F_Z = !(reg&0xFF);)

	CPU_INSTRUCTION_CB_ALU(rl, int wide = (reg<<1) | F_C; reg = wide; F_C = wide>>8;)
	CPU_INSTRUCTION_CB_ALU(rlc, reg = (reg<<1) | (reg>>7); F_C=reg&1;)
	CPU_INSTRUCTION_CB_ALU(rr, int low = reg&1; reg = (reg>>1) | (F_C<<7); F_C = low;)
	CPU_INSTRUCTION_CB_ALU(rrc, F_C = reg&1; reg = (reg>>1) | (F_C<<7);)
	CPU_INSTRUCTION_CB_ALU(sla, F_C = reg>>7; reg <<= 1;)
	CPU_INSTRUCTION_CB_ALU(sra, F_C = reg&1; reg = ((s8)reg) >> 1;)
	CPU_INSTRUCTION_CB_ALU(srl, F_C = reg&1; reg >>= 1;)
	CPU_INSTRUCTION_CB_ALU(swap, reg = (reg<<4) | (reg>>4); F_C = 0;)

	#define CPU_INSTRUCTIONS_CB_BITS(NAME, WRITE_BACK, BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 0, WRITE_BACK, u8 bit=0x01; BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 1, WRITE_BACK, u8 bit=0x02; BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 2, WRITE_BACK, u8 bit=0x04; BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 3, WRITE_BACK, u8 bit=0x08; BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 4, WRITE_BACK, u8 bit=0x10; BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 5, WRITE_BACK, u8 bit=0x20; BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 6, WRITE_BACK, u8 bit=0x40; BODY) \
		CPU_INSTRUCTIONS_CB(NAME ## 7, WRITE_BACK, u8 bit=0x80; BODY)

	CPU_INSTRUCTIONS_CB_BITS(bit, CPU_STATE_FETCH, \
		F_Z = !(reg&bit); F_N = 0; F_H = 1;)
	CPU_INSTRUCTIONS_CB_BITS(res, CPU_STATE_MEMORY_STORE, reg &= ~bit;)
	CPU_INSTRUCTIONS_CB_BITS(set, CPU_STATE_MEMORY_STORE, reg |= bit;)

	Instruction cb_instructions[0x100] = {
		&CPU::rlc_b,   // 0x00
		&CPU::rlc_c,   // 0x01
		&CPU::rlc_d,   // 0x02
		&CPU::rlc_e,   // 0x03
		&CPU::rlc_h,   // 0x04
		&CPU::rlc_l,   // 0x05
		&CPU::rlc_hl,  // 0x06
		&CPU::rlc_a,   // 0x07
		&CPU::rrc_b,   // 0x08
		&CPU::rrc_c,   // 0x09
		&CPU::rrc_d,   // 0x0A
		&CPU::rrc_e,   // 0x0B
		&CPU::rrc_h,   // 0x0C
		&CPU::rrc_l,   // 0x0D
		&CPU::rrc_hl,  // 0x0E
		&CPU::rrc_a,   // 0x0F
		&CPU::rl_b,    // 0x10
		&CPU::rl_c,    // 0x11
		&CPU::rl_d,    // 0x12
		&CPU::rl_e,    // 0x13
		&CPU::rl_h,    // 0x14
		&CPU::rl_l,    // 0x15
		&CPU::rl_hl,   // 0x16
		&CPU::rl_a,    // 0x17
		&CPU::rr_b,    // 0x18
		&CPU::rr_c,    // 0x19
		&CPU::rr_d,    // 0x1A
		&CPU::rr_e,    // 0x1B
		&CPU::rr_h,    // 0x1C
		&CPU::rr_l,    // 0x1D
		&CPU::rr_hl,   // 0x1E
		&CPU::rr_a,    // 0x1F
		&CPU::sla_b,   // 0x20
		&CPU::sla_c,   // 0x21
		&CPU::sla_d,   // 0x22
		&CPU::sla_e,   // 0x23
		&CPU::sla_h,   // 0x24
		&CPU::sla_l,   // 0x25
		&CPU::sla_hl,  // 0x26
		&CPU::sla_a,   // 0x27
		&CPU::sra_b,   // 0x28
		&CPU::sra_c,   // 0x29
		&CPU::sra_d,   // 0x2A
		&CPU::sra_e,   // 0x2B
		&CPU::sra_h,   // 0x2C
		&CPU::sra_l,   // 0x2D
		&CPU::sra_hl,  // 0x2E
		&CPU::sra_a,   // 0x2F
		&CPU::swap_b,  // 0x30
		&CPU::swap_c,  // 0x31
		&CPU::swap_d,  // 0x32
		&CPU::swap_e,  // 0x33
		&CPU::swap_h,  // 0x34
		&CPU::swap_l,  // 0x35
		&CPU::swap_hl, // 0x36
		&CPU::swap_a,  // 0x37
		&CPU::srl_b,   // 0x38
		&CPU::srl_c,   // 0x39
		&CPU::srl_d,   // 0x3A
		&CPU::srl_e,   // 0x3B
		&CPU::srl_h,   // 0x3C
		&CPU::srl_l,   // 0x3D
		&CPU::srl_hl,  // 0x3E
		&CPU::srl_a,   // 0x3F
		&CPU::bit0_b,  // 0x40
		&CPU::bit0_c,  // 0x41
		&CPU::bit0_d,  // 0x42
		&CPU::bit0_e,  // 0x43
		&CPU::bit0_h,  // 0x44
		&CPU::bit0_l,  // 0x45
		&CPU::bit0_hl, // 0x46
		&CPU::bit0_a,  // 0x47
		&CPU::bit1_b,  // 0x48
		&CPU::bit1_c,  // 0x49
		&CPU::bit1_d,  // 0x4A
		&CPU::bit1_e,  // 0x4B
		&CPU::bit1_h,  // 0x4C
		&CPU::bit1_l,  // 0x4D
		&CPU::bit1_hl, // 0x4E
		&CPU::bit1_a,  // 0x4F
		&CPU::bit2_b,  // 0x50
		&CPU::bit2_c,  // 0x51
		&CPU::bit2_d,  // 0x52
		&CPU::bit2_e,  // 0x53
		&CPU::bit2_h,  // 0x54
		&CPU::bit2_l,  // 0x55
		&CPU::bit2_hl, // 0x56
		&CPU::bit2_a,  // 0x57
		&CPU::bit3_b,  // 0x58
		&CPU::bit3_c,  // 0x59
		&CPU::bit3_d,  // 0x5A
		&CPU::bit3_e,  // 0x5B
		&CPU::bit3_h,  // 0x5C
		&CPU::bit3_l,  // 0x5D
		&CPU::bit3_hl, // 0x5E
		&CPU::bit3_a,  // 0x5F
		&CPU::bit4_b,  // 0x60
		&CPU::bit4_c,  // 0x61
		&CPU::bit4_d,  // 0x62
		&CPU::bit4_e,  // 0x63
		&CPU::bit4_h,  // 0x64
		&CPU::bit4_l,  // 0x65
		&CPU::bit4_hl, // 0x66
		&CPU::bit4_a,  // 0x67
		&CPU::bit5_b,  // 0x68
		&CPU::bit5_c,  // 0x69
		&CPU::bit5_d,  // 0x6A
		&CPU::bit5_e,  // 0x6B
		&CPU::bit5_h,  // 0x6C
		&CPU::bit5_l,  // 0x6D
		&CPU::bit5_hl, // 0x6E
		&CPU::bit5_a,  // 0x6F
		&CPU::bit6_b,  // 0x70
		&CPU::bit6_c,  // 0x71
		&CPU::bit6_d,  // 0x72
		&CPU::bit6_e,  // 0x73
		&CPU::bit6_h,  // 0x74
		&CPU::bit6_l,  // 0x75
		&CPU::bit6_hl, // 0x76
		&CPU::bit6_a,  // 0x77
		&CPU::bit7_b,  // 0x78
		&CPU::bit7_c,  // 0x79
		&CPU::bit7_d,  // 0x7A
		&CPU::bit7_e,  // 0x7B
		&CPU::bit7_h,  // 0x7C
		&CPU::bit7_l,  // 0x7D
		&CPU::bit7_hl, // 0x7E
		&CPU::bit7_a,  // 0x7F
		&CPU::res0_b,  // 0x80
		&CPU::res0_c,  // 0x81
		&CPU::res0_d,  // 0x82
		&CPU::res0_e,  // 0x83
		&CPU::res0_h,  // 0x84
		&CPU::res0_l,  // 0x85
		&CPU::res0_hl, // 0x86
		&CPU::res0_a,  // 0x87
		&CPU::res1_b,  // 0x88
		&CPU::res1_c,  // 0x89
		&CPU::res1_d,  // 0x8A
		&CPU::res1_e,  // 0x8B
		&CPU::res1_h,  // 0x8C
		&CPU::res1_l,  // 0x8D
		&CPU::res1_hl, // 0x8E
		&CPU::res1_a,  // 0x8F
		&CPU::res2_b,  // 0x90
		&CPU::res2_c,  // 0x91
		&CPU::res2_d,  // 0x92
		&CPU::res2_e,  // 0x93
		&CPU::res2_h,  // 0x94
		&CPU::res2_l,  // 0x95
		&CPU::res2_hl, // 0x96
		&CPU::res2_a,  // 0x97
		&CPU::res3_b,  // 0x98
		&CPU::res3_c,  // 0x99
		&CPU::res3_d,  // 0x9A
		&CPU::res3_e,  // 0x9B
		&CPU::res3_h,  // 0x9C
		&CPU::res3_l,  // 0x9D
		&CPU::res3_hl, // 0x9E
		&CPU::res3_a,  // 0x9F
		&CPU::res4_b,  // 0xA0
		&CPU::res4_c,  // 0xA1
		&CPU::res4_d,  // 0xA2
		&CPU::res4_e,  // 0xA3
		&CPU::res4_h,  // 0xA4
		&CPU::res4_l,  // 0xA5
		&CPU::res4_hl, // 0xA6
		&CPU::res4_a,  // 0xA7
		&CPU::res5_b,  // 0xA8
		&CPU::res5_c,  // 0xA9
		&CPU::res5_d,  // 0xAA
		&CPU::res5_e,  // 0xAB
		&CPU::res5_h,  // 0xAC
		&CPU::res5_l,  // 0xAD
		&CPU::res5_hl, // 0xAE
		&CPU::res5_a,  // 0xAF
		&CPU::res6_b,  // 0xB0
		&CPU::res6_c,  // 0xB1
		&CPU::res6_d,  // 0xB2
		&CPU::res6_e,  // 0xB3
		&CPU::res6_h,  // 0xB4
		&CPU::res6_l,  // 0xB5
		&CPU::res6_hl, // 0xB6
		&CPU::res6_a,  // 0xB7
		&CPU::res7_b,  // 0xB8
		&CPU::res7_c,  // 0xB9
		&CPU::res7_d,  // 0xBA
		&CPU::res7_e,  // 0xBB
		&CPU::res7_h,  // 0xBC
		&CPU::res7_l,  // 0xBD
		&CPU::res7_hl, // 0xBE
		&CPU::res7_a,  // 0xBF
		&CPU::set0_b,  // 0xC0
		&CPU::set0_c,  // 0xC1
		&CPU::set0_d,  // 0xC2
		&CPU::set0_e,  // 0xC3
		&CPU::set0_h,  // 0xC4
		&CPU::set0_l,  // 0xC5
		&CPU::set0_hl, // 0xC6
		&CPU::set0_a,  // 0xC7
		&CPU::set1_b,  // 0xC8
		&CPU::set1_c,  // 0xC9
		&CPU::set1_d,  // 0xCA
		&CPU::set1_e,  // 0xCB
		&CPU::set1_h,  // 0xCC
		&CPU::set1_l,  // 0xCD
		&CPU::set1_hl, // 0xCE
		&CPU::set1_a,  // 0xCF
		&CPU::set2_b,  // 0xD0
		&CPU::set2_c,  // 0xD1
		&CPU::set2_d,  // 0xD2
		&CPU::set2_e,  // 0xD3
		&CPU::set2_h,  // 0xD4
		&CPU::set2_l,  // 0xD5
		&CPU::set2_hl, // 0xD6
		&CPU::set2_a,  // 0xD7
		&CPU::set3_b,  // 0xD8
		&CPU::set3_c,  // 0xD9
		&CPU::set3_d,  // 0xDA
		&CPU::set3_e,  // 0xDB
		&CPU::set3_h,  // 0xDC
		&CPU::set3_l,  // 0xDD
		&CPU::set3_hl, // 0xDE
		&CPU::set3_a,  // 0xDF
		&CPU::set4_b,  // 0xE0
		&CPU::set4_c,  // 0xE1
		&CPU::set4_d,  // 0xE2
		&CPU::set4_e,  // 0xE3
		&CPU::set4_h,  // 0xE4
		&CPU::set4_l,  // 0xE5
		&CPU::set4_hl, // 0xE6
		&CPU::set4_a,  // 0xE7
		&CPU::set5_b,  // 0xE8
		&CPU::set5_c,  // 0xE9
		&CPU::set5_d,  // 0xEA
		&CPU::set5_e,  // 0xEB
		&CPU::set5_h,  // 0xEC
		&CPU::set5_l,  // 0xED
		&CPU::set5_hl, // 0xEE
		&CPU::set5_a,  // 0xEF
		&CPU::set6_b,  // 0xF0
		&CPU::set6_c,  // 0xF1
		&CPU::set6_d,  // 0xF2
		&CPU::set6_e,  // 0xF3
		&CPU::set6_h,  // 0xF4
		&CPU::set6_l,  // 0xF5
		&CPU::set6_hl, // 0xF6
		&CPU::set6_a,  // 0xF7
		&CPU::set7_b,  // 0xF8
		&CPU::set7_c,  // 0xF9
		&CPU::set7_d,  // 0xFA
		&CPU::set7_e,  // 0xFB
		&CPU::set7_h,  // 0xFC
		&CPU::set7_l,  // 0xFD
		&CPU::set7_hl, // 0xFE
		&CPU::set7_a,  // 0xFF
	};

	void cb_delegate() {
		(this->*cb_instructions[bus])();
	}



	#define CPU_INSTRUCTION_LD_REG(NAME, REG, OPERAND) \
		CPU_INSTRUCTION(ld_ ## NAME, REG = OPERAND;)

	#define CPU_INSTRUCTIONS_LD_REG(NAME, REG) \
		CPU_INSTRUCTION_LD_REG(NAME ## a, REG, A) \
		CPU_INSTRUCTION_LD_REG(NAME ## b, REG, B) \
		CPU_INSTRUCTION_LD_REG(NAME ## c, REG, C) \
		CPU_INSTRUCTION_LD_REG(NAME ## d, REG, D) \
		CPU_INSTRUCTION_LD_REG(NAME ## e, REG, E) \
		CPU_INSTRUCTION_LD_REG(NAME ## h, REG, H) \
		CPU_INSTRUCTION_LD_REG(NAME ## l, REG, L) \
		CPU_INSTRUCTION_LD_REG(NAME ## bus, REG, bus) \
		CPU_INSTRUCTION(ld_ ## NAME ## d8, \
			state = CPU_STATE_READ_PC; \
			instruction = &CPU::ld_ ## NAME ## bus;) \
		CPU_INSTRUCTION(ld_ ## NAME ## hl, \
			state = CPU_STATE_MEMORY_LOAD; \
			address = HL; \
			instruction = &CPU::ld_ ## NAME ## bus;)

	CPU_INSTRUCTIONS_LD_REG(a_, A)
	CPU_INSTRUCTIONS_LD_REG(b_, B)
	CPU_INSTRUCTIONS_LD_REG(c_, C)
	CPU_INSTRUCTIONS_LD_REG(d_, D)
	CPU_INSTRUCTIONS_LD_REG(e_, E)
	CPU_INSTRUCTIONS_LD_REG(h_, H)
	CPU_INSTRUCTIONS_LD_REG(l_, L)

	CPU_INSTRUCTION_LD_REG(s_bus, S, bus)

	void ld_bc_delay() { C = bus; state = CPU_STATE_READ_PC; instruction = &CPU::ld_b_bus; }
	void ld_de_delay() { E = bus; state = CPU_STATE_READ_PC; instruction = &CPU::ld_d_bus; }
	void ld_hl_delay() { L = bus; state = CPU_STATE_READ_PC; instruction = &CPU::ld_h_bus; }
	void ld_sp_delay() { P = bus; state = CPU_STATE_READ_PC; instruction = &CPU::ld_s_bus; }

	void ldh_a8_a_delay() {
		address = 0xFF00 | bus;
		bus = A;
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::nop;
	}
	void ldh_a_a8_delay() {
		address = 0xFF00 | bus;
		state = CPU_STATE_MEMORY_LOAD;
		instruction = &CPU::ld_a_bus;
	}

	// 0x01 LD BC,d16
	void ld_bc_d16() { state = CPU_STATE_READ_PC; instruction = &CPU::ld_bc_delay; }
	// 0x11 LD DE,d16
	void ld_de_d16() { state = CPU_STATE_READ_PC; instruction = &CPU::ld_de_delay; }
	// 0x21 LD HL,d16
	void ld_hl_d16() { state = CPU_STATE_READ_PC; instruction = &CPU::ld_hl_delay; }
	// 0x31 LD SP,d16
	void ld_sp_d16() { state = CPU_STATE_READ_PC; instruction = &CPU::ld_sp_delay; }

	void ld_hl_bus() { state = CPU_STATE_MEMORY_STORE; address = HL; instruction = &CPU::nop; }
	// 0x36 LD (HL),d8
	void ld_hl_d8() { state = CPU_STATE_READ_PC; instruction = &CPU::ld_hl_bus; }

	// 0x0A LD A,(BC)
	void ld_a_bc() { state = CPU_STATE_MEMORY_LOAD; address = BC; instruction = &CPU::ld_a_bus; }
	// 0x1A LD A,(DE)
	void ld_a_de() { state = CPU_STATE_MEMORY_LOAD; address = DE; instruction = &CPU::ld_a_bus; }
	// 0x2A LD A,(HL+)
	void ld_a_hli() { state = CPU_STATE_MEMORY_LOAD; address = HL++; instruction = &CPU::ld_a_bus; }
	// 0x3A LD A,(HL-)
	void ld_a_hld() { state = CPU_STATE_MEMORY_LOAD; address = HL--; instruction = &CPU::ld_a_bus; }

	// 0x02 LD (BC),A
	void ld_bc_a() { state = CPU_STATE_MEMORY_STORE; address = BC; bus = A; instruction = &CPU::nop; }
	// 0x12 LD (DE),A
	void ld_de_a() { state = CPU_STATE_MEMORY_STORE; address = DE; bus = A; instruction = &CPU::nop; }

	void ld_hl_b() { state = CPU_STATE_MEMORY_STORE; address = HL; bus = B; instruction = &CPU::nop; }
	void ld_hl_c() { state = CPU_STATE_MEMORY_STORE; address = HL; bus = C; instruction = &CPU::nop; }
	void ld_hl_d() { state = CPU_STATE_MEMORY_STORE; address = HL; bus = D; instruction = &CPU::nop; }
	void ld_hl_e() { state = CPU_STATE_MEMORY_STORE; address = HL; bus = E; instruction = &CPU::nop; }
	void ld_hl_h() { state = CPU_STATE_MEMORY_STORE; address = HL; bus = H; instruction = &CPU::nop; }
	void ld_hl_l() { state = CPU_STATE_MEMORY_STORE; address = HL; bus = L; instruction = &CPU::nop; }

	void ld_hl_sp_r8_delay() {
		int diff = (s8)bus; // signed
		int sum = SP + diff;
		F_Z = F_N = 0;
		F_C = (diff&0xFF) + (SP&0xFF) >= 0x100;
		F_H = (diff&0xF) + (SP&0xF) >= 0x10;
		HL = sum;
		state = CPU_STATE_STALL;
	}
	// 0xF8 LD HL,SP+r8
	void ld_hl_sp_r8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ld_hl_sp_r8_delay;
	}
	// 0xF9 LD SP,HL
	void ld_sp_hl() {
		SP = HL;
		state = CPU_STATE_STALL;
	}

	// 0x77 LD (HL),A
	void ld_hl_a() { state = CPU_STATE_MEMORY_STORE; address = HL; bus = A; instruction = &CPU::nop; }
	// 0x22 LD (HL+),A
	void ld_hli_a() { state = CPU_STATE_MEMORY_STORE; address = HL++; bus = A; instruction = &CPU::nop; }
	// 0x32 LD (HL-),A
	void ld_hld_a() { state = CPU_STATE_MEMORY_STORE; address = HL--; bus = A; instruction = &CPU::nop; }

	void ld_a16_a_pch() {
		address |= (bus << 8);
		bus = A;
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::nop;
	}
	void ld_a16_a_pcl() {
		address = bus;
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ld_a16_a_pch;
	}
	// 0xEA LD (a16),A
	void ld_a16_a() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ld_a16_a_pcl;
	}
	
	void ld_a_a16_pch() {
		address |= (bus << 8);
		state = CPU_STATE_MEMORY_LOAD;
		instruction = &CPU::ld_a_bus;
	}
	void ld_a_a16_pcl() {
		address = bus;
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ld_a_a16_pch;
	}
	// 0xFA LD A,(a16)
	void ld_a_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ld_a_a16_pcl;
	}

	void ld_a16_sp_store_h() {
		address++;
		bus = S;
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::nop;
	}
	void ld_a16_sp_store_l() {
		address |= bus<<8;
		bus = P;
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::ld_a16_sp_store_h;
	}
	void ld_a16_sp_read() {
		address = bus;
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ld_a16_sp_store_l;
	}
	// 0x08 LD (a16),SP
	void ld_a16_sp() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ld_a16_sp_read;
	}

	// 0xE2 LD (C),A
	void ld_io_c_a() {
		state = CPU_STATE_MEMORY_STORE;
		address = 0xFF00 | C;
		bus = A;
		instruction = &CPU::nop;
	}
	// 0xF2 LD A,(C)
	void ld_io_a_c() {
		state = CPU_STATE_MEMORY_LOAD;
		address = 0xFF00 | C;
		instruction = &CPU::ld_a_bus;
	}

	// 0xE0 LDH (a8),A
	void ldh_a8_a() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ldh_a8_a_delay;
	}

	// 0xF0 LDH A,(a8)
	void ldh_a_a8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::ldh_a_a8_delay;
	}



	// 0x03 INC BC
	void inc_bc() { BC++; state = CPU_STATE_STALL; }
	// 0x13 INC DE
	void inc_de() { DE++; state = CPU_STATE_STALL; }
	// 0x23 INC HL
	void inc_hl() { HL++; state = CPU_STATE_STALL; }
	// 0x33 INC SP
	void inc_sp() { SP++; state = CPU_STATE_STALL; }

	// 0x0B DEC BC
	void dec_bc() { BC--; state = CPU_STATE_STALL; }
	// 0x1B DEC DE
	void dec_de() { DE--; state = CPU_STATE_STALL; }
	// 0x2B DEC HL
	void dec_hl() { HL--; state = CPU_STATE_STALL; }
	// 0x3B DEC SP
	void dec_sp() { SP--; state = CPU_STATE_STALL; }



	// 0x07 RLCA
	void rlca() {
		A = (A<<1) | (A>>7);
		F_Z = F_H = F_N = 0;
		F_C = A&1;
	}
	// 0x17 RLA
	void rla() {
		int wide = (A<<1) | F_C;
		A = wide;
		F_Z = F_H = F_N = 0;
		F_C = wide >> 8;
	}
	// 0x0F RRCA
	void rrca() {
		int low = A&1;
		A = (A>>1) | (low<<7);
		F_Z = F_H = F_N = 0;
		F_C = low;
	}
	// 0x1F RRA
	void rra() {
		int low = A&1;
		A = (A>>1) | (F_C<<7);
		F_Z = F_H = F_N = 0;
		F_C = low;
	}



	// 0x27 DAA
	void daa() {
		if (F_N) {
			if (F_H) A += 0xFA;
			if (F_C) A += 0xA0;
		} else {
			int wide = A;
			if ((wide&0xF) > 0x9 || F_H) wide += 0x6;
			if ((wide&0x1F0) > 0x90 || F_C) {
				wide += 0x60;
				F_C = 1;
			} else {
				F_C = 0;
			}
			A = wide;
		}
		F_H = 0;
		F_Z = !A;
	}



	// 0x37 SCF
	void scf() {
		F_C = 1;
		F_N = F_H = 0;
	}
	// 0x3F CCF
	void ccf() {
		F_C = !F_C;
		F_N = F_H = 0;
	}



	void jp_finish() {
		if (condition) {
			address = (bus << 8) | address;
			PC = address;
			state = CPU_STATE_STALL;
		}
	}
	void jp_delay() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jp_finish;
		address = bus;
	}
	void jr_finish() {
		if (condition) {
			PC += (s8)bus;
			state = CPU_STATE_STALL;
		}
	}
	// 0xC3 JP a16
	void jp_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jp_delay;
		condition = true;
	}
	void jp_nz_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jp_delay;
		condition = !F_Z;
	}
	void jp_z_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jp_delay;
		condition = F_Z;
	}
	void jp_nc_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jp_delay;
		condition = !F_C;
	}
	void jp_c_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jp_delay;
		condition = F_C;
	}

	// 0xE9 JP (HL)
	void jp_hl() { PC = HL; }

	// 0x18 JR r8
	void jr_r8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jr_finish;
		condition = true;
	}
	void jr_nz_r8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jr_finish;
		condition = !F_Z;
	}
	void jr_z_r8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jr_finish;
		condition = F_Z;
	}
	void jr_nc_r8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jr_finish;
		condition = !F_C;
	}
	void jr_c_r8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::jr_finish;
		condition = F_C;
	}


	
	// 0x2F CPL
	void cpl() { A = ~A; F_H = 1; F_N = 1; }



	#define CPU_INSTRUCTION_INC(NAME, OPERAND) \
		CPU_INSTRUCTION(inc_ ## NAME, \
			int res = OPERAND + 1; \
			F_H = (OPERAND&0xF) == 0xF; \
			OPERAND = res; \
			F_N = 0; \
			F_Z = !OPERAND;)

	#define CPU_INSTRUCTION_DEC(NAME, OPERAND) \
		CPU_INSTRUCTION(dec_ ## NAME, \
			int res = OPERAND - 1; \
			F_H = (OPERAND&0xF) == 0x0; \
			OPERAND = res; \
			F_N = 1; \
			F_Z = !OPERAND;)

	#define CPU_INSTRUCTION_ADD(NAME, OPERAND) \
		CPU_INSTRUCTION(add_ ## NAME, \
			int res = A + OPERAND; \
			F_N = 0; \
			F_H = (A&0xF) + (OPERAND&0xF) >= 0x10; \
			F_C = res >= 0x100; \
			A = res; \
			F_Z = !A;)

	#define CPU_INSTRUCTION_ADC(NAME, OPERAND) \
		CPU_INSTRUCTION(adc_ ## NAME, \
			int res = A + OPERAND + F_C; \
			F_N = 0; \
			F_H = (A&0xF) + (OPERAND&0xF) + F_C >= 0x10; \
			F_C = res >= 0x100; \
			A = res; \
			F_Z = !A;)

	#define CPU_INSTRUCTION_SUB(NAME, OPERAND) \
		CPU_INSTRUCTION(sub_ ## NAME, \
			int res = A - OPERAND; \
			F_N = 1; \
			F_H = (A&0xF) - (OPERAND&0xF) < 0; \
			F_C = res < 0; \
			A = res; \
			F_Z = !A;)

	#define CPU_INSTRUCTION_SBC(NAME, OPERAND) \
		CPU_INSTRUCTION(sbc_ ## NAME, \
			int res = A - OPERAND - F_C; \
			F_N = 1; \
			F_H = (A&0xF) - (OPERAND&0xF) - F_C < 0; \
			F_C = res < 0; \
			A = res; \
			F_Z = !A;)

	#define CPU_INSTRUCTION_AND(NAME, OPERAND) \
		CPU_INSTRUCTION(and_ ## NAME, \
			A &= OPERAND; F_Z = !A; F_N = F_C = 0; F_H = 1;)

	#define CPU_INSTRUCTION_XOR(NAME, OPERAND) \
		CPU_INSTRUCTION(xor_ ## NAME, \
			A ^= OPERAND; F_Z = !A; F_N = F_H = F_C = 0;)

	#define CPU_INSTRUCTION_OR(NAME, OPERAND) \
		CPU_INSTRUCTION(or_ ## NAME, \
			A |= OPERAND; F_Z = !A; F_N = F_H = F_C = 0;)

	#define CPU_INSTRUCTION_CP(NAME, OPERAND) \
		CPU_INSTRUCTION(cp_ ## NAME, \
			int res = A - OPERAND; \
			F_Z = !(res&0xFF); \
			F_N = 1; \
			F_H = (A&0xF) - (OPERAND&0xF) < 0; \
			F_C = res < 0;)

	// register / bus
	#define CPU_INSTRUCTIONS_ALU(OP_NAME) \
		CPU_INSTRUCTION_ ## OP_NAME(a, A) \
		CPU_INSTRUCTION_ ## OP_NAME(b, B) \
		CPU_INSTRUCTION_ ## OP_NAME(c, C) \
		CPU_INSTRUCTION_ ## OP_NAME(d, D) \
		CPU_INSTRUCTION_ ## OP_NAME(e, E) \
		CPU_INSTRUCTION_ ## OP_NAME(h, H) \
		CPU_INSTRUCTION_ ## OP_NAME(l, L) \
		CPU_INSTRUCTION_ ## OP_NAME(bus, bus)

	// (HL) or PC
	#define CPU_INSTRUCTIONS_ALU_MEM(FN_NAME) \
		CPU_INSTRUCTION(FN_NAME ## _hl, \
			state = CPU_STATE_MEMORY_LOAD; \
			address = HL; \
			instruction = &CPU::FN_NAME ## _bus;) \
		CPU_INSTRUCTION(FN_NAME ## _d8, \
			state = CPU_STATE_READ_PC; \
			instruction = &CPU::FN_NAME ## _bus;)

	CPU_INSTRUCTIONS_ALU(INC)
	CPU_INSTRUCTIONS_ALU(DEC)
	CPU_INSTRUCTIONS_ALU(ADD)
	CPU_INSTRUCTIONS_ALU(ADC)
	CPU_INSTRUCTIONS_ALU(SUB)
	CPU_INSTRUCTIONS_ALU(SBC)
	CPU_INSTRUCTIONS_ALU(AND)
	CPU_INSTRUCTIONS_ALU(XOR)
	CPU_INSTRUCTIONS_ALU(OR)
	CPU_INSTRUCTIONS_ALU(CP)

	CPU_INSTRUCTIONS_ALU_MEM(add)
	CPU_INSTRUCTIONS_ALU_MEM(adc)
	CPU_INSTRUCTIONS_ALU_MEM(sub)
	CPU_INSTRUCTIONS_ALU_MEM(sbc)
	CPU_INSTRUCTIONS_ALU_MEM(and)
	CPU_INSTRUCTIONS_ALU_MEM(xor)
	CPU_INSTRUCTIONS_ALU_MEM(or)
	CPU_INSTRUCTIONS_ALU_MEM(cp)

	// indirect inc and dec
	void inc_hl_store() {
		inc_bus();
		state = CPU_STATE_MEMORY_STORE;
		address = HL;
		instruction = &CPU::nop;
	}
	void inc_hl_() {
		state = CPU_STATE_MEMORY_LOAD;
		address = HL;
		instruction = &CPU::inc_hl_store;
	}
	void dec_hl_store() {
		dec_bus();
		state = CPU_STATE_MEMORY_STORE;
		address = HL;
		instruction = &CPU::nop;
	}
	void dec_hl_() {
		state = CPU_STATE_MEMORY_LOAD;
		address = HL;
		instruction = &CPU::dec_hl_store;
	}

	// 16 bit addition
	#define CPU_INSTRUCTION_ADD_HL(NAME, REG, REG_HI, REG_LO) \
		CPU_INSTRUCTION(add_hl_ ## NAME ## _finish, \
			int res = H + REG_HI + F_C; \
			F_N = 0; \
			F_H = (H&0xF) + (REG_HI&0xF) + F_C >= 0x10; \
			F_C = res >= 0x100; \
			H = res;) \
		CPU_INSTRUCTION(add_hl_ ## NAME, \
			int res = L + REG_LO; \
			L = res; \
			F_C = res >= 0x100; \
			state = CPU_STATE_OP2; \
			instruction = &CPU::add_hl_ ## NAME ## _finish;)

	// 0x09 ADD HL,BC
	CPU_INSTRUCTION_ADD_HL(bc, BC, B, C);
	// 0x19 ADD HL,DE
	CPU_INSTRUCTION_ADD_HL(de, DE, D, E);
	// 0x29 ADD HL,HL
	CPU_INSTRUCTION_ADD_HL(hl, HL, H, L);
	// 0x39 ADD HL,SP
	CPU_INSTRUCTION_ADD_HL(sp, SP, S, P);



	void add_sp_r8_finish() {
		SP = address;
		state = CPU_STATE_STALL;
	}
	void add_sp_r8_delay() {
		int diff = (s8)bus; // signed
		int sum = SP + diff;
		F_Z = F_N = 0;
		F_C = (diff&0xFF) + (SP&0xFF) >= 0x100;
		F_H = (diff&0xF) + (SP&0xF) >= 0x10;
		address = sum;
		state = CPU_STATE_OP2;
		instruction = &CPU::add_sp_r8_finish;
	}
	// 0xE8 ADD SP,r8
	void add_sp_r8() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::add_sp_r8_delay;
	}




	#define CPU_INSTRUCTIONS_PUSHPOP(NAME, NAME_HI, REG_HI, REG_LO) \
		CPU_INSTRUCTION(push_ ## NAME ## _finish, state = CPU_STATE_STALL;) \
		CPU_INSTRUCTION(push_ ## NAME ## _delay, \
			state = CPU_STATE_MEMORY_STORE; \
			address = --SP; \
			bus = REG_LO; \
			instruction = &CPU::push_ ## NAME ## _finish;) \
		CPU_INSTRUCTION(push_ ## NAME, \
			state = CPU_STATE_MEMORY_STORE; \
			address = --SP; \
			bus = REG_HI; \
			instruction = &CPU::push_ ## NAME ## _delay;) \
		CPU_INSTRUCTION(pop_ ## NAME ## _delay, \
			state = CPU_STATE_MEMORY_LOAD; \
			address = SP++; \
			REG_LO = bus; \
			F &= 0xF0; \
			instruction = &CPU::ld_ ## NAME_HI ## _bus;) \
		CPU_INSTRUCTION(pop_ ## NAME, \
			state = CPU_STATE_MEMORY_LOAD; \
			address = SP++; \
			instruction = &CPU::pop_ ## NAME ## _delay;) \

	CPU_INSTRUCTIONS_PUSHPOP(af, a, A, F);
	CPU_INSTRUCTIONS_PUSHPOP(bc, b, B, C);
	CPU_INSTRUCTIONS_PUSHPOP(de, d, D, E);
	CPU_INSTRUCTIONS_PUSHPOP(hl, h, H, L);



	// 0xCB PREFIX CB
	void prefix_cb() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::cb_delegate;
	}



	#define CPU_INSTRUCTION_RST(ADDRESS) \
		CPU_INSTRUCTION(rst_ ## ADDRESS ## _spl, \
			address = --SP; \
			bus = PC; \
			PC = 0x ## ADDRESS; \
			state = CPU_STATE_MEMORY_STORE; \
			instruction = &CPU::nop;) \
		CPU_INSTRUCTION(rst_ ## ADDRESS ## _sph, \
			address = --SP; \
			bus = PC >> 8; \
			state = CPU_STATE_MEMORY_STORE; \
			instruction = &CPU::rst_ ## ADDRESS ## _spl;) \
		CPU_INSTRUCTION(rst_ ## ADDRESS, \
			state = CPU_STATE_OP2; \
			instruction = &CPU::rst_ ## ADDRESS ## _sph;)

	CPU_INSTRUCTION_RST(00)
	CPU_INSTRUCTION_RST(08)
	CPU_INSTRUCTION_RST(10)
	CPU_INSTRUCTION_RST(18)
	CPU_INSTRUCTION_RST(20)
	CPU_INSTRUCTION_RST(28)
	CPU_INSTRUCTION_RST(30)
	CPU_INSTRUCTION_RST(38)



	void call_spl() {
		address--;
		bus = SP;
		SP = address;
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::nop;
	}
	void call_sph() {
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::call_spl;
	}
	void call_pch() {
		if (condition) {
			u16 pc_new = (bus << 8) | address;
			bus = PC >> 8;
			address = SP-1;
			SP = PC;
			PC = pc_new;
			state = CPU_STATE_OP2;
			instruction = &CPU::call_sph;
		}
	}
	void call_pcl() {
		address = bus;
		state = CPU_STATE_READ_PC;
		instruction = &CPU::call_pch;
	}
	// 0xCD CALL a16
	void call_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::call_pcl;
		condition = true;
	}
	void call_nz_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::call_pcl;
		condition = !F_Z;
	}
	void call_nc_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::call_pcl;
		condition = !F_C;
	}
	void call_z_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::call_pcl;
		condition = F_Z;
	}
	void call_c_a16() {
		state = CPU_STATE_READ_PC;
		instruction = &CPU::call_pcl;
		condition = F_C;
	}

	void ret_finish() {
		SP += 2;
		PC |= bus << 8;
		state = CPU_STATE_STALL;
	}
	void ret_spl() {
		address = SP + 1;
		PC = bus;
		state = CPU_STATE_MEMORY_LOAD;
		instruction = &CPU::ret_finish;
	}
	void ret_sph() {
		if (condition) {
			address = SP;
			state = CPU_STATE_MEMORY_LOAD;
			instruction = &CPU::ret_spl;
		}
	}
	// 0xC9 RET
	void ret() {
		address = SP;
		state = CPU_STATE_MEMORY_LOAD;
		instruction = &CPU::ret_spl;
	}
	//0xD9 RETI
	void reti() {
		condition = true;
		state = CPU_STATE_OP2;
		instruction = &CPU::ret_sph;
		ei();
	}

	void ret_c() {
		condition = F_C;
		state = CPU_STATE_OP2;
		instruction = &CPU::ret_sph;
	}
	void ret_z() {
		condition = F_Z;
		state = CPU_STATE_OP2;
		instruction = &CPU::ret_sph;
	}
	void ret_nc() {
		condition = !F_C;
		state = CPU_STATE_OP2;
		instruction = &CPU::ret_sph;
	}
	// 0xC0 RET NZ
	void ret_nz() {
		condition = !F_Z;
		state = CPU_STATE_OP2;
		instruction = &CPU::ret_sph;
	}


	// 0xF3 DI (master disable interrupts)
	void di() { IME = 0; }	
	// 0xFB EI (master enable interrupts)
	void ei() { IME = 1; }



	void irq_spl() {
		address--;
		bus = SP; // old PC low byte
		SP = address;
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::nop;
	}
	void irq() {
		u16 new_pc = address;
		address = SP-1;
		bus = PC>>8; // high byte
		SP = PC; // tmp var
		PC = new_pc;
		state = CPU_STATE_MEMORY_STORE;
		instruction = &CPU::irq_spl;
	}



	Instruction instructions[0x100] = {
		&CPU::nop,        // 0x00
		&CPU::ld_bc_d16,  // 0x01
		&CPU::ld_bc_a,    // 0x02
		&CPU::inc_bc,     // 0x03
		&CPU::inc_b,      // 0x04
		&CPU::dec_b,      // 0x05
		&CPU::ld_b_d8,    // 0x06
		&CPU::rlca,       // 0x07
		&CPU::ld_a16_sp,  // 0x08
		&CPU::add_hl_bc,  // 0x09
		&CPU::ld_a_bc,    // 0x0A
		&CPU::dec_bc,     // 0x0B
		&CPU::inc_c,      // 0x0C
		&CPU::dec_c,      // 0x0D
		&CPU::ld_c_d8,    // 0x0E
		&CPU::rrca,       // 0x0F
		&CPU::stop,       // 0x10
		&CPU::ld_de_d16,  // 0x11
		&CPU::ld_de_a,    // 0x12
		&CPU::inc_de,     // 0x13
		&CPU::inc_d,      // 0x14
		&CPU::dec_d,      // 0x15
		&CPU::ld_d_d8,    // 0x16
		&CPU::rla,        // 0x17
		&CPU::jr_r8,      // 0x18
		&CPU::add_hl_de,  // 0x19
		&CPU::ld_a_de,    // 0x1A
		&CPU::dec_de,     // 0x1B
		&CPU::inc_e,      // 0x1C
		&CPU::dec_e,      // 0x1D
		&CPU::ld_e_d8,    // 0x1E
		&CPU::rra,        // 0x1F
		&CPU::jr_nz_r8,   // 0x20
		&CPU::ld_hl_d16,  // 0x21
		&CPU::ld_hli_a,   // 0x22
		&CPU::inc_hl,     // 0x23
		&CPU::inc_h,      // 0x24
		&CPU::dec_h,      // 0x25
		&CPU::ld_h_d8,    // 0x26
		&CPU::daa,        // 0x27
		&CPU::jr_z_r8,    // 0x28
		&CPU::add_hl_hl,  // 0x29
		&CPU::ld_a_hli,   // 0x2A
		&CPU::dec_hl,     // 0x2B
		&CPU::inc_l,      // 0x2C
		&CPU::dec_l,      // 0x2D
		&CPU::ld_l_d8,    // 0x2E
		&CPU::cpl,        // 0x2F
		&CPU::jr_nc_r8,   // 0x30
		&CPU::ld_sp_d16,  // 0x31
		&CPU::ld_hld_a,   // 0x32
		&CPU::inc_sp,     // 0x33
		&CPU::inc_hl_,    // 0x34
		&CPU::dec_hl_,    // 0x35
		&CPU::ld_hl_d8,   // 0x36
		&CPU::scf,        // 0x37
		&CPU::jr_c_r8,    // 0x38
		&CPU::add_hl_sp,  // 0x39
		&CPU::ld_a_hld,   // 0x3A
		&CPU::dec_sp,     // 0x3B
		&CPU::inc_a,      // 0x3C
		&CPU::dec_a,      // 0x3D
		&CPU::ld_a_d8,    // 0x3E
		&CPU::ccf,        // 0x3F
		&CPU::ld_b_b,     // 0x40
		&CPU::ld_b_c,     // 0x41
		&CPU::ld_b_d,     // 0x42
		&CPU::ld_b_e,     // 0x43
		&CPU::ld_b_h,     // 0x44
		&CPU::ld_b_l,     // 0x45
		&CPU::ld_b_hl,    // 0x46
		&CPU::ld_b_a,     // 0x47
		&CPU::ld_c_b,     // 0x48
		&CPU::ld_c_c,     // 0x49
		&CPU::ld_c_d,     // 0x4A
		&CPU::ld_c_e,     // 0x4B
		&CPU::ld_c_h,     // 0x4C
		&CPU::ld_c_l,     // 0x4D
		&CPU::ld_c_hl,    // 0x4E
		&CPU::ld_c_a,     // 0x4F
		&CPU::ld_d_b,     // 0x50
		&CPU::ld_d_c,     // 0x51
		&CPU::ld_d_d,     // 0x52
		&CPU::ld_d_e,     // 0x53
		&CPU::ld_d_h,     // 0x54
		&CPU::ld_d_l,     // 0x55
		&CPU::ld_d_hl,    // 0x56
		&CPU::ld_d_a,     // 0x57
		&CPU::ld_e_b,     // 0x58
		&CPU::ld_e_c,     // 0x59
		&CPU::ld_e_d,     // 0x5A
		&CPU::ld_e_e,     // 0x5B
		&CPU::ld_e_h,     // 0x5C
		&CPU::ld_e_l,     // 0x5D
		&CPU::ld_e_hl,    // 0x5E
		&CPU::ld_e_a,     // 0x5F
		&CPU::ld_h_b,     // 0x60
		&CPU::ld_h_c,     // 0x61
		&CPU::ld_h_d,     // 0x62
		&CPU::ld_h_e,     // 0x63
		&CPU::ld_h_h,     // 0x64
		&CPU::ld_h_l,     // 0x65
		&CPU::ld_h_hl,    // 0x66
		&CPU::ld_h_a,     // 0x67
		&CPU::ld_l_b,     // 0x68
		&CPU::ld_l_c,     // 0x69
		&CPU::ld_l_d,     // 0x6A
		&CPU::ld_l_e,     // 0x6B
		&CPU::ld_l_h,     // 0x6C
		&CPU::ld_l_l,     // 0x6D
		&CPU::ld_l_hl,    // 0x6E
		&CPU::ld_l_a,     // 0x6F
		&CPU::ld_hl_b,    // 0x70
		&CPU::ld_hl_c,    // 0x71
		&CPU::ld_hl_d,    // 0x72
		&CPU::ld_hl_e,    // 0x73
		&CPU::ld_hl_h,    // 0x74
		&CPU::ld_hl_l,    // 0x75
		&CPU::halt,       // 0x76
		&CPU::ld_hl_a,    // 0x77
		&CPU::ld_a_b,     // 0x78
		&CPU::ld_a_c,     // 0x79
		&CPU::ld_a_d,     // 0x7A
		&CPU::ld_a_e,     // 0x7B
		&CPU::ld_a_h,     // 0x7C
		&CPU::ld_a_l,     // 0x7D
		&CPU::ld_a_hl,    // 0x7E
		&CPU::ld_a_a,     // 0x7F
		&CPU::add_b,      // 0x80
		&CPU::add_c,      // 0x81
		&CPU::add_d,      // 0x82
		&CPU::add_e,      // 0x83
		&CPU::add_h,      // 0x84
		&CPU::add_l,      // 0x85
		&CPU::add_hl,     // 0x86
		&CPU::add_a,      // 0x87
		&CPU::adc_b,      // 0x88
		&CPU::adc_c,      // 0x89
		&CPU::adc_d,      // 0x8A
		&CPU::adc_e,      // 0x8B
		&CPU::adc_h,      // 0x8C
		&CPU::adc_l,      // 0x8D
		&CPU::adc_hl,     // 0x8E
		&CPU::adc_a,      // 0x8F
		&CPU::sub_b,      // 0x90
		&CPU::sub_c,      // 0x91
		&CPU::sub_d,      // 0x92
		&CPU::sub_e,      // 0x93
		&CPU::sub_h,      // 0x94
		&CPU::sub_l,      // 0x95
		&CPU::sub_hl,     // 0x96
		&CPU::sub_a,      // 0x97
		&CPU::sbc_b,      // 0x98
		&CPU::sbc_c,      // 0x99
		&CPU::sbc_d,      // 0x9A
		&CPU::sbc_e,      // 0x9B
		&CPU::sbc_h,      // 0x9C
		&CPU::sbc_l,      // 0x9D
		&CPU::sbc_hl,     // 0x9E
		&CPU::sbc_a,      // 0x9F
		&CPU::and_b,      // 0xA0
		&CPU::and_c,      // 0xA1
		&CPU::and_d,      // 0xA2
		&CPU::and_e,      // 0xA3
		&CPU::and_h,      // 0xA4
		&CPU::and_l,      // 0xA5
		&CPU::and_hl,     // 0xA6
		&CPU::and_a,      // 0xA7
		&CPU::xor_b,      // 0xA8
		&CPU::xor_c,      // 0xA9
		&CPU::xor_d,      // 0xAA
		&CPU::xor_e,      // 0xAB
		&CPU::xor_h,      // 0xAC
		&CPU::xor_l,      // 0xAD
		&CPU::xor_hl,     // 0xAE
		&CPU::xor_a,      // 0xAF
		&CPU::or_b,       // 0xB0
		&CPU::or_c,       // 0xB1
		&CPU::or_d,       // 0xB2
		&CPU::or_e,       // 0xB3
		&CPU::or_h,       // 0xB4
		&CPU::or_l,       // 0xB5
		&CPU::or_hl,      // 0xB6
		&CPU::or_a,       // 0xB7
		&CPU::cp_b,       // 0xB8
		&CPU::cp_c,       // 0xB9
		&CPU::cp_d,       // 0xBA
		&CPU::cp_e,       // 0xBB
		&CPU::cp_h,       // 0xBC
		&CPU::cp_l,       // 0xBD
		&CPU::cp_hl,      // 0xBE
		&CPU::cp_a,       // 0xBF
		&CPU::ret_nz,     // 0xC0
		&CPU::pop_bc,     // 0xC1
		&CPU::jp_nz_a16,  // 0xC2
		&CPU::jp_a16,     // 0xC3
		&CPU::call_nz_a16,// 0xC4
		&CPU::push_bc,    // 0xC5
		&CPU::add_d8,     // 0xC6
		&CPU::rst_00,     // 0xC7
		&CPU::ret_z,      // 0xC8
		&CPU::ret,        // 0xC9
		&CPU::jp_z_a16,   // 0xCA
		&CPU::prefix_cb,  // 0xCB
		&CPU::call_z_a16, // 0xCC
		&CPU::call_a16,   // 0xCD
		&CPU::adc_d8,     // 0xCE
		&CPU::rst_08,     // 0xCF
		&CPU::ret_nc,     // 0xD0
		&CPU::pop_de,     // 0xD1
		&CPU::jp_nc_a16,  // 0xD2
		&CPU::ill,        // 0xD3
		&CPU::call_nc_a16,// 0xD4
		&CPU::push_de,    // 0xD5
		&CPU::sub_d8,     // 0xD6
		&CPU::rst_10,     // 0xD7
		&CPU::ret_c,      // 0xD8
		&CPU::reti,       // 0xD9
		&CPU::jp_c_a16,   // 0xDA
		&CPU::ill,        // 0xDB
		&CPU::call_c_a16, // 0xDC
		&CPU::ill,        // 0xDD
		&CPU::sbc_d8,     // 0xDE
		&CPU::rst_18,     // 0xDF
		&CPU::ldh_a8_a,   // 0xE0
		&CPU::pop_hl,     // 0xE1
		&CPU::ld_io_c_a,  // 0xE2
		&CPU::ill,        // 0xE3
		&CPU::ill,        // 0xE4
		&CPU::push_hl,    // 0xE5
		&CPU::and_d8,     // 0xE6
		&CPU::rst_20,     // 0xE7
		&CPU::add_sp_r8,  // 0xE8
		&CPU::jp_hl,      // 0xE9
		&CPU::ld_a16_a,   // 0xEA
		&CPU::ill,        // 0xEB
		&CPU::ill,        // 0xEC
		&CPU::ill,        // 0xED
		&CPU::xor_d8,     // 0xEE
		&CPU::rst_28,     // 0xEF
		&CPU::ldh_a_a8,   // 0xF0
		&CPU::pop_af,     // 0xF1
		&CPU::ld_io_a_c,  // 0xF2
		&CPU::di,         // 0xF3
		&CPU::ill,        // 0xF4
		&CPU::push_af,    // 0xF5
		&CPU::or_d8,      // 0xF6
		&CPU::rst_30,     // 0xF7
		&CPU::ld_hl_sp_r8,// 0xF8
		&CPU::ld_sp_hl,   // 0xF9
		&CPU::ld_a_a16,   // 0xFA
		&CPU::ei,         // 0xFB
		&CPU::ill,        // 0xFC
		&CPU::ill,        // 0xFD
		&CPU::cp_d8,      // 0xFE
		&CPU::rst_38,     // 0xFF
	};

	struct InstructionInfo {
		const char *mnemonic;
		int length; // in bytes
	};

	InstructionInfo instruction_infos[0x100] = {
		{ "NOP",         1 },
		{ "LD BC,d16",   3 },
		{ "LD (BC),A",   1 },
		{ "INC BC",      1 },
		{ "INC B",       1 },
		{ "DEC B",       1 },
		{ "LD B,d8",     2 },
		{ "RLCA",        1 },
		{ "LD (a16),SP", 3 },
		{ "ADD HL,BC",   1 },
		{ "LD A,(BC)",   1 },
		{ "DEC BC",      1 },
		{ "INC C",       1 },
		{ "DEC C",       1 },
		{ "LD C,d8",     2 },
		{ "RRCA",        1 },
		{ "STOP 0",      2 },
		{ "LD DE,d16",   3 },
		{ "LD (DE),A",   1 },
		{ "INC DE",      1 },
		{ "INC D",       1 },
		{ "DEC D",       1 },
		{ "LD D,d8",     2 },
		{ "RLA",         1 },
		{ "JR r8",       2 },
		{ "ADD HL,DE",   1 },
		{ "LD A,(DE)",   1 },
		{ "DEC DE",      1 },
		{ "INC E",       1 },
		{ "DEC E",       1 },
		{ "LD E,d8",     2 },
		{ "RRA",         1 },
		{ "JR NZ,r8",    2 },
		{ "LD HL,d16",   3 },
		{ "LD (HL+),A",  1 },
		{ "INC HL",      1 },
		{ "INC H",       1 },
		{ "DEC H",       1 },
		{ "LD H,d8",     2 },
		{ "DAA",         1 },
		{ "JR Z,r8",     2 },
		{ "ADD HL,HL",   1 },
		{ "LD A,(HL+)",  1 },
		{ "DEC HL",      1 },
		{ "INC L",       1 },
		{ "DEC L",       1 },
		{ "LD L,d8",     2 },
		{ "CPL",         1 },
		{ "JR NC,r8",    2 },
		{ "LD SP,d16",   3 },
		{ "LD (HL-),A",  1 },
		{ "INC SP",      1 },
		{ "INC (HL)",    1 },
		{ "DEC (HL)",    1 },
		{ "LD (HL),d8",  2 },
		{ "SCF",         1 },
		{ "JR C,r8",     2 },
		{ "ADD HL,SP",   1 },
		{ "LD A,(HL-)",  1 },
		{ "DEC SP",      1 },
		{ "INC A",       1 },
		{ "DEC A",       1 },
		{ "LD A,d8",     2 },
		{ "CCF",         1 },
		{ "LD B,B",      1 },
		{ "LD B,C",      1 },
		{ "LD B,D",      1 },
		{ "LD B,E",      1 },
		{ "LD B,H",      1 },
		{ "LD B,L",      1 },
		{ "LD B,(HL)",   1 },
		{ "LD B,A",      1 },
		{ "LD C,B", 1 },
		{ "LD C,C", 1 },
		{ "LD C,D", 1 },
		{ "LD C,E", 1 },
		{ "LD C,H", 1 },
		{ "LD C,L", 1 },
		{ "LD C,(HL)", 1 },
		{ "LD C,A", 1 },
		{ "LD D,B", 1 },
		{ "LD D,C", 1 },
		{ "LD D,D", 1 },
		{ "LD D,E", 1 },
		{ "LD D,H", 1 },
		{ "LD D,L", 1 },
		{ "LD D,(HL)", 1 },
		{ "LD D,A", 1 },
		{ "LD E,B", 1 },
		{ "LD E,C", 1 },
		{ "LD E,D", 1 },
		{ "LD E,E", 1 },
		{ "LD E,H", 1 },
		{ "LD E,L", 1 },
		{ "LD E,(HL)", 1 },
		{ "LD E,A", 1 },
		{ "LD H,B", 1 },
		{ "LD H,C", 1 },
		{ "LD H,D", 1 },
		{ "LD H,E", 1 },
		{ "LD H,H", 1 },
		{ "LD H,L", 1 },
		{ "LD H,(HL)", 1 },
		{ "LD H,A", 1 },
		{ "LD L,B", 1 },
		{ "LD L,C", 1 },
		{ "LD L,D", 1 },
		{ "LD L,E", 1 },
		{ "LD L,H", 1 },
		{ "LD L,L", 1 },
		{ "LD L,(HL)", 1 },
		{ "LD L,A", 1 },
		{ "LD (HL),B", 1 },
		{ "LD (HL),C", 1 },
		{ "LD (HL),D", 1 },
		{ "LD (HL),E", 1 },
		{ "LD (HL),H", 1 },
		{ "LD (HL),L", 1 },
		{ "HALT", 1 },
		{ "LD (HL),A", 1 },
		{ "LD A,B",      1 },
		{ "LD A,C", 1 },
		{ "LD A,D", 1 },
		{ "LD A,E", 1 },
		{ "LD A,H", 1 },
		{ "LD A,L", 1 },
		{ "LD A,(HL)", 1 },
		{ "LD A,A", 1 },
		{ "ADD A,B", 1 },
		{ "ADD A,C", 1 },
		{ "ADD A,D", 1 },
		{ "ADD A,E", 1 },
		{ "ADD A,H", 1 },
		{ "ADD A,L", 1 },
		{ "ADD A,(HL)", 1 },
		{ "ADD A,A", 1 },
		{ "ADC A,B", 1 },
		{ "ADC A,C", 1 },
		{ "ADC A,D", 1 },
		{ "ADC A,E", 1 },
		{ "ADC A,H", 1 },
		{ "ADC A,L", 1 },
		{ "ADC A,(HL)", 1 },
		{ "ADC A,A", 1 },
		{ "SUB B", 1 },
		{ "SUB C", 1 },
		{ "SUB D", 1 },
		{ "SUB E", 1 },
		{ "SUB H", 1 },
		{ "SUB L", 1 },
		{ "SUB (HL)", 1 },
		{ "SUB A", 1 },
		{ "SBC A,B", 1 },
		{ "SBC A,C", 1 },
		{ "SBC A,D", 1 },
		{ "SBC A,E", 1 },
		{ "SBC A,H", 1 },
		{ "SBC A,L", 1 },
		{ "SBC A,(HL)", 1 },
		{ "SBC A,A", 1 },
		{ "AND B", 1 },
		{ "AND C", 1 },
		{ "AND D", 1 },
		{ "AND E", 1 },
		{ "AND H", 1 },
		{ "AND L", 1 },
		{ "AND (HL)", 1 },
		{ "AND A", 1 },
		{ "XOR B", 1 },
		{ "XOR C", 1 },
		{ "XOR D", 1 },
		{ "XOR E", 1 },
		{ "XOR H", 1 },
		{ "XOR L", 1 },
		{ "XOR (HL)", 1 },
		{ "XOR A",       1 },
		{ "OR B", 1 },
		{ "OR C",        1 },
		{ "OR D", 1 },
		{ "OR E", 1 },
		{ "OR H", 1 },
		{ "OR L", 1 },
		{ "OR (HL)", 1 },
		{ "OR A", 1 },
		{ "CP B", 1 },
		{ "CP C", 1 },
		{ "CP D", 1 },
		{ "CP E", 1 },
		{ "CP H", 1 },
		{ "CP L", 1 },
		{ "CP (HL)", 1 },
		{ "CP A", 1 },
		{ "RET NZ", 1 },
		{ "POP BC", 1 },
		{ "JP NZ,a16", 3 },
		{ "JP a16",      3 },
		{ "CALL NZ,a16", 3 },
		{ "PUSH BC", 1 },
		{ "ADD A,d8", 2 },
		{ "RST 00H", 1 },
		{ "RET Z", 1 },
		{ "RET",         1 },
		{ "JP Z,a16", 3 },
		{ "PREFIX CB",   1 },
		{ "CALL Z,a16", 3 },
		{ "CALL a16",    3 },
		{ "ADC A,d8", 2 },
		{ "RST 08H", 1 },
		{ "RET NC", 1 },
		{ "POP DE", 1 },
		{ "JP NC,a16", 3 },
		{ "ILL", 0 },
		{ "CALL NC,a16", 3 },
		{ "PUSH DE",     1 },
		{ "SUB d8",      2 },
		{ "RST 10H",     1 },
		{ "RET C",       1 },
		{ "RETI",        1 },
		{ "JP C,a16",    3 },
		{ "ILL",         0 },
		{ "CALL C,a16",  3 },
		{ "ILL",         0 },
		{ "SBC A,d8",    2 },
		{ "RST 18H",     1 },
		{ "LDH (a8),A",  2 },
		{ "POP HL",      1 },
		{ "LD (C),A",    2 },
		{ "ILL",         0 },
		{ "ILL",         0 },
		{ "PUSH HL",     1 },
		{ "AND d8",      2 },
		{ "RST 20H",     1 },
		{ "ADD SP,r8",   2 },
		{ "JP (HL)",     1 },
		{ "LD (a16),A",  3 },
		{ "ILL",         0 },
		{ "ILL",         0 },
		{ "ILL",         0 },
		{ "XOR d8",      2 },
		{ "RST 28H",     1 },
		{ "LDH A,(a8)",  2 },
		{ "POP AF",      1 },
		{ "LD A,(C)",    2 },
		{ "DI",          1 },
		{ "ILL",         0 },
		{ "PUSH AF",     1 },
		{ "OR d8",       2 },
		{ "RST 30H",     1 },
		{ "LD HL,SP+r8", 2 },
		{ "LD SP,HL",    1 },
		{ "LD A,(a16)",  3 },
		{ "EI",          1 },
		{ "ILL",         0 },
		{ "ILL",         0 },
		{ "CP d8",       2 },
		{ "RST 38H",     1 },
	};
};

enum PPUState {
	PPU_STATE_HALT,
	PPU_STATE_OAM_SEARCH, // mode 2
	PPU_STATE_PIXEL_TRANSFER, // mode 3
	PPU_STATE_HBLANK, // mode 0
	PPU_STATE_VBLANK // mode 1
};

// picture processing unit
struct PPU {
	u8 framebuffer[LCD_HEIGHT*LCD_WIDTH]; // pixel values 0-3

	PPUState state;
	u64 cycle_count;
	u64 cycle_begin; // when did the current mode begin

	bool vsync = false;
	u64 frame_count = 0;

	u8 line_objs[OBJ_PER_LINE];
	int line_obj_count;
	int line_obj_index;

	int LX; // current x position in line

	u8 pixel_fifo[16];
	int pixel_fifo_begin = 0;
	int pixel_fifo_end = 0;

	void reset();
	void step();

// private:
	GameBoy *gb;
};

struct GameBoy {
	CPU cpu;
	PPU ppu;
	Memory memory;

	// input
	ButtonState button_right;
	ButtonState button_left;
	ButtonState button_up;
	ButtonState button_down;
	ButtonState button_a;
	ButtonState button_b;
	ButtonState button_select;
	ButtonState button_start;

	bool running = false;

	void init();

	void loadROM(const char *filepath);

	void reset();
	void step();

	void enableLCD(); // basically resets the LCD

	void onIORead(u16 address);
	u8 onIOWrite(u16 address, u8 value); // might return updated value
};

void PPU::reset() {
	memset(framebuffer, 0, sizeof(framebuffer));
	state = PPU_STATE_OAM_SEARCH;
	cycle_count = 0;
	cycle_begin = 0;
	vsync = false;
	frame_count = 0;
}

void PPU::step() {
	u8 *vram = gb->memory.vram;
	OAM *oam = &gb->memory.oam;
	IO *io = &gb->memory.io;

	cycle_count += 4;

	int obj_h = io->LCDC_obj_size ? 16 : 8;

	// (R) 0: HBLANK, 1: VBLANK, 2: OAM-RAM, 3: transfer data to LCD
	switch (state) {
	case PPU_STATE_OAM_SEARCH:
	{
		int min_x = 0; // 0=invisible
		if (line_obj_count > 0) {
			min_x = oam->objs[line_objs[line_obj_count-1]].x;
		}

		for (int i = 0; i < ARRAY_COUNT(oam->objs); i++) {
			if (line_obj_count >= ARRAY_COUNT(line_objs)) break;
			if (oam->objs[i].x == 0) continue; // invisible
			if (oam->objs[i].x < min_x) continue;
			if (line_obj_count > 0 && i == line_objs[line_obj_count-1]) continue;
			if (io->LY+16 >= oam->objs[i].y
			 && io->LY+16 <  oam->objs[i].y+obj_h) {
				line_objs[line_obj_count++] = i;
				break;
			}
		}

		io->STAT_mode = 2; // OAM search
		if (cycle_count - cycle_begin >= OAM_SEARCH_CYCLES) {
			state = PPU_STATE_PIXEL_TRANSFER;
			cycle_begin = cycle_count;

			// init pixel transfer
			LX = 0;
			pixel_fifo_begin = pixel_fifo_end = 0;
			line_obj_index = 0;
		}
	} break;
	case PPU_STATE_PIXEL_TRANSFER:
	{
		// prepare palettes for lookup
		u8 BGP[4];
		u8 OBP0[4];
		u8 OBP1[4];
		for (int i = 0; i < 4; i++) {
			BGP[i]  = (io->BGP  >> (2*i)) & 0x03;
			OBP0[i] = (io->OBP0 >> (2*i)) & 0x03;
			OBP1[i] = (io->OBP1 >> (2*i)) & 0x03;
		}
		u8 *palettes[] = { BGP, OBP0, OBP1 };
		// vram addresses
		int map_adr  = io->LCDC_bg_tile_map ? 0x1C00 : 0x1800;
		int tile_adr = io->LCDC_tile_data   ? 0x0000 : 0x1000;

		int LY = io->LY;
		assert(LY >= 0 && LY < LCD_HEIGHT);
		assert(LX >= 0 && LX < LCD_WIDTH);

		int cycle = (cycle_count - cycle_begin - 1) / 4;
		// try to draw 8 pixels per cycle
		for (int px = 0; px < 8; px++) {
			// discard pixels (subtile scrolling)
			if (LX == 0) pixel_fifo_begin += io->SCX&0x7;

			// less than 8 pixels in the fifo
			while (pixel_fifo_end - pixel_fifo_begin < 8) {
				int fifo_pos = pixel_fifo_end - pixel_fifo_begin;
				if (fifo_pos < 0) fifo_pos = 0;

				u8 llb = 0; // line low bits
				u8 lhb = 0; // line high bits
				// fetch bg tile
				if (io->LCDC_bg_enable) {
					int sy = (LY + io->SCY) & 0xFF;
					int sx = (LX + io->SCX + fifo_pos) & 0xFF;
					int ty = sy/8;
					int tx = sx/8;
					int ti = io->LCDC_tile_data ?
						vram[map_adr+ty*0x20+tx] :
						((s8*)vram)[map_adr+ty*0x20+tx]; // signed
					// line with high and low bits
					llb = vram[tile_adr+2*(8*ti+(sy&0x7))+0];
					lhb = vram[tile_adr+2*(8*ti+(sy&0x7))+1];
				}

				// convert line to 8 pixels
				for (int x = 0; x < 8; x++) {
					int lb = (llb >> (7-x)) & 0x1;
					int hb = (lhb >> (7-x)) & 0x1;
					int pixel = (hb<<1) | lb;

					pixel_fifo[pixel_fifo_end++ & 0xF] = pixel | (0<<2);
				}
			}

			// draw objs
			if (io->LCDC_obj_enable) {
				while (line_obj_index < line_obj_count) {
					OAM::OBJ *obj = &oam->objs[line_objs[line_obj_index]];
					if (obj->x > LX+8) break; // not yet

					// blit obj over the first 8 pixel in the fifo
					int sy = (LY - obj->y) & 0x7;
					if (obj->ATTRIB_flip_y) sy = 7-sy;
					// line with high and low bits
					u8 llb = vram[2*(8*obj->tile+sy)+0];
					u8 lhb = vram[2*(8*obj->tile+sy)+1];

					for (int x = 0; x < 8; x++) {
						int shift = obj->ATTRIB_flip_x ? x : (7-x);
						int lb = (llb >> shift) & 0x1;
						int hb = (lhb >> shift) & 0x1;
						int pixel = (hb<<1) | lb;
						if (pixel == 0) continue; // transparent

						pixel_fifo[(pixel_fifo_begin+x) & 0xF] = pixel
							| ((1+obj->ATTRIB_palette)<<2);
					}

					line_obj_index++; // next
				}
			}

			int pixel = pixel_fifo[pixel_fifo_begin++ & 0xF];
			framebuffer[LY*LCD_WIDTH+LX] = palettes[(pixel>>2)&0x3][pixel&0x3];

			LX++;
		}

		io->STAT_mode = 3; // pixel transfer
		//if (cycle_count - cycle_begin >= PIXEL_TRANSFER_CYCLES) {
		if (LX >= LCD_WIDTH) {
			state = PPU_STATE_HBLANK;
			// don't reset cycle_begin (this state lasts as long as needed)
		}
	} break;
	case PPU_STATE_HBLANK:
		io->STAT_mode = 0; // H-Blank
		if (cycle_count - cycle_begin >= PIXEL_TRANSFER_CYCLES + HBLANK_CYCLES) {
			io->LY++;
			if (io->LY == LCD_HEIGHT) {
				state = PPU_STATE_VBLANK;
				vsync = true; // do vsync
				frame_count++;
				io->IF_vblank = 1; // raise IRQ
				gb->cpu.halted = false;
			} else {
				state = PPU_STATE_OAM_SEARCH;
				// init oam search
				line_obj_count = 0;
			}
			cycle_begin = cycle_count;
		}
		break;
	case PPU_STATE_VBLANK:
		vsync = false;
		io->STAT_mode = 1; // V-Blank
		io->LY = LCD_HEIGHT + (cycle_count - cycle_begin)
			/ (OAM_SEARCH_CYCLES + PIXEL_TRANSFER_CYCLES + HBLANK_CYCLES);
		if (cycle_count - cycle_begin >= VBLANK_CYCLES) {
			// frame complete
			io->LY = 0;
			cycle_begin = cycle_count;
			io->IF_vblank = 0; // clear IRQ

			state = PPU_STATE_OAM_SEARCH;
			// init oam search
			line_obj_count = 0;
		}
		break;
	default: break;
	}
}

void GameBoy::init() {
	memory.gb = this;
	cpu.memory = &memory;
	ppu.gb = this;
	reset();
}

void GameBoy::reset() {
	cpu.reset();
	ppu.reset();
	memory.init();
	running = false;
}

void GameBoy::step() {
	cpu.step();
	ppu.step();
}

void GameBoy::enableLCD() {
	ppu.state = PPU_STATE_OAM_SEARCH;
	ppu.cycle_begin = ppu.cycle_count;
	ppu.vsync = false;

	memory.io.LY = 0;
}

void GameBoy::onIORead(u16 address) {
	switch (address) {
	case REG_INPUT:
		if (!memory.io.INPUT_select_buttons) {
			memory.io.INPUT_a = !button_a.down();
			memory.io.INPUT_b = !button_b.down();
			memory.io.INPUT_select = !button_select.down();
			memory.io.INPUT_start = !button_start.down();
		} else if (!memory.io.INPUT_select_directions) {
			memory.io.INPUT_right = !button_right.down();
			memory.io.INPUT_left = !button_left.down();
			memory.io.INPUT_up = !button_up.down();
			memory.io.INPUT_down = !button_down.down();
		}
		break;
	case REG_DIV:
		break;
	case REG_TIMA:
	case REG_TMA:
	case REG_TAC:
		LOGI("timer not implemented!");
		break;
	default: break;
	}
}
u8 GameBoy::onIOWrite(u16 address, u8 value) {
	switch (address) {
	case REG_DIV:
		return 0; // writes reset DIV to 0
	case REG_LCDC:
	{
		bool was_enabled = memory.io.LCDC_enable;
		memory.io.LCDC = value;
		if (!was_enabled && memory.io.LCDC_enable) {
			enableLCD();
		}
	} break;
	case REG_DMA:
	{
		// instant dma transfer TODO: do this within 160 cycles
		assert(sizeof(memory.oam) == 0xA0);
		u8 *src = memory.map(value<<8);
		memcpy((void*)&memory.oam, src, 0xA0);
	} break;
	default: break;
	}

	return value;
}

u8 Memory::load8(u16 address) {
	if ((address >= ADR_IO && address < ADR_IO+SIZE_IO) || address == ADR_IE) {
		gb->onIORead(address - ADR_IO);
	}
	return *map(address);
}

void Memory::store8(u16 address, u8 value) {
	if ((address >= ADR_IO && address < ADR_IO+SIZE_IO) || address == ADR_IE) {
		value = gb->onIOWrite(address - ADR_IO, value);
	}
	if (address < ADR_CART_BANK0 + 2*SIZE_ROM_BANK) { // rom
		if (address == 0x2000) { // switch bank
			assert(value * SIZE_ROM_BANK < rom_size);
			rom_bank1 = &rom[value * SIZE_ROM_BANK];
		} else {
			LOGI("attempt to write 0x%02X to ROM at 0x%04X", value, address);
		}
	} else {
		*map(address) = value;
	}
}

u8 *Memory::map(u16 address) {
	if (address < sizeof(boot_rom) && !io.BOOT) {
		return &boot_rom[address];
	} else if (address >= ADR_CART_BANK0
		    && address <  ADR_CART_BANK0 + SIZE_ROM_BANK) {
		return &rom_bank0[address - ADR_CART_BANK0];
	} else if (address >= ADR_CART_BANK1
		    && address <  ADR_CART_BANK1 + SIZE_ROM_BANK) {
		return &rom_bank1[address - ADR_CART_BANK1];
	} else if (address >= ADR_VRAM
		    && address <  ADR_VRAM + SIZE_VRAM) {
		return &vram[address - ADR_VRAM];
	} else if (address >= ADR_RAM_EXTERNAL
		    && address <  ADR_RAM_EXTERNAL + SIZE_RAM) {
		LOGE("sram not implemented");
		gb->cpu.DEBUG_not_implemented_error = true;
	 	static u8 sram = 0; // unimplemented
	 	return &sram;
	} else if (address >= ADR_RAM_INTERNAL_BANK0
		    && address <  ADR_RAM_INTERNAL_BANK0 + SIZE_RAM) {
		return &ram[address - ADR_RAM_INTERNAL_BANK0];
	} else if (address >= ADR_RAM_INTERNAL_MIRROR
		    && address <  ADR_OAM) { // size != SIZE_RAM
		LOGI("accessing RAM (echo) at 0x%04X", address);
		return &ram[address - ADR_RAM_INTERNAL_MIRROR];
	} else if (address >= ADR_OAM
		    && address < ADR_OAM + SIZE_OAM) {
		return &((u8*)&oam)[address - ADR_OAM];
	} else if (address >= ADR_EMPTY && address < ADR_IO) {
		static u8 empty = 0;
		return &empty;
	} else if (address >= ADR_IO && address < ADR_HRAM) {
		//LOGI("accessing IO at 0x%04X", address);
		return &((u8*)&io)[address - ADR_IO];
	} else if (address >= ADR_HRAM
		    && address <  ADR_HRAM + SIZE_HRAM) {
		return &hram[address - ADR_HRAM];
	} else if (address == ADR_IE) { // interrupt enable register
		return &io.IE;
	} else {
		LOGE("address space not implemented 0x%04X", address);
		gb->cpu.DEBUG_not_implemented_error = true;
		return ram;
	}
}

#pragma pack(push, 1)
struct CartridgeHeader { // start at 0x100 in file
	u8 instructions[0x4];
	u8 nintendo_graphic[0x30];
	u8 game_title[0x0F];
	u8 gameboy_color; // true iff equal to 0x80
	u8 license_h;
	u8 license_l;
	u8 super_gameboy_funcs; // supported iff equal to 0x03
	u8 cartridge_type;
	u8 cartridge_size;
	u8 ram_size;
	u8 region_code; // 0: japan, 1: non-japanese
	u8 license_code; // 33: check license_h/l, 79 Accolade, A4: Konami
	u8 mask_rom_version;
	u8 complement_check; // adjusts sum so it's 0 in the end
	u8 checksum_h;
	u8 checksum_l;

	bool isChecksumCorrect();
};
#pragma pack(pop)

bool CartridgeHeader::isChecksumCorrect() {
	u16 sum = 25;
	for (u8 *b = game_title; b != &checksum_h; b++) sum += *b;
	return (sum & 0xFF) == 0; // lower byte needs to be zero
}

void GameBoy::loadROM(const char *filepath) {
	memory.rom_bank0 = nullptr;
	memory.rom_bank1 = nullptr;
	if (memory.rom) { delete [] memory.rom; }
	memory.rom = readDataFromFile(filepath, &memory.rom_size);
	if (!memory.rom) return;
	CartridgeHeader *header = (CartridgeHeader*)(memory.rom+0x100);
	if (!header->isChecksumCorrect()) {
		LOGE("cartridge has incorrect checksum");
		exit(2);
	}
	memory.rom_bank0 = memory.rom;
	memory.rom_bank1 = memory.rom+SIZE_ROM_BANK;
	//LOGI("cartridge type: 0x%02X", header->cartridge_type);
}

void CPU::reset() {
	cycle_count = 0;
	halted = false;
	state = CPU_STATE_FETCH;
	address = 0;
	bus = 0;
	instruction = nullptr;
	condition = false;

	AF = 0;
	BC = 0;
	DE = 0;
	HL = 0;
	SP = 0;
	PC = 0;

	DEBUG_not_implemented_error = false;
}

void CPU::step() {
	// update timers
	// CPU_HZ = 1<<22; 4 MiHz
	// DIV_HZ = 1<<14; 16 KiH
	if ((cycle_count&0xFF) == 0)  memory->io.DIV++;

	if (memory->io.TAC_stop == 1) { // timer running
		int period = 0;
		switch (memory->io.TAC_clock) {
			case 0: period = 1<<10; break; // 1<<12 Hz
			case 1: period = 1<<4; break; // 1<<18 Hz
			case 2: period = 1<<6; break; // 1<<16 Hz
			case 3: period = 1<<8; break; // 1<<14 Hz
		}
		if ((cycle_count&(period-1)) == 0) {
			if (memory->io.TIMA == 0xFF) {
				memory->io.TIMA = memory->io.TMA; // reset
				memory->io.IF_timer = 1; // raise interrupt
				halted = false;
			} else {
				memory->io.TIMA++;
			}
		}
	}

	cycle_count++;

	CPUState old_state = state;
	state = CPU_STATE_IDLE0;

	switch (old_state) {
	case CPU_STATE_FETCH:
		// TODO: handle more IRQs
		if (IME) {
			if (memory->io.IE_vblank && memory->io.IF_vblank) {
				halted = false;
				address = IRQ_ADR_VBLANK;
				instruction = &CPU::irq;
				memory->io.IF_vblank = 0;
				break;
			}
			if (memory->io.IE_timer && memory->io.IF_timer) {
				halted = false;
				address = IRQ_ADR_TIMER;
				instruction = &CPU::irq;
				memory->io.IF_timer = 0;
				break;
			}
		}
		if (!halted) {
			bus = memory->load8(PC++);
			instruction = instructions[bus];
		}
		break;
	case CPU_STATE_MEMORY_LOAD:
		bus = memory->load8(address);
		break;
	case CPU_STATE_MEMORY_STORE:
		memory->store8(address, bus);
		break;
	case CPU_STATE_READ_PC:
		bus = memory->load8(PC++);
		break;
	case CPU_STATE_STALL:
		instruction = &CPU::nop;
		break;
	default: break;
	}

	cycle_count += 2;

	state = CPU_STATE_FETCH;
	if (instruction == nullptr) {
		LOGE("unimplemented instruction 0x%02X %s",
			bus, instruction_infos[bus].mnemonic);
		DEBUG_not_implemented_error = true;
		return;
	}
	(this->*instruction)();
	
	cycle_count++;
}
