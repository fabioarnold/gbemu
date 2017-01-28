// object attribute memory
struct OAM {
	struct OBJ {
		u8 y; // minus 16
		u8 x; // minus 8
		u8 tile;
		union {
			u8 attrib;
			struct {
				u8 attrib_cgb_palette   : 3;
				u8 attrib_cgb_vram_bank : 1;
				u8 attrib_palette       : 1;
				u8 attrib_flip_x        : 1;
				u8 attrib_flip_y        : 1;
				// 0=OBJ above BG, 1 OBJ behind (but above pixel value 0)
				u8 attrib_priority      : 1;
			};
		};
	} objs[OBJ_COUNT];
};
