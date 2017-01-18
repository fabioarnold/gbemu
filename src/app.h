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
	void updateGLTextures();
};
