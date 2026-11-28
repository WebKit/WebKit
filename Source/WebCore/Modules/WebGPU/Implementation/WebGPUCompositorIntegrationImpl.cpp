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

#include "CVUtilities.h"
#include "VideoFrameCV.h"
#include "WebGPUDevice.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/IOSurface.h>
#include <WebCore/NativeImage.h>
#include <WebCore/WebGPUCppAPI.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/Borrow.h>
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
    for (auto& ioSurface : borrow(m_renderBuffers))
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

    m_renderBufferSize = renderBufferSize(width, height);
    m_renderBufferColorSpace = WTF::move(colorSpace);
    m_renderBufferFormat = colorFormat;
    for (unsigned bufferIndex = 0; bufferIndex < 3; ++bufferIndex) {
        if (auto buffer = WebCore::IOSurface::create(nullptr, m_renderBufferSize, m_renderBufferColorSpace, IOSurface::Name::WebGPU, m_renderBufferFormat))
            m_renderBuffers.append(makeUniqueRefFromNonNullUniquePtr(WTF::move(buffer)));
    }

    auto renderBuffers = m_renderBuffers.map([](auto& ioSurface) {
        return ioSurface->surface();
    });
    m_renderBuffersWereRecreatedCallback(borrow(renderBuffers)->span());

    return m_renderBuffers.size();
}

void WebGPUCompositorIntegrationImpl::prepareForDisplay(uint32_t frameIndex, CompletionHandler<void(MachSendRight&&, std::optional<uint32_t>)>&& completionHandler)
{
    if (RefPtr presentationContext = m_presentationContext)
        presentationContext->present(frameIndex);

    MachSendRight displayBuffer;
    std::optional<uint32_t> replacedRenderBufferIndex;
    if (frameIndex < m_renderBuffers.size()) {
        displayBuffer = borrow(m_renderBuffers)[frameIndex]->createSendRight();
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
    if (!borrow(m_renderBuffers)[index]->isInUse())
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

    addSpareRenderBuffer(std::exchange(borrow(m_renderBuffers)[index], makeUniqueRefFromNonNullUniquePtr(WTF::move(replacement))));
    m_renderBufferWasReplacedCallback(index, RetainPtr { m_renderBuffers[index]->surface() }.get());
    return true;
}

std::optional<size_t> WebGPUCompositorIntegrationImpl::availableSpareRenderBufferIndex(IOSurface::Format format, const ColorSpace& colorSpace)
{
    auto index = m_spareRenderBuffers.findIf([&](auto& spare) {
        return spare.surface->pixelFormat() == format && spare.surface->colorSpace() == colorSpace && !spare.surface->isInUse();
    });
    if (index == notFound)
        return std::nullopt;
    Borrow spares = m_spareRenderBuffers;
    auto& spare = spares[index];
    if (spare.isVolatile) {
        // The contents do not matter, so neither does whether they were purged.
        spare.surface->setVolatile(false);
        spare.isVolatile = false;
    }
    return index;
}

std::unique_ptr<WebCore::IOSurface> WebGPUCompositorIntegrationImpl::takeSpareRenderBuffer()
{
    auto index = availableSpareRenderBufferIndex(m_renderBufferFormat, m_renderBufferColorSpace);
    if (!index)
        return nullptr;
    auto spare = WTF::move(m_spareRenderBuffers[*index]);
    m_spareRenderBuffers.removeAt(*index);
    return spare.surface.moveToUniquePtr();
}

void WebGPUCompositorIntegrationImpl::addSpareRenderBuffer(UniqueRef<IOSurface>&& surface)
{
    m_spareRenderBuffers.append({ WTF::move(surface) });
    if (m_spareRenderBuffers.size() > maxSpareRenderBuffers)
        m_spareRenderBuffers.removeAt(0);
}

void WebGPUCompositorIntegrationImpl::updateSpareRenderBuffers()
{
    for (auto& spare : borrow(m_spareRenderBuffers)) {
        if (spare.isVolatile || spare.surface->isInUse())
            continue;
        spare.surface->setVolatile(true);
        spare.isVolatile = true;
    }
}

IOSurface::Format WebGPUCompositorIntegrationImpl::copyFormat() const
{
    // CoreVideo cannot wrap RGBA surfaces, so the copies of 8-bit render buffers are BGRA.
    switch (m_renderBufferFormat) {
    case IOSurface::Format::RGBA:
        return IOSurface::Format::BGRA;
    case IOSurface::Format::RGBX:
        return IOSurface::Format::BGRX;
    default:
        return m_renderBufferFormat;
    }
}

CGImageAlphaInfo WebGPUCompositorIntegrationImpl::copyAlphaInfo() const
{
    bool isOpaque = m_alphaMode != AlphaPremultiplication::Premultiplied;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (copyFormat() == IOSurface::Format::RGBA16F)
        return isOpaque ? kCGImageAlphaNoneSkipLast : kCGImageAlphaPremultipliedLast;
#endif
    return isOpaque ? kCGImageAlphaNoneSkipFirst : kCGImageAlphaPremultipliedFirst;
}

IOSurface* WebGPUCompositorIntegrationImpl::copyRenderBufferDestination(IOSurface::Format format, const ColorSpace& colorSpace)
{
    if (auto index = availableSpareRenderBufferIndex(format, colorSpace))
        return m_spareRenderBuffers[*index].surface.ptr();
    auto surface = IOSurface::create(nullptr, m_renderBufferSize, colorSpace, IOSurface::Name::WebGPU, format);
    if (!surface)
        return nullptr;
    auto* destination = surface.get();
    addSpareRenderBuffer(makeUniqueRefFromNonNullUniquePtr(WTF::move(surface)));
    return destination;
}
#endif

RetainPtr<CVPixelBufferRef> WebGPUCompositorIntegrationImpl::copyRenderBufferToPixelBuffer(uint32_t bufferIndex, IOSurface::Format format, WebGPU::RenderBufferCopyColorSpace copyColorSpace)
{
    // The render buffer is drawn to again in later frames, so its contents are copied. The copy is a
    // spare render buffer, which is in use for as long as the pixel buffer is, and is reused after that.
    bool isSRGBCopy = copyColorSpace == WebGPU::RenderBufferCopyColorSpace::SRGB;
    auto* copy = copyRenderBufferDestination(format, isSRGBCopy ? ColorSpace::SRGB() : m_renderBufferColorSpace);
    if (!copy)
        return nullptr;
#if HAVE(SUPPORT_HDR_DISPLAY)
    // A copy in sRGB is in the standard range, so it has no headroom.
    if (m_contentsHeadroom && !isSRGBCopy)
        copy->setContentEDRHeadroom(*m_contentsHeadroom);
#endif
    RetainPtr surface = copy->surface();
    RefPtr presentationContext = m_presentationContext;
    bool didCopy = bufferIndex < m_renderBuffers.size() && m_device.get() && presentationContext && presentationContext->copyRenderBuffer(bufferIndex, surface.get(), copyColorSpace);
    if (!didCopy) {
        // The copy of a render buffer that cannot be read is transparent, as the reader waits for a
        // copy to be published in any case.
        RetainPtr context = copy->createPlatformContext();
        if (!context)
            return nullptr;
        CGContextClearRect(context.get(), CGRectMake(0, 0, m_renderBufferSize.width(), m_renderBufferSize.height()));
        CGContextFlush(context.get());
    }

    auto pixelBuffer = createCVPixelBuffer(surface.get());
    if (!pixelBuffer)
        return nullptr;
    return WTF::move(*pixelBuffer);
}

RefPtr<NativeImage> WebGPUCompositorIntegrationImpl::copyNativeImage(uint32_t bufferIndex)
{
    RetainPtr pixelBuffer = copyRenderBufferToPixelBuffer(bufferIndex, copyFormat(), WebGPU::RenderBufferCopyColorSpace::RenderBuffer);
    if (!pixelBuffer)
        return nullptr;
    // The image has the headroom of the contents, as the render buffers do.
    auto contentHeadroom = Headroom::None;
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (m_contentsHeadroom)
        contentHeadroom = Headroom { std::max<float>(*m_contentsHeadroom, Headroom::None) };
#endif
    return NativeImage::createTransient(WTF::move(pixelBuffer), copyAlphaInfo(), m_renderBufferColorSpace.platformColorSpace(), contentHeadroom);
}

#if ENABLE(MEDIA_STREAM) || ENABLE(WEB_CODECS)
RefPtr<VideoFrame> WebGPUCompositorIntegrationImpl::copyVideoFrame(uint32_t bufferIndex)
{
    // A video frame is an 8-bit BGRA copy. The extended range of a 16-bit float render buffer does not
    // fit in it, so its copy is converted to sRGB, and the colors are clamped after the conversion.
    auto copyColorSpace = WebGPU::RenderBufferCopyColorSpace::RenderBuffer;
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (m_renderBufferFormat == IOSurface::Format::RGBA16F)
        copyColorSpace = WebGPU::RenderBufferCopyColorSpace::SRGB;
#endif
    auto colorSpace = videoColorSpaceForRGBContents(copyColorSpace == WebGPU::RenderBufferCopyColorSpace::SRGB ? ColorSpace::SRGB() : m_renderBufferColorSpace);
    if (!colorSpace)
        return nullptr;
    bool isOpaque = m_alphaMode != AlphaPremultiplication::Premultiplied;
    RetainPtr pixelBuffer = copyRenderBufferToPixelBuffer(bufferIndex, isOpaque ? IOSurface::Format::BGRX : IOSurface::Format::BGRA, copyColorSpace);
    if (!pixelBuffer)
        return nullptr;
    setAlphaInfoForCVPixelBuffer(pixelBuffer.get(), isOpaque ? kCGImageAlphaNoneSkipFirst : kCGImageAlphaPremultipliedFirst);
    attachVideoColorSpaceForRGBContents(*colorSpace, pixelBuffer.get());
    // The alpha of an opaque canvas is ignored, as the copy keeps whatever alpha the contents have.
    return VideoFrameCV::create({ }, false, VideoFrame::Rotation::None, WTF::move(pixelBuffer), WTF::move(*colorSpace), isOpaque ? VideoFrameCV::ShouldIgnoreAlpha::Yes : VideoFrameCV::ShouldIgnoreAlpha::No);
}
#endif

} // namespace WebCore

#endif // HAVE(WEBGPU_IMPLEMENTATION)
