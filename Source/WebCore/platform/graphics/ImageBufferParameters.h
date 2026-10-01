/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include <WebCore/ColorSpace.h>
#include <WebCore/FloatSize.h>
#include <WebCore/ImageBufferFormat.h>
#include <WebCore/IntSize.h>
#include <WebCore/PixelFormat.h>
#include <WebCore/RenderingMode.h>

namespace WebCore {

// The size of the backing store that a logical size and a resolution scale ask for.
// Empty if that cannot be expressed as an IntSize, which the callers treat as a
// failure to allocate.
WEBCORE_EXPORT IntSize calculateImageBufferBackendSize(FloatSize logicalSize, float resolutionScale);

struct ImageBufferParameters {
    FloatSize logicalSize;
    float resolutionScale;
    ColorSpace colorSpace;
    ImageBufferFormat bufferFormat;
    RenderingPurpose purpose;

    IntSize backendSize() const { return calculateImageBufferBackendSize(logicalSize, resolutionScale); }

    friend bool operator==(const ImageBufferParameters&, const ImageBufferParameters&) = default;
};

} // namespace WebCore
