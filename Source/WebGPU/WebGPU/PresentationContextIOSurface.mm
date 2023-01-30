/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/IOTypesSPI.h>

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
    return adoptCF(IOSurfaceCreate(bridge_cast(options)));
}

static RetainPtr<IOSurfaceRef> createSurfaceFromDescriptor(const WGPUSwapChainDescriptor& descriptor)
{
    if (descriptor.nextInChain) {
        // FIXME: We need better error handling.
        return nullptr;
    }

    return createIOSurface(descriptor.width, descriptor.height);
}

PresentationContextIOSurface::PresentationContextIOSurface(const WGPUSurfaceDescriptor&)
{
}

PresentationContextIOSurface::~PresentationContextIOSurface() = default;

void PresentationContextIOSurface::configure(Device&, const WGPUSwapChainDescriptor& descriptor)
{
    m_displayBuffer = createSurfaceFromDescriptor(descriptor);
    m_drawingBuffer = createSurfaceFromDescriptor(descriptor);
}

void PresentationContextIOSurface::present()
{
    nextDrawable();
}

Texture* PresentationContextIOSurface::getCurrentTexture()
{
    return nullptr;
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
