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

using WTF::WidestUnalignedUnit;

constexpr size_t maxFastFillCount = 4 * sizeof(WidestUnalignedUnit);

// Writes count bytes as two overlapping runs of unitsPerEnd stores, so it needs
// unitsPerEnd * sizeof(T) <= count <= 2 * unitsPerEnd * sizeof(T). Every store writes the same
// splatted value, so unlike the copy case the order of the stores does not matter.
template<typename T, unsigned unitsPerEnd>
ALWAYS_INLINE void fillOverlappingEnds(uint8_t* dst, T value, size_t count)
{
    for (unsigned i = 0; i < unitsPerEnd; ++i)
        WTF::unalignedStore<T>(dst + i * sizeof(T), value);
    for (unsigned i = 0; i < unitsPerEnd; ++i)
        WTF::unalignedStore<T>(dst + count - (unitsPerEnd - i) * sizeof(T), value);
}

} // namespace

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryCopy, void, (void* dst, const void* src, size_t count))
{
    // Short copies dominate this operation, because memory.copy is what a compiler targeting wasm
    // emits for memcpy. libc's memmove spends more on dispatching by size than a short copy costs,
    // so handle the short counts here. One unsigned compare rejects both the too-short and the
    // too-long counts.
    if (count - sizeof(uint32_t) <= WTF::maxSmallCopySize - sizeof(uint32_t)) {
        WTF::copySmallMemory<sizeof(uint32_t)>(static_cast<uint8_t*>(dst), static_cast<const uint8_t*>(src), count);
        return;
    }
    memmove(dst, src, count);
}

JSC_DEFINE_NOEXCEPT_JIT_OPERATION(operationMemoryFill, void, (void* dst, int32_t target, size_t count))
{
    if (count - 1 <= maxFastFillCount - 1) {
        auto* to = static_cast<uint8_t*>(dst);
        uint64_t splat = static_cast<uint8_t>(target) * 0x0101010101010101ULL;
        WidestUnalignedUnit widest = splat;
        if constexpr (sizeof(WidestUnalignedUnit) > sizeof(uint64_t))
            widest = (static_cast<WidestUnalignedUnit>(splat) << 64) | splat;
        if (count < sizeof(uint16_t))
            fillOverlappingEnds<uint8_t, 1>(to, static_cast<uint8_t>(splat), count);
        else if (count < sizeof(uint32_t))
            fillOverlappingEnds<uint16_t, 1>(to, static_cast<uint16_t>(splat), count);
        else if (count < sizeof(uint64_t))
            fillOverlappingEnds<uint32_t, 1>(to, static_cast<uint32_t>(splat), count);
        else if (count < 2 * sizeof(uint64_t))
            fillOverlappingEnds<uint64_t, 1>(to, splat, count);
        else if (count < 2 * sizeof(WidestUnalignedUnit))
            fillOverlappingEnds<WidestUnalignedUnit, 1>(to, widest, count);
        else
            fillOverlappingEnds<WidestUnalignedUnit, 2>(to, widest, count);
        return;
    }
    memset(dst, target, count);
}

} // namespace JSC::B3

#endif // ENABLE(B3_JIT)

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
