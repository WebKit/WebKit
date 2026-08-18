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

#pragma once

#include <WebCore/FourCC.h>
#include <wtf/FlipBytes.h>
#include <wtf/Forward.h>
#include <wtf/StdIntExtras.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/TypeCasts.h>

namespace WebCore {

class SharedBuffer;

class ISOBox {
    WTF_MAKE_TZONE_ALLOCATED(ISOBox);
public:
    WEBCORE_EXPORT ISOBox();
    WEBCORE_EXPORT ISOBox(FourCC boxType);
    WEBCORE_EXPORT ISOBox(const ISOBox&);
    WEBCORE_EXPORT ISOBox(ISOBox&&);
    WEBCORE_EXPORT virtual ~ISOBox();

    ISOBox& operator=(const ISOBox&) = default;
    ISOBox& operator=(ISOBox&&) = default;

    using ByteView = std::span<const uint8_t>;
    using MutableByteView = std::span<uint8_t>;

    using PeekResult = std::optional<std::pair<FourCC, uint64_t>>;
    static PeekResult peekBox(const ByteView&, unsigned offset);
    static constexpr size_t minimumBoxSize() { return 2 * sizeof(uint32_t); }

    WEBCORE_EXPORT bool read(const ByteView&);
    bool read(const ByteView&, unsigned& offset);
    bool write(MutableByteView&, unsigned& offset) const;

    WEBCORE_EXPORT Ref<SharedBuffer> serialize() const;

    uint64_t size() const { return m_size; }
    FourCC boxType() const { return m_boxType; }
    using ExtendedType = std::array<uint8_t, 16>;
    const std::optional<ExtendedType>& extendedType() const LIFETIME_BOUND { return m_extendedType; }

    WEBCORE_EXPORT virtual void updateSize();
    virtual uint64_t partialSize() const;
    WEBCORE_EXPORT virtual uint64_t requiredSize() const;

protected:
    virtual bool parse(const ByteView&, unsigned& offset);
    virtual bool pack(MutableByteView&, unsigned& offset) const;

    enum Endianness { BigEndian, LittleEndian };

    template<typename T, typename R>
    static bool checkedRead(R& returnValue, const ByteView& view, unsigned& offset, Endianness endianness)
    {
        if (offset + sizeof(T) > view.size())
            return false;

        T value;
        memcpySpan(asMutableByteSpan(value), view.subspan(offset, sizeof(T)));
        returnValue = flipBytesIfLittleEndian(value, endianness == LittleEndian);
        offset += sizeof(T);
        RELEASE_ASSERT(isInBounds<uint32_t>(offset));

        return true;
    }

    template<typename T, typename S>
    static bool checkedReadSequence(S& returnValue, const ByteView& view, unsigned& offset, Endianness endianness)
    {
        unsigned actualOffset = offset;
        for (auto& value : returnValue) {
            if (!checkedRead<T>(value, view, actualOffset, endianness))
                return false;
        }
        offset = actualOffset;
        return true;
    }

    template<typename T, typename R>
    static bool checkedWrite(const R& value, MutableByteView& view, unsigned& offset, Endianness endianness)
    {
        if (offset + sizeof(T) > view.size())
            return false;

        T flipped = flipBytesIfLittleEndian(static_cast<T>(value), endianness == LittleEndian);
        memcpySpan(view.subspan(offset, sizeof(T)), asByteSpan(flipped));
        offset += sizeof(T);
        RELEASE_ASSERT(isInBounds<uint32_t>(offset));
        return true;
    }

    template<typename T, typename S>
    static bool checkedWriteSequence(const S& sequence, MutableByteView& view, unsigned& offset, Endianness endianness)
    {
        unsigned actualOffset = offset;
        for (auto& value : sequence) {
            if (!checkedWrite<T>(value, view, actualOffset, endianness))
                return false;
        }
        offset = actualOffset;
        return true;
    }

    uint64_t m_size { 0 };
    FourCC m_boxType;
    std::optional<ExtendedType> m_extendedType;
};

class ISOFullBox : public ISOBox {
public:
    WEBCORE_EXPORT ISOFullBox();
    WEBCORE_EXPORT ISOFullBox(FourCC boxType, uint8_t version, uint32_t flags);
    WEBCORE_EXPORT ISOFullBox(const ISOFullBox&);
    WEBCORE_EXPORT ISOFullBox(ISOFullBox&&);

    uint8_t version() const { return m_version; }
    void setVersion(uint8_t version) { m_version = version; }

    uint32_t flags() const { return m_flags; }
    void setFlags(uint32_t flags) { m_flags = flags; }

protected:
    uint64_t partialSize() const override { return ISOBox::partialSize() + 4; }
    bool parse(const ByteView&, unsigned& offset) override;
    bool parseVersionAndFlags(const ByteView&, unsigned& offset);
    bool pack(MutableByteView&, unsigned& offset) const override;

    uint8_t m_version { 0 };
    uint32_t m_flags { 0 };
};

}

#define SPECIALIZE_TYPE_TRAITS_ISOBOX(ISOBoxType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ISOBoxType) \
static bool isType(const WebCore::ISOBox& box) { return box.boxType() == WebCore::ISOBoxType::boxTypeName(); } \
SPECIALIZE_TYPE_TRAITS_END()
