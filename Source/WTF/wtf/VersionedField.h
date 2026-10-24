/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include <algorithm>
#include <wtf/Atomics.h>

namespace WTF {

// A value paired with a version, for tracking a concurrent high/low watermark when writes in one direction
// must succeed but writes in the other may spuriously fail. We call these hard and soft writers, respectively.
//
// When soft writing, the writer should `watch()` the field, compute the update, then publish with a
// `tryUpdateWatched()`. This will fail if anyone wrote to the field in the meantime.
//
// Hard writers should use minimize() / maximize(). While the field is watched these bump the version even
// when they don't change the value, so a soft writer can't publish over an update it never saw.
//
// Since this isn't a lock, users must provide their own ordering between this field and the data it tracks.
// In particular, a Dekker-style pattern (a hard writer writes some other state then updates this field, while
// a soft writer watches this field then reads that state) needs a full fence between the two accesses on each
// side: either a storeLoadFence() or a FullyFenced read-modify-write, such as watch<MemoryOrder::FullyFenced>().
//
// FIXME: Make this a template over the value and version types if another user ever needs them. Both could
// theoretically be a uintptr_t, we'd just have to add double-word atomics to WTF::Atomic.
class VersionedField {
public:
    struct Snapshot {
        bool isWatched() const { return version & isWatchedBit; }
        friend bool operator==(const Snapshot&, const Snapshot&) = default;

        uint32_t value { 0 };
        // Assumes fewer than 2**31 updates happen between a watch() and its tryUpdateWatched().
        uint32_t version { 0 };

    };

    constexpr VersionedField(uint32_t value = 0)
        : m_field(std::bit_cast<uint64_t>(Snapshot { value, 0 }))
    {
    }

    template<MemoryOrder order = MemoryOrder::SeqCst>
    uint32_t value() const { return std::bit_cast<Snapshot>(m_field.load<order>()).value; }

    // When already watched, an Acquire load suffices even for FullyFenced: by multi-copy atomicity,
    // reading the watching RMW orders us after everything its fence ordered.
    template<MemoryOrder order = MemoryOrder::SeqCst>
    Snapshot watch()
    {
        static constexpr uint64_t watchedMask = std::bit_cast<uint64_t>(Snapshot { 0, isWatchedBit });

        static_assert(order != MemoryOrder::Release && order != MemoryOrder::AcquireRelease);
        uint64_t bits = m_field.load<order == MemoryOrder::FullyFenced ? MemoryOrder::Acquire : order>();
        if (bits & watchedMask)
            return std::bit_cast<Snapshot>(bits);
        // We could do a transaction here but on ARM64 with LSE the exchangeOr is cheaper and can't fail.
        return std::bit_cast<Snapshot>(m_field.exchangeOr<order>(watchedMask) | watchedMask);
    }

    // We don't clear the isWatched here, this avoids an atomic RMW in watch() for subsequent watches.
    template<MemoryOrder order = MemoryOrder::SeqCst>
    bool tryUpdateWatched(Snapshot expected, uint32_t newValue)
    {
        ASSERT_WITH_MESSAGE(expected.isWatched(), "The old expected value should have been watched.");
        Snapshot desired { newValue, expected.version + softVersionStep };
        uint64_t expectedBits = std::bit_cast<uint64_t>(expected);
        return m_field.compareExchangeStrong<order>(expectedBits, std::bit_cast<uint64_t>(desired)) == expectedBits;
    }

    template<MemoryOrder order = MemoryOrder::SeqCst>
    void minimize(uint32_t value) { update<order>(value, [](uint32_t a, uint32_t b) { return std::min(a, b); }); }
    template<MemoryOrder order = MemoryOrder::SeqCst>
    void maximize(uint32_t value) { update<order>(value, [](uint32_t a, uint32_t b) { return std::max(a, b); }); }

    // Unconditionally sets the value and invalidates any outstanding watch.
    template<MemoryOrder order = MemoryOrder::SeqCst>
    void set(uint32_t value)
    {
        transaction<order>([&](Snapshot& field) {
            field.value = value;
            field.version = (field.version | isWatchedBit) + 1;
            ASSERT(!field.isWatched());
            return true;
        });
    }

private:
    static constexpr uint32_t isWatchedBit = 1;
    static constexpr uint32_t softVersionStep = 2;

    template<MemoryOrder order>
    bool transaction(NOESCAPE const Invocable<bool(Snapshot&)> auto& func)
    {
        return m_field.transaction<order>([&](uint64_t& bits) {
            auto field = std::bit_cast<Snapshot>(bits);
            if (!func(field))
                return false;
            bits = std::bit_cast<uint64_t>(field);
            return true;
        });
    }

    template<MemoryOrder order>
    void update(uint32_t value, NOESCAPE const Invocable<uint32_t(uint32_t, uint32_t)> auto& combine)
    {
        transaction<order>([&](Snapshot& field) {
            uint32_t combined = combine(field.value, value);
            if (!field.isWatched()) {
                if (combined == field.value)
                    return false;
                field.value = combined;
                return true;
            }
            field.value = combined;
            // Clears the watched bit, so any soft writer has to watch again.
            field.version++;
            ASSERT(!field.isWatched());
            return true;
        });
    }

    Atomic<uint64_t> m_field;
};

} // namespace WTF

using WTF::VersionedField;
