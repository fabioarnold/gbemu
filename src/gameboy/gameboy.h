/*
TODO:
DMA within 160 cycles (not instant)
8x16 sprites
input interrupt
serial interrupt
audio
serial
memory bank controllers MBC1 √ MBC3 MBC5
actually guard SRAM if sram_enabled becomes false
halt/stop behavior
LCD disable
*/

const int CPU_FREQ_HZ  = 4<<20; // 4 MiHz
const int RAM_FREQ_HZ  = 1<<20; // 1 MiHz
const int PPU_FREQ_HZ  = 4<<20; // 4 MiHz
const int VRAM_FREQ_HZ = 2<<20; // 2 MiHz

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
