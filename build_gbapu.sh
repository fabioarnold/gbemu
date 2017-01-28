#!/bin/bash
TARGET=libGbApu.a
UNITYBUILD_CPP_FILE="lib/gbapu/gbapu_ub.cpp"
INCLUDE_DIRS="-Ilib/gbapu"
DEBUG_FLAGS="-O0 -g"
RELEASE_FLAGS="-O2"
CFLAGS="$CFLAGS $INCLUDE_DIRS $DEBUG_FLAGS"

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
