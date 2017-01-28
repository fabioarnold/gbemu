const int LCD_WIDTH  = 160;
const int LCD_HEIGHT = 144;

const int OBJ_COUNT    = 40;
const int OBJ_PER_LINE = 10;

enum PPUState {
	PPU_STATE_HALT,
	PPU_STATE_OAM_SEARCH, // mode 2
	PPU_STATE_PIXEL_TRANSFER, // mode 3
	PPU_STATE_HBLANK, // mode 0
	PPU_STATE_VBLANK // mode 1
};

struct GameBoy;

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
