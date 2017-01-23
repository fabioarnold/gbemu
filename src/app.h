struct App {
	bool quit;
	VideoMode video;

	GameBoy gb;

	GLuint lcd_tex;
	GLuint tiles_tex;
	GLuint bg_map_tex;
	GLuint window_map_tex;

	void init();
	void update();

private:
	MemoryEditor rom_editor;
	MemoryEditor ram_editor;
	MemoryEditor hram_editor;
	MemoryEditor vram_editor;
	void updateGLTextures();
};
