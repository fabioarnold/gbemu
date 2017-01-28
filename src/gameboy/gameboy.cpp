void GameBoy::init() {
	memory.gb = this;
	cpu.memory = &memory;
	ppu.gb = this;
	reset();
}

void GameBoy::reset() {
	cpu.reset();
	ppu.reset();
	memory.reset();
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
		u16 src_address = value<<8;
		memory.DMAtoOAM(src_address);
	} break;
	default: break;
	}

	return value;
}

void GameBoy::loadROM(const char *filepath) {
	memory.loadROM(filepath);
	if (memory.rom) { // success
		cpu.reset();
		ppu.reset();
	}
}
