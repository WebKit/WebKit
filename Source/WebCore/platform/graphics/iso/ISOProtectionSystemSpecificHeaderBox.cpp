/*
 * Copyright (C) 2017-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L
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
#include "ISOProtectionSystemSpecificHeaderBox.h"

#include <wtf/CheckedArithmetic.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>

namespace WebCore {

auto ISOProtectionSystemSpecificHeaderBox::commonSystemID() -> const SystemID& {
    static NeverDestroyed<SystemID> systemID = SystemID({ 0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b });
    return systemID;
}

ISOProtectionSystemSpecificHeaderBox::ISOProtectionSystemSpecificHeaderBox()
    : ISOFullBox(boxTypeName(), 0, 0)
    , m_systemID { }
{
}

ISOProtectionSystemSpecificHeaderBox::ISOProtectionSystemSpecificHeaderBox(const ISOProtectionSystemSpecificHeaderBox&) = default;
ISOProtectionSystemSpecificHeaderBox::ISOProtectionSystemSpecificHeaderBox(ISOProtectionSystemSpecificHeaderBox&&) = default;

ISOProtectionSystemSpecificHeaderBox::ISOProtectionSystemSpecificHeaderBox(SystemID systemID)
    : ISOFullBox(boxTypeName(), 0, 0)
    , m_systemID { systemID }
{
}

ISOProtectionSystemSpecificHeaderBox::~ISOProtectionSystemSpecificHeaderBox() = default;

auto ISOProtectionSystemSpecificHeaderBox::peekSystemID(ByteView& view, unsigned offset) -> std::optional<SystemID>
{
    auto peekResult = ISOBox::peekBox(view, offset);
    if (!peekResult || peekResult.value().first != boxTypeName())
        return std::nullopt;

    ISOProtectionSystemSpecificHeaderBox psshBox;
    // checkedReadSequence() populates elements as it goes and only bails on the one that
    // fails, so a partially parsed box would otherwise report a half-populated systemID
    // that callers would treat as authoritative.
    if (!psshBox.parse(view, offset))
        return std::nullopt;

    return psshBox.systemID();
}

bool ISOProtectionSystemSpecificHeaderBox::parse(const ByteView& view, unsigned& offset)
{
    auto startOffset = offset;
    if (!ISOFullBox::parse(view, offset))
        return false;

    auto endOffset = checkedSum<uint64_t>(startOffset, m_size);
    if (endOffset.hasOverflowed())
        return false;

    auto remainingInBox = [&]() -> std::optional<uint64_t> {
        uint64_t remaining;
        if (!WTF::safeSub(endOffset.value(), offset, remaining))
            return std::nullopt;
        return remaining;
    };

    if (!remainingInBox())
        return false;

    // ISO/IEC 23001-7-2016 Section 8.1.1
    if (!checkedReadSequence<uint8_t>(m_systemID, view, offset, BigEndian))
        return false;

    if (m_version) {
        uint32_t keyIDCount = 0;
        if (!checkedRead<uint32_t>(keyIDCount, view, offset, BigEndian))
            return false;

        auto remaining = remainingInBox();
        if (!remaining || keyIDCount > *remaining / sizeof(KeyID))
            return false;

        m_keyIDs.clear();
        while (keyIDCount--) {
            KeyID keyID;
            if (!checkedReadSequence<uint8_t>(keyID, view, offset, BigEndian))
                return false;
            m_keyIDs.append(WTF::move(keyID));
        }
    }

    uint32_t dataSize = 0;
    if (!checkedRead<uint32_t>(dataSize, view, offset, BigEndian))
        return false;

    auto remaining = remainingInBox();
    if (!remaining || *remaining < dataSize)
        return false;

    return parseData(view, offset, dataSize);
}

bool ISOProtectionSystemSpecificHeaderBox::parseData(const ByteView& view, unsigned& offset, uint64_t size)
{
    uint64_t remainingInView;
    if (!WTF::safeSub(view.size(), offset, remainingInView))
        return false;

    if (remainingInView < size)
        return false;

    m_data.resize(size);
    return checkedReadSequence<uint8_t>(m_data, view, offset, BigEndian);
}

bool ISOProtectionSystemSpecificHeaderBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOFullBox::pack(view, offset))
        return false;

    if (!checkedWriteSequence<uint8_t>(m_systemID, view, offset, BigEndian))
        return false;

    if (m_version) {
        if (!checkedWrite<uint32_t>(m_keyIDs.size(), view, offset, BigEndian))
            return false;
        for (auto& keyID : m_keyIDs) {
            if (!checkedWriteSequence<uint8_t>(keyID, view, offset, BigEndian))
                return false;
        }
    }

    if (!checkedWrite<uint32_t>(dataSize(), view, offset, BigEndian))
        return false;

    return writeData(view, offset);
}

bool ISOProtectionSystemSpecificHeaderBox::writeData(MutableByteView& view, unsigned& offset) const
{
    return checkedWriteSequence<uint8_t>(m_data, view, offset, BigEndian);
}

uint64_t ISOProtectionSystemSpecificHeaderBox::partialSize() const
{
    uint64_t size = ISOFullBox::partialSize();

    // unsigned int(8)[16] SystemID;
    size += 16;

    if (version() > 0) {
        // unsigned int(32) KID_count;
        size += 4;
        // {
        // unsigned int(8)[16] KID;
        // } [KID_count];
        size += 16 * m_keyIDs.size();
    }

    // unsigned int(32) DataSize;
    size += 4;
    // unsigned int(8)[DataSize] Data;
    size += m_data.size();

    return size;
}

}
