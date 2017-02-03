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
const int REG_BGP  = 0X47;

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

	// FF10 - NR10 - Channel 1 Sweep register (R/W)
	union {
		u8 NR10;
		struct {
			u8 NR10_sweep_shift : 3; // X(t) = X(t-1) +/- X(t-1)/2^n
			u8 NR10_sweep_dir   : 1; // 0=Addition, 1=Subtraction
			u8 NR10_sweep_time  : 3; // n/128 Hz
			u8 NR10_unused      : 1;
		};
	};
	// FF11 - NR11 - Channel 1 Sound length/Wave pattern duty (R/W)
	union {
		u8 NR11;
		struct {
			u8 NR11_sound_length_data : 6; // (WO) (64-n) / 256 seconds
			u8 NR11_wave_pattern_duty : 2; // 12.5%, 25%, 50%, 75%
		};
	};
	// FF12 - NR12 - Channel 1 Volume Envelope (R/W)
	union {
		u8 NR12;
		struct {
			u8 NR12_envelope_sweep  : 3; // length of 1 step = n*(1/64) seconds
			u8 NR12_envelope_dir    : 1; // 0=Decrease, 1=Increase
			u8 NR12_envelope_volume : 4; // initial volume
		};
	};
	// FF13 - NR13 - Channel 1 Frequency lo (Write Only)
	u8 NR13; // Lower 8 bits of 11 bit frequency. Next 3 bit are in NR14
	// FF14 - NR14 - Channel 1 Frequency hi (R/W)
	union {
		u8 NR14;
		struct {
			u8 NR14_frequency_hi       : 3; // (WO) Frequency = 131072/(2048-x) Hz
			u8 NR14_unused             : 3;
			u8 NR14_consecutive_select : 1; // 1=Stop output when length in NR11 expires
			u8 NR14_init               : 1; // (WO) 1=Restart Sound
		};
	};
	// FF15
	u8 FF15; // unused
	// FF16 - NR21 - Channel 2 Sound length/Wave pattern duty (R/W)
	union {
		u8 NR21;
		struct {
			u8 NR21_sound_length_data : 6; // (WO) (64-n) / 256 seconds
			u8 NR21_wave_pattern_duty : 2; // 12.5%, 25%, 50%, 75%
		};
	};
	// FF17 - NR12 - Channel 2 Volume Envelope (R/W)
	union {
		u8 NR22;
		struct {
			u8 NR22_envelope_sweep  : 3; // length of 1 step = n*(1/64) seconds
			u8 NR22_envelope_dir    : 1; // 0=Decrease, 1=Increase
			u8 NR22_envelope_volume : 4; // initial volume
		};
	};
	// FF18 - NR23 - Channel 2 Frequency lo (Write Only)
	u8 NR23; // Lower 8 bits of 11 bit frequency. Next 3 bit are in NR24
	// FF19 - NR24 - Channel 2 Frequency hi (R/W)
	union {
		u8 NR24;
		struct {
			u8 NR24_frequency_hi       : 3; // (WO) Frequency = 131072/(2048-x) Hz
			u8 NR24_unused             : 3;
			u8 NR24_consecutive_select : 1; // 1=Stop output when length in NR21 expires
			u8 NR24_init               : 1; // (WO) 1=Restart Sound
		};
	};
	// FF1A - NR30 - Channel 3 Sound on/off (R/W)
	union {
		u8 NR30;
		struct {
			u8 NR30_enable : 1; // 0=Stop, 1=Playback
			u8 NR30_unused : 7;
		};
	};
	// FF1B - NR31 - Channel 3 Sound Length
	u8 NR31; // sound length (256-n)/256, This value is used only if Bit 6 in NR34 is set.
	// FF1C - NR32 - Channel 3 Select output level (R/W)
	union {
		u8 NR32;
		struct {
			u8 NR32_unused0 : 5;
			u8 NR32_volume  : 2; // 0: Mute, 1: 100%, 2: 50%, 3: 25%
			u8 NR32_unused1 : 1;
		};
	};
	// FF1D - NR33 - Channel 3 Frequency's lower data (W)
	u8 NR33; // Lower 8 bits of 11 bit frequency. Next 3 bit are in NR34
	// FF1E - NR34 - Channel 3 Frequency's higher data (R/W)
	union {
		u8 NR34;
		struct {
			u8 NR34_frequency_hi       : 3; // (WO) Hz = 65536/(2048-x) Hz
			u8 NR34_unused             : 3;
			u8 NR34_consecutive_select : 1; // 1=Stop output when length in NR31 expires
			u8 NR34_init               : 1; // (WO) 1=Restart Sound
		};
	};
	// FF1F-FF2F
	u8 FF1F_FF2F[0x11]; // unused
	// FF30-FF3F - Wave Pattern RAM
	u8 WAVE[0x10]; // Contents - Waveform storage for arbitrary sound data

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
	u8 FF51_FF7F[0x2F]; // unused

	// FF80-FFFE
	u8 FF80_FFFE[0x7F]; // HRAM

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
