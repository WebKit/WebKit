/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebGPUCompositorIntegrationImpl.h"

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "WebGPUDevice.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/IOSurface.h>
#include <WebCore/NativeImage.h>
#include <WebCore/WebGPUCppAPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebGPUCompositorIntegrationImpl);

WebGPUCompositorIntegrationImpl::WebGPUCompositorIntegrationImpl()
{
}

WebGPUCompositorIntegrationImpl::~WebGPUCompositorIntegrationImpl() = default;

Seconds WebGPUCompositorIntegrationImpl::lastFrameGPUCost() const
{
    if (RefPtr presentationContext = m_presentationContext)
        return presentationContext->lastFrameGPUCost();
    return 0_s;
}

Seconds WebGPUCompositorIntegrationImpl::lastFramePresentStall() const
{
    if (RefPtr presentationContext = m_presentationContext)
        return presentationContext->lastFramePresentStall();
    return 0_s;
}

void WebGPUCompositorIntegrationImpl::updateContentsHeadroom(float headroom)
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    m_contentsHeadroom = headroom;
    for (auto& ioSurface : m_renderBuffers)
        ioSurface->setContentEDRHeadroom(headroom);
#else
    UNUSED_PARAM(headroom);
#endif
}

#if PLATFORM(COCOA)
uint32_t WebGPUCompositorIntegrationImpl::recreateRenderBuffers(int width, int height, WebCore::ColorSpace&& colorSpace, WebCore::AlphaPremultiplication alphaMode, WebGPU::TextureFormat textureFormat, WebGPU::Device& device)
{
    m_renderBuffers.clear();
    m_spareRenderBuffers.clear();
#if HAVE(SUPPORT_HDR_DISPLAY)
    m_contentsHeadroom = std::nullopt;
#endif
    m_device = device;
    m_alphaMode = alphaMode;

    if (RefPtr presentationContext = m_presentationContext) {
        presentationContext->unconfigure();
    }

    constexpr int max2DTextureSize = 16384;
    width = std::max(1, std::min(max2DTextureSize, width));
    height = std::max(1, std::min(max2DTextureSize, height));
    IOSurface::Format colorFormat;
    switch (textureFormat) {
    case WebGPU::TextureFormat::Rgba8unorm:
    case WebGPU::TextureFormat::Rgba8unormSRGB:
        colorFormat = alphaMode == AlphaPremultiplication::Unpremultiplied ? IOSurface::Format::RGBX : IOSurface::Format::RGBA;
        break;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    case WebGPU::TextureFormat::Rgba16float:
        colorFormat = IOSurface::Format::RGBA16F;
        break;
#endif
    default:
        colorFormat = alphaMode == AlphaPremultiplication::Unpremultiplied ? IOSurface::Format::BGRX : IOSurface::Format::BGRA;
        break;
    }

    m_renderBufferSize = { width, height };
    m_renderBufferColorSpace = WTF::move(colorSpace);
    m_renderBufferFormat = colorFormat;
    for (unsigned bufferIndex = 0; bufferIndex < 3; ++bufferIndex) {
        if (auto buffer = WebCore::IOSurface::create(nullptr, m_renderBufferSize, m_renderBufferColorSpace, IOSurface::Name::WebGPU, m_renderBufferFormat))
            m_renderBuffers.append(makeUniqueRefFromNonNullUniquePtr(WTF::move(buffer)));
    }

    auto renderBuffers = m_renderBuffers.map([](auto& ioSurface) {
        return ioSurface->surface();
    });
    m_renderBuffersWereRecreatedCallback(renderBuffers.span());

    return m_renderBuffers.size();
}

void WebGPUCompositorIntegrationImpl::prepareForDisplay(uint32_t frameIndex, CompletionHandler<void(MachSendRight&&, std::optional<uint32_t>)>&& completionHandler)
{
    if (RefPtr presentationContext = m_presentationContext)
        presentationContext->present(frameIndex);

    MachSendRight displayBuffer;
    std::optional<uint32_t> replacedRenderBufferIndex;
    if (frameIndex < m_renderBuffers.size()) {
        displayBuffer = m_renderBuffers[frameIndex]->createSendRight();
        uint32_t nextFrameIndex = (frameIndex + 1) % m_renderBuffers.size();
        if (nextFrameIndex != frameIndex && replaceRenderBufferIfInUse(nextFrameIndex))
            replacedRenderBufferIndex = nextFrameIndex;
    }

    m_onSubmittedWorkScheduledCallback([completionHandler = WTF::move(completionHandler), displayBuffer = WTF::move(displayBuffer), replacedRenderBufferIndex] mutable {
        completionHandler(WTF::move(displayBuffer), replacedRenderBufferIndex);
    });
}

bool WebGPUCompositorIntegrationImpl::replaceRenderBufferIfInUse(uint32_t index)
{
    updateSpareRenderBuffers();
    if (!m_renderBuffers[index]->isInUse())
        return false;

    auto replacement = takeSpareRenderBuffer();
    if (!replacement)
        replacement = WebCore::IOSurface::create(nullptr, m_renderBufferSize, m_renderBufferColorSpace, IOSurface::Name::WebGPU, m_renderBufferFormat);
    if (!replacement)
        return false;
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (m_contentsHeadroom)
        replacement->setContentEDRHeadroom(*m_contentsHeadroom);
#endif

    m_spareRenderBuffers.append({ std::exchange(m_renderBuffers[index], makeUniqueRefFromNonNullUniquePtr(WTF::move(replacement))) });
    if (m_spareRenderBuffers.size() > maxSpareRenderBuffers)
        m_spareRenderBuffers.removeAt(0);
    m_renderBufferWasReplacedCallback(index, RetainPtr { m_renderBuffers[index]->surface() }.get());
    return true;
}

std::unique_ptr<WebCore::IOSurface> WebGPUCompositorIntegrationImpl::takeSpareRenderBuffer()
{
    auto index = m_spareRenderBuffers.findIf([](auto& spare) {
        return !spare.surface->isInUse();
    });
    if (index == notFound)
        return nullptr;

    auto spare = WTF::move(m_spareRenderBuffers[index]);
    m_spareRenderBuffers.removeAt(index);
    if (spare.isVolatile)
        spare.surface->setVolatile(false);
    return spare.surface.moveToUniquePtr();
}

void WebGPUCompositorIntegrationImpl::updateSpareRenderBuffers()
{
    for (auto& spare : m_spareRenderBuffers) {
        if (spare.isVolatile || spare.surface->isInUse())
            continue;
        spare.surface->setVolatile(true);
        spare.isVolatile = true;
    }
}
#endif

void WebGPUCompositorIntegrationImpl::withDisplayBufferAsNativeImage(uint32_t bufferIndex, Function<void(WebCore::NativeImage*)> completion)
{
    if (!m_renderBuffers.size() || bufferIndex >= m_renderBuffers.size() || !m_device.get())
        return completion(nullptr);

    RefPtr<NativeImage> displayImage;
    bool isIOSurfaceSupportedFormat = false;
    if (RefPtr presentationContextPtr = m_presentationContext)
        displayImage = NativeImage::create(presentationContextPtr->getTextureAsNativeImage(bufferIndex, isIOSurfaceSupportedFormat));

    if (displayImage) {
        CGImageSetCachingFlags(displayImage->platformImage().get(), kCGImageCachingTransient);
        return completion(displayImage.get());
    }

    if (!isIOSurfaceSupportedFormat)
        return completion(nullptr);

    // A premultiplied canvas must keep its alpha channel so transparent pixels
    // composite over the page; an opaque canvas forces the alpha to be ignored.
    // This matters for RGBA16F, whose IOSurface format cannot itself encode opacity.
    auto shouldForceOpaque = m_alphaMode == WebCore::AlphaPremultiplication::Premultiplied ? IOSurface::ShouldForceOpaque::No : IOSurface::ShouldForceOpaque::Yes;
    m_renderBuffers[bufferIndex]->withNativeImage(shouldForceOpaque, [&](NativeImage* image) {
        if (image)
            CGImageSetCachingFlags(image->platformImage().get(), kCGImageCachingTransient);
        completion(image);
    });
}

void WebGPUCompositorIntegrationImpl::paintCompositedResultsToCanvas(WebCore::ImageBuffer&, uint32_t)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore

#endif // HAVE(WEBGPU_IMPLEMENTATION)
