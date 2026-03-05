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
#include <wtf/text/WYHash.h>

namespace WTF {

template<typename T, typename Converter>
unsigned StringHasher::computeHashAndMaskTop8Bits(std::span<const T> data)
{
    return WYHash::computeHashAndMaskTop8Bits<T, Converter>(data);
}

template<typename T, unsigned characterCount>
constexpr unsigned StringHasher::computeLiteralHashAndMaskTop8Bits(const T (&characters)[characterCount])
{
    constexpr unsigned characterCountWithoutNull = characterCount - 1;
    return WYHash::computeHashAndMaskTop8Bits<T>(unsafeMakeSpan(characters, characterCountWithoutNull));
}

inline void StringHasher::addCharacter(char16_t character)
{
    static constexpr unsigned bufferCapacity = numberOfCharactersInLargestBulkForWYHash * 2;
    if (m_bufferSize == bufferCapacity) {
        // This algorithm must stay in sync with WYHash::hash function.
        if (!m_pendingHashValue) {
            m_seed = WYHash::initSeed();
            m_see1 = m_seed;
            m_see2 = m_seed;
            m_pendingHashValue = true;
        }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        char16_t* p = m_buffer.data();
        while (m_bufferSize >= 24) {
            WYHash::consume24Characters(p, WYHash::Reader16Bit<char16_t>::wyr8, m_seed, m_see1, m_see2);
            p += 24;
            m_bufferSize -= 24;
        }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
        ASSERT(!m_bufferSize);
        m_numberOfProcessedCharacters += bufferCapacity;
    }

    ASSERT(m_bufferSize < bufferCapacity);
    m_buffer[m_bufferSize++] = character;
}

inline unsigned StringHasher::hashWithTop8BitsMasked()
{
    static constexpr unsigned bufferCapacity = numberOfCharactersInLargestBulkForWYHash * 2;
    unsigned hashValue;
    if (!m_pendingHashValue) {
        hashValue = WYHash::computeHashAndMaskTop8Bits<char16_t>(std::span { m_buffer }.first(m_bufferSize));
    } else {
        // This algorithm must stay in sync with WYHash::hash function.
        auto wyr8 = WYHash::Reader16Bit<char16_t>::wyr8;
        unsigned i = m_bufferSize;
        if (i <= 24)
            m_seed ^= m_see1 ^ m_see2;
WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN
        char16_t* p = m_buffer.data();
        WYHash::handleGreaterThan8CharactersCase(p, i, wyr8, m_seed, m_see1, m_see2);

        uint64_t a = 0;
        uint64_t b = 0;
        if (m_bufferSize >= 8) {
            a = wyr8(p + i - 8);
            b = wyr8(p + i - 4);
        } else {
            char16_t tmp[8];
            unsigned bufferIndex = bufferCapacity - (8 - i);
            for (unsigned tmpIndex = 0; tmpIndex < 8; tmpIndex++) {
                tmp[tmpIndex] = m_buffer[bufferIndex];
                bufferIndex = (bufferIndex + 1) % bufferCapacity;
            }

            char16_t* tmpPtr = tmp;
            a = wyr8(tmpPtr);
            b = wyr8(tmpPtr + 4);
        }
WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

        const uint64_t totalByteCount = (static_cast<uint64_t>(m_numberOfProcessedCharacters) + static_cast<uint64_t>(m_bufferSize)) << 1;
        hashValue = StringHasher::avoidZero(WYHash::handleEndCase(a, b, m_seed, totalByteCount) & StringHasher::maskHash);

        m_pendingHashValue = false;
        m_numberOfProcessedCharacters = m_seed = m_see1 = m_see2 = 0;
    }
    m_bufferSize = 0;
    return hashValue;
}

} // namespace WTF

using WTF::StringHasher;
