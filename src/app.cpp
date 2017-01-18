void App::init() {
#if 0
	// print out statistic of how many instructions have been implemented
	int total_instruction_count = 0;
	int implemented_instruction_count = 0;
	for (int i = 0; i < (int)ARRAY_COUNT(gb.cpu.instructions); i++) {
		if (gb.cpu.instructions[i] == &CPU::ill) continue;
		total_instruction_count++;
		if (gb.cpu.instructions[i] != nullptr) implemented_instruction_count++;
	}
	LOGI("implemented %d of %d instructions",
		implemented_instruction_count, total_instruction_count);
#endif

	size_t dmg_rom_size = 0;
	u8 *dmg_rom = readDataFromFile("dmg_rom.bin", &dmg_rom_size);
	assert(dmg_rom_size == 0x100);
	memcpy(gb.memory.boot_rom, dmg_rom, 0x100);

	gb.readROM("tetris.gb");

	gb.init();

	glGenTextures(1, &framebuffer_tex);
	glGenTextures(1, &tiles_tex);
	glGenTextures(1, &map_tex);

	glBindTexture(GL_TEXTURE_2D, framebuffer_tex);
	setFilterTexture2D(GL_NEAREST, GL_NEAREST);
	setWrapTexture2D(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, /*level*/0, GL_RGB8, LCD_WIDTH, LCD_HEIGHT,
		/*border*/0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, tiles_tex);
	setFilterTexture2D(GL_NEAREST, GL_NEAREST);
	setWrapTexture2D(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, /*level*/0, GL_RGB8, 16*8, 24*8,
		/*border*/0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, map_tex);
	setFilterTexture2D(GL_NEAREST, GL_NEAREST);
	setWrapTexture2D(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
	glTexImage2D(GL_TEXTURE_2D, /*level*/0, GL_RGB8, 32*8, 32*8,
		/*border*/0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);

	glBindTexture(GL_TEXTURE_2D, 0);
}

MemoryEditor memory_editor;

void cpuGUI(GameBoy *gb) {
	CPU *cpu = &gb->cpu;

	ImGui::Begin("CPU");
	if (ImGui::Button("Reset")) gb->reset();
	ImGui::SameLine();
	if (ImGui::Button(gb->running ? "Stop" : "Run")) {
		gb->running = !gb->running;
	}
	if (ImGui::Button("Single Step")) gb->step();
	if (ImGui::Button("Next Frame")) {
		// run until vsync
		do {
			if (gb->cpu.DEBUG_not_implemented_error) {
				gb->cpu.DEBUG_not_implemented_error = false;
				gb->running = false;
				break;
			}
			gb->step();
			if (gb->cpu.PC == gb->cpu.DEBUG_break_point) {
				gb->running = false;
			}
		} while (!gb->ppu.vsync);
	}
	static int break_frame = 0;
	if (ImGui::Button("Run to ")) {
		while (gb->ppu.frame_count < break_frame) {
			if (gb->cpu.DEBUG_not_implemented_error) {
				gb->cpu.DEBUG_not_implemented_error = false;
				gb->running = false;
				break;
			}
			gb->step();
		}
	}
	ImGui::SameLine();
	ImGui::InputInt("Frame", &break_frame);

	ImGui::Text("Cycles: %llu", cpu->cycle_count);
	ImGui::Text("Halted: %d", cpu->halted);

	ImGui::Text("State: %s", cpu_state_names[cpu->state]);
	ImGui::Text("Address: 0x%04X", cpu->address);
	ImGui::Text("Bus: 0x%02X", cpu->bus);

	ImGui::Text("Registers:");
	ImGui::Text("A: %X", cpu->A); ImGui::SameLine();
	ImGui::Text("F: %X", cpu->F);
	ImGui::Text("B: %X", cpu->B); ImGui::SameLine();
	ImGui::Text("C: %X", cpu->C);
	ImGui::Text("D: %X", cpu->D); ImGui::SameLine();
	ImGui::Text("E: %X", cpu->E);
	ImGui::Text("H: %X", cpu->H); ImGui::SameLine();
	ImGui::Text("L: %X", cpu->L);
	ImGui::Text("SP: %X", cpu->SP);
	ImGui::Text("PC: %X", cpu->PC);
	ImGui::Text("IME: %X", cpu->IME);

	static char str_pc[8] = "FFFF";
	ImGui::InputText("Break ", str_pc, sizeof(str_pc));
	cpu->DEBUG_break_point = (int)strtol(str_pc, NULL, 16);

	static u8 opcode = 0x00; // nop
	if (cpu->state == CPU_STATE_FETCH) {
		opcode = cpu->memory->load8(cpu->PC);
	}
	ImGui::Text("instr: %s", cpu->instruction_infos[opcode].mnemonic);

	ImGui::Text("Flags:");
	u32 flags = cpu->F;
	ImGui::CheckboxFlags("Zero", &flags, 0x80);
	ImGui::CheckboxFlags("Substract", &flags, 0x40);
	ImGui::CheckboxFlags("Half carry", &flags, 0x20);
	ImGui::CheckboxFlags("Carry", &flags, 0x10);
	cpu->F = flags;
	ImGui::End();
}

void ppuGUI(PPU *ppu) {
	IO *io = &ppu->memory->io;
	ImGui::Begin("PPU");
	ImGui::Text("cycle %llu", ppu->cycle_count);
	ImGui::Text("frame %llu", ppu->frame_count);

	ImGui::Text("LCDC 0x%02X", io->LCDC);
	u32 lcdc = io->LCDC;
	ImGui::CheckboxFlags("BG Display Enable", &lcdc, 0x01);
	ImGui::CheckboxFlags("OBJ Display Enable", &lcdc, 0x02);
	ImGui::CheckboxFlags("OBJ Size (8x8, 8x16)", &lcdc, 0x04);
	ImGui::CheckboxFlags("BG Tile Map Display Select", &lcdc, 0x08);
	ImGui::CheckboxFlags("BG & Window Tile Data Select", &lcdc, 0x10);
	ImGui::CheckboxFlags("Window Display Enable", &lcdc, 0x20);
	ImGui::CheckboxFlags("Window Tile Map Display Select", &lcdc, 0x40);
	ImGui::CheckboxFlags("LCD Display Enable", &lcdc, 0x80);
	io->LCDC = lcdc;

	u32 stat = io->STAT;
	const char *mode_names[] = {"HBLANK", "VBLANK", "OAM", "LCD"};
	ImGui::Text("STAT Mode 0x%02X %s", io->STAT_mode,
		mode_names[io->STAT_mode]);
	ImGui::CheckboxFlags("Coincide LY (0:<>, 1:==)", &stat, 0x04);
	ImGui::CheckboxFlags("H-Blank Interrupt Enable", &stat, 0x08);
	ImGui::CheckboxFlags("V-Blank Interrupt Enable", &stat, 0x10);
	ImGui::CheckboxFlags("OAM Interrupt Enable", &stat, 0x20);
	ImGui::CheckboxFlags("Coincide Interrupt Enable", &stat, 0x40);
	io->STAT = stat;

	int scy = io->SCY;
	ImGui::SliderInt("SCY", &scy, 0x00, 0xFF);
	io->SCY = scy;
	int scx = io->SCX;
	ImGui::SliderInt("SCX", &scx, 0x00, 0xFF);
	io->SCX = scx;
	ImGui::Text("LY  0x%02X", io->LY);
	ImGui::Text("LYC 0x%02X", io->LYC);
	ImGui::Text("DMA 0x%02X", io->DMA);
	ImGui::Text("BGP 0x%02X", io->BGP);
	ImGui::Text("OBP0 0x%02X", io->OBP0);
	ImGui::Text("OBP1 0x%02X", io->OBP1);
	ImGui::Text("WY  0x%02X", io->WY);
	ImGui::Text("WX  0x%02X", io->WX);
	ImGui::Text("VBK 0x%02X", io->VBK);

	ImGui::End();
}

void ioGUI(IO *io) {
	ImGui::Begin("IO");
	ImGui::Text("IF 0x%02X", io->IF);
	u32 IF = io->IF;
	ImGui::CheckboxFlags("V-Blank Request", &IF, 0x01);
	ImGui::CheckboxFlags("LCD STAT Request", &IF, 0x02);
	ImGui::CheckboxFlags("Timer Request", &IF, 0x04);
	ImGui::CheckboxFlags("Serial Request", &IF, 0x08);
	ImGui::CheckboxFlags("Joypad Request", &IF, 0x10);
	ImGui::Text("IE 0x%02X", io->IE);
	u32 IE = io->IE;
	ImGui::CheckboxFlags("V-Blank Enable", &IE, 0x01);
	ImGui::CheckboxFlags("LCD STAT Enable", &IE, 0x02);
	ImGui::CheckboxFlags("Timer Enable", &IE, 0x04);
	ImGui::CheckboxFlags("Serial Enable", &IE, 0x08);
	ImGui::CheckboxFlags("Joypad Enable", &IE, 0x10);
	ImGui::End();
}

void App::updateGLTextures() {
	// convert 2bit to 24bit colors
	u8 palette[4][3] = {
		{0xFF,0xFF,0xFF},
		{0xAA,0xAA,0xAA},
		{0x55,0x55,0x55},
		{0x00,0x00,0x00}
	};
	u8 pixels[LCD_HEIGHT*LCD_WIDTH*3];

#if 0
	// same 2bit encoding as tile data
	for (int y = 0; y < LCD_HEIGHT; y++) {
		for (int lx = 0; lx < LCD_WIDTH/8; lx++) {
			u8 llb = gb.ppu.framebuffer[y*LCD_WIDTH/4+2*lx+0];
			u8 lhb = gb.ppu.framebuffer[y*LCD_WIDTH/4+2*lx+1];
			for (int x = 0; x < 8; x++) {
				int palette_idx = (llb >> (7-x)) & 0x1;
				palette_idx |= ((lhb >> (7-x)) & 0x1) << 1;

				pixels[3*(y*LCD_WIDTH+8*lx+x)+0] = palette[palette_idx][0];
				pixels[3*(y*LCD_WIDTH+8*lx+x)+1] = palette[palette_idx][1];
				pixels[3*(y*LCD_WIDTH+8*lx+x)+2] = palette[palette_idx][2];
			}
		}
	}
#else
	for (int y = 0; y < LCD_HEIGHT; y++) {
		for (int x = 0; x < LCD_WIDTH; x++) {
			int pixel = gb.ppu.framebuffer[y*LCD_WIDTH+x];
			pixels[3*(y*LCD_WIDTH+x)+0] = palette[pixel][0];
			pixels[3*(y*LCD_WIDTH+x)+1] = palette[pixel][1];
			pixels[3*(y*LCD_WIDTH+x)+2] = palette[pixel][2];
		}
	}
#endif
	glBindTexture(GL_TEXTURE_2D, framebuffer_tex);
	glTexSubImage2D(GL_TEXTURE_2D, /*level*/0, /*x*/0, /*y*/0,
		LCD_WIDTH, LCD_HEIGHT, GL_RGB, GL_UNSIGNED_BYTE, pixels);
	
	int map_adr = gb.memory.io.LCDC_bg_tile_map ?
		0x1C00 : 0x1800;
	int tile_adr = gb.memory.io.LCDC_tile_data ?
		0x0000 : 0x1000;

	u8 tile_pixels[384*8*8*3];
	for (int ti = 0; ti < 384; ti++) {
		int ty = ti/16;
		int tx = ti%16;
		for (int y = 0; y < 8; y++) {
			// line with high and low bits
			u8 llb = gb.memory.vram[2*(8*ti+y)+0];
			u8 lhb = gb.memory.vram[2*(8*ti+y)+1];
			for (int x = 0; x < 8; x++) {
				int palette_idx = (llb >> (7-x)) & 0x1;
				palette_idx |= ((lhb >> (7-x)) & 0x1) << 1;

				u8 *tile = &tile_pixels[3*(16*8*(8*ty+y)+8*tx+x)];
				tile[0] = palette[palette_idx][0];
				tile[1] = palette[palette_idx][1];
				tile[2] = palette[palette_idx][2];
			}
		}
	}
	glBindTexture(GL_TEXTURE_2D, tiles_tex);
	glTexSubImage2D(GL_TEXTURE_2D, /*level*/0, /*x*/0, /*y*/0,
		16*8, 24*8, GL_RGB, GL_UNSIGNED_BYTE, tile_pixels);

	u8 map_pixels[32*32*8*8*3];
	for (int ty = 0; ty < 0x20; ty++) {
		for (int tx = 0; tx < 0x20; tx++) {
			int ti = gb.memory.io.LCDC_tile_data ?
				gb.memory.vram[map_adr+ty*0x20+tx] : 
				((s8*)gb.memory.vram)[map_adr+ty*0x20+tx]; // signed
			for (int y = 0; y < 8; y++) {
				// line with high and low bits
				u8 llb = gb.memory.vram[tile_adr + 2*(8*ti+y)+0];
				u8 lhb = gb.memory.vram[tile_adr + 2*(8*ti+y)+1];
				for (int x = 0; x < 8; x++) {
					int palette_idx = (llb >> (7-x)) & 0x1;
					palette_idx |= ((lhb >> (7-x)) & 0x1) << 1;

					u8 *map = &map_pixels[3*(32*8*(8*ty+y)+8*tx+x)];
					map[0] = palette[palette_idx][0];
					map[1] = palette[palette_idx][1];
					map[2] = palette[palette_idx][2];
				}
			}
		}
	}
	glBindTexture(GL_TEXTURE_2D, map_tex);
	glTexSubImage2D(GL_TEXTURE_2D, /*level*/0, /*x*/0, /*y*/0,
		32*8, 32*8, GL_RGB, GL_UNSIGNED_BYTE, map_pixels);

	glBindTexture(GL_TEXTURE_2D, 0);
}

struct MyImTexture {
	GLenum target;
	GLuint id;
};

void App::update() {
	glClear(GL_COLOR_BUFFER_BIT);
	memory_editor.Draw("ROM Editor", gb.memory.rom, gb.memory.rom_size);
	memory_editor.Draw("RAM Editor", gb.memory.ram, sizeof(gb.memory.ram));
	memory_editor.Draw("HRAM Editor", gb.memory.hram, sizeof(gb.memory.hram));
	memory_editor.Draw("VRAM Editor", gb.memory.vram, sizeof(gb.memory.vram));
	cpuGUI(&gb);
	ppuGUI(&gb.ppu);
	ioGUI(&gb.memory.io);

	while (gb.running) {
		if (gb.cpu.DEBUG_not_implemented_error) {
			gb.cpu.DEBUG_not_implemented_error = false;
			gb.running = false;
			break;
		}
		gb.step();
		if (gb.cpu.PC == gb.cpu.DEBUG_break_point) {
			gb.running = false;
		}
		if (gb.ppu.vsync) break;
	}

	updateGLTextures();

	static MyImTexture tex0, tex1, tex2;
	tex0.target = GL_TEXTURE_2D;
	tex1.target = GL_TEXTURE_2D;
	tex2.target = GL_TEXTURE_2D;

	ImGui::Begin("Framebuffer");
	tex0.id = framebuffer_tex;
	ImGui::Image((ImTextureID)&tex0, ImVec2(LCD_WIDTH, LCD_HEIGHT));
	ImGui::End();

	ImGui::Begin("Tiles");
	tex1.id = tiles_tex;
	ImGui::Image((ImTextureID)&tex1, ImVec2(16*8, 24*8));
	ImGui::End();

	ImGui::Begin("BG Map");
	tex2.id = map_tex;
	ImGui::Image((ImTextureID)&tex2, ImVec2(32*8, 32*8));
	ImGui::End();
}