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

#include <wtf/CheckedArithmetic.h>
#include <wtf/TZoneMallocInlines.h>
#include "SharedBuffer.h"

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ISOBox);

ISOBox::ISOBox() = default;
ISOBox::ISOBox(FourCC boxType)
    : m_boxType { boxType }
{
}

ISOBox::~ISOBox() = default;
ISOBox::ISOBox(const ISOBox&) = default;
ISOBox::ISOBox(ISOBox&&) = default;

// A box's size field and the cursor these parsers walk it with are both 32-bit, so a larger view
// cannot be walked at all: its remaining length would not even be representable.
static bool isWalkableView(const ISOBox::ByteView& view)
{
    return isInBounds<unsigned>(view.size());
}

ISOBox::PeekResult ISOBox::peekBox(const ByteView& view, unsigned offset)
{
    if (!isWalkableView(view))
        return std::nullopt;

    unsigned maximumPossibleSize = view.size() - offset;

    uint64_t size = 0;
    if (!checkedRead<uint32_t>(size, view, offset, BigEndian))
        return std::nullopt;

    FourCC type;
    if (!checkedRead<uint32_t>(type, view, offset, BigEndian))
        return std::nullopt;

    if (size == 1 && !checkedRead<uint64_t>(size, view, offset, BigEndian))
        return std::nullopt;

    if (size > maximumPossibleSize)
        size = maximumPossibleSize;
    else if (!size)
        size = maximumPossibleSize;

    return std::make_pair(type, size);
}

bool ISOBox::read(const ByteView& view)
{
    unsigned localOffset { 0 };
    if (!parse(view, localOffset))
        return false;

    return localOffset <= m_size;
}

bool ISOBox::read(const ByteView& view, unsigned& offset)
{
    if (!isWalkableView(view))
        return false;

    unsigned localOffset = offset;
    if (!parse(view, localOffset))
        return false;

    unsigned consumed;
    if (!WTF::safeSub(localOffset, offset, consumed) || consumed > m_size)
        return false;

    offset += m_size;
    return true;
}

bool ISOBox::write(MutableByteView& view, unsigned& offset) const
{
    if (!isWalkableView(view))
        return false;

    unsigned localOffset = offset;
    if (!pack(view, localOffset))
        return false;

    unsigned consumed;
    if (!WTF::safeSub(localOffset, offset, consumed) || consumed > m_size)
        return false;

    offset += m_size;
    return true;
}

void ISOBox::updateSize()
{
    m_size = requiredSize();
}

uint64_t ISOBox::partialSize() const
{
    uint64_t size = 8;
    if (m_boxType == std::span { "uuid" })
        size += 16;
    return size;
}

uint64_t ISOBox::requiredSize() const
{
    auto partialSize = this->partialSize();
    if (partialSize > std::numeric_limits<uint32_t>::max())
        partialSize += 4;
    return partialSize;
}

bool ISOBox::parse(const ByteView& view, unsigned& offset)
{
    if (!isWalkableView(view))
        return false;

    unsigned maximumPossibleSize = view.size() - offset;

    if (!checkedRead<uint32_t>(m_size, view, offset, BigEndian))
        return false;

    if (!checkedRead<uint32_t>(m_boxType, view, offset, BigEndian))
        return false;

    if (m_size == 1 && !checkedRead<uint64_t>(m_size, view, offset, BigEndian))
        return false;

    if (m_size > maximumPossibleSize)
        m_size = maximumPossibleSize;
    else if (!m_size)
        m_size = maximumPossibleSize;

    if (m_boxType == std::span { "uuid" }) {
        ExtendedType extendedType;
        if (!checkedRead<ExtendedType>(extendedType, view, offset, BigEndian))
            return false;

        m_extendedType = WTF::move(extendedType);
    }

    return true;
}

bool ISOBox::pack(MutableByteView& view, unsigned& offset) const
{
    uint32_t firstSize = 1u;
    if (m_size <= std::numeric_limits<uint32_t>::max())
        firstSize = m_size;

    if (!checkedWrite<uint32_t>(firstSize, view, offset, BigEndian))
        return false;

    if (!checkedWrite<uint32_t>(m_boxType.value, view, offset, BigEndian))
        return false;

    if (firstSize == 1 && !checkedWrite<uint64_t>(m_size, view, offset, BigEndian))
        return false;

    if (m_boxType == std::span { "uuid" }) {
        ASSERT(m_extendedType);
        if (!m_extendedType)
            return false;

        if (!checkedWriteSequence<uint8_t>(*m_extendedType, view, offset, BigEndian))
            return false;
    }

    return true;
}

Ref<SharedBuffer> ISOBox::serialize() const
{
    auto buffer = Vector<uint8_t>(m_size);
    MutableByteView view = buffer.mutableSpan();
    unsigned offset = 0;
    if (!write(view, offset))
        return SharedBuffer::create();

    return SharedBuffer::create(WTF::move(buffer));
}

ISOFullBox::ISOFullBox() = default;
ISOFullBox::ISOFullBox(FourCC boxType, uint8_t version, uint32_t flags)
    : ISOBox(boxType)
    , m_version { version }
    , m_flags { flags }
{
}
ISOFullBox::ISOFullBox(const ISOFullBox&) = default;
ISOFullBox::ISOFullBox(ISOFullBox&&) = default;

bool ISOFullBox::parse(const ByteView& view, unsigned& offset)
{
    if (!ISOBox::parse(view, offset))
        return false;

    return parseVersionAndFlags(view, offset);
}

bool ISOFullBox::pack(MutableByteView& view, unsigned& offset) const
{
    if (!ISOBox::pack(view, offset))
        return false;

    uint32_t versionAndFlags = (m_version << 24) + m_flags;
    if (!checkedWrite<uint32_t>(versionAndFlags, view, offset, BigEndian))
        return false;

    return true;
}

bool ISOFullBox::parseVersionAndFlags(const ByteView& view, unsigned& offset)
{
    uint32_t versionAndFlags = 0;
    if (!checkedRead<uint32_t>(versionAndFlags, view, offset, BigEndian))
        return false;

    m_version = versionAndFlags >> 24;
    m_flags = versionAndFlags & 0xFFFFFF;
    return true;
}

}
