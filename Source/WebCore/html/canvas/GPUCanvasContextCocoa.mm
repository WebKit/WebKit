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
#include "PlatformCALayerDelegatedContents.h"
#include "RenderBox.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

class GPUDisplayBufferDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<GPUDisplayBufferDisplayDelegate> create(bool isOpaque = true, float contentsScale = 1)
    {
        return adoptRef(*new GPUDisplayBufferDisplayDelegate(isOpaque, contentsScale));
    }
    // GraphicsLayerContentsDisplayDelegate overrides.
    void prepareToDelegateDisplay(PlatformCALayer& layer) final
    {
        layer.setOpaque(m_isOpaque);
        layer.setContentsScale(m_contentsScale);
    }
    void display(PlatformCALayer& layer) final
    {
        if (m_displayBuffer)
            layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, { }, std::nullopt });
        else
            layer.clearContents();
    }
    GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return GraphicsLayer::CompositingCoordinatesOrientation::TopDown;
    }
    void setDisplayBuffer(WTF::MachSendRight& displayBuffer)
    {
        if (!displayBuffer) {
            m_displayBuffer = { };
            return;
        }

        if (m_displayBuffer && displayBuffer.sendRight() == m_displayBuffer.sendRight())
            return;

        m_displayBuffer = MachSendRight { displayBuffer };
    }
private:
    GPUDisplayBufferDisplayDelegate(bool isOpaque, float contentsScale)
        : m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }
    WTF::MachSendRight m_displayBuffer;
    const float m_contentsScale;
    const bool m_isOpaque;
};

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
        return htmlCanvas->width();
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
        return htmlCanvas->height();
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [](const RefPtr<OffscreenCanvas>& offscreenCanvas) -> int {
        return offscreenCanvas->height();
    }
#endif
    );
}

GPUCanvasContextCocoa::CanvasType GPUCanvasContextCocoa::htmlOrOffscreenCanvas() const
{
    if (auto* c = htmlCanvas())
        return c;

    auto& base = canvasBase();
    RELEASE_ASSERT(is<OffscreenCanvas>(base));

    return &downcast<OffscreenCanvas>(base);
}

GPUCanvasContextCocoa::GPUCanvasContextCocoa(CanvasBase& canvas, GPU& gpu)
    : GPUCanvasContext(canvas)
    , m_layerContentsDisplayDelegate(GPUDisplayBufferDisplayDelegate::create())
    , m_compositorIntegration(gpu.createCompositorIntegration())
    , m_presentationContext(gpu.createPresentationContext(presentationContextDescriptor(m_compositorIntegration)))
    , m_width(getCanvasWidth(htmlOrOffscreenCanvas()))
    , m_height(getCanvasHeight(htmlOrOffscreenCanvas()))
{
}

void GPUCanvasContextCocoa::reshape(int width, int height)
{
    if (m_width == width && m_height == height)
        return;

    m_width = width;
    m_height = height;

    auto configuration = WTFMove(m_configuration);
    m_configuration.reset();
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

void GPUCanvasContextCocoa::prepareForDisplayWithPaint()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=233622 - Support offscreen canvas with WebGPU
    prepareForDisplay();
}

void GPUCanvasContextCocoa::paintRenderingResultsToCanvas()
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=233622 - Support offscreen canvas with WebGPU
}

GPUCanvasContext::CanvasType GPUCanvasContextCocoa::canvas()
{
    return htmlOrOffscreenCanvas();
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
    // FIXME: This ASSERT() is wrong. It's totally possible for the IPC to the GPU process to timeout if the GPUP is busy, and return nothing here.
    ASSERT(!renderBuffers.isEmpty());

    m_presentationContext->configure(configuration);

    m_configuration = {
        *configuration.device,
        configuration.format,
        configuration.usage,
        configuration.viewFormats,
        configuration.colorSpace,
        configuration.alphaMode,
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

    markContextChangedAndNotifyCanvasObservers();
    m_currentTexture = m_presentationContext->getCurrentTexture();
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
        m_presentationContext->present();
    });
}

void GPUCanvasContextCocoa::markContextChangedAndNotifyCanvasObservers()
{
    m_compositingResultsNeedsUpdating = true;

    bool canvasIsDirty = false;

    auto canvas = htmlOrOffscreenCanvas();
    CanvasBase *canvasBase = nullptr;
    WTF::switchOn(canvas, [&](RefPtr<HTMLCanvasElement>& htmlCanvas) {
        auto* renderBox = htmlCanvas->renderBox();
        canvasBase = htmlCanvas.get();
        if (isAccelerated() && renderBox && renderBox->hasAcceleratedCompositing()) {
            canvasIsDirty = true;
            htmlCanvas->clearCopiedImage();
            renderBox->contentChanged(CanvasChanged);
        }
    }, [&](RefPtr<OffscreenCanvas>& offscreenCanvas) {
        UNUSED_PARAM(offscreenCanvas);
        canvasBase = offscreenCanvas.get();
        // FIXME: https://bugs.webkit.org/show_bug.cgi?id=233622 - [WebGPU] Hook it up to WorkerNavigator
    });

    if (!canvasIsDirty)
        this->canvasBase().didDraw({ });

    if (!isAccelerated())
        return;

    if (!canvasBase)
        return;

    canvasBase->notifyObserversCanvasChanged({ });
}

} // namespace WebCore
