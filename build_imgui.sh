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

TARGET=libImGui.a
UNITYBUILD_CPP_FILE="lib/gamelib/third_party/imgui/imgui_sdl2_gl2_ub.cpp"
INCLUDE_DIRS="-Ilib/imgui -Ilib/gamelib/third_party/imgui"
DEBUG_FLAGS="-O0 -g"
RELEASE_FLAGS="-O2"
CFLAGS="$CFLAGS `sdl2-config --cflags` $INCLUDE_DIRS $DEBUG_FLAGS"

if [[ $PLATFORM == "Linux" ]]; then
	CFLAGS="$CFLAGS -DLINUX_DESKTOP -DUSE_GLEW"
elif [[ $PLATFORM == "OpenPandora" ]]; then
	CFLAGS="$CFLAGS `pkg-config --cflags glesv1_cm glesv2` -DUSE_OPENGLES"
fi

mkdir -p build
c++ $CFLAGS -c ${UNITYBUILD_CPP_FILE} -o ${UNITYBUILD_CPP_FILE}.o
EXIT_STATUS=$?
if [ $EXIT_STATUS = 0 ]; then
	rm -f build/$TARGET # prevents warning msg of compiler
	ar rcs build/$TARGET ${UNITYBUILD_CPP_FILE}.o
	rm ${UNITYBUILD_CPP_FILE}.o
else
	exit 1
fi
