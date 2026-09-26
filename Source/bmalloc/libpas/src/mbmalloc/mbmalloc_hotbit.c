/*
 * Copyright (c) 2019-2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "hotbit_heap.h"

#if PAS_ENABLE_HOTBIT

#include "pas_scavenger.h"

#if PAS_OS(DARWIN)
#include <malloc/malloc.h>
#endif

void* mbmalloc(size_t size)
{
    return hotbit_try_allocate(size);
}

void* mbmemalign(size_t alignment, size_t size)
{
    return hotbit_try_allocate_with_alignment(size, alignment);
}

void* mbrealloc(void* p, size_t ignored_old_size, size_t new_size)
{
    return hotbit_try_reallocate(p, new_size, pas_reallocate_free_if_successful);
}

void mbfree(void* p, size_t ignored_size)
{
    hotbit_deallocate(p);
}

void mbscavenge(void)
{
    pas_scavenger_run_synchronously_now();
#if PAS_OS(DARWIN)
    malloc_zone_pressure_relief(NULL, 0);
#endif
}

#endif /* PAS_ENABLE_HOTBIT */
