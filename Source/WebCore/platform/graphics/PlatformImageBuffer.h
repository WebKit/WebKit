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

#include "ConcreteImageBuffer.h"
#include "DisplayListImageBuffer.h"

#if USE(CG)
#include "ImageBufferCGBitmapBackend.h"
#elif USE(DIRECT2D)
#include "ImageBufferDirect2DBackend.h"
#elif USE(CAIRO)
#include "ImageBufferCairoImageSurfaceBackend.h"
#endif

#if HAVE(IOSURFACE)
#include "ImageBufferIOSurfaceBackend.h"
#endif

namespace WebCore {

#if USE(CG)
using UnacceleratedImageBufferBackend = ImageBufferCGBitmapBackend;
#elif USE(DIRECT2D)
using UnacceleratedImageBufferBackend = ImageBufferDirect2DBackend;
#elif USE(CAIRO)
using UnacceleratedImageBufferBackend = ImageBufferCairoImageSurfaceBackend;
#endif

#if HAVE(IOSURFACE)
using AcceleratedImageBufferBackend = ImageBufferIOSurfaceBackend;
#else
using AcceleratedImageBufferBackend = UnacceleratedImageBufferBackend;
#endif

using UnacceleratedImageBuffer = ConcreteImageBuffer<UnacceleratedImageBufferBackend>;
using DisplayListUnacceleratedImageBuffer = DisplayList::ImageBuffer<UnacceleratedImageBufferBackend>;
using DisplayListAcceleratedImageBuffer = DisplayList::ImageBuffer<AcceleratedImageBufferBackend>;

#if HAVE(IOSURFACE)
class IOSurfaceImageBuffer final : public ConcreteImageBuffer<ImageBufferIOSurfaceBackend> {
    using Base = ConcreteImageBuffer<ImageBufferIOSurfaceBackend>;
public:
    static auto create(const WebCore::FloatSize& size, float resolutionScale, const WebCore::DestinationColorSpace& colorSpace, WebCore::PixelFormat pixelFormat, const HostWindow* hostWindow = nullptr)
    {
        return Base::create<IOSurfaceImageBuffer>(size, resolutionScale, colorSpace, pixelFormat, hostWindow);
    }
    static auto create(const FloatSize& size, const GraphicsContext& context)
    {
        return Base::create<IOSurfaceImageBuffer>(size, context);
    }
    using Base::Base;

    IOSurface& surface() { return *m_backend->surface(); }
};
using AcceleratedImageBuffer = IOSurfaceImageBuffer;
#else
using AcceleratedImageBuffer = ConcreteImageBuffer<UnacceleratedImageBufferBackend>;
#endif


} // namespace WebCore

#if HAVE(IOSURFACE)
SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::IOSurfaceImageBuffer)
    static bool isType(const WebCore::ImageBuffer& buffer) { return buffer.renderingMode() == WebCore::RenderingMode::Accelerated; }
SPECIALIZE_TYPE_TRAITS_END()
#endif
