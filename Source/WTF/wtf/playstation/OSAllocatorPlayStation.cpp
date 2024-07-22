/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
 * Copyright (C) 2024 SONY Interactive Entertainment Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include <wtf/OSAllocator.h>

#include <errno.h>
#include <memory-extra/vss.h>
#include <wtf/Assertions.h>
#include <wtf/DataLog.h>
#include <wtf/MathExtras.h>
#include <wtf/PageBlock.h>
#include <wtf/SafeStrerror.h>
#include <wtf/text/CString.h>

namespace WTF {

void* OSAllocator::tryReserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    ASSERT_UNUSED(includesGuardPages, !includesGuardPages);
    ASSERT_UNUSED(jitCageEnabled, !jitCageEnabled);
    ASSERT_UNUSED(executable, !executable);

    void* result = memory_extra::vss::reserve(bytes);
    if (!result)
        return nullptr;

    bool success = memory_extra::vss::commit(result, bytes, writable, usage);
    if (!success) {
        memory_extra::vss::release(result, bytes);
        return nullptr;
    }

    return result;
}

void* OSAllocator::tryReserveUncommitted(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    UNUSED_PARAM(usage);
    UNUSED_PARAM(writable);
    ASSERT_UNUSED(includesGuardPages, !includesGuardPages);
    ASSERT_UNUSED(jitCageEnabled, !jitCageEnabled);
    ASSERT_UNUSED(executable, !executable);

    void* result = memory_extra::vss::reserve(bytes);
    if (!result)
        return nullptr;

    return result;
}

void* OSAllocator::reserveUncommitted(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    void* result = tryReserveUncommitted(bytes, usage, writable, executable, jitCageEnabled, includesGuardPages);
    RELEASE_ASSERT(result);
    return result;
}

void* OSAllocator::tryReserveUncommittedAligned(size_t bytes, size_t alignment, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    ASSERT_UNUSED(includesGuardPages, !includesGuardPages);
    ASSERT_UNUSED(jitCageEnabled, !jitCageEnabled);
    ASSERT_UNUSED(executable, !executable);
    UNUSED_PARAM(usage);
    UNUSED_PARAM(writable);
    ASSERT(hasOneBitSet(alignment) && alignment >= pageSize());

    void* result = memory_extra::vss::reserve(bytes, alignment);
    if (!result)
        return nullptr;

    return result;
}

void* OSAllocator::reserveAndCommit(size_t bytes, Usage usage, bool writable, bool executable, bool jitCageEnabled, bool includesGuardPages)
{
    void* result = tryReserveAndCommit(bytes, usage, writable, executable, jitCageEnabled, includesGuardPages);
    RELEASE_ASSERT(result);
    return result;
}

void OSAllocator::commit(void* address, size_t bytes, bool writable, bool executable)
{
    ASSERT_UNUSED(executable, !executable);
    bool success = memory_extra::vss::commit(address, bytes, writable, -1);
    RELEASE_ASSERT(success);
}

void OSAllocator::decommit(void* address, size_t bytes)
{
    bool success = memory_extra::vss::decommit(address, bytes);
    RELEASE_ASSERT(success);
}

void OSAllocator::hintMemoryNotNeededSoon(void* address, size_t bytes)
{
    UNUSED_PARAM(address);
    UNUSED_PARAM(bytes);
}

void OSAllocator::releaseDecommitted(void* address, size_t bytes)
{
    bool success = memory_extra::vss::release(address, bytes);
    RELEASE_ASSERT(success);
}

bool OSAllocator::tryProtect(void* address, size_t bytes, bool readable, bool writable)
{
    return memory_extra::vss::protect(address, bytes, readable, writable, -1);
}

void OSAllocator::protect(void* address, size_t bytes, bool readable, bool writable)
{
    if (bool result = tryProtect(address, bytes, readable, writable); UNLIKELY(!result)) {
        dataLogLn("mprotect failed: ", safeStrerror(errno).data());
        RELEASE_ASSERT_NOT_REACHED();
    }
}

} // namespace WTF
