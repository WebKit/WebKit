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
#import "Texture.h"
#import "TextureView.h"
#import <IOSurface/IOSurfaceObjC.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace WebGPU {

PresentationContextIOSurface::PresentationContextIOSurface(const WGPUSurfaceDescriptor& descriptor)
{
    const WGPUSurfaceDescriptorCocoaCustomSurface& cocoaSurface = *reinterpret_cast<const WGPUSurfaceDescriptorCocoaCustomSurface*>(descriptor.nextInChain);
    m_recreateIOSurfaces = cocoaSurface.recreateIOSurfaces;
}

PresentationContextIOSurface::~PresentationContextIOSurface() = default;

void PresentationContextIOSurface::configure(Device& device, const WGPUSwapChainDescriptor& descriptor)
{
    m_renderBuffers.clear();
    m_renderBufferViews.clear();
    m_currentIndex = 0;

    if (descriptor.nextInChain)
        return;

    if (descriptor.format != WGPUTextureFormat_BGRA8Unorm)
        return;

    NSArray<IOSurface *> *iosurfaces = bridge_cast(m_recreateIOSurfaces(&descriptor));
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
    for (IOSurface *iosurface in iosurfaces) {
        id<MTLTexture> texture = [device.device() newTextureWithDescriptor:textureDescriptor iosurface:bridge_cast(iosurface) plane:0];
        texture.label = fromAPI(descriptor.label);
        auto viewFormats = Vector<WGPUTextureFormat> { Texture::pixelFormat(descriptor.format) };
        m_renderBuffers.append(Texture::create(texture, wgpuTextureDescriptor, WTFMove(viewFormats), device));
        m_renderBufferViews.append(TextureView::create(texture, wgpuTextureViewDescriptor, { { descriptor.width, descriptor.height, 1 } }, device));
    }
    ASSERT(m_renderBuffers.size() == m_renderBufferViews.size());
}

void PresentationContextIOSurface::present()
{
    ASSERT(m_renderBuffers.size() == m_renderBufferViews.size());

    if (m_renderBuffers.isEmpty())
        return;

    m_currentIndex = (m_currentIndex + 1) % m_renderBuffers.size();
}

Texture* PresentationContextIOSurface::getCurrentTexture()
{
    ASSERT(m_renderBuffers.size() == m_renderBufferViews.size());

    if (m_renderBuffers.isEmpty()) {
        // FIXME: This should return an invalid texture view.
        return nullptr;
    }

    return m_renderBuffers[m_currentIndex].ptr();
}

TextureView* PresentationContextIOSurface::getCurrentTextureView()
{
    ASSERT(m_renderBuffers.size() == m_renderBufferViews.size());

    if (m_renderBufferViews.isEmpty()) {
        // FIXME: This should return an invalid texture view.
        return nullptr;
    }

    return m_renderBufferViews[m_currentIndex].ptr();
}

} // namespace WebGPU
