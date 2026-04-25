/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "PlatformMediaCapabilitiesColorGamut.h"

namespace WebCore {

enum class ColorGamut : uint8_t {
    SRGB,
    P3,
    Rec2020,
};

inline PlatformMediaCapabilitiesColorGamut toPlatform(ColorGamut value)
{
    switch (value) {
    case ColorGamut::SRGB:    return PlatformMediaCapabilitiesColorGamut::SRGB;
    case ColorGamut::P3:      return PlatformMediaCapabilitiesColorGamut::P3;
    case ColorGamut::Rec2020: return PlatformMediaCapabilitiesColorGamut::Rec2020;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

inline ColorGamut fromPlatform(PlatformMediaCapabilitiesColorGamut value)
{
    switch (value) {
    case PlatformMediaCapabilitiesColorGamut::SRGB:    return ColorGamut::SRGB;
    case PlatformMediaCapabilitiesColorGamut::P3:      return ColorGamut::P3;
    case PlatformMediaCapabilitiesColorGamut::Rec2020: return ColorGamut::Rec2020;
    }
    RELEASE_ASSERT_NOT_REACHED();
}

} // namespace WebCore
