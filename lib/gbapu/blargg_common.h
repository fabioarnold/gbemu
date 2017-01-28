
// Sets up common environment for Shay Green's libraries.
//
// Don't modify this file directly; #define HAVE_CONFIG_H and put your
// configuration into "config.h".

// Copyright (C) 2004-2005 Shay Green.

#ifndef BLARGG_COMMON_H
#define BLARGG_COMMON_H

// Common standard headers
#include <cstdint>
#include <cstddef>
#include <cassert>
#include <climits>

// blargg_err_t (NULL on success, otherwise error string)
typedef const char* blargg_err_t;
const blargg_err_t blargg_success = 0;

// If expr returns non-NULL error string, return it from current function, otherwise continue.
#define BLARGG_RETURN_ERR( expr ) do {                          \
		blargg_err_t blargg_return_err_ = (expr);               \
		if ( blargg_return_err_ ) return blargg_return_err_;    \
	} while ( 0 )

#endif

