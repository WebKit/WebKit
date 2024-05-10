/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "config.h"

#include <algorithm>
#include <wtf/Assertions.h>
#include <wtf/FastMalloc.h>
#include "../../../ThirdParty/skia/include/private/base/SkMalloc.h"

void sk_abort_no_print()
{
    CRASH();
}

void sk_out_of_memory(void)
{
    RELEASE_ASSERT_NOT_REACHED_WITH_MESSAGE("sk_out_of_memory");
}

void sk_free(void* p)
{
    WTF::fastFree(p);
}

void* sk_realloc_throw(void* addr, size_t size)
{
    return WTF::fastRealloc(addr, size);
}

void* sk_malloc_flags(size_t size, unsigned flags)
{
    if (flags & SK_MALLOC_ZERO_INITIALIZE) {
        if (flags & SK_MALLOC_THROW)
            return WTF::fastZeroedMalloc(size);

        auto result = WTF::tryFastZeroedMalloc(size);
        void* ptr;
        if (result.getValue(ptr))
            return ptr;
        return nullptr;
    }

    if (flags & SK_MALLOC_THROW)
        return WTF::fastMalloc(size);

    auto result = WTF::tryFastMalloc(size);
    void* ptr;
    if (result.getValue(ptr))
        return ptr;
    return nullptr;
}

size_t sk_malloc_size(void *ptr, size_t size)
{
    // bmalloc/fastMalloc may be disabled either at build time or run
    // time, and in both cases WTF::fastMallocSize() will return 1.
    return std::max(WTF::fastMallocSize(ptr), size);
}
