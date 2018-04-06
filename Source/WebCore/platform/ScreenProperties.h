/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)

#include "FloatRect.h"
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

typedef struct CGColorSpace *CGColorSpaceRef;

namespace WebCore {

struct ScreenProperties {
    FloatRect screenAvailableRect;
    FloatRect screenRect;
    RetainPtr<CGColorSpaceRef> colorSpace;
    int screenDepth { 0 };
    int screenDepthPerComponent { 0 };
    bool screenSupportsExtendedColor { false };
    bool screenHasInvertedColors { false };
    bool screenIsMonochrome { false };

    enum EncodedColorSpaceDataType {
        Null,
        ColorSpaceName,
        ColorSpaceData,
    };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ScreenProperties> decode(Decoder&);
};

template<class Encoder>
void ScreenProperties::encode(Encoder& encoder) const
{
    encoder << screenAvailableRect << screenRect << screenDepth << screenDepthPerComponent << screenSupportsExtendedColor << screenHasInvertedColors << screenIsMonochrome;

    if (colorSpace) {
        // Try to encode the name.
        if (auto name = adoptCF(CGColorSpaceCopyName(colorSpace.get()))) {
            encoder.encodeEnum(ColorSpaceName);
            encoder << String(name.get());
            return;
        }

        // Failing that, just encode the ICC data.
        if (auto profileData = adoptCF(CGColorSpaceCopyICCData(colorSpace.get()))) {
            encoder.encodeEnum(ColorSpaceData);

            Vector<uint8_t> iccData;
            iccData.append(CFDataGetBytePtr(profileData.get()), CFDataGetLength(profileData.get()));

            encoder << iccData;
            return;
        }
    }

    // The color space was null or failed to be encoded.
    encoder << Null;
}

template<class Decoder>
std::optional<ScreenProperties> ScreenProperties::decode(Decoder& decoder)
{
    std::optional<FloatRect> screenAvailableRect;
    decoder >> screenAvailableRect;
    if (!screenAvailableRect)
        return std::nullopt;

    std::optional<FloatRect> screenRect;
    decoder >> screenRect;
    if (!screenRect)
        return std::nullopt;

    std::optional<int> screenDepth;
    decoder >> screenDepth;
    if (!screenDepth)
        return std::nullopt;

    std::optional<int> screenDepthPerComponent;
    decoder >> screenDepthPerComponent;
    if (!screenDepthPerComponent)
        return std::nullopt;

    std::optional<bool> screenSupportsExtendedColor;
    decoder >> screenSupportsExtendedColor;
    if (!screenSupportsExtendedColor)
        return std::nullopt;

    std::optional<bool> screenHasInvertedColors;
    decoder >> screenHasInvertedColors;
    if (!screenHasInvertedColors)
        return std::nullopt;

    std::optional<bool> screenIsMonochrome;
    decoder >> screenIsMonochrome;
    if (!screenIsMonochrome)
        return std::nullopt;

    EncodedColorSpaceDataType dataType;
    if (!decoder.decodeEnum(dataType))
        return std::nullopt;

    RetainPtr<CGColorSpaceRef> cgColorSpace;
    switch (dataType) {
    case Null:
        break;
    case ColorSpaceName: {
        std::optional<String> colorSpaceName;
        decoder >> colorSpaceName;
        ASSERT(colorSpaceName);
        if (!colorSpaceName)
            return std::nullopt;

        cgColorSpace = adoptCF(CGColorSpaceCreateWithName(colorSpaceName->createCFString().get()));
        break;
    }
    case ColorSpaceData: {
        std::optional<Vector<uint8_t>> iccData;
        decoder >> iccData;
        ASSERT(iccData);
        if (!iccData)
            return std::nullopt;

        auto colorSpaceData = adoptCF(CFDataCreate(kCFAllocatorDefault, iccData->data(), iccData->size()));
        // FIXME: <http://webkit.org/b/184358> We should switch to CGColorSpaceCreateICCBased.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        cgColorSpace = adoptCF(CGColorSpaceCreateWithICCProfile(colorSpaceData.get()));
#pragma clang diagnostic pop
        break;
    }
    }

    return { { WTFMove(*screenAvailableRect), WTFMove(*screenRect), WTFMove(cgColorSpace), WTFMove(*screenDepth), WTFMove(*screenDepthPerComponent), WTFMove(*screenSupportsExtendedColor), WTFMove(*screenHasInvertedColors), WTFMove(*screenIsMonochrome) } };
}

} // namespace WebCore

#endif // PLATFORM(MAC)
