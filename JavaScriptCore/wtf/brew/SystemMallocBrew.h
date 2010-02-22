/*
 * Copyright (C) 2010 Company 100, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SystemMallocBrew_h
#define SystemMallocBrew_h

#include <AEEStdLib.h>

static inline void* mallocBrew(size_t n)
{
    // By default, memory allocated using MALLOC() is initialized
    // to zero. This behavior can be disabled by performing a bitwise
    // OR of the flag ALLOC_NO_ZMEM with the dwSize parameter.
    return MALLOC(n | ALLOC_NO_ZMEM);
}

static inline void* callocBrew(size_t numElements, size_t elementSize)
{
    return MALLOC(numElements * elementSize);
}

static inline void freeBrew(void* p)
{
    return FREE(p);
}

static inline void* reallocBrew(void* p, size_t n)
{
    return REALLOC(p, n | ALLOC_NO_ZMEM);
}

// Use MALLOC macro instead of the standard malloc function.
// Although RVCT provides malloc, we can't use it in BREW
// because the loader does not initialize the base address properly.
#define malloc(n) mallocBrew(n)
#define calloc(n, s) callocBrew(n, s)
#define realloc(p, n) reallocBrew(p, n)
#define free(p) freeBrew(p)

#endif // SystemMallocBrew_h
