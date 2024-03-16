/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/text/WYHash.h>

namespace WTF {

class StreamingWYHash {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static constexpr unsigned numberOfCharactersInLargestBulkForWYHash = 24; // Don't change this value. It's fixed for WYhash algorithm.

    // Things need to do to update this threshold:
    // 1. This threshold must stay in sync with the threshold in the scripts create_hash_table, Hasher.pm, and hasher.py.
    // 2. Run script `run-bindings-tests --reset-results` to update all CompactHashIndex's under path `WebCore/bindings/scripts/test/JS/`.
    // 3. Manually update all CompactHashIndex's in JSDollarVM.cpp by using createHashTable in hasher.py.
    static constexpr unsigned smallStringThreshold = numberOfCharactersInLargestBulkForWYHash * 2;

    void addCharactersAssumingAligned(UChar a, UChar b)
    {
        addCharacter(a);
        addCharacter(b);
    }

    void addCharacter(UChar character)
    {
        if (m_bufferSize == smallStringThreshold) {
            // This algorithm must stay in sync with WYHash::hash function.
            if (!m_pendingHashValue) {
                m_see1 = m_see2 = m_seed = WYHash::initSeed();
                m_pendingHashValue = true;
            }
            UChar* p = m_buffer.data();
            while (m_bufferSize >= 24) {
                WYHash::consume24Characters(p, WYHash::Reader16Bit<UChar>::wyr8, m_seed, m_see1, m_see2);
                p += 24;
                m_bufferSize -= 24;
            }
            ASSERT(!m_bufferSize);
            m_numberOfProcessedCharacters += smallStringThreshold;
        }

        ASSERT(m_bufferSize < smallStringThreshold);
        m_buffer[m_bufferSize++] = character;
    }

    unsigned finalize() const
    {
        unsigned hashValue;
        if (!m_pendingHashValue) {
            ASSERT(m_bufferSize <= smallStringThreshold);
            hashValue = WYHash::computeHash<UChar>(m_buffer.data(), m_bufferSize);
        } else {
            // This algorithm must stay in sync with WYHash::hash function.
            auto wyr8 = WYHash::Reader16Bit<UChar>::wyr8;
            unsigned i = m_bufferSize;
            uint64_t seed = m_seed;
            uint64_t see1 = m_see1;
            uint64_t see2 = m_see2;
            if (i <= 24)
                seed ^= see1 ^ see2;
            const UChar* p = m_buffer.data();
            WYHash::handleGreaterThan8CharactersCase(p, i, wyr8, seed, see1, see2);

            uint64_t a = 0;
            uint64_t b = 0;
            if (m_bufferSize >= 8) {
                a = wyr8(p + i - 8);
                b = wyr8(p + i - 4);
            } else {
                UChar tmp[8];
                unsigned bufferIndex = smallStringThreshold - (8 - i);
                for (unsigned tmpIndex = 0; tmpIndex < 8; tmpIndex++) {
                    tmp[tmpIndex] = m_buffer[bufferIndex];
                    bufferIndex = (bufferIndex + 1) % smallStringThreshold;
                }

                UChar* tmpPtr = tmp;
                a = wyr8(tmpPtr);
                b = wyr8(tmpPtr + 4);
            }

            const uint64_t totalByteCount = (static_cast<uint64_t>(m_numberOfProcessedCharacters) + static_cast<uint64_t>(m_bufferSize)) << 1;
            hashValue = WYHash::handleEndCase(a, b, seed, totalByteCount);
        }
        return hashValue;
    }

    unsigned hash() const
    {
        return HasherHelpers::finalize(finalize());
    }

private:
    uint64_t m_seed { 0 };
    uint64_t m_see1 { 0 };
    uint64_t m_see2 { 0 };
    unsigned m_numberOfProcessedCharacters { 0 };
    unsigned m_bufferSize { 0 };
    bool m_pendingHashValue { false };
    std::array<UChar, smallStringThreshold> m_buffer;
};

} // namespace WTF

using WTF::StreamingWYHash;
