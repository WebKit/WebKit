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

#include "DestinationColorSpace.h"
#include "GPUAdapter.h"
#include "GPUCanvasConfiguration.h"
#include "GPUPresentationContext.h"
#include "GPUPresentationContextDescriptor.h"
#include "GPUTextureDescriptor.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "ImageBitmap.h"
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

static GPUIntegerCoordinate getCanvasWidth(const GPUCanvasContext::CanvasType& canvas)
{
    return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& htmlCanvas) -> GPUIntegerCoordinate {
        return htmlCanvas->width();
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [](const RefPtr<OffscreenCanvas>& offscreenCanvas) -> GPUIntegerCoordinate {
        return offscreenCanvas->width();
    }
#endif
    );
}

static GPUIntegerCoordinate getCanvasHeight(const GPUCanvasContext::CanvasType& canvas)
{
    return WTF::switchOn(canvas, [](const RefPtr<HTMLCanvasElement>& htmlCanvas) -> GPUIntegerCoordinate {
        return htmlCanvas->height();
    }
#if ENABLE(OFFSCREEN_CANVAS)
    , [](const RefPtr<OffscreenCanvas>& offscreenCanvas) -> GPUIntegerCoordinate {
        return offscreenCanvas->height();
    }
#endif
    );
}

GPUCanvasContextCocoa::CanvasType GPUCanvasContextCocoa::htmlOrOffscreenCanvas() const
{
    if (auto* c = htmlCanvas())
        return c;
    return &downcast<OffscreenCanvas>(canvasBase());
}

GPUCanvasContextCocoa::GPUCanvasContextCocoa(CanvasBase& canvas, GPU& gpu)
    : GPUCanvasContext(canvas)
    , m_layerContentsDisplayDelegate(GPUDisplayBufferDisplayDelegate::create())
    , m_compositorIntegration(gpu.createCompositorIntegration())
    , m_presentationContext(m_compositorIntegration ? gpu.createPresentationContext(presentationContextDescriptor(*m_compositorIntegration)) : nullptr)
    , m_width(getCanvasWidth(htmlOrOffscreenCanvas()))
    , m_height(getCanvasHeight(htmlOrOffscreenCanvas()))
{
}

void GPUCanvasContextCocoa::reshape(int width, int height, int oldWidth, int oldHeight)
{
    UNUSED_PARAM(oldWidth);
    UNUSED_PARAM(oldHeight);

    if (auto* texture = m_currentTexture.get()) {
        texture->destroy();
        m_currentTexture = nullptr;
    }

    m_width = static_cast<GPUIntegerCoordinate>(width);
    m_height = static_cast<GPUIntegerCoordinate>(height);

    auto configuration = WTFMove(m_configuration);
    m_configuration.reset();
    unconfigure();
    if (configuration) {
        GPUCanvasConfiguration canvasConfiguration {
            configuration->device.ptr(),
            configuration->format,
            configuration->usage,
            configuration->viewFormats,
            configuration->colorSpace,
            configuration->compositingAlphaMode,
        };
        configure(WTFMove(canvasConfiguration), true);
    }
}

void GPUCanvasContextCocoa::drawBufferToCanvas(SurfaceBuffer)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=263957): WebGPU should support obtaining drawing buffer for Web Inspector.
    m_compositorIntegration->prepareForDisplay([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        auto& base = canvasBase();
        base.clearCopiedImage();
        if (auto buffer = base.buffer(); buffer && m_configuration) {
            buffer->flushDrawingContext();
            if (m_compositorIntegration)
                m_compositorIntegration->paintCompositedResultsToCanvas(*buffer, m_configuration->frameCount);
            present();
        }
    });
}

ExceptionOr<RefPtr<ImageBitmap>> GPUCanvasContextCocoa::getCurrentTextureAsImageBitmap(ImageBuffer& buffer, bool originClean)
{
    if (m_configuration) {
        buffer.flushDrawingContext();
        if (m_compositorIntegration)
            m_compositorIntegration->paintCompositedResultsToCanvas(buffer, m_configuration->frameCount);
        m_currentTexture = nullptr;
        if (m_presentationContext)
            m_presentationContext->present(true);

        return { ImageBitmap::create(buffer, originClean) };
    }

    return { ImageBitmap::create(buffer, originClean) };
}

GPUCanvasContext::CanvasType GPUCanvasContextCocoa::canvas()
{
    return htmlOrOffscreenCanvas();
}

static bool equalConfigurations(const auto& a, const auto& b)
{
    return a.device.ptr() == b.device.get()
        && a.format         == b.format
        && a.usage          == b.usage
        && a.viewFormats    == b.viewFormats
        && a.colorSpace     == b.colorSpace;
}

static DestinationColorSpace toWebCoreColorSpace(const GPUPredefinedColorSpace& colorSpace)
{
    switch (colorSpace) {
    case GPUPredefinedColorSpace::SRGB:
        return DestinationColorSpace::SRGB();
    case GPUPredefinedColorSpace::DisplayP3:
        return DestinationColorSpace::DisplayP3();
    }

    return DestinationColorSpace::SRGB();
}

ExceptionOr<void> GPUCanvasContextCocoa::configure(GPUCanvasConfiguration&& configuration, bool dueToReshape)
{
    if (isConfigured()) {
        if (dueToReshape && equalConfigurations(*m_configuration, configuration))
            return { };

        unconfigure();
    }

    ASSERT(configuration.device);
    if (!configuration.device)
        return Exception { ExceptionCode::TypeError, "GPUCanvasContextCocoa::configure: Device is required but missing"_s };

    if (!configuration.device->isSupportedFormat(configuration.format))
        return Exception { ExceptionCode::TypeError, "GPUCanvasContext.configure: Unsupported texture format."_s };

    for (auto viewFormat : configuration.viewFormats) {
        if (!configuration.device->isSupportedFormat(viewFormat))
            return Exception { ExceptionCode::TypeError, "Unsupported texture view format."_s };
    }

    if (!m_compositorIntegration)
        return { };

    auto renderBuffers = m_compositorIntegration->recreateRenderBuffers(m_width, m_height, toWebCoreColorSpace(configuration.colorSpace), configuration.alphaMode == GPUCanvasAlphaMode::Premultiplied ? WebCore::AlphaPremultiplication::Premultiplied : WebCore::AlphaPremultiplication::Unpremultiplied, WebCore::convertToBacking(configuration.format), configuration.device->backing());
    // FIXME: This ASSERT() is wrong. It's totally possible for the IPC to the GPU process to timeout if the GPUP is busy, and return nothing here.
    ASSERT(!renderBuffers.isEmpty());

    bool reportValidationErrors = !dueToReshape;
    if (!m_presentationContext || !m_presentationContext->configure(configuration, m_width, m_height, reportValidationErrors))
        return Exception { ExceptionCode::InvalidStateError, "GPUCanvasContext.configure: Unable to configure."_s };

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
    return { };
}

ExceptionOr<void> GPUCanvasContextCocoa::configure(GPUCanvasConfiguration&& configuration)
{
    return configure(WTFMove(configuration), false);
}

void GPUCanvasContextCocoa::unconfigure()
{
    if (m_presentationContext)
        m_presentationContext->unconfigure();
    m_configuration = std::nullopt;
    m_currentTexture = nullptr;
    ASSERT(!isConfigured());
}

ExceptionOr<RefPtr<GPUTexture>> GPUCanvasContextCocoa::getCurrentTexture()
{
    if (!isConfigured())
        return Exception { ExceptionCode::InvalidStateError, "GPUCanvasContextCocoa::getCurrentTexture: canvas is not configured"_s };

    RefPtr<GPUTexture> protectedCurrentTexture = m_currentTexture;
    if (protectedCurrentTexture)
        return protectedCurrentTexture;

    markContextChangedAndNotifyCanvasObservers();
    if (!m_presentationContext)
        return nullptr;
    m_currentTexture = m_presentationContext->getCurrentTexture();
    protectedCurrentTexture = m_currentTexture;
    return protectedCurrentTexture;
}

PixelFormat GPUCanvasContextCocoa::pixelFormat() const
{
    return m_configuration ? PixelFormat::BGRA8 : PixelFormat::BGRX8;
}

DestinationColorSpace GPUCanvasContextCocoa::colorSpace() const
{
    if (!m_configuration)
        return DestinationColorSpace::SRGB();

    switch (m_configuration->colorSpace) {
    case GPUPredefinedColorSpace::SRGB:
        return DestinationColorSpace::SRGB();
    case GPUPredefinedColorSpace::DisplayP3:
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        return DestinationColorSpace::DisplayP3();
#else
        return DestinationColorSpace::SRGB();
#endif
    }
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GPUCanvasContextCocoa::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate.ptr();
}

void GPUCanvasContextCocoa::present()
{
    m_compositingResultsNeedsUpdating = false;
    m_configuration->frameCount = (m_configuration->frameCount + 1) % m_configuration->renderBuffers.size();
    if (m_currentTexture)
        m_currentTexture->destroy();
    m_currentTexture = nullptr;
    if (m_presentationContext)
        m_presentationContext->present();
}

void GPUCanvasContextCocoa::prepareForDisplay()
{
    if (!isConfigured())
        return;

    ASSERT(m_configuration->frameCount < m_configuration->renderBuffers.size());

    if (!m_compositorIntegration)
        return;

    m_compositorIntegration->prepareForDisplay([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;
        if (m_configuration->frameCount >= m_configuration->renderBuffers.size())
            return;
        m_layerContentsDisplayDelegate->setDisplayBuffer(m_configuration->renderBuffers[m_configuration->frameCount]);
        present();
    });
}

void GPUCanvasContextCocoa::markContextChangedAndNotifyCanvasObservers()
{
    m_compositingResultsNeedsUpdating = true;
    markCanvasChanged();
}


} // namespace WebCore
