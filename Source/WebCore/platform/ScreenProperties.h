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
    template<class Decoder> static std::optional<ScreenData> decode(Decoder&);
};

using ScreenDataMap = HashMap<PlatformDisplayID, ScreenData>;

struct ScreenProperties {
    PlatformDisplayID primaryDisplayID { 0 };
    ScreenDataMap screenDataMap;

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<ScreenProperties> decode(Decoder&);
};

template<class Encoder>
void ScreenProperties::encode(Encoder& encoder) const
{
    encoder << primaryDisplayID << screenDataMap;
}

template<class Decoder>
std::optional<ScreenProperties> ScreenProperties::decode(Decoder& decoder)
{
    std::optional<PlatformDisplayID> primaryDisplayID;
    decoder >> primaryDisplayID;
    if (!primaryDisplayID)
        return std::nullopt;

    std::optional<ScreenDataMap> screenDataMap;
    decoder >> screenDataMap;
    if (!screenDataMap)
        return std::nullopt;

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
std::optional<ScreenData> ScreenData::decode(Decoder& decoder)
{
    std::optional<FloatRect> screenAvailableRect;
    decoder >> screenAvailableRect;
    if (!screenAvailableRect)
        return std::nullopt;

    std::optional<FloatRect> screenRect;
    decoder >> screenRect;
    if (!screenRect)
        return std::nullopt;

    std::optional<DestinationColorSpace> screenColorSpace;
    decoder >> screenColorSpace;
    if (!screenColorSpace)
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

    std::optional<bool> screenSupportsHighDynamicRange;
    decoder >> screenSupportsHighDynamicRange;
    if (!screenSupportsHighDynamicRange)
        return std::nullopt;

#if PLATFORM(MAC)
    std::optional<bool> screenIsMonochrome;
    decoder >> screenIsMonochrome;
    if (!screenIsMonochrome)
        return std::nullopt;

    std::optional<uint32_t> displayMask;
    decoder >> displayMask;
    if (!displayMask)
        return std::nullopt;

    std::optional<IORegistryGPUID> gpuID;
    decoder >> gpuID;
    if (!gpuID)
        return std::nullopt;

    std::optional<DynamicRangeMode> preferredDynamicRangeMode;
    decoder >> preferredDynamicRangeMode;
    if (!preferredDynamicRangeMode)
        return std::nullopt;
#endif

#if PLATFORM(MAC) || PLATFORM(IOS_FAMILY)
    std::optional<float> scaleFactor;
    decoder >> scaleFactor;
    if (!scaleFactor)
        return std::nullopt;
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
