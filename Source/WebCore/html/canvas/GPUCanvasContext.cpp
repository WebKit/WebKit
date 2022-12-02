/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if HAVE(WEBGPU_IMPLEMENTATION)

#include "GPUCanvasContext.h"

#include "GPUAdapter.h"
#include "GPUCanvasConfiguration.h"
#include "GPUSurface.h"
#include "GPUSurfaceDescriptor.h"
#include "GPUSwapChain.h"
#include "GPUSwapChainDescriptor.h"
#include "GPUTextureDescriptor.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "RenderBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

static IntSize getCanvasSizeAsIntSize(const GPUCanvasContext::CanvasType& canvas)
{
    return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& htmlCanvas) -> IntSize {
        return { static_cast<int>(htmlCanvas->width()), static_cast<int>(htmlCanvas->height()) };
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [](const RefPtr<OffscreenCanvas>& offscreenCanvas) -> IntSize {
        return { static_cast<int>(offscreenCanvas->width()), static_cast<int>(offscreenCanvas->height()) };
    }
#endif
    );
}

static bool platformSupportsWebGPUSurface()
{
#if PLATFORM(COCOA)
    return true;
#else
    return false;
#endif
}

WTF_MAKE_ISO_ALLOCATED_IMPL(GPUCanvasContext);

std::unique_ptr<GPUCanvasContext> GPUCanvasContext::create(CanvasBase& canvas)
{
    if (!platformSupportsWebGPUSurface())
        return nullptr;

    auto context = std::unique_ptr<GPUCanvasContext>(new GPUCanvasContext(canvas));
    context->suspendIfNeeded();
    return context;
}

GPUCanvasContext::GPUCanvasContext(CanvasBase& canvas)
    : GPUBasedCanvasRenderingContext(canvas)
    , m_layerContentsDisplayDelegate(DisplayBufferDisplayDelegate::create())
{
    ASSERT(platformSupportsWebGPUSurface());
}

void GPUCanvasContext::reshape(int width, int height)
{
    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;
    m_swapChain = nullptr;

    createSwapChainIfNeeded();
}

GPUCanvasContext::CanvasType GPUCanvasContext::canvas()
{
    return htmlCanvas();
}

void GPUCanvasContext::configure(GPUCanvasConfiguration&& configuration)
{
    m_configuration = WTFMove(configuration);

    auto canvasSize = getCanvasSizeAsIntSize(htmlCanvas());
    reshape(canvasSize.width(), canvasSize.height());
}

void GPUCanvasContext::createSwapChainIfNeeded()
{
    if (m_swapChain || !m_configuration)
        return;

    GPUSurfaceDescriptor surfaceDescriptor = {
        { "WebGPU Canvas surface"_s },
        GPUExtent3DDict { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 },
        1 /* sampleCount */,
        m_configuration->format,
        m_configuration->usage
    };

    m_surface = m_configuration->device->createSurface(surfaceDescriptor);
    ASSERT(m_surface);

    GPUSwapChainDescriptor descriptor = {
        { "WebGPU Canvas swap chain"_s },
        GPUExtent3DDict { static_cast<uint32_t>(m_width), static_cast<uint32_t>(m_height), 1 },
        1 /* sampleCount */,
        m_configuration->format,
        m_configuration->usage
    };

    m_swapChain = m_configuration->device->createSwapChain(*m_surface, descriptor);
    ASSERT(m_swapChain);
}

RefPtr<GPUTexture> GPUCanvasContext::getCurrentTexture()
{
    if (!m_configuration)
        return nullptr;

    createSwapChainIfNeeded();

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
    return m_configuration->device->createSurfaceTexture(descriptor, *m_surface);
}

PixelFormat GPUCanvasContext::pixelFormat() const
{
    return PixelFormat::BGRA8;
}

void GPUCanvasContext::unconfigure()
{
    m_configuration.reset();
}

DestinationColorSpace GPUCanvasContext::colorSpace() const
{
    return DestinationColorSpace::SRGB();
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GPUCanvasContext::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate.ptr();
}

void GPUCanvasContext::prepareForDisplay()
{
#if PLATFORM(COCOA)
    m_swapChain->prepareForDisplay([protectedThis = Ref { *this }] (auto sendRight) {
        protectedThis->m_layerContentsDisplayDelegate->setDisplayBuffer(WTFMove(sendRight));
        protectedThis->m_compositingResultsNeedsUpdating = false;
    });
#endif
}

void GPUCanvasContext::markContextChangedAndNotifyCanvasObservers()
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

}

#endif // HAVE(WEBGPU_IMPLEMENTATION)
