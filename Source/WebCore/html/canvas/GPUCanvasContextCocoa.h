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
#include <wtf/MachSendRight.h>
#include <wtf/Ref.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Document;
class GPUDisplayBufferDisplayDelegate;

class GPUCanvasContextCocoa final : public GPUCanvasContext {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(GPUCanvasContextCocoa);
public:
#if ENABLE(OFFSCREEN_CANVAS)
    using CanvasType = Variant<RefPtr<HTMLCanvasElement>, RefPtr<OffscreenCanvas>>;
#else
    using CanvasType = Variant<RefPtr<HTMLCanvasElement>>;
#endif

    static std::unique_ptr<GPUCanvasContextCocoa> create(CanvasBase&, GPU&, Document*);

    DestinationColorSpace colorSpace() const override;
    bool compositingResultsNeedUpdating() const override { return m_compositingResultsNeedsUpdating; }
    RefPtr<GraphicsLayerContentsDisplayDelegate> layerContentsDisplayDelegate() override;
    bool needsPreparationForDisplay() const override { return true; }
    void prepareForDisplay() override;
    ImageBufferPixelFormat pixelFormat() const override;
    bool isOpaque() const override;
    void reshape() override;


    RefPtr<ImageBuffer> surfaceBufferToImageBuffer(SurfaceBuffer) override;
    // GPUCanvasContext methods:
    CanvasType canvas() override;
    ExceptionOr<void> configure(GPUCanvasConfiguration&&) override;
    void unconfigure() override;
    std::optional<GPUCanvasConfiguration> getConfiguration() const override;
    ExceptionOr<RefPtr<GPUTexture>> getCurrentTexture() override;
    RefPtr<ImageBuffer> transferToImageBuffer() override;

#if HAVE(SUPPORT_HDR_DISPLAY) && ENABLE(PIXEL_FORMAT_RGBA16F)
    void setDynamicRangeLimit(PlatformDynamicRangeLimit) override;
    std::optional<double> getEffectiveDynamicRangeLimitValue() const override;
#endif

private:
    explicit GPUCanvasContextCocoa(CanvasBase&, Ref<GPUCompositorIntegration>&&, Ref<GPUPresentationContext>&&, Document*);

    void markContextChangedAndNotifyCanvasObservers();

    bool isConfigured() const
    {
        return static_cast<bool>(m_configuration);
    }

    CanvasType htmlOrOffscreenCanvas() const;
    ExceptionOr<void> configure(GPUCanvasConfiguration&&, bool);
    void present(uint32_t frameIndex);
#if HAVE(SUPPORT_HDR_DISPLAY)
    float computeContentsHeadroom();
    void updateContentsHeadroom();
    void updateScreenHeadroom(float, bool suppressEDR);
    void updateScreenHeadroomFromScreenProperties();
#endif // HAVE(SUPPORT_HDR_DISPLAY)

    struct Configuration {
        Ref<GPUDevice> device;
        GPUTextureFormat format { GPUTextureFormat::R8unorm };
        GPUTextureUsageFlags usage { GPUTextureUsage::RENDER_ATTACHMENT };
        Vector<GPUTextureFormat> viewFormats;
        GPUPredefinedColorSpace colorSpace { GPUPredefinedColorSpace::SRGB };
        GPUCanvasToneMapping toneMapping;
        GPUCanvasAlphaMode compositingAlphaMode { GPUCanvasAlphaMode::Opaque };
        Vector<MachSendRight> renderBuffers;
        unsigned frameCount { 0 };
    };
    std::optional<Configuration> m_configuration;

    const Ref<GPUDisplayBufferDisplayDelegate> m_layerContentsDisplayDelegate;
    const Ref<GPUCompositorIntegration> m_compositorIntegration;
    const Ref<GPUPresentationContext> m_presentationContext;
    RefPtr<GPUTexture> m_currentTexture;

    GPUIntegerCoordinate m_width { 0 };
    GPUIntegerCoordinate m_height { 0 };
#if HAVE(SUPPORT_HDR_DISPLAY)
    using ScreenPropertiesChangedObserver = Observer<void(PlatformDisplayID)>;
    std::optional<ScreenPropertiesChangedObserver> m_screenPropertiesChangedObserver;
    PlatformDynamicRangeLimit m_dynamicRangeLimit { PlatformDynamicRangeLimit::initialValue() };
    float m_currentEDRHeadroom { 1 };
    bool m_suppressEDR { false };
#endif // HAVE(SUPPORT_HDR_DISPLAY)
    bool m_compositingResultsNeedsUpdating { false };
};

} // namespace WebCore
