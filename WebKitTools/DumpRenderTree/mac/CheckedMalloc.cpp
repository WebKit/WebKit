/*
 * Copyright (C) 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
 *           (C) 2007 Graham Dennis (graham.dennis@gmail.com)
 *           (C) 2007 Eric Seidel <eric@webkit.org>
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

#import "config.h" 
#import "CheckedMalloc.h"

#import <malloc/malloc.h>
#import <sys/mman.h>

static void* (*savedMalloc)(malloc_zone_t*, size_t);
static void* (*savedRealloc)(malloc_zone_t*, void*, size_t);

static void* checkedMalloc(malloc_zone_t* zone, size_t size)
{
    if (size >= 0x10000000)
        return 0;
    return savedMalloc(zone, size);
}

static void* checkedRealloc(malloc_zone_t* zone, void* ptr, size_t size)
{
    if (size >= 0x10000000)
        return 0;
    return savedRealloc(zone, ptr, size);
}

void makeLargeMallocFailSilently()
{
    malloc_zone_t* zone = malloc_default_zone();

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    vm_address_t pageStart = reinterpret_cast<vm_address_t>(zone) & static_cast<vm_size_t>(~(getpagesize() - 1));
    vm_size_t len = reinterpret_cast<vm_address_t>(zone) - pageStart + sizeof(malloc_zone_t);
    mprotect(reinterpret_cast<void*>(pageStart), len, PROT_READ | PROT_WRITE);
#endif

    savedMalloc = zone->malloc;
    savedRealloc = zone->realloc;
    zone->malloc = checkedMalloc;
    zone->realloc = checkedRealloc;

#if !defined(BUILDING_ON_TIGER) && !defined(BUILDING_ON_LEOPARD) && !defined(BUILDING_ON_SNOW_LEOPARD)
    mprotect(reinterpret_cast<void*>(pageStart), len, PROT_READ);
#endif
}
