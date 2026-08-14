/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
#include "B3Operations.h"

#include <wtf/Int128.h>
#include <wtf/UnalignedAccess.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if ENABLE(B3_JIT)

namespace JSC::B3 {

namespace {

#if HAVE(INT128_T)
using WidestCopyUnit = UInt128;
#else
using WidestCopyUnit = uint64_t;
#endif

constexpr size_t maxFastCopyCount = 4 * sizeof(WidestCopyUnit);

// Copies count bytes as two overlapping sizeof(T) accesses, so it needs
// sizeof(T) <= count <= 2 * sizeof(T). Both source reads happen before either destination write,
// which is what makes this safe for overlapping ranges without comparing the two pointers.
template<typename T>
ALWAYS_INLINE void copyOverlappingEnds(uint8_t* dst, const uint8_t* src, size_t count)
{
    T low = WTF::unalignedLoad<T>(src);
    T high = WTF::unalignedLoad<T>(src + count - sizeof(T));
    WTF::unalignedStore<T>(dst, low);
    WTF::unalignedStore<T>(dst + count - sizeof(T), high);
}

// The same idea one step wider: two overlapping pairs, for
// 2 * sizeof(T) <= count <= 4 * sizeof(T).
template<typename T>
ALWAYS_INLINE void copyOverlappingEndPairs(uint8_t* dst, const uint8_t* src, size_t count)
{
    T low0 = WTF::unalignedLoad<T>(src);
    T low1 = WTF::unalignedLoad<T>(src + sizeof(T));
    T high0 = WTF::unalignedLoad<T>(src + count - 2 * sizeof(T));
    T high1 = WTF::unalignedLoad<T>(src + count - sizeof(T));
    WTF::unalignedStore<T>(dst, low0);
    WTF::unalignedStore<T>(dst + sizeof(T), low1);
    WTF::unalignedStore<T>(dst + count - 2 * sizeof(T), high0);
    WTF::unalignedStore<T>(dst + count - sizeof(T), high1);
}

} // namespace

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryCopy, void, (void* dst, const void* src, size_t count))
{
    // Short copies dominate this operation, because memory.copy is what a compiler targeting wasm
    // emits for memcpy. libc's memmove spends more on dispatching by size than a short copy costs,
    // so handle the short counts here. One unsigned compare rejects both the too-short and the
    // too-long counts.
    if (count - sizeof(uint32_t) <= maxFastCopyCount - sizeof(uint32_t)) {
        auto* to = static_cast<uint8_t*>(dst);
        const auto* from = static_cast<const uint8_t*>(src);
        if (count < sizeof(uint64_t))
            copyOverlappingEnds<uint32_t>(to, from, count);
        else if (count < 2 * sizeof(uint64_t))
            copyOverlappingEnds<uint64_t>(to, from, count);
        else if (count < 2 * sizeof(WidestCopyUnit))
            copyOverlappingEnds<WidestCopyUnit>(to, from, count);
        else
            copyOverlappingEndPairs<WidestCopyUnit>(to, from, count);
        return;
    }
    memmove(dst, src, count);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryFill, void, (void* dst, int32_t target, size_t count))
{
    memset(dst, target, count);
}

} // namespace JSC::B3

#endif // ENABLE(B3_JIT)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
