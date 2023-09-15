/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <wtf/Assertions.h>
#include <wtf/text/StringHasher.h>
#include <wtf/text/SuperFastHash.h>
#include <wtf/text/WYHash.h>

namespace WTF {

template<typename T, typename Converter>
unsigned StringHasher::computeHashAndMaskTop8Bits(const T* data, unsigned characterCount)
{
#if PLATFORM(MAC)
    if (LIKELY(characterCount <= smallStringThreshold))
        return SuperFastHash::computeHashAndMaskTop8Bits<T, Converter>(data, characterCount);
    return WYHash::computeHashAndMaskTop8Bits<T, Converter>(data, characterCount);
#else
    return SuperFastHash::computeHashAndMaskTop8Bits<T, Converter>(data, characterCount);
#endif
}

template<typename T, unsigned characterCount>
constexpr unsigned StringHasher::computeLiteralHashAndMaskTop8Bits(const T (&characters)[characterCount])
{
    constexpr unsigned characterCountWithoutNull = characterCount - 1;
#if PLATFORM(MAC)
    if constexpr (LIKELY(characterCountWithoutNull <= smallStringThreshold))
        return SuperFastHash::computeHashAndMaskTop8Bits<T>(characters, characterCountWithoutNull);
    return WYHash::computeHashAndMaskTop8Bits<T>(characters, characterCountWithoutNull);
#else
    return SuperFastHash::computeHashAndMaskTop8Bits<T>(characters, characterCountWithoutNull);
#endif
}

inline void StringHasher::addCharacter(UChar character)
{
#if PLATFORM(MAC)
    if (m_bufferSize == smallStringThreshold) {
        // This algorithm must stay in sync with WYHash::hash function.
        if (!m_pendingHashValue) {
            m_seed = WYHash::initSeed();
            m_see1 = m_seed;
            m_see2 = m_seed;
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
#else
    SuperFastHash::addCharacterImpl(character, m_hasPendingCharacter, m_pendingCharacter, m_hash);
#endif
}

inline unsigned StringHasher::hashWithTop8BitsMasked()
{
#if PLATFORM(MAC)
    unsigned hashValue;
    if (!m_pendingHashValue) {
        ASSERT(m_bufferSize <= smallStringThreshold);
        hashValue = SuperFastHash::computeHashAndMaskTop8Bits<UChar>(m_buffer.data(), m_bufferSize);
    } else {
        // This algorithm must stay in sync with WYHash::hash function.
        auto wyr8 = WYHash::Reader16Bit<UChar>::wyr8;
        unsigned i = m_bufferSize;
        if (i <= 24)
            m_seed ^= m_see1 ^ m_see2;
        UChar* p = m_buffer.data();
        WYHash::handleGreaterThan8CharactersCase(p, i, wyr8, m_seed, m_see1, m_see2);

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
        hashValue = StringHasher::avoidZero(WYHash::handleEndCase(a, b, m_seed, totalByteCount) & StringHasher::maskHash);

        m_pendingHashValue = false;
        m_numberOfProcessedCharacters = m_seed = m_see1 = m_see2 = 0;
    }
    m_bufferSize = 0;
    return hashValue;
#else
    unsigned hashValue = SuperFastHash::hashWithTop8BitsMaskedImpl(m_hasPendingCharacter, m_pendingCharacter, m_hash);
    m_hasPendingCharacter = false;
    m_pendingCharacter = 0;
    m_hash = stringHashingStartValue;
    return hashValue;
#endif
}

} // namespace WTF

using WTF::StringHasher;
