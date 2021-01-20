/*
 * Copyright (C) 2005 Filip Pizlo. All rights reserved.
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * THIS SOFTWARE IS PROVIDED BY FILIP PIZLO ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL FILIP PIZLO OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

/*
 * Hacks inttypes to work right on NetBSD (and possibly other systems) and C++.
 */

/* these are the two libraries that might #define the intXXX macros.
   including them here prevents them from being included after I've
   already #undefined the macros. */
#include <inttypes.h>
#include <sys/types.h>

#ifndef TSF_INTTYPES_WORK_RIGHT
#undef int8_t
#undef uint8_t
#undef int16_t
#undef uint16_t
#undef int32_t
#undef uint32_t
#undef int64_t
#undef uint64_t
#endif

/* wow.  just wow.  the way C++ sabotages inttypes.h is spectacular. */
#ifndef UINT32_MAX
#define UINT32_MAX (((uint32_t)0)-((uint32_t)1))
#endif


