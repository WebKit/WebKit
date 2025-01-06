/* sysprof-version-macros.h
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Subject to the terms and conditions of this license, each copyright holder
 * and contributor hereby grants to those receiving rights under this license
 * a perpetual, worldwide, non-exclusive, no-charge, royalty-free,
 * irrevocable (except for failure to satisfy the conditions of this license)
 * patent license to make, have made, use, offer to sell, sell, import, and
 * otherwise transfer this software, where such license applies only to those
 * patent claims, already acquired or hereafter acquired, licensable by such
 * copyright holder or contributor that are necessarily infringed by:
 *
 * (a) their Contribution(s) (the licensed copyrights of copyright holders
 *     and non-copyrightable additions of contributors, in source or binary
 *     form) alone; or
 *
 * (b) combination of their Contribution(s) with the work of authorship to
 *     which such Contribution(s) was added by such copyright holder or
 *     contributor, if, at the time the Contribution is added, such addition
 *     causes such combination to be necessarily infringed. The patent license
 *     shall not apply to any other combinations which include the
 *     Contribution.
 *
 * Except as expressly stated above, no rights or licenses from any copyright
 * holder or contributor is granted under this license, whether expressly, by
 * implication, estoppel or otherwise.
 *
 * DISCLAIMER
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#pragma once

#include "sysprof-version.h"

#ifndef _SYSPROF_EXTERN
#define _SYSPROF_EXTERN extern
#endif

#if defined(__GNUC__) || defined (__clang__)
#define _SYSPROF_DEPRECATED __attribute__((__deprecated__))
#define _SYSPROF_DEPRECATED_FOR(f) __attribute__((__deprecated__("Use '" #f "' instead")))
#define _SYSPROF_UNAVAILABLE(maj,min) __attribute__((__deprecated__("Not available before " #maj "." #min)))
#elif defined(_MSC_VER)
#define _SYSPROF_DEPRECATED __declspec(deprecated)
#define _SYSPROF_DEPRECATED_FOR(f) __declspec(deprecated("is deprecated. Use '" #f "' instead"))
#define _SYSPROF_UNAVAILABLE(maj,min) __declspec(deprecated("is not available before " #maj "." #min))
#else
#define _SYSPROF_DEPRECATED
#define _SYSPROF_DEPRECATED_FOR(f)
#define _SYSPROF_UNAVAILABLE(maj,min)
#endif

#ifdef SYSPROF_DISABLE_DEPRECATION_WARNINGS
#define SYSPROF_DEPRECATED _SYSPROF_EXTERN
#define SYSPROF_DEPRECATED_FOR(f) _SYSPROF_EXTERN
#define SYSPROF_UNAVAILABLE(maj,min) _SYSPROF_EXTERN
#else
#define SYSPROF_DEPRECATED _SYSPROF_DEPRECATED _SYSPROF_EXTERN
#define SYSPROF_DEPRECATED_FOR(f) _SYSPROF_DEPRECATED_FOR(f) _SYSPROF_EXTERN
#define SYSPROF_UNAVAILABLE(maj,min) _SYSPROF_UNAVAILABLE(maj,min) _SYSPROF_EXTERN
#endif

#define SYSPROF_VERSION_3_34 (SYSPROF_ENCODE_VERSION (3, 34, 0))
#define SYSPROF_VERSION_3_36 (SYSPROF_ENCODE_VERSION (3, 36, 0))
#define SYSPROF_VERSION_3_38 (SYSPROF_ENCODE_VERSION (3, 38, 0))
#define SYSPROF_VERSION_3_40 (SYSPROF_ENCODE_VERSION (3, 40, 0))
#define SYSPROF_VERSION_3_46 (SYSPROF_ENCODE_VERSION (3, 46, 0))

#if (SYSPROF_MINOR_VERSION == 99)
# define SYSPROF_VERSION_CUR_STABLE (SYSPROF_ENCODE_VERSION (SYSPROF_MAJOR_VERSION + 1, 0, 0))
#elif (SYSPROF_MINOR_VERSION % 2)
# define SYSPROF_VERSION_CUR_STABLE (SYSPROF_ENCODE_VERSION (SYSPROF_MAJOR_VERSION, SYSPROF_MINOR_VERSION + 1, 0))
#else
# define SYSPROF_VERSION_CUR_STABLE (SYSPROF_ENCODE_VERSION (SYSPROF_MAJOR_VERSION, SYSPROF_MINOR_VERSION, 0))
#endif

#if (SYSPROF_MINOR_VERSION == 99)
# define SYSPROF_VERSION_PREV_STABLE (SYSPROF_ENCODE_VERSION (SYSPROF_MAJOR_VERSION + 1, 0, 0))
#elif (SYSPROF_MINOR_VERSION % 2)
# define SYSPROF_VERSION_PREV_STABLE (SYSPROF_ENCODE_VERSION (SYSPROF_MAJOR_VERSION, SYSPROF_MINOR_VERSION - 1, 0))
#else
# define SYSPROF_VERSION_PREV_STABLE (SYSPROF_ENCODE_VERSION (SYSPROF_MAJOR_VERSION, SYSPROF_MINOR_VERSION - 2, 0))
#endif

/**
 * SYSPROF_VERSION_MIN_REQUIRED:
 *
 * A macro that should be defined by the user prior to including
 * the ide.h header.
 *
 * The definition should be one of the predefined IDE version
 * macros: %SYSPROF_VERSION_3_34, ...
 *
 * This macro defines the lower bound for the Builder API to use.
 *
 * If a function has been deprecated in a newer version of Builder,
 * it is possible to use this symbol to avoid the compiler warnings
 * without disabling warning for every deprecated function.
 *
 * Since: 3.34
 */
#ifndef SYSPROF_VERSION_MIN_REQUIRED
# define SYSPROF_VERSION_MIN_REQUIRED (SYSPROF_VERSION_CUR_STABLE)
#endif

/**
 * SYSPROF_VERSION_MAX_ALLOWED:
 *
 * A macro that should be defined by the user prior to including
 * the ide.h header.

 * The definition should be one of the predefined Builder version
 * macros: %SYSPROF_VERSION_1_0, %SYSPROF_VERSION_1_2,...
 *
 * This macro defines the upper bound for the IDE API to use.
 *
 * If a function has been introduced in a newer version of Builder,
 * it is possible to use this symbol to get compiler warnings when
 * trying to use that function.
 *
 * Since: 3.34
 */
#ifndef SYSPROF_VERSION_MAX_ALLOWED
# if SYSPROF_VERSION_MIN_REQUIRED > SYSPROF_VERSION_PREV_STABLE
#  define SYSPROF_VERSION_MAX_ALLOWED (SYSPROF_VERSION_MIN_REQUIRED)
# else
#  define SYSPROF_VERSION_MAX_ALLOWED (SYSPROF_VERSION_CUR_STABLE)
# endif
#endif

#if SYSPROF_VERSION_MAX_ALLOWED < SYSPROF_VERSION_MIN_REQUIRED
#error "SYSPROF_VERSION_MAX_ALLOWED must be >= SYSPROF_VERSION_MIN_REQUIRED"
#endif
#if SYSPROF_VERSION_MIN_REQUIRED < SYSPROF_VERSION_3_34
#error "SYSPROF_VERSION_MIN_REQUIRED must be >= SYSPROF_VERSION_3_34"
#endif

#define SYSPROF_AVAILABLE_IN_ALL                   _SYSPROF_EXTERN

#if SYSPROF_VERSION_MIN_REQUIRED >= SYSPROF_VERSION_3_34
# define SYSPROF_DEPRECATED_IN_3_34                SYSPROF_DEPRECATED
# define SYSPROF_DEPRECATED_IN_3_34_FOR(f)         SYSPROF_DEPRECATED_FOR(f)
#else
# define SYSPROF_DEPRECATED_IN_3_34                _SYSPROF_EXTERN
# define SYSPROF_DEPRECATED_IN_3_34_FOR(f)         _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MAX_ALLOWED < SYSPROF_VERSION_3_34
# define SYSPROF_AVAILABLE_IN_3_34                 SYSPROF_UNAVAILABLE(3, 34)
#else
# define SYSPROF_AVAILABLE_IN_3_34                 _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MIN_REQUIRED >= SYSPROF_VERSION_3_36
# define SYSPROF_DEPRECATED_IN_3_36                SYSPROF_DEPRECATED
# define SYSPROF_DEPRECATED_IN_3_36_FOR(f)         SYSPROF_DEPRECATED_FOR(f)
#else
# define SYSPROF_DEPRECATED_IN_3_36                _SYSPROF_EXTERN
# define SYSPROF_DEPRECATED_IN_3_36_FOR(f)         _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MAX_ALLOWED < SYSPROF_VERSION_3_36
# define SYSPROF_AVAILABLE_IN_3_36                 SYSPROF_UNAVAILABLE(3, 36)
#else
# define SYSPROF_AVAILABLE_IN_3_36                 _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MIN_REQUIRED >= SYSPROF_VERSION_3_38
# define SYSPROF_DEPRECATED_IN_3_38                SYSPROF_DEPRECATED
# define SYSPROF_DEPRECATED_IN_3_38_FOR(f)         SYSPROF_DEPRECATED_FOR(f)
#else
# define SYSPROF_DEPRECATED_IN_3_38                _SYSPROF_EXTERN
# define SYSPROF_DEPRECATED_IN_3_38_FOR(f)         _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MAX_ALLOWED < SYSPROF_VERSION_3_38
# define SYSPROF_AVAILABLE_IN_3_38                 SYSPROF_UNAVAILABLE(3, 38)
#else
# define SYSPROF_AVAILABLE_IN_3_38                 _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MIN_REQUIRED >= SYSPROF_VERSION_3_40
# define SYSPROF_DEPRECATED_IN_3_40                SYSPROF_DEPRECATED
# define SYSPROF_DEPRECATED_IN_3_40_FOR(f)         SYSPROF_DEPRECATED_FOR(f)
#else
# define SYSPROF_DEPRECATED_IN_3_40                _SYSPROF_EXTERN
# define SYSPROF_DEPRECATED_IN_3_40_FOR(f)         _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MAX_ALLOWED < SYSPROF_VERSION_3_40
# define SYSPROF_AVAILABLE_IN_3_40                 SYSPROF_UNAVAILABLE(3, 40)
#else
# define SYSPROF_AVAILABLE_IN_3_40                 _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MIN_REQUIRED >= SYSPROF_VERSION_3_46
# define SYSPROF_DEPRECATED_IN_3_46                SYSPROF_DEPRECATED
# define SYSPROF_DEPRECATED_IN_3_46_FOR(f)         SYSPROF_DEPRECATED_FOR(f)
#else
# define SYSPROF_DEPRECATED_IN_3_46                _SYSPROF_EXTERN
# define SYSPROF_DEPRECATED_IN_3_46_FOR(f)         _SYSPROF_EXTERN
#endif

#if SYSPROF_VERSION_MAX_ALLOWED < SYSPROF_VERSION_3_46
# define SYSPROF_AVAILABLE_IN_3_46                 SYSPROF_UNAVAILABLE(3, 46)
#else
# define SYSPROF_AVAILABLE_IN_3_46                 _SYSPROF_EXTERN
#endif
