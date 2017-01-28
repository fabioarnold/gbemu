#!/bin/bash

# detect platform
OS_NAME="$(uname)" # {Darwin, Linux}
MACHINE_NAME="$(uname -m)" # {x86_64, armv7l}
PLATFORM="unknown"
# TODO: find more reliable way to determine if it's a Pandora
if [[ $OS_NAME == "Linux" && $MACHINE_NAME == "armv7l" ]]; then
	PLATFORM="OpenPandora"
else
	PLATFORM=$OS_NAME
fi

TARGET="gbemu"

if [[ $1 = "clean" ]]; then
	rm -r build
	exit 0
fi

DEBUG_FLAGS="-O0 -g -DDEBUG"
RELEASE_FLAGS="-O2"
if [[ $1 = "release" ]]; then
	CFLAGS="$CFLAGS -std=c++11 $RELEASE_FLAGS"
else
	CFLAGS="$CFLAGS -std=c++11 $DEBUG_FLAGS"
fi
LDFLAGS="-Lbuild"

# gamelib
INCLUDE_DIRS="-Ilib/gamelib/src"

# SDL2
LIB_SDL2="`pkg-config --libs sdl2`"

# OpenGL and Windowing
LIB_OPENGL="-lGL -lGLEW"

# dear imgui
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/imgui -Ilib/gamelib/third_party/imgui"
LIB_IMGUI="-lImGui"
if [ ! -f build/libImGui.a ]; then
	echo "building dear imgui..."
	./build_imgui.sh
	if [ $? = 1 ]; then
		exit 1
	fi
fi

# Gb Apu
INCLUDE_DIRS="$INCLUDE_DIRS -Ilib/gbapu"
LIB_GBAPU="-lGbApu"
if [ ! -f build/libGbApu.a ]; then
	echo "building Gb Apu..."
	./build_gbapu.sh
	if [ $? = 1 ]; then
		exit 1
	fi
fi

# platform specific changes
if [[ $PLATFORM == "Darwin" ]]; then
	LIB_OPENGL="-framework OpenGL -framework Cocoa"
elif [[ $PLATFORM == "Linux" ]]; then
	CFLAGS="$CFLAGS -DUSE_GLEW"
	LIB_OPENGL="$LIB_OPENGL -lGLEW"
elif [[ $PLATFORM == "OpenPandora" ]]; then
	CFLAGS="$CFLAGS `pkg-config --cflags glesv1_cm glesv2` -DUSE_OPENGLES"
	LIB_OPENGL="`pkg-config --libs glesv2`"
	source pandora_setup.sh
fi

# final compiler flags
CFLAGS="$CFLAGS `pkg-config --cflags sdl2` $INCLUDE_DIRS"
LDFLAGS="$LDFLAGS $LIB_SDL2 $LIB_OPENGL $LIB_IMGUI $LIB_GBAPU"

mkdir -p build
c++ $CFLAGS src/main_sdl2_ub.cpp $LDFLAGS -o build/$TARGET
EXIT_STATUS=$?
if [[ $EXIT_STATUS = 0 && $1 = "run" ]]; then
	./build/$TARGET
fi
