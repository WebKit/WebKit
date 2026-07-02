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

#include <WebCore/PlatformMediaCapabilitiesColorGamut.h>
#include <WebCore/PlatformMediaCapabilitiesHdrMetadataType.h>
#include <WebCore/PlatformMediaCapabilitiesTransferFunction.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct PlatformMediaCapabilitiesVideoConfiguration {
    String contentType;
    uint32_t width { };
    uint32_t height { };
    uint64_t bitrate { };
    double framerate { };
    std::optional<bool> alphaChannel;
    std::optional<PlatformMediaCapabilitiesHdrMetadataType> hdrMetadataType;
    std::optional<PlatformMediaCapabilitiesColorGamut> colorGamut;
    std::optional<PlatformMediaCapabilitiesTransferFunction> transferFunction;

    PlatformMediaCapabilitiesVideoConfiguration isolatedCopy() const &;
    PlatformMediaCapabilitiesVideoConfiguration isolatedCopy() &&;
};

inline PlatformMediaCapabilitiesVideoConfiguration PlatformMediaCapabilitiesVideoConfiguration::isolatedCopy() const &
{
    return {
        contentType.isolatedCopy(),
        width,
        height,
        bitrate,
        framerate,
        alphaChannel,
        hdrMetadataType,
        colorGamut,
        transferFunction
    };
}

inline PlatformMediaCapabilitiesVideoConfiguration PlatformMediaCapabilitiesVideoConfiguration::isolatedCopy() &&
{
    return {
        WTF::move(contentType).isolatedCopy(),
        width,
        height,
        bitrate,
        framerate,
        alphaChannel,
        hdrMetadataType,
        colorGamut,
        transferFunction
    };
}

} // namespace WebCore
