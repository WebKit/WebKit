/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

#include "Sizes.h"
#include "Zone.h"

namespace bmalloc {

// The memory analysis API requires the contents of this struct to be a static
// constant in the program binary. The leaks process will load this struct
// out of the program binary (and not out of the running process).
static malloc_introspection_t introspect = {
    .enumerator = Zone::enumerator
};

template<typename T> static void remoteRead(task_t task, memory_reader_t reader, vm_address_t pointer, T& result)
{
    void* tmp;
    (*reader)(task, pointer, sizeof(T), &tmp);
    memcpy(&result, tmp, sizeof(T));
}

// Support malloc_zone_from_ptr, which calls size() on each registered zone.
size_t Zone::size(malloc_zone_t*, const void*)
{
    // Our zone is not public API, so no pointer can belong to us.
    return 0;
}

// This function runs inside the leaks process.
kern_return_t Zone::enumerator(task_t task, void* context, unsigned type_mask, vm_address_t zone_address, memory_reader_t reader, vm_range_recorder_t recorder)
{
    Zone remoteZone;
    remoteRead(task, reader, zone_address, remoteZone);
    
    for (auto* superChunk : remoteZone.superChunks()) {
        vm_range_t range = { reinterpret_cast<vm_address_t>(superChunk), superChunkSize };

        if ((type_mask & MALLOC_PTR_REGION_RANGE_TYPE))
            (*recorder)(task, context, MALLOC_PTR_REGION_RANGE_TYPE, &range, 1);

        if ((type_mask & MALLOC_PTR_IN_USE_RANGE_TYPE))
            (*recorder)(task, context, MALLOC_PTR_IN_USE_RANGE_TYPE, &range, 1);
    }

    return 0;
}

Zone::Zone()
{
    malloc_zone_t::size = size;
    malloc_zone_t::zone_name = "WebKit Malloc";
    malloc_zone_t::introspect = &bmalloc::introspect;
    malloc_zone_t::version = 4;
    malloc_zone_register(this);
}

} // namespace bmalloc
