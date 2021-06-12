/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if USE(CG)
#include <wtf/RetainPtr.h>
typedef struct CGColorSpace* CGColorSpaceRef;
#else
#include <optional>
#include <wtf/EnumTraits.h>
#endif

namespace WebCore {

#if USE(CG)

using PlatformColorSpace = RetainPtr<CGColorSpaceRef>;
using PlatformColorSpaceValue = CGColorSpaceRef;

#else

class PlatformColorSpace {
public:
    enum class Name : uint8_t {
        SRGB
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        , LinearSRGB
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        , DisplayP3
#endif
    };

    PlatformColorSpace(Name name)
        : m_name { name }
    {
    }

    Name get() const { return m_name; }

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<PlatformColorSpace> decode(Decoder&);

private:
    Name m_name;

};
using PlatformColorSpaceValue = PlatformColorSpace::Name;

template<class Encoder> void PlatformColorSpace::encode(Encoder& encoder) const
{
    encoder << m_name;
}

template<class Decoder> std::optional<PlatformColorSpace> PlatformColorSpace::decode(Decoder& decoder)
{
    std::optional<PlatformColorSpace::Name> name;
    decoder >> name;
    if (!name)
        return std::nullopt;

    return { { *name } };
}

#endif

}

#if !USE(CG)

namespace WTF {

template<> struct EnumTraits<WebCore::PlatformColorSpace::Name> {
    using values = EnumValues<
        WebCore::PlatformColorSpace::Name,
        WebCore::PlatformColorSpace::Name::SRGB
#if ENABLE(DESTINATION_COLOR_SPACE_LINEAR_SRGB)
        , WebCore::PlatformColorSpace::Name::LinearSRGB
#endif
#if ENABLE(DESTINATION_COLOR_SPACE_DISPLAY_P3)
        , WebCore::PlatformColorSpace::Name::DisplayP3
#endif
    >;
};

} // namespace WTF

#endif // !USE(CG)
