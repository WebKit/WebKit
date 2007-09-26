/*
 * Copyright (C) 2006 Apple Computer, Inc.
 * Copyright (C) 2006 Samuel Weinig <sam.weinig@gmail.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include <wtf/Platform.h>

#if PLATFORM(MAC)
#define HAVE_JNI 1
#endif

#if PLATFORM(DARWIN)

#define HAVE_ERRNO_H 1
#define HAVE_FUNC_ISINF 1
#define HAVE_FUNC_ISNAN 1
#define HAVE_MMAP 1
#define HAVE_MERGESORT 1
#define HAVE_SBRK 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TIMEB_H 1

#elif PLATFORM(WIN_OS)

// If we don't define these, they get defined in windef.h. 
// We want to use std::min and std::max
#define max max
#define min min

// We need to define this before the first #include of stdlib.h or it won't contain rand_s.
#define _CRT_RAND_S

#define HAVE_FLOAT_H 1
#define HAVE_FUNC__FINITE 1
#define HAVE_SYS_TIMEB_H 1
#define HAVE_VIRTUALALLOC 1

#else

/* FIXME: is this actually used or do other platforms generate their own config.h? */

#define HAVE_ERRNO_H 1
#define HAVE_FUNC_ISINF 1
#define HAVE_FUNC_ISNAN 1
#define HAVE_MMAP 1
#define HAVE_SBRK 1
#define HAVE_STRINGS_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_TIME_H 1

#endif

#define HAVE_PCREPOSIX 1

/* FIXME: if all platforms have these, do they really need #defines? */
#define HAVE_STDINT_H 1
#define HAVE_STRING_H 1

#define WTF_CHANGES 1

#ifdef __cplusplus
#undef new
#undef delete
#include <wtf/FastMalloc.h>
#endif
