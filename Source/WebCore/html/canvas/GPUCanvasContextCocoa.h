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

#pragma once

#include "GPU.h"
#include "GPUBasedCanvasRenderingContext.h"
#include "GPUCanvasConfiguration.h"
#include "GPUCanvasContext.h"
#include "GPUPresentationContext.h"
#include "GPUTexture.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "HTMLCanvasElement.h"
#include "IOSurface.h"
#include "OffscreenCanvas.h"
#include "PlatformCALayer.h"
#include <variant>
#include <wtf/MachSendRight.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class DisplayBufferDisplayDelegate final : public GraphicsLayerContentsDisplayDelegate {
public:
    static Ref<DisplayBufferDisplayDelegate> create(bool isOpaque = true, float contentsScale = 1)
    {
        return adoptRef(*new DisplayBufferDisplayDelegate(isOpaque, contentsScale));
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
            layer.setContents(m_displayBuffer);
        else
            layer.clearContents();
    }
    GraphicsLayer::CompositingCoordinatesOrientation orientation() const final
    {
        return GraphicsLayer::CompositingCoordinatesOrientation::TopDown;
    }
    void setDisplayBuffer(WTF::MachSendRight&& displayBuffer)
    {
        if (!displayBuffer) {
            m_displayBuffer = { };
            return;
        }

        if (m_displayBuffer && displayBuffer.sendRight() == m_displayBuffer.sendRight())
            return;

        m_displayBuffer = displayBuffer.copySendRight();
    }
private:
    DisplayBufferDisplayDelegate(bool isOpaque, float contentsScale)
        : m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }
    WTF::MachSendRight m_displayBuffer;
    const float m_contentsScale;
    const bool m_isOpaque;
};

class GPUCanvasContextCocoa final : public GPUCanvasContext {
    WTF_MAKE_ISO_ALLOCATED(GPUCanvasContextCocoa);
public:
#if ENABLE(OFFSCREEN_CANVAS)
    using CanvasType = std::variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;
#else
    using CanvasType = std::variant<RefPtr<HTMLCanvasElement>>;
#endif

    static std::unique_ptr<GPUCanvasContextCocoa> create(CanvasBase&, GPU&);

    DestinationColorSpace colorSpace() const override;
    bool compositingResultsNeedUpdating() const override { return m_compositingResultsNeedsUpdating; }
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;
    bool needsPreparationForDisplay() const override { return true; }
    void prepareForDisplay() override;
    PixelFormat pixelFormat() const override;
    void reshape(int width, int height) override;

    // FIXME: implement these to allow for painting
    void prepareForDisplayWithPaint() override { }
    void paintRenderingResultsToCanvas() override { }
    // GPUCanvasContext methods:
    CanvasType canvas() override;
    void configure(GPUCanvasConfiguration&&) override;
    void unconfigure() override;
    RefPtr<GPUTexture> getCurrentTexture() override;

    bool isWebGPU() const override { return true; }
    const char* activeDOMObjectName() const override
    {
        return "GPUCanvasElement";
    }

private:
    explicit GPUCanvasContextCocoa(CanvasBase&, GPU&);

    void markContextChangedAndNotifyCanvasObservers();
    void configurePresentationContextIfNeeded();
    void unconfigurePresentationContextIfNeeded();

    std::optional<GPUCanvasConfiguration> m_configuration;
    Ref<DisplayBufferDisplayDelegate> m_layerContentsDisplayDelegate;
    Ref<GPUCompositorIntegration> m_compositorIntegration;
    Ref<GPUPresentationContext> m_presentationContext;
    RefPtr<GPUTexture> m_currentTexture;

    int m_width { 0 };
    int m_height { 0 };
    bool m_presentationContextIsConfigured { false };
    bool m_compositingResultsNeedsUpdating { false };
};

} // namespace WebCore
