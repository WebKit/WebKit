/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
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
#include "ISOTrackEncryptionBox.h"

#include "BitReader.h"
#include <wtf/CheckedArithmetic.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

ISOTrackEncryptionBox::ISOTrackEncryptionBox()
    : ISOFullBox(boxTypeName(), 0, 0)
{
}

ISOTrackEncryptionBox::~ISOTrackEncryptionBox() = default;

bool ISOTrackEncryptionBox::parse(const ByteView& view, unsigned& offset)
{
    // ISO/IEC 23001-7-2015 Section 8.2.2
    if (!ISOFullBox::parse(view, offset))
        return false;

    return parsePayload(view, offset);
}

bool ISOTrackEncryptionBox::parsePayload(const ByteView& view, unsigned& offset)
{
    // unsigned int(8) reserved = 0;
    offset += 1;

    if (!m_version) {
        // unsigned int(8) reserved = 0;
        offset += 1;
    } else {
        int8_t cryptAndSkip = 0;
        if (!checkedRead<int8_t>(cryptAndSkip, view, offset, BigEndian))
            return false;

        m_defaultCryptByteBlock = cryptAndSkip >> 4;
        m_defaultSkipByteBlock = cryptAndSkip & 0xF;
    }

    if (!checkedRead<int8_t>(m_defaultIsProtected, view, offset, BigEndian))
        return false;

    if (!checkedRead<int8_t>(m_defaultPerSampleIVSize, view, offset, BigEndian))
        return false;

    size_t remaining;
    if (!WTF::safeSub(view.size(), offset, remaining) || remaining < 16)
        return false;

    m_defaultKID.resize(16);
    if (!checkedReadSequence<uint8_t>(m_defaultKID, view, offset, BigEndian))
        return false;

    if (m_defaultIsProtected == 1 && !m_defaultPerSampleIVSize) {
        int8_t defaultConstantIVSize = 0;
        if (!checkedRead<int8_t>(defaultConstantIVSize, view, offset, BigEndian) || defaultConstantIVSize < 0)
            return false;

        Vector<uint8_t> defaultConstantIV;
        defaultConstantIV.reserveInitialCapacity(defaultConstantIVSize);
        while (defaultConstantIVSize--) {
            int8_t character = 0;
            if (!checkedRead<int8_t>(character, view, offset, BigEndian))
                return false;
            defaultConstantIV.append(character);
        }
        m_defaultConstantIV = WTF::move(defaultConstantIV);
    }

    return true;
}

bool ISOTrackEncryptionBox::pack(MutableByteView& view, unsigned& offset) const
{
    // ISO/IEC 23001-7-2015 Section 8.2.2
    if (!ISOFullBox::pack(view, offset))
        return false;

    // unsigned int(8) reserved = 0;
    if (!checkedWrite<int8_t>(0, view, offset, BigEndian))
        return false;

    if (!m_version) {
        // unsigned int(8) reserved = 0;
        if (!checkedWrite<int8_t>(0, view, offset, BigEndian))
            return false;
    } else {
        int8_t cryptAndSkip = (m_defaultCryptByteBlock.value_or(0) << 4) | (m_defaultSkipByteBlock.value_or(0) & 0xF);
        if (!checkedWrite<int8_t>(cryptAndSkip, view, offset, BigEndian))
            return false;
    }

    if (!checkedWrite<int8_t>(m_defaultIsProtected, view, offset, BigEndian))
        return false;

    if (!checkedWrite<int8_t>(m_defaultPerSampleIVSize, view, offset, BigEndian))
        return false;

    if (m_defaultKID.size() != 16)
        return false;

    if (!checkedWriteSequence<uint8_t>(m_defaultKID, view, offset, BigEndian))
        return false;

    if (m_defaultIsProtected == 1 && !m_defaultPerSampleIVSize) {
        if (m_defaultConstantIV.size() > std::numeric_limits<int8_t>::max())
            return false;

        if (!checkedWrite<int8_t>(static_cast<int8_t>(m_defaultConstantIV.size()), view, offset, BigEndian))
            return false;

        if (!checkedWriteSequence<uint8_t>(m_defaultConstantIV, view, offset, BigEndian))
            return false;
    }

    return true;
}

uint64_t ISOTrackEncryptionBox::partialSize() const
{
    // reserved(1) + reserved-or-crypt/skip(1) + isProtected(1) + perSampleIVSize(1) + KID(16)
    uint64_t size = ISOFullBox::partialSize() + 20;

    if (m_defaultIsProtected == 1 && !m_defaultPerSampleIVSize)
        size += 1 + m_defaultConstantIV.size();

    return size;
}

bool ISOTrackEncryptionBox::parseWithoutTypeAndSize(std::span<const uint8_t> data)
{
    BitReader reader(data);

    // parseVersionAndFlags: 1 byte version + 3 bytes flags
    auto version = reader.read<uint8_t>();
    if (!version)
        return false;
    m_version = *version;

    auto flags = reader.read(24);
    if (!flags)
        return false;
    m_flags = static_cast<uint32_t>(*flags);

    // unsigned int(8) reserved = 0;
    if (!reader.skipBytes(1))
        return false;

    if (!m_version) {
        // unsigned int(8) reserved = 0;
        if (!reader.skipBytes(1))
            return false;
    } else {
        auto cryptAndSkip = reader.read<uint8_t>();
        if (!cryptAndSkip)
            return false;
        m_defaultCryptByteBlock = static_cast<int8_t>(*cryptAndSkip) >> 4;
        m_defaultSkipByteBlock = static_cast<int8_t>(*cryptAndSkip) & 0xF;
    }

    auto defaultIsProtected = reader.read<uint8_t>();
    if (!defaultIsProtected)
        return false;
    m_defaultIsProtected = static_cast<int8_t>(*defaultIsProtected);

    auto defaultPerSampleIVSize = reader.read<uint8_t>();
    if (!defaultPerSampleIVSize)
        return false;
    m_defaultPerSampleIVSize = static_cast<int8_t>(*defaultPerSampleIVSize);

    auto kIDOffset = reader.byteOffset();
    if (data.size() < kIDOffset + 16)
        return false;
    m_defaultKID = data.subspan(kIDOffset, 16);
    if (!reader.skipBytes(16))
        return false;

    if (m_defaultIsProtected == 1 && !m_defaultPerSampleIVSize) {
        auto ivSize = reader.read<uint8_t>();
        if (!ivSize)
            return false;
        auto ivOffset = reader.byteOffset();
        if (data.size() < ivOffset + *ivSize)
            return false;
        m_defaultConstantIV = data.subspan(ivOffset, *ivSize);
    }

    return true;
}

}
