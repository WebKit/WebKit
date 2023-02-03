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
#import "Texture.h"
#import "TextureView.h"
#import <wtf/cocoa/TypeCastsCocoa.h>
#import <wtf/spi/cocoa/IOTypesSPI.h>

namespace WebGPU {

Ref<PresentationContextIOSurface> PresentationContextIOSurface::create(const WGPUSurfaceDescriptor& surfaceDescriptor)
{
    auto presentationContextIOSurface = adoptRef(*new PresentationContextIOSurface(surfaceDescriptor));

    ASSERT(surfaceDescriptor.nextInChain);
    ASSERT(surfaceDescriptor.nextInChain->sType == static_cast<WGPUSType>(WGPUSTypeExtended_SurfaceDescriptorCocoaSurfaceBacking));
    const auto& descriptor = *reinterpret_cast<const WGPUSurfaceDescriptorCocoaCustomSurface*>(surfaceDescriptor.nextInChain);
    descriptor.compositorIntegrationRegister([presentationContext = presentationContextIOSurface.copyRef()](CFArrayRef ioSurfaces) {
        presentationContext->renderBuffersWereRecreated(bridge_cast(ioSurfaces));
    });

    return presentationContextIOSurface;
}

PresentationContextIOSurface::PresentationContextIOSurface(const WGPUSurfaceDescriptor&)
{
}

PresentationContextIOSurface::~PresentationContextIOSurface() = default;

IOSurface *PresentationContextIOSurface::displayBuffer() const
{
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());
    size_t index = (m_currentIndex + 1) % m_renderBuffers.size();
    return m_ioSurfaces[index];
}

IOSurface *PresentationContextIOSurface::drawingBuffer() const
{
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());
    return m_ioSurfaces[m_currentIndex];
}

void PresentationContextIOSurface::renderBuffersWereRecreated(NSArray<IOSurface *> *ioSurfaces)
{
    m_ioSurfaces = ioSurfaces;
    m_renderBuffers.clear();
}

void PresentationContextIOSurface::configure(Device& device, const WGPUSwapChainDescriptor& descriptor)
{
    m_renderBuffers.clear();
    m_currentIndex = 0;

    if (descriptor.nextInChain)
        return;

    if (descriptor.format != WGPUTextureFormat_BGRA8Unorm)
        return;

    WGPUTextureDescriptor wgpuTextureDescriptor = {
        nullptr,
        descriptor.label,
        descriptor.usage,
        WGPUTextureDimension_2D, {
            descriptor.width,
            descriptor.height,
            1,
        },
        descriptor.format,
        1,
        1,
        1,
        &descriptor.format,
    };
    WGPUTextureViewDescriptor wgpuTextureViewDescriptor = {
        nullptr,
        descriptor.label,
        descriptor.format,
        WGPUTextureViewDimension_2D,
        0,
        1,
        0,
        1,
        WGPUTextureAspect_All,
    };
    MTLTextureDescriptor *textureDescriptor = [MTLTextureDescriptor texture2DDescriptorWithPixelFormat:Texture::pixelFormat(descriptor.format) width:descriptor.width height:descriptor.height mipmapped:NO];
    textureDescriptor.usage = Texture::usage(descriptor.usage);
    for (IOSurface *iosurface in m_ioSurfaces) {
        id<MTLTexture> texture = [device.device() newTextureWithDescriptor:textureDescriptor iosurface:bridge_cast(iosurface) plane:0];
        texture.label = fromAPI(descriptor.label);
        auto viewFormats = Vector<WGPUTextureFormat> { Texture::pixelFormat(descriptor.format) };
        m_renderBuffers.append({ Texture::create(texture, wgpuTextureDescriptor, WTFMove(viewFormats), device), TextureView::create(texture, wgpuTextureViewDescriptor, { { descriptor.width, descriptor.height, 1 } }, device) });
    }
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());
}

void PresentationContextIOSurface::unconfigure()
{
    m_ioSurfaces = nil;
    m_renderBuffers.clear();
    m_currentIndex = 0;
}

void PresentationContextIOSurface::present()
{
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());
    m_currentIndex = (m_currentIndex + 1) % m_renderBuffers.size();
}

Texture* PresentationContextIOSurface::getCurrentTexture()
{
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());
    return m_renderBuffers[m_currentIndex].texture.ptr();
}

TextureView* PresentationContextIOSurface::getCurrentTextureView()
{
    ASSERT(m_ioSurfaces.count == m_renderBuffers.size());
    return m_renderBuffers[m_currentIndex].textureView.ptr();
}

} // namespace WebGPU

#pragma mark WGPU Stubs

IOSurfaceRef wgpuSurfaceCocoaCustomSurfaceGetDisplayBuffer(WGPUSurface surface)
{
    if (auto* presentationContextIOSurface = downcast<WebGPU::PresentationContextIOSurface>(&WebGPU::fromAPI(surface)))
        return bridge_cast(presentationContextIOSurface->displayBuffer());
    return nullptr;
}

IOSurfaceRef wgpuSurfaceCocoaCustomSurfaceGetDrawingBuffer(WGPUSurface surface)
{
    if (auto* presentationContextIOSurface = downcast<WebGPU::PresentationContextIOSurface>(&WebGPU::fromAPI(surface)))
        return bridge_cast(presentationContextIOSurface->drawingBuffer());
    return nullptr;
}
