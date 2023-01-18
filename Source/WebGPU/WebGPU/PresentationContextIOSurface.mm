/*
 * Copyright (c) 2021-2022 Apple Inc. All rights reserved.
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

#import "config.h"
#import "PresentationContextIOSurface.h"

#import "APIConversions.h"

// borrowed from pal/spi/cocoa/IOTypesSPI.h
#if PLATFORM(MAC) || USE(APPLE_INTERNAL_SDK)
#include <IOKit/IOTypes.h>
#else

enum {
    kIOWriteCombineCache = 4,
};

enum {
    kIOMapCacheShift = 8,
    kIOMapWriteCombineCache = kIOWriteCombineCache << kIOMapCacheShift,
};
#endif

namespace WebGPU {

static NSDictionary *optionsFor32BitSurface(int width, int height, unsigned pixelFormat)
{
    unsigned bytesPerElement = 4;
    unsigned bytesPerPixel = 4;

    size_t bytesPerRow = IOSurfaceAlignProperty(kIOSurfaceBytesPerRow, width * bytesPerPixel);
    ASSERT(bytesPerRow);

    size_t totalBytes = IOSurfaceAlignProperty(kIOSurfaceAllocSize, height * bytesPerRow);
    ASSERT(totalBytes);

    return @{
        (id)kIOSurfaceWidth: @(width),
        (id)kIOSurfaceHeight: @(height),
        (id)kIOSurfacePixelFormat: @(pixelFormat),
        (id)kIOSurfaceBytesPerElement: @(bytesPerElement),
        (id)kIOSurfaceBytesPerRow: @(bytesPerRow),
        (id)kIOSurfaceAllocSize: @(totalBytes),
#if PLATFORM(IOS_FAMILY)
        (id)kIOSurfaceCacheMode: @(kIOMapWriteCombineCache),
#endif
        (id)kIOSurfaceElementHeight: @(1)
    };
}

static RetainPtr<IOSurfaceRef> createIOSurface(int width, int height)
{
    NSDictionary *options = optionsFor32BitSurface(width, height, 'BGRA');
    return adoptCF(IOSurfaceCreate((CFDictionaryRef)options));
}

static RetainPtr<IOSurfaceRef> createSurfaceFromDescriptor(const WGPUSurfaceDescriptor& descriptor)
{
    if (!descriptor.nextInChain || descriptor.nextInChain->sType != static_cast<WGPUSType>(WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking))
        return nullptr;

    auto widthHeight = reinterpret_cast<const WGPUSurfaceDescriptorCocoaCustomSurface*>(descriptor.nextInChain);
    return createIOSurface(widthHeight->width, widthHeight->height);
}

PresentationContextIOSurface::PresentationContextIOSurface(const WGPUSurfaceDescriptor& descriptor)
    : m_displayBuffer(createSurfaceFromDescriptor(descriptor))
    , m_drawingBuffer(createSurfaceFromDescriptor(descriptor))
{
}

PresentationContextIOSurface::~PresentationContextIOSurface() = default;

void PresentationContextIOSurface::configure(Device&, const WGPUSwapChainDescriptor&)
{
}

void PresentationContextIOSurface::present()
{
    nextDrawable();
}

TextureView* PresentationContextIOSurface::getCurrentTextureView()
{
    return nullptr;
}

RetainPtr<IOSurfaceRef> PresentationContextIOSurface::nextDrawable()
{
    // FIXME: wait until a buffer is available
    auto nextBuffer = m_drawingBuffer;
    m_drawingBuffer = m_displayBuffer;
    m_displayBuffer = nextBuffer;

    return m_drawingBuffer;
}

} // namespace WebGPU

#pragma mark WGPU Stubs

IOSurfaceRef wgpuSurfaceCocoaCustomSurfaceGetDisplayBuffer(WGPUSurface surface)
{
    if (auto* presentationContextIOSurface = downcast<WebGPU::PresentationContextIOSurface>(&WebGPU::fromAPI(surface)))
        return presentationContextIOSurface->displayBuffer().get();
    return nullptr;
}

IOSurfaceRef wgpuSurfaceCocoaCustomSurfaceGetDrawingBuffer(WGPUSurface surface)
{
    if (auto* presentationContextIOSurface = downcast<WebGPU::PresentationContextIOSurface>(&WebGPU::fromAPI(surface)))
        return presentationContextIOSurface->drawingBuffer().get();
    return nullptr;
}
