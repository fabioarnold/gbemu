struct App {
	bool quit;
	VideoMode video;

	GameBoy gb;

	GLuint framebuffer_tex;
	GLuint tiles_tex;
	GLuint map_tex;

	void init();
	void update();

private:
	MemoryEditor rom_editor;
	MemoryEditor ram_editor;
	MemoryEditor hram_editor;
	MemoryEditor vram_editor;
	void updateGLTextures();
};
