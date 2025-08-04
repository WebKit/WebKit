/*
 * dex-version-macros.h
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#pragma once

#include <glib.h>

#include "dex-version.h"

#ifndef _DEX_EXTERN
# define _DEX_EXTERN extern
#endif

#define DEX_VERSION_CUR_STABLE (G_ENCODE_VERSION (DEX_MAJOR_VERSION, 0))

#ifdef DEX_DISABLE_DEPRECATION_WARNINGS
# define DEX_DEPRECATED _DEX_EXTERN
# define DEX_DEPRECATED_FOR(f) _DEX_EXTERN
# define DEX_UNAVAILABLE(maj,min) _DEX_EXTERN
#else
# define DEX_DEPRECATED G_DEPRECATED _DEX_EXTERN
# define DEX_DEPRECATED_FOR(f) G_DEPRECATED_FOR (f) _DEX_EXTERN
# define DEX_UNAVAILABLE(maj,min) G_UNAVAILABLE (maj, min) _DEX_EXTERN
#endif

#define DEX_VERSION_1_0 (G_ENCODE_VERSION (1, 0))

#if DEX_MAJOR_VERSION == DEX_VERSION_1_0
# define DEX_VERSION_PREV_STABLE (DEX_VERSION_1_0)
#else
# define DEX_VERSION_PREV_STABLE (G_ENCODE_VERSION (DEX_MAJOR_VERSION - 1, 0))
#endif

/**
 * DEX_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including
 * the dex.h header.
 *
 * The definition should be one of the predefined DEX version
 * macros: %DEX_VERSION_1_0, ...
 *
 * This macro defines the lower bound for the Builder API to use.
 *
 * If a function has been deprecated in a newer version of Builder,
 * it is possible to use this symbol to avoid the compiler warnings
 * without disabling warning for every deprecated function.
 */
#ifndef DEX_VERSION_MIN_REQUIRED
# define DEX_VERSION_MIN_REQUIRED (DEX_VERSION_CUR_STABLE)
#endif

/**
 * DEX_VERSION_MAX_ALLOWED:
 *
 * A macro that should be defined by the user prior to including
 * the dex.h header.

 * The definition should be one of the predefined Builder version
 * macros: %DEX_VERSION_1_0, %DEX_VERSION_1_2,...
 *
 * This macro defines the upper bound for the DEX API to use.
 *
 * If a function has been introduced in a newer version of Builder,
 * it is possible to use this symbol to get compiler warnings when
 * trying to use that function.
 */
#ifndef DEX_VERSION_MAX_ALLOWED
# if DEX_VERSION_MIN_REQUIRED > DEX_VERSION_PREV_STABLE
#  define DEX_VERSION_MAX_ALLOWED (DEX_VERSION_MIN_REQUIRED)
# else
#  define DEX_VERSION_MAX_ALLOWED (DEX_VERSION_CUR_STABLE)
# endif
#endif

#define DEX_AVAILABLE_IN_ALL _DEX_EXTERN

#if DEX_VERSION_MIN_REQUIRED >= DEX_VERSION_1_0
# define DEX_DEPRECATED_IN_1_0 DEX_DEPRECATED
# define DEX_DEPRECATED_IN_1_0_FOR(f) DEX_DEPRECATED_FOR(f)
#else
# define DEX_DEPRECATED_IN_1_0 _DEX_EXTERN
# define DEX_DEPRECATED_IN_1_0_FOR(f) _DEX_EXTERN
#endif
#if DEX_VERSION_MAX_ALLOWED < DEX_VERSION_1_0
# define DEX_AVAILABLE_IN_1_0 DEX_UNAVAILABLE(1, 0)
#else
# define DEX_AVAILABLE_IN_1_0 _DEX_EXTERN
#endif
