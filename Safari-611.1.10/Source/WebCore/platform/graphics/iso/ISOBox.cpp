/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#include "ISOBox.h"

#include <JavaScriptCore/DataView.h>

using JSC::DataView;

namespace WebCore {

ISOBox::PeekResult ISOBox::peekBox(DataView& view, unsigned offset)
{
    uint64_t size = 0;
    if (!checkedRead<uint32_t>(size, view, offset, BigEndian))
        return WTF::nullopt;

    FourCC type = { uint32_t { 0 } };
    if (!checkedRead<uint32_t>(type, view, offset, BigEndian))
        return WTF::nullopt;

    if (size == 1 && !checkedRead<uint64_t>(size, view, offset, BigEndian))
        return WTF::nullopt;
    else if (!size)
        size = view.byteLength();

    return std::make_pair(type, size);
}

bool ISOBox::read(DataView& view)
{
    unsigned localOffset { 0 };
    return parse(view, localOffset);
}

bool ISOBox::read(DataView& view, unsigned& offset)
{
    unsigned localOffset = offset;
    if (!parse(view, localOffset))
        return false;

    offset += m_size;
    return true;
}

bool ISOBox::parse(DataView& view, unsigned& offset)
{
    if (!checkedRead<uint32_t>(m_size, view, offset, BigEndian))
        return false;

    if (!checkedRead<uint32_t>(m_boxType, view, offset, BigEndian))
        return false;

    if (m_size == 1 && !checkedRead<uint64_t>(m_size, view, offset, BigEndian))
        return false;
    else if (!m_size)
        m_size = view.byteLength();

    if (m_boxType == "uuid") {
        struct ExtendedType {
            uint8_t value[16];
        } extendedTypeStruct;
        if (!checkedRead<ExtendedType>(extendedTypeStruct, view, offset, BigEndian))
            return false;

        Vector<uint8_t> extendedType;
        extendedType.reserveInitialCapacity(16);
        for (auto& character : extendedTypeStruct.value)
            extendedType.uncheckedAppend(character);
        m_extendedType = WTFMove(extendedType);
    }

    return true;
}

bool ISOFullBox::parse(DataView& view, unsigned& offset)
{
    if (!ISOBox::parse(view, offset))
        return false;

    uint32_t versionAndFlags = 0;
    if (!checkedRead<uint32_t>(versionAndFlags, view, offset, BigEndian))
        return false;

    m_version = versionAndFlags >> 24;
    m_flags = versionAndFlags & 0xFFFFFF;
    return true;
}

}
