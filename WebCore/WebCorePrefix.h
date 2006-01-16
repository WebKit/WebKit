/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

// This prefix file is for use on Mac OS X only. It should contain only:
//    1) files to precompile on Mac OS X for faster builds
//    2) in one case at least: OS-X-specific performance bug workarounds
//    3) the special trick to catch us using new or delete without including "config.h"
// The project should be able to build without this header, although we rarely test that.

// Things that need to be defined globally should go into "config.h".

#ifdef __cplusplus
#define NULL __null
#else
#define NULL ((void *)0)
#endif

#include <assert.h>
#include <ctype.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pthread.h>
#include <regex.h>
#include <setjmp.h>
#include <signal.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#ifdef __cplusplus

#include <algorithm>
#include <cstddef>
#include <new>

// Work around bug 3553309 by re-including <ctype.h>.
#include <cctype>
#define isalnum(c)      __istype((c), (_CTYPE_A|_CTYPE_D))
#define isalpha(c)      __istype((c), _CTYPE_A)
#define iscntrl(c)      __istype((c), _CTYPE_C)
#define isdigit(c)      __isctype((c), _CTYPE_D)        /* ANSI -- locale independent */
#define isgraph(c)      __istype((c), _CTYPE_G)
#define islower(c)      __istype((c), _CTYPE_L)
#define isprint(c)      __istype((c), _CTYPE_R)
#define ispunct(c)      __istype((c), _CTYPE_P)
#define isspace(c)      __istype((c), _CTYPE_S)
#define isupper(c)      __istype((c), _CTYPE_U)
#define isxdigit(c)     __isctype((c), _CTYPE_X)        /* ANSI -- locale independent */
#define tolower(c)      __tolower(c)
#define toupper(c)      __toupper(c)

#endif

#include <sys/types.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <time.h>

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>

#ifdef __OBJC__

#import <Cocoa/Cocoa.h>

#endif

#ifdef __cplusplus
#define new ("if you use new/delete make sure to include config.h at the top of the file"()) 
#define delete ("if you use new/delete make sure to include config.h at the top of the file"()) 
#endif

// Work around bug with C++ library that screws up Objective-C++ when exception support is disabled.
#undef try
#undef catch
