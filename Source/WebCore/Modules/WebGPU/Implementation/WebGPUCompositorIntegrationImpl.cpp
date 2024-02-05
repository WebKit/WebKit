/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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

#include "WebGPUConvertToBackingContext.h"
#include <CoreFoundation/CoreFoundation.h>
#include <WebCore/IOSurface.h>
#include <WebCore/NativeImage.h>
#include <WebGPU/WebGPUExt.h>
#include <pal/spi/cg/CoreGraphicsSPI.h>
#include <wtf/spi/cocoa/IOSurfaceSPI.h>

namespace WebCore::WebGPU {

CompositorIntegrationImpl::CompositorIntegrationImpl(ConvertToBackingContext& convertToBackingContext)
    : m_convertToBackingContext(convertToBackingContext)
{
}

CompositorIntegrationImpl::~CompositorIntegrationImpl() = default;

void CompositorIntegrationImpl::prepareForDisplay(CompletionHandler<void()>&& completionHandler)
{
    if (auto* presentationContext = m_presentationContext.get())
        presentationContext->present();

    m_onSubmittedWorkScheduledCallback(WTFMove(completionHandler));
}

#if PLATFORM(COCOA)
Vector<MachSendRight> CompositorIntegrationImpl::recreateRenderBuffers(int width, int height)
{
    m_renderBuffers.clear();

    if (auto* presentationContext = m_presentationContext.get()) {
        static_cast<PresentationContext*>(presentationContext)->unconfigure();
        presentationContext->setSize(width, height);
    }

    constexpr int max2DTextureSize = 16384;
    width = std::max(1, std::min(max2DTextureSize, width));
    height = std::max(1, std::min(max2DTextureSize, height));
    m_renderBuffers.append(WebCore::IOSurface::create(nullptr, WebCore::IntSize(width, height), WebCore::DestinationColorSpace::SRGB()));
    m_renderBuffers.append(WebCore::IOSurface::create(nullptr, WebCore::IntSize(width, height), WebCore::DestinationColorSpace::SRGB()));

    {
        auto renderBuffers = adoptCF(CFArrayCreateMutable(kCFAllocatorDefault, 2, &kCFTypeArrayCallBacks));
        for (auto& ioSurface : m_renderBuffers)
            CFArrayAppendValue(renderBuffers.get(), ioSurface->surface());
        m_renderBuffersWereRecreatedCallback(static_cast<CFArrayRef>(renderBuffers));
    }

    return m_renderBuffers.map([](const auto& renderBuffer) {
        return renderBuffer->createSendRight();
    });
}
#endif

void CompositorIntegrationImpl::withDisplayBufferAsNativeImage(uint32_t bufferIndex, Function<void(WebCore::NativeImage&)> completion)
{
    if (!m_renderBuffers.size() || bufferIndex >= m_renderBuffers.size())
        return;

    RefPtr<NativeImage> displayImage;

    // Use the IOSurface backed image directly
    auto& renderBuffer = m_renderBuffers[bufferIndex];
    RetainPtr<CGContextRef> cgContext = renderBuffer->createPlatformContext();
    if (cgContext)
        displayImage = NativeImage::create(renderBuffer->createImage(cgContext.get()));

    if (!displayImage)
        return;

    CGImageSetCachingFlags(displayImage->platformImage().get(), kCGImageCachingTransient);
    completion(*displayImage);
}

void CompositorIntegrationImpl::paintCompositedResultsToCanvas(WebCore::ImageBuffer&, uint32_t)
{
    ASSERT_NOT_REACHED();
}

} // namespace WebCore::WebGPU

#endif // HAVE(WEBGPU_IMPLEMENTATION)
