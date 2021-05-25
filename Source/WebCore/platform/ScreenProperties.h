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

#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include "PlatformScreen.h"
#include <wtf/EnumTraits.h>
#include <wtf/HashMap.h>
#include <wtf/RetainPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct ScreenData {
    FloatRect screenAvailableRect;
    FloatRect screenRect;
    DestinationColorSpace colorSpace { DestinationColorSpace::SRGB() };
    int screenDepth { 0 };
    int screenDepthPerComponent { 0 };
    bool screenSupportsExtendedColor { false };
    bool screenHasInvertedColors { false };
    bool screenSupportsHighDynamicRange { false };
#if PLATFORM(MAC)
    bool screenIsMonochrome { false };
    uint32_t displayMask { 0 };
    IORegistryGPUID gpuID { 0 };
    DynamicRangeMode preferredDynamicRangeMode { DynamicRangeMode::Standard };
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    float scaleFactor { 1 };
#endif

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ScreenData> decode(Decoder&);
};

using ScreenDataMap = HashMap<PlatformDisplayID, ScreenData>;

struct ScreenProperties {
    PlatformDisplayID primaryDisplayID { 0 };
    ScreenDataMap screenDataMap;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static Optional<ScreenProperties> decode(Decoder&);
};

template<class Encoder>
void ScreenProperties::encode(Encoder& encoder) const
{
    encoder << primaryDisplayID << screenDataMap;
}

template<class Decoder>
Optional<ScreenProperties> ScreenProperties::decode(Decoder& decoder)
{
    Optional<PlatformDisplayID> primaryDisplayID;
    decoder >> primaryDisplayID;
    if (!primaryDisplayID)
        return WTF::nullopt;

    Optional<ScreenDataMap> screenDataMap;
    decoder >> screenDataMap;
    if (!screenDataMap)
        return WTF::nullopt;

    return { { *primaryDisplayID, WTFMove(*screenDataMap) } };
}

template<class Encoder>
void ScreenData::encode(Encoder& encoder) const
{
    encoder << screenAvailableRect << screenRect << colorSpace << screenDepth << screenDepthPerComponent << screenSupportsExtendedColor << screenHasInvertedColors << screenSupportsHighDynamicRange;

#if PLATFORM(MAC)
    encoder << screenIsMonochrome << displayMask << gpuID << preferredDynamicRangeMode;
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    encoder << scaleFactor;
#endif
}

template<class Decoder>
Optional<ScreenData> ScreenData::decode(Decoder& decoder)
{
    Optional<FloatRect> screenAvailableRect;
    decoder >> screenAvailableRect;
    if (!screenAvailableRect)
        return WTF::nullopt;

    Optional<FloatRect> screenRect;
    decoder >> screenRect;
    if (!screenRect)
        return WTF::nullopt;

    Optional<DestinationColorSpace> screenColorSpace;
    decoder >> screenColorSpace;
    if (!screenColorSpace)
        return WTF::nullopt;

    Optional<int> screenDepth;
    decoder >> screenDepth;
    if (!screenDepth)
        return WTF::nullopt;

    Optional<int> screenDepthPerComponent;
    decoder >> screenDepthPerComponent;
    if (!screenDepthPerComponent)
        return WTF::nullopt;

    Optional<bool> screenSupportsExtendedColor;
    decoder >> screenSupportsExtendedColor;
    if (!screenSupportsExtendedColor)
        return WTF::nullopt;

    Optional<bool> screenHasInvertedColors;
    decoder >> screenHasInvertedColors;
    if (!screenHasInvertedColors)
        return WTF::nullopt;

    Optional<bool> screenSupportsHighDynamicRange;
    decoder >> screenSupportsHighDynamicRange;
    if (!screenSupportsHighDynamicRange)
        return WTF::nullopt;

#if PLATFORM(MAC)
    Optional<bool> screenIsMonochrome;
    decoder >> screenIsMonochrome;
    if (!screenIsMonochrome)
        return WTF::nullopt;

    Optional<uint32_t> displayMask;
    decoder >> displayMask;
    if (!displayMask)
        return WTF::nullopt;

    Optional<IORegistryGPUID> gpuID;
    decoder >> gpuID;
    if (!gpuID)
        return WTF::nullopt;

    Optional<DynamicRangeMode> preferredDynamicRangeMode;
    decoder >> preferredDynamicRangeMode;
    if (!preferredDynamicRangeMode)
        return WTF::nullopt;
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    Optional<float> scaleFactor;
    decoder >> scaleFactor;
    if (!scaleFactor)
        return WTF::nullopt;
#endif

    return { {
        WTFMove(*screenAvailableRect),
        WTFMove(*screenRect),
        WTFMove(*screenColorSpace),
        WTFMove(*screenDepth),
        WTFMove(*screenDepthPerComponent),
        WTFMove(*screenSupportsExtendedColor),
        WTFMove(*screenHasInvertedColors),
        WTFMove(*screenSupportsHighDynamicRange),
#if PLATFORM(MAC)
        WTFMove(*screenIsMonochrome),
        WTFMove(*displayMask),
        WTFMove(*gpuID),
        WTFMove(*preferredDynamicRangeMode),
#endif
#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
        WTFMove(*scaleFactor),
#endif
    } };
}

} // namespace WebCore
