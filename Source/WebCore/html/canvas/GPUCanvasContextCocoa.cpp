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
#include "GPUCanvasContextCocoa.h"

#include "GPUAdapter.h"
#include "GPUCanvasConfiguration.h"
#include "GPUPresentationContext.h"
#include "GPUPresentationContextDescriptor.h"
#include "GPUTextureDescriptor.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "RenderBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(GPUCanvasContextCocoa);

std::unique_ptr<GPUCanvasContext> GPUCanvasContext::create(CanvasBase& canvas, GPU& gpu)
{
    auto context = GPUCanvasContextCocoa::create(canvas, gpu);
    if (context)
        context->suspendIfNeeded();
    return context;
}

std::unique_ptr<GPUCanvasContextCocoa> GPUCanvasContextCocoa::create(CanvasBase& canvas, GPU& gpu)
{
    return std::unique_ptr<GPUCanvasContextCocoa>(new GPUCanvasContextCocoa(canvas, gpu));
}

static GPUPresentationContextDescriptor presentationContextDescriptor(GPUCompositorIntegration& compositorIntegration)
{
    return GPUPresentationContextDescriptor {
        compositorIntegration,
    };
}

static int getCanvasWidth(const GPUCanvasContext::CanvasType& canvas)
{
    return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& htmlCanvas) -> int {
        auto scaleFactor = htmlCanvas->document().deviceScaleFactor();
        return scaleFactor * htmlCanvas->width();
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [](const RefPtr<OffscreenCanvas>& offscreenCanvas) -> int {
        return offscreenCanvas->width();
    }
#endif
    );
}

static int getCanvasHeight(const GPUCanvasContext::CanvasType& canvas)
{
    return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& htmlCanvas) -> int {
        auto scaleFactor = htmlCanvas->document().deviceScaleFactor();
        return scaleFactor * htmlCanvas->height();
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [](const RefPtr<OffscreenCanvas>& offscreenCanvas) -> int {
        return offscreenCanvas->height();
    }
#endif
    );
}

GPUCanvasContextCocoa::GPUCanvasContextCocoa(CanvasBase& canvas, GPU& gpu)
    : GPUCanvasContext(canvas)
    , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create())
    , m_compositorIntegration(gpu.createCompositorIntegration())
    , m_presentationContext(gpu.createPresentationContext(presentationContextDescriptor(m_compositorIntegration)))
    , m_width(getCanvasWidth(htmlCanvas()))
    , m_height(getCanvasHeight(htmlCanvas()))
{
}

void GPUCanvasContextCocoa::reshape(int width, int height)
{
    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;

    auto configuration = WTFMove(m_configuration);
    ASSERT(!isConfigured());
    if (configuration) {
        GPUCanvasConfiguration canvasConfiguration {
            configuration->device.ptr(),
            configuration->format,
            configuration->usage,
            configuration->viewFormats,
            configuration->colorSpace,
            configuration->compositingAlphaMode,
        };
        configure(WTFMove(canvasConfiguration));
    }
}

GPUCanvasContext::CanvasType GPUCanvasContextCocoa::canvas()
{
    return htmlCanvas();
}

void GPUCanvasContextCocoa::configure(GPUCanvasConfiguration&& configuration)
{
    if (isConfigured())
        return;

    if (!m_width || !m_height)
        return; // FIXME: This should probably do something more sensible.

    ASSERT(configuration.device);
    if (!configuration.device)
        return;

    auto renderBuffers = m_compositorIntegration->recreateRenderBuffers(m_width, m_height);
    ASSERT(!renderBuffers.isEmpty());

    m_presentationContext->configure(configuration);

    m_configuration = {
        *configuration.device,
        configuration.format,
        configuration.usage,
        configuration.viewFormats,
        configuration.colorSpace,
        configuration.compositingAlphaMode,
        WTFMove(renderBuffers),
        0,
    };
}

void GPUCanvasContextCocoa::unconfigure()
{
    m_presentationContext->unconfigure();
    m_configuration = std::nullopt;
    ASSERT(!isConfigured());
}

RefPtr<GPUTexture> GPUCanvasContextCocoa::getCurrentTexture()
{
    if (!isConfigured()) {
        // FIXME: I think we're supposed to return an invalid texture here.
        return nullptr;
    }

    if (m_currentTexture)
        return m_currentTexture;

    GPUTextureDescriptor descriptor = {
        { "WebGPU Display texture"_s },
        GPUExtent3DDict { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 },
        1 /* mipMapCount */,
        1 /* sampleCount */,
        GPUTextureDimension::_2d,
        m_configuration->format,
        m_configuration->usage,
        m_configuration->viewFormats
    };

    markContextChangedAndNotifyCanvasObservers();
    // FIXME: This should use PresentationContext::getCurrentTexture() instead.
    m_currentTexture = m_configuration->device->createSurfaceTexture(descriptor, m_presentationContext);
    return m_currentTexture;
}

PixelFormat GPUCanvasContextCocoa::pixelFormat() const
{
    return PixelFormat::BGRA8;
}

DestinationColorSpace GPUCanvasContextCocoa::colorSpace() const
{
    return DestinationColorSpace::SRGB();
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GPUCanvasContextCocoa::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate.ptr();
}

void GPUCanvasContextCocoa::prepareForDisplay()
{
    if (!isConfigured())
        return;

    ASSERT(m_configuration->frameCount < m_configuration->renderBuffers.size());

    m_compositorIntegration->prepareForDisplay([&] {
        m_layerContentsDisplayDelegate->setDisplayBuffer(m_configuration->renderBuffers[m_configuration->frameCount]);
        m_compositingResultsNeedsUpdating = false;
        m_configuration->frameCount = (m_configuration->frameCount + 1) % m_configuration->renderBuffers.size();
        m_currentTexture = nullptr;
    });
}

void GPUCanvasContextCocoa::markContextChangedAndNotifyCanvasObservers()
{
    m_compositingResultsNeedsUpdating = true;

    bool canvasIsDirty = false;

    if (auto* canvas = htmlCanvas()) {
        auto* renderBox = canvas->renderBox();
        if (isAccelerated() && renderBox && renderBox->hasAcceleratedCompositing()) {
            canvasIsDirty = true;
            canvas->clearCopiedImage();
            renderBox->contentChanged(CanvasChanged);
        }
    }

    if (!canvasIsDirty) {
        canvasIsDirty = true;
        canvasBase().didDraw({ });
    }

    if (!isAccelerated())
        return;

    auto* canvas = htmlCanvas();
    if (!canvas)
        return;

    canvas->notifyObserversCanvasChanged({ });
}

} // namespace WebCore
