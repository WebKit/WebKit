/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/GraphicsTypes.h>
#include <WebCore/ImageTypes.h>
#include <WebCore/PlatformDynamicRangeLimit.h>
#include <WebCore/StyleLinkParameters.h>
#include <wtf/URL.h>

namespace WebCore {

// The parts of an ImageDrawDescriptor that change what an image draws, rather than where or how it is
// composited. Caches of an image's output are keyed on these.
struct ImageRenditionOptions {
    URL fragmentURL;
    Style::LinkParameters linkParameters { CSS::Keyword::None { } };
    IgnoreRootPreserveAspectRatio ignoreRootPreserveAspectRatio { IgnoreRootPreserveAspectRatio::No };
    InterpolationQuality interpolationQuality { InterpolationQuality::Default };
    DrawsHDRContent drawsHDRContent { DrawsHDRContent::No };
    Headroom headroom { Headroom::FromImage };
    PlatformDynamicRangeLimit dynamicRangeLimit { PlatformDynamicRangeLimit::initialValue() };
#if ENABLE(AX_CUSTOM_COLOR_MODE)
    InvertContent invertContent { InvertContent::FromResource };
#endif

    bool operator==(const ImageRenditionOptions&) const = default;
};

} // namespace WebCore
