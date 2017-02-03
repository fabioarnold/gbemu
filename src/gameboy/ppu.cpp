void PPU::reset() {
	memset(framebuffer, 0, sizeof(framebuffer));
	state = PPU_STATE_OAM_SEARCH;
	cycle_count = 0;
	cycle_begin = 0;
	vsync = false;
	frame_count = 0;
}

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
		// find the next leftmost obj
		int min_obj = -1;
		int min_x = 0; // 0=invisible
		int max_x = LCD_WIDTH+8;
		if (line_obj_count > 0) {
			min_x = oam->objs[line_objs[line_obj_count-1]].x;
		}

		for (int i = 0; i < ARRAY_COUNT(oam->objs); i++) {
			if (line_obj_count >= ARRAY_COUNT(line_objs)) break;
			if (oam->objs[i].x == 0) continue; // invisible
			if (oam->objs[i].x < min_x) continue;
			if (io->LY+16 >= oam->objs[i].y
			 && io->LY+16 <  oam->objs[i].y+obj_h) {
				if (oam->objs[i].x <= max_x) { // last wins
					bool in_line_buf = false;
					for (int j = 0; j < line_obj_count; j++) {
						if (line_objs[j] == i) {
							in_line_buf = true;
							break;
						}
					}
					if (in_line_buf) continue;
					max_x = oam->objs[i].x;
					min_obj = i;
				}
			}
		}

		// we found an obj
		if (min_obj != -1) line_objs[line_obj_count++] = min_obj;

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
		int window_map_adr = io->LCDC_window_tile_map ? 0x1C00 : 0x1800;
		int bg_map_adr     = io->LCDC_bg_tile_map ? 0x1C00 : 0x1800;
		int tile_adr       = io->LCDC_tile_data   ? 0x0000 : 0x1000;

		int LY = io->LY;
		assert(LY >= 0 && LY < LCD_HEIGHT);
		assert(LX >= 0 && LX < LCD_WIDTH);

		int cycle = (cycle_count - cycle_begin - 1) / 4;
		// try to draw 8 pixels per cycle
		for (int px = 0; px < 8; px++) {
			// discard pixels (subtile scrolling)
			if (LX == 0) pixel_fifo_begin += io->SCX&0x7;

			if (io->LCDC_window_enable
			 && io->LY >= io->WY && (LX+7 == io->WX || io->WX < 7))
			{
				pixel_fifo_begin = pixel_fifo_end; // clear fifo
			}

			// less than 8 pixels in the fifo
			while (pixel_fifo_end - pixel_fifo_begin < 8) {
				int fifo_pos = pixel_fifo_end - pixel_fifo_begin;
				if (fifo_pos < 0) fifo_pos = 0;

				u8 llb = 0; // line low bits
				u8 lhb = 0; // line high bits

				if (io->LCDC_window_enable
				 && LX+7 >= io->WX && io->LY >= io->WY)
				{
					// fetch window tile
					int sy = io->LY - io->WY;
					int sx = LX+7 - io->WX + fifo_pos;
					int ty = sy/8;
					int tx = sx/8;
					int ti = io->LCDC_tile_data ?
						vram[window_map_adr+ty*0x20+tx] :
						((s8*)vram)[window_map_adr+ty*0x20+tx]; // signed
					// line with high and low bits
					llb = vram[tile_adr+2*(8*ti+(sy&0x7))+0];
					lhb = vram[tile_adr+2*(8*ti+(sy&0x7))+1];
				} else if (io->LCDC_bg_enable) {
					// fetch bg tile
					int sy = (LY + io->SCY) & 0xFF;
					int sx = (LX + io->SCX + fifo_pos) & 0xFF;
					int ty = sy/8;
					int tx = sx/8;
					int ti = io->LCDC_tile_data ?
						vram[bg_map_adr+ty*0x20+tx] :
						((s8*)vram)[bg_map_adr+ty*0x20+tx]; // signed
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
					int sy = (LY - obj->y) & (obj_h-1);
					if (obj->attrib_flip_y) sy = obj_h-1-sy;
					// line with high and low bits
					u8 llb = vram[2*(8*obj->tile+sy)+0];
					u8 lhb = vram[2*(8*obj->tile+sy)+1];

					for (int x = 0; x < 8; x++) {
						int shift = obj->attrib_flip_x ? x : (7-x);
						int lb = (llb >> shift) & 0x1;
						int hb = (lhb >> shift) & 0x1;
						int pixel = (hb<<1) | lb;
						if (pixel == 0) continue; // transparent

						// test if obj is supposed to be behind bg
						int bg_pixel = pixel_fifo[(pixel_fifo_begin+x)&0xF]&0x3;
						if (obj->attrib_priority == 1 && bg_pixel > 0) continue;

						pixel_fifo[(pixel_fifo_begin+x) & 0xF] = pixel
							| ((1+obj->attrib_palette)<<2);
					}

					line_obj_index++; // next
				}
			}

			int pixel = pixel_fifo[pixel_fifo_begin++ & 0xF];
			framebuffer[LY*LCD_WIDTH+LX] = palettes[(pixel>>2)&0x3][pixel&0x3];
			//if (!io->LCDC_enable) framebuffer[LY*LCD_WIDTH+LX] = 0; // TODO: hack!

			LX++;
		}

		io->STAT_mode = 3; // pixel transfer
		//if (cycle_count - cycle_begin >= PIXEL_TRANSFER_CYCLES) {
		if (LX >= LCD_WIDTH) {
			state = PPU_STATE_HBLANK;
			if (io->STAT_hblank_interrupt_enable) {
				io->IF_lcd_stat = 1;
				gb->cpu.halted = false;
			}
			// don't reset cycle_begin (this state lasts as long as needed)
		}
	} break;
	case PPU_STATE_HBLANK:
		io->STAT_mode = 0; // H-Blank
		if (cycle_count - cycle_begin >= PIXEL_TRANSFER_CYCLES + HBLANK_CYCLES) {
			io->LY++;
			if (io->STAT_coincide_interrupt_enable && io->LY == io->LYC) {
				io->IF_lcd_stat = 1;
				gb->cpu.halted = false;
			}
			if (io->LY == LCD_HEIGHT) {
				state = PPU_STATE_VBLANK;
				if (io->STAT_vblank_interrupt_enable) {
					io->IF_lcd_stat = 1;
					gb->cpu.halted = false;
				}
				vsync = true; // do vsync
				frame_count++;
				io->IF_vblank = 1; // raise IRQ
				gb->cpu.halted = false;
			} else {
				state = PPU_STATE_OAM_SEARCH;
				if (io->STAT_oam_interrupt_enable) {
					io->IF_lcd_stat = 1;
					gb->cpu.halted = false;
				}
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
			if (io->STAT_coincide_interrupt_enable && io->LY == io->LYC) {
				io->IF_lcd_stat = 1;
				gb->cpu.halted = false;
			}
			cycle_begin = cycle_count;

			state = PPU_STATE_OAM_SEARCH;
			// init oam search
			line_obj_count = 0;
		}
		break;
	default: break;
	}

	io->STAT_coincide = io->LY == io->LYC; // TODO: only when LY gets updated
}
