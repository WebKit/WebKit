/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/ContentsFormat.h>
#include <WebCore/DestinationColorSpace.h>
#include <WebCore/FloatSize.h>
#include <WebCore/ImageBufferFormat.h>
#include <WebCore/RenderingMode.h>

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
#include <WebCore/DynamicContentScalingDisplayList.h>
#endif

#if ENABLE(GPU_PROCESS)

namespace WebKit {

struct RemoteImageBufferSetConfiguration {
    WebCore::FloatSize logicalSize;
    float resolutionScale { 1.0f };
    WebCore::DestinationColorSpace colorSpace { WebCore::DestinationColorSpace::SRGB() };
    WebCore::ContentsFormat contentsFormat { WebCore::ContentsFormat::RGBA8 }; // FIXME: Is this used?
    WebCore::ImageBufferFormat bufferFormat { WebCore::PixelFormat::BGRA8, WebCore::UseLosslessCompression::No };
    WebCore::RenderingMode renderingMode { WebCore::RenderingMode::Unaccelerated };
    WebCore::RenderingPurpose renderingPurpose { WebCore::RenderingPurpose::Unspecified };

#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    WebCore::IncludeDynamicContentScalingDisplayList includeDisplayList { WebCore::IncludeDynamicContentScalingDisplayList::No };
#endif
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
