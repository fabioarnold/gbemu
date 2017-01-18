#include <cstdio>
// docs: http://marc.rawer.de/Gameboy/index.html

#include <stdarg.h>
#include <ctime>

// SDL2
#include <SDL.h>
#ifdef USE_GLEW
	#include <GL/glew.h>
#endif
#ifdef USE_OPENGLES
	#include <SDL_opengles2.h>
	#include <GLES2/gl2extimg.h> // texture compression ext
#else
	#define GL_GLEXT_PROTOTYPES
	#include <SDL_opengl.h>
	#include <SDL_opengl_glext.h>
#endif

// ImGui
#include <imgui.h>
#include "imgui_impl_sdl2_gl2.h" // the impl we want to use



#include "system/defines.h"
#include "system/log.h"
#include "system/files.h"

#include "math/vector_math.h"

#include "input/input.h"

#include "video/video_mode.h"
#include "video/image.h"
#include "video/texture.h"

#include "gameboy.h"
#include "app.h"

#include "gui/memory_editor.h"



#include "system/log.cpp"
#include "system/files.cpp"

#include "video/image.cpp"
#include "video/texture.cpp"

#include "app.cpp"

#define WINDOW_TITLE "GameBoy Emulator"
SDL_Window *sdl_window;
SDL_GLContext sdl_gl_context;

/* inits sdl and creates an opengl window */
static void initSDL(VideoMode *video) {
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER);

	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 16);
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
	//SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 4);

#ifdef EMSCRIPTEN
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
						SDL_GL_CONTEXT_PROFILE_ES);
#else
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 1);
	SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK,
						SDL_GL_CONTEXT_PROFILE_COMPATIBILITY);
#endif

	int window_flags =
		  SDL_WINDOW_SHOWN
		| SDL_WINDOW_RESIZABLE
		| SDL_WINDOW_OPENGL
		//| SDL_WINDOW_ALLOW_HIGHDPI
		;

	sdl_window = SDL_CreateWindow(WINDOW_TITLE,
		SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
		video->width, video->height, window_flags);
	if (!sdl_window) {
		LOGE("Failed to create an OpenGL Window.");
		exit(1);
	}

	sdl_gl_context = SDL_GL_CreateContext(sdl_window);
	if (!sdl_gl_context) {
		LOGE("Failed to create an OpenGL Context.");
		exit(1);
	}

	int drawable_width, drawable_height;
	SDL_GL_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
	if (drawable_width != video->width) {
		LOGI("Created high DPI window. (%dx%d)", drawable_width, drawable_height);
		video->pixel_scale = (float)drawable_width / (float)video->width;
	} else {
		video->pixel_scale = 1.0f;
	}

	if (SDL_GL_SetSwapInterval(1) == -1) { // sync with monitor refresh rate
		LOGW("Could not enable VSync.");
	}

	#ifndef __APPLE__
	glewInit();
	#endif
}

void quitSDL() {
	SDL_DestroyWindow(sdl_window);
	SDL_GL_DeleteContext(sdl_gl_context);
	SDL_Quit();
}



App *app = nullptr;

void mainLoop() {
	keyboard.beginFrame();

	SDL_Event sdl_event;
	while (SDL_PollEvent(&sdl_event)) {
		ImGui_ImplSdlGL2_ProcessEvent(&sdl_event);
		switch (sdl_event.type) {
		case SDL_KEYDOWN:
		case SDL_KEYUP:
			keyboard.onKey(sdl_event.key.keysym.scancode,
				sdl_event.key.state == SDL_PRESSED);
			app->quit = sdl_event.key.keysym.sym == SDLK_ESCAPE;
			break;
		case SDL_WINDOWEVENT:
			switch (sdl_event.window.event) {
        	case SDL_WINDOWEVENT_SIZE_CHANGED:
				app->video.width  = sdl_event.window.data1;
				app->video.height = sdl_event.window.data2;
				{ // update opengl viewport
					int drawable_width, drawable_height;
					SDL_GL_GetDrawableSize(sdl_window, &drawable_width, &drawable_height);
					glViewport(0, 0, drawable_width, drawable_height);
				}
				break;
			}
			break;
		case SDL_QUIT:
			app->quit = true;
			break;
		}
	}

	ImGui_ImplSdlGL2_NewFrame();

	app->update();

	ImGui::Render();
	SDL_GL_SwapWindow(sdl_window);
}

int main(int argc, char *argv[]) {
	app = new App();

	// init default key bindings
	keyboard.bind(SDL_SCANCODE_A, &app->gb.button_a);
	keyboard.bind(SDL_SCANCODE_S, &app->gb.button_b);
	keyboard.bind(SDL_SCANCODE_RETURN, &app->gb.button_start);
	keyboard.bind(SDL_SCANCODE_BACKSPACE, &app->gb.button_select);
	keyboard.bind(SDL_SCANCODE_LEFT,  &app->gb.button_left);
	keyboard.bind(SDL_SCANCODE_RIGHT, &app->gb.button_right);
	keyboard.bind(SDL_SCANCODE_UP,    &app->gb.button_up);
	keyboard.bind(SDL_SCANCODE_DOWN,  &app->gb.button_down);

	// video settings
	app->video.width = 1280;
	app->video.height = 720;
	app->video.pixel_scale = 1.0f;
	
	// init video
	initSDL(&app->video);
	ImGui_ImplSdlGL2_Init(sdl_window);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

	app->init();

	do {
		Uint32 beginTicks = SDL_GetTicks();
		mainLoop();
		// hack for when vsync isn't working
		Uint32 elapsedTicks = SDL_GetTicks() - beginTicks;
		if (elapsedTicks < 16) {
			SDL_Delay(16 - elapsedTicks);
		}
	} while(!app->quit);

	ImGui_ImplSdlGL2_Shutdown();
	quitSDL();
}
