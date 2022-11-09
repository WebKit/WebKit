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

#if ENABLE(WEBGPU)

#include "GPUCanvasContext.h"

#include "GPUAdapter.h"
#include "GPUCanvasConfiguration.h"
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
{
    ASSERT(platformSupportsWebGPUSurface());
}

void GPUCanvasContext::reshape(int width, int height)
{
    UNUSED_PARAM(width);
    UNUSED_PARAM(height);
}

GPUCanvasContext::CanvasType GPUCanvasContext::canvas()
{
    return htmlCanvas();
}

void GPUCanvasContext::configure(GPUCanvasConfiguration&& configuration)
{
    UNUSED_PARAM(configuration);

    auto canvasSize = getCanvasSizeAsIntSize(htmlCanvas());
    reshape(canvasSize.width(), canvasSize.height());
}

RefPtr<GPUTexture> GPUCanvasContext::getCurrentTexture()
{
    return nullptr;
}

PixelFormat GPUCanvasContext::pixelFormat() const
{
    return PixelFormat::BGRA8;
}

void GPUCanvasContext::unconfigure()
{
}

DestinationColorSpace GPUCanvasContext::colorSpace() const
{
    return DestinationColorSpace::SRGB();
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GPUCanvasContext::layerContentsDisplayDelegate()
{
    return nullptr;
}

void GPUCanvasContext::prepareForDisplay()
{
    m_compositingResultsNeedsUpdating = false;
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

#endif // ENABLE(WEBGPU_BY_DEFAULT)
