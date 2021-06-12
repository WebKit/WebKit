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

#include "FourCC.h"
#include <wtf/Forward.h>
#include <wtf/TypeCasts.h>

namespace JSC {
class DataView;
}

namespace WebCore {

class WEBCORE_EXPORT ISOBox {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~ISOBox() = default;

    using PeekResult = std::optional<std::pair<FourCC, uint64_t>>;
    static PeekResult peekBox(JSC::DataView&, unsigned offset);
    static size_t minimumBoxSize() { return 2 * sizeof(uint32_t); }

    bool read(JSC::DataView&);
    bool read(JSC::DataView&, unsigned& offset);

    uint64_t size() const { return m_size; }
    const FourCC& boxType() const { return m_boxType; }
    const Vector<uint8_t>& extendedType() const { return m_extendedType; }

protected:
    virtual bool parse(JSC::DataView&, unsigned& offset);

    enum Endianness {
        BigEndian,
        LittleEndian,
    };

    template <typename T, typename R, typename V>
    static bool checkedRead(R& returnValue, V& view, unsigned& offset, Endianness endianness)
    {
        bool readStatus = false;
        T value = view.template read<T>(offset, endianness == LittleEndian, &readStatus);
        if (!readStatus)
            return false;

        returnValue = value;
        return true;
    }

    uint64_t m_size { 0 };
    FourCC m_boxType { uint32_t { 0 } };
    Vector<uint8_t> m_extendedType;
};

class WEBCORE_EXPORT ISOFullBox : public ISOBox {
public:
    uint8_t version() const { return m_version; }
    uint32_t flags() const { return m_flags; }

protected:
    bool parse(JSC::DataView&, unsigned& offset) override;

    uint8_t m_version { 0 };
    uint32_t m_flags { 0 };
};

}

#define SPECIALIZE_TYPE_TRAITS_ISOBOX(ISOBoxType) \
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::ISOBoxType) \
static bool isType(const WebCore::ISOBox& box) { return box.boxType() == WebCore::ISOBoxType::boxTypeName(); } \
SPECIALIZE_TYPE_TRAITS_END()
