/*
 * Copyright (C) 2020 Apple Inc.  All rights reserved.
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

#include "DisplayListImageBuffer.h"
#include "ImageBuffer.h"

#if USE(CG)
#include "ImageBufferCGBitmapBackend.h"
#elif USE(CAIRO)
#include "ImageBufferCairoImageSurfaceBackend.h"
#endif

#if HAVE(IOSURFACE)
#include "ImageBufferIOSurfaceBackend.h"
#endif

namespace WebCore {

#if USE(CG)
using UnacceleratedImageBufferBackend = ImageBufferCGBitmapBackend;
#elif USE(CAIRO)
using UnacceleratedImageBufferBackend = ImageBufferCairoImageSurfaceBackend;
#endif

#if HAVE(IOSURFACE)
using AcceleratedImageBufferBackend = ImageBufferIOSurfaceBackend;
#else
using AcceleratedImageBufferBackend = UnacceleratedImageBufferBackend;
#endif

#if HAVE(IOSURFACE)
class IOSurfaceImageBuffer final : public ImageBuffer {
public:
    static auto create(const FloatSize& size, float resolutionScale, const DestinationColorSpace& colorSpace, PixelFormat pixelFormat, RenderingPurpose purpose, const ImageBufferCreationContext& creationContext = { })
    {
        return ImageBuffer::create<ImageBufferIOSurfaceBackend, IOSurfaceImageBuffer>(size, resolutionScale, colorSpace, pixelFormat, purpose, creationContext);
    }

    static auto create(const FloatSize& size, const GraphicsContext& context, RenderingPurpose purpose)
    {
        return ImageBuffer::create<ImageBufferIOSurfaceBackend, IOSurfaceImageBuffer>(size, context, purpose);
    }

    IOSurface& surface() { return *static_cast<ImageBufferIOSurfaceBackend&>(*m_backend).surface(); }

protected:
    using ImageBuffer::ImageBuffer;
};
#endif

} // namespace WebCore

#if HAVE(IOSURFACE)
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::IOSurfaceImageBuffer)
    static bool isType(const WebCore::ImageBuffer& buffer) { return buffer.renderingMode() == WebCore::RenderingMode::Accelerated; }
SPECIALIZE_TYPE_TRAITS_END()
#endif
