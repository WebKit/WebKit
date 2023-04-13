/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include "IsoMallocFallback.h"

#include "DebugHeap.h"
#include "Environment.h"
#include "bmalloc.h"

namespace bmalloc { namespace IsoMallocFallback {

MallocFallbackState mallocFallbackState;

namespace {

void determineMallocFallbackState()
{
    static std::once_flag onceFlag;
    std::call_once(
        onceFlag,
        [] {
            if (mallocFallbackState != MallocFallbackState::Undecided)
                return;

            if (Environment::get()->isDebugHeapEnabled()) {
                mallocFallbackState = MallocFallbackState::FallBackToMalloc;
                return;
            }

            const char* env = getenv("bmalloc_IsoHeap");
            if (env && (!strcasecmp(env, "false") || !strcasecmp(env, "no") || !strcmp(env, "0")))
                mallocFallbackState = MallocFallbackState::FallBackToMalloc;
            else
                mallocFallbackState = MallocFallbackState::DoNotFallBack;
        });
}

} // anonymous namespace

MallocResult tryMalloc(
    size_t size
#if BENABLE_MALLOC_HEAP_BREAKDOWN
    , malloc_zone_t* zone
#endif
    )
{
    for (;;) {
        switch (mallocFallbackState) {
        case MallocFallbackState::Undecided:
            determineMallocFallbackState();
            continue;
        case MallocFallbackState::FallBackToMalloc:
#if BENABLE_MALLOC_HEAP_BREAKDOWN
            return malloc_zone_malloc(zone, size);
#else
            return api::tryMalloc(size);
#endif
        case MallocFallbackState::DoNotFallBack:
            return MallocResult();
        }
        RELEASE_BASSERT_NOT_REACHED();
    }
}

bool tryFree(
    void* ptr
#if BENABLE_MALLOC_HEAP_BREAKDOWN
    , malloc_zone_t* zone
#endif
    )
{
    for (;;) {
        switch (mallocFallbackState) {
        case MallocFallbackState::Undecided:
            determineMallocFallbackState();
            continue;
        case MallocFallbackState::FallBackToMalloc:
#if BENABLE_MALLOC_HEAP_BREAKDOWN
            malloc_zone_free(zone, ptr);
#else
            api::free(ptr);
#endif
            return true;
        case MallocFallbackState::DoNotFallBack:
            return false;
        }
        RELEASE_BASSERT_NOT_REACHED();
    }
}

} } // namespace bmalloc::IsoMallocFallback
