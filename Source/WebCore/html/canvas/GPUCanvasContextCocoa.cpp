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
#include "GPUPresentationConfiguration.h"
#include "GPUPresentationContext.h"
#include "GPUPresentationContextDescriptor.h"
#include "GPUTextureDescriptor.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "RenderBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

static IntSize getCanvasSizeAsIntSize(const GPUCanvasContext::CanvasType& canvas)
{
    return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& htmlCanvas) -> IntSize {
        auto scaleFactor = htmlCanvas->document().deviceScaleFactor();
        return { static_cast<int>(scaleFactor * htmlCanvas->width()), static_cast<int>(scaleFactor * htmlCanvas->height()) };
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [](const RefPtr<OffscreenCanvas>& offscreenCanvas) -> IntSize {
        return { static_cast<int>(offscreenCanvas->width()), static_cast<int>(offscreenCanvas->height()) };
    }
#endif
    );
}

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

GPUCanvasContextCocoa::GPUCanvasContextCocoa(CanvasBase& canvas, GPU& gpu)
    : GPUCanvasContext(canvas)
    , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create())
    , m_compositorIntegration(gpu.createCompositorIntegration())
    , m_presentationContext(gpu.createPresentationContext(presentationContextDescriptor(m_compositorIntegration)))
{
}

void GPUCanvasContextCocoa::reshape(int width, int height)
{
    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;
    unconfigurePresentationContextIfNeeded();

    configurePresentationContextIfNeeded();
}

GPUCanvasContext::CanvasType GPUCanvasContextCocoa::canvas()
{
    return htmlCanvas();
}

void GPUCanvasContextCocoa::configure(GPUCanvasConfiguration&& configuration)
{
    m_configuration = WTFMove(configuration);

    auto canvasSize = getCanvasSizeAsIntSize(htmlCanvas());
    reshape(canvasSize.width(), canvasSize.height());
}

void GPUCanvasContextCocoa::unconfigurePresentationContextIfNeeded()
{
    if (!m_presentationContextIsConfigured)
        return;

    m_presentationContext->unconfigure();
    m_presentationContextIsConfigured = false;
}

void GPUCanvasContextCocoa::configurePresentationContextIfNeeded()
{
    if (m_presentationContextIsConfigured)
        return;

    if (!m_configuration)
        return;

    GPUPresentationConfiguration configuration = {
        m_configuration->device, // FIXME: https://bugs.webkit.org/show_bug.cgi?id=250995 This is definitely a UAF
        m_configuration->format,
        m_configuration->usage,
        m_configuration->viewFormats,
        m_configuration->colorSpace,
        m_configuration->compositingAlphaMode,
        static_cast<uint32_t>(m_width), // FIXME: Is it possible for these to be negative?
        static_cast<uint32_t>(m_height),
    };

    m_presentationContext->configure(configuration);
    m_presentationContextIsConfigured = true;
}

RefPtr<GPUTexture> GPUCanvasContextCocoa::getCurrentTexture()
{
    if (m_currentTexture)
        return m_currentTexture;

    if (!m_configuration)
        return nullptr;

    configurePresentationContextIfNeeded();

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

void GPUCanvasContextCocoa::unconfigure()
{
    m_configuration.reset();
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
    m_presentationContext->prepareForDisplay([protectedThis = Ref { *this }] (auto sendRight) {
        protectedThis->m_layerContentsDisplayDelegate->setDisplayBuffer(WTFMove(sendRight));
        protectedThis->m_compositingResultsNeedsUpdating = false;
    });
    m_currentTexture = nullptr;
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
