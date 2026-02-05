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
#include "GPUCanvasContextCocoa.h"

#include "DestinationColorSpace.h"
#include "GPUAdapter.h"
#include "GPUCanvasConfiguration.h"
#include "GPUPresentationContext.h"
#include "GPUPresentationContextDescriptor.h"
#include "GPUTextureDescriptor.h"
#include "GraphicsClient.h"
#include "GraphicsLayerContentsDisplayDelegate.h"
#include "GraphicsLayerEnums.h"
#include "ImageBitmap.h"
#include "PlatformCALayerDelegatedContents.h"
#include "PlatformScreen.h"
#include "RenderBox.h"
#include "ScreenProperties.h"
#include "Settings.h"
#include <wtf/TZoneMallocInlines.h>

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
        layer.setContentsFormat(m_contentsFormat);
    }
    void display(PlatformCALayer& layer) final
    {
        if (layer.isOpaque() != m_isOpaque)
            layer.setOpaque(m_isOpaque);
        if (m_displayBuffer) {
            layer.setContentsFormat(m_contentsFormat);
            layer.setDelegatedContents({ MachSendRight { m_displayBuffer }, { }, std::nullopt });
        } else
            layer.clearContents();
    }
    GraphicsLayerCompositingCoordinatesOrientation orientation() const final
    {
        return GraphicsLayerCompositingCoordinatesOrientation::TopDown;
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
    void setContentsFormat(ContentsFormat contentsFormat)
    {
        m_contentsFormat = contentsFormat;
    }
    void setOpaque(bool opaque)
    {
        m_isOpaque = opaque;
    }
private:
    GPUDisplayBufferDisplayDelegate(bool isOpaque, float contentsScale)
        : m_contentsScale(contentsScale)
        , m_isOpaque(isOpaque)
    {
    }
    WTF::MachSendRight m_displayBuffer;
    const float m_contentsScale;
    bool m_isOpaque;
    ContentsFormat m_contentsFormat { ContentsFormat::RGBA8 };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(GPUCanvasContextCocoa);

std::unique_ptr<GPUCanvasContext> GPUCanvasContext::create(CanvasBase& canvas, GPU& gpu, Document* document)
{
    auto context = GPUCanvasContextCocoa::create(canvas, gpu, document);
    if (context)
        context->suspendIfNeeded();
    return context;
}

static GPUPresentationContextDescriptor presentationContextDescriptor(GPUCompositorIntegration& compositorIntegration)
{
    return GPUPresentationContextDescriptor {
        compositorIntegration,
    };
}

std::unique_ptr<GPUCanvasContextCocoa> GPUCanvasContextCocoa::create(CanvasBase& canvas, GPU& gpu, Document* document)
{
    RefPtr compositorIntegration = gpu.createCompositorIntegration();
    if (!compositorIntegration)
        return nullptr;
    RefPtr presentationContext = gpu.createPresentationContext(presentationContextDescriptor(*compositorIntegration));
    if (!presentationContext)
        return nullptr;
    return std::unique_ptr<GPUCanvasContextCocoa>(new GPUCanvasContextCocoa(canvas, compositorIntegration.releaseNonNull(), presentationContext.releaseNonNull(), document));
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

GPUCanvasContext::CanvasType GPUCanvasContextCocoa::htmlOrOffscreenCanvas() const
{
    if (RefPtr canvas = htmlCanvas())
        return canvas;
    return &downcast<OffscreenCanvas>(canvasBase());
}

GPUCanvasContextCocoa::GPUCanvasContextCocoa(CanvasBase& canvas, Ref<GPUCompositorIntegration>&& compositorIntegration, Ref<GPUPresentationContext>&& presentationContext, Document* document)
    : GPUCanvasContext(canvas)
    , m_layerContentsDisplayDelegate(GPUDisplayBufferDisplayDelegate::create())
    , m_compositorIntegration(WTF::move(compositorIntegration))
    , m_presentationContext(WTF::move(presentationContext))
    , m_width(getCanvasWidth(htmlOrOffscreenCanvas()))
    , m_height(getCanvasHeight(htmlOrOffscreenCanvas()))
#if HAVE(SUPPORT_HDR_DISPLAY)
    , m_screenPropertiesChangedObserver(ScreenPropertiesChangedObserver::create([weakThis = WeakPtr { *this }](PlatformDisplayID displayID) {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (auto* screenData = WebCore::screenData(displayID))
            protectedThis->updateScreenHeadroom(screenData->currentEDRHeadroom, screenData->suppressEDR);
    }))
#endif // HAVE(SUPPORT_HDR_DISPLAY)
{
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (document)
        document->addScreenPropertiesChangedObserver(*m_screenPropertiesChangedObserver);
    else
        m_screenPropertiesChangedObserver = nullptr;
#else
    UNUSED_PARAM(document);
#endif
}

#if HAVE(SUPPORT_HDR_DISPLAY)
static float interpolateHeadroom(float headroomForLow, float headroomForHigh, float limit, float limitLow, float limitHigh)
{
    if (headroomForHigh <= headroomForLow || limitHigh <= limitLow)
        return headroomForHigh;
    return std::lerp(headroomForLow, headroomForHigh, (limit - limitLow) / (limitHigh - limitLow));
}

float GPUCanvasContextCocoa::computeContentsHeadroom()
{
    if (m_currentEDRHeadroom <= 1.f)
        return m_currentEDRHeadroom;

    if (m_dynamicRangeLimit == PlatformDynamicRangeLimit::noLimit())
        return m_currentEDRHeadroom;

    constexpr auto forcedStandardHeadroom = 1.0000001f;

    if (m_dynamicRangeLimit == PlatformDynamicRangeLimit::standard())
        return forcedStandardHeadroom;

    auto limitValue = m_dynamicRangeLimit.value();

    if (m_suppressEDR) {
        if (limitValue >= PlatformDynamicRangeLimit::constrained().value())
            return m_currentEDRHeadroom;
        return interpolateHeadroom(forcedStandardHeadroom, m_currentEDRHeadroom, limitValue, PlatformDynamicRangeLimit::standard().value(), PlatformDynamicRangeLimit::constrained().value());
    }

    constexpr auto maxConstrainedHeadroom = 1.6f;
    auto suppressedHeadroom = std::min(maxConstrainedHeadroom, m_currentEDRHeadroom);
    if (limitValue <= PlatformDynamicRangeLimit::constrained().value())
        return interpolateHeadroom(forcedStandardHeadroom, suppressedHeadroom, limitValue, PlatformDynamicRangeLimit::standard().value(), PlatformDynamicRangeLimit::constrained().value());
    return interpolateHeadroom(suppressedHeadroom, m_currentEDRHeadroom, limitValue, PlatformDynamicRangeLimit::constrained().value(), PlatformDynamicRangeLimit::noLimit().value());
}

void GPUCanvasContextCocoa::updateContentsHeadroom()
{
    m_compositorIntegration->updateContentsHeadroom(computeContentsHeadroom());
}

void GPUCanvasContextCocoa::updateScreenHeadroom(float currentEDRHeadroom, bool suppressEDR)
{
    if (m_suppressEDR == suppressEDR && m_currentEDRHeadroom == currentEDRHeadroom)
        return;

    m_currentEDRHeadroom = currentEDRHeadroom;
    m_suppressEDR = suppressEDR;
    updateContentsHeadroom();
}

void GPUCanvasContextCocoa::updateScreenHeadroomFromScreenProperties()
{
    m_currentEDRHeadroom = 1.f;
    m_suppressEDR = false;
    for (const auto& screenData : WebCore::getScreenProperties().screenDataMap.values()) {
        m_currentEDRHeadroom = std::max(m_currentEDRHeadroom, screenData.currentEDRHeadroom);
        m_suppressEDR |= screenData.suppressEDR;
    }
    updateContentsHeadroom();
}

#if ENABLE(PIXEL_FORMAT_RGBA16F)
void GPUCanvasContextCocoa::setDynamicRangeLimit(PlatformDynamicRangeLimit dynamicRangeLimit)
{
    if (m_dynamicRangeLimit == dynamicRangeLimit)
        return;

    m_dynamicRangeLimit = dynamicRangeLimit;

    if (!m_screenPropertiesChangedObserver || m_currentEDRHeadroom < 1.f)
        return updateScreenHeadroomFromScreenProperties();

    updateContentsHeadroom();
}

std::optional<double> GPUCanvasContextCocoa::getEffectiveDynamicRangeLimitValue() const
{
    auto limitValue = m_dynamicRangeLimit.value();
    auto suppressValue = m_suppressEDR ? PlatformDynamicRangeLimit::constrained().value() : PlatformDynamicRangeLimit::noLimit().value();
    return std::min(limitValue, suppressValue);
}
#endif // ENABLE(PIXEL_FORMAT_RGBA16F)
#endif // HAVE(SUPPORT_HDR_DISPLAY)

void GPUCanvasContextCocoa::reshape()
{
    if (RefPtr currentTexture = m_currentTexture) {
        currentTexture->destroy();
        m_currentTexture = nullptr;
    }
    auto newSize = canvasBase().size();
    auto newWidth = static_cast<GPUIntegerCoordinate>(newSize.width());
    auto newHeight = static_cast<GPUIntegerCoordinate>(newSize.height());
    if (m_width == newWidth && m_height == newHeight)
        return;

    m_width = newWidth;
    m_height = newHeight;

    auto configuration = WTF::move(m_configuration);
    m_configuration.reset();
    unconfigure();
    if (configuration) {
        GPUCanvasConfiguration canvasConfiguration {
            configuration->device,
            configuration->format,
            configuration->usage,
            configuration->viewFormats,
            configuration->colorSpace,
            configuration->toneMapping,
            configuration->compositingAlphaMode,
        };
        configure(WTF::move(canvasConfiguration), true);
    }
}

RefPtr<ImageBuffer> GPUCanvasContextCocoa::surfaceBufferToImageBuffer(SurfaceBuffer)
{
    // FIXME(https://bugs.webkit.org/show_bug.cgi?id=263957): WebGPU should support obtaining drawing buffer for Web Inspector.
    if (!m_configuration)
        return canvasBase().buffer();

    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=294654 - OffscreenCanvas may not reflect the display the OffscreenCanvas is displayed on during background / resume
#if HAVE(SUPPORT_HDR_DISPLAY)
    if (!m_screenPropertiesChangedObserver)
        updateScreenHeadroomFromScreenProperties();
#endif

    auto frameCount = m_configuration->frameCount;
    m_compositorIntegration->prepareForDisplay(frameCount, [weakThis = WeakPtr { *this }, frameCount] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;

        RefPtr base = protectedThis->canvasBase();
        base->clearCopiedImage();
        if (RefPtr buffer = base->buffer(); buffer && protectedThis->m_configuration) {
            buffer->flushDrawingContext();
            protectedThis->m_compositorIntegration->paintCompositedResultsToCanvas(*buffer, frameCount);
            protectedThis->present(frameCount);
        }
    });
    return canvasBase().buffer();
}

RefPtr<ImageBuffer> GPUCanvasContextCocoa::transferToImageBuffer()
{
    RefPtr scriptExecutionContext = canvasBase().scriptExecutionContext();
    if (!scriptExecutionContext)
        return nullptr;
    const auto size = canvasBase().size();
    if (size.isEmpty())
        return nullptr;
    RefPtr buffer = ImageBuffer::create(size, RenderingMode::Accelerated, RenderingPurpose::Canvas, 1, DestinationColorSpace::SRGB(), PixelFormat::BGRA8, scriptExecutionContext->graphicsClient());
    if (!buffer)
        return nullptr;
    Ref<ImageBuffer> bufferRef = buffer.releaseNonNull();
    if (m_configuration) {
        m_compositorIntegration->paintCompositedResultsToCanvas(bufferRef, m_configuration->frameCount);
        m_currentTexture = nullptr;
        m_presentationContext->present(m_configuration->frameCount, true);
    }
    return bufferRef;
}

GPUCanvasContext::CanvasType GPUCanvasContextCocoa::canvas()
{
    return htmlOrOffscreenCanvas();
}

static bool equalConfigurations(const auto& a, const auto& b)
{
    return a.device.ptr()   == b.device.ptr()
        && a.format         == b.format
        && a.usage          == b.usage
        && a.viewFormats    == b.viewFormats
        && a.colorSpace     == b.colorSpace;
}

static DestinationColorSpace toWebCoreColorSpace(const GPUPredefinedColorSpace& colorSpace, const GPUCanvasToneMapping& toneMapping)
{
    switch (colorSpace) {
    case GPUPredefinedColorSpace::SRGB:
        return toneMapping.mode == GPUCanvasToneMappingMode::Standard ? DestinationColorSpace::SRGB() : DestinationColorSpace::ExtendedSRGB();
    case GPUPredefinedColorSpace::DisplayP3:
#if ENABLE(PREDEFINED_COLOR_SPACE_DISPLAY_P3)
        return toneMapping.mode == GPUCanvasToneMappingMode::Standard ? DestinationColorSpace::DisplayP3() : DestinationColorSpace::ExtendedDisplayP3();
#else
        return toneMapping.mode == GPUCanvasToneMappingMode::Standard ? DestinationColorSpace::SRGB() : DestinationColorSpace::ExtendedSRGB();
#endif
    }

    return DestinationColorSpace::SRGB();
}

static WebGPU::TextureFormat computeTextureFormat(GPUTextureFormat format, GPUCanvasToneMappingMode toneMappingMode)
{
    // Force Bgra8unorm to both: clamp color values to SDR, and opt out of CALayer HDR.
    if (format == GPUTextureFormat::Rgba16float && toneMappingMode == GPUCanvasToneMappingMode::Standard)
        return WebGPU::TextureFormat::Bgra8unorm;

    return WebCore::convertToBacking(format);
}

static bool isSupportedContextFormat(GPUTextureFormat format)
{
    return format == GPUTextureFormat::Bgra8unorm || format == GPUTextureFormat::Rgba8unorm || format == GPUTextureFormat::Rgba16float;
}

ExceptionOr<void> GPUCanvasContextCocoa::configure(GPUCanvasConfiguration&& configuration, bool dueToReshape)
{
    if (isConfigured()) {
        if (dueToReshape && equalConfigurations(*m_configuration, configuration))
            return { };

        unconfigure();
    }

    if (auto error = configuration.device->errorValidatingSupportedFormat(configuration.format))
        return Exception { ExceptionCode::TypeError, makeString("GPUCanvasContext.configure: Unsupported texture format: "_s, *error) };

    for (auto viewFormat : configuration.viewFormats) {
        if (auto error = configuration.device->errorValidatingSupportedFormat(viewFormat))
            return Exception { ExceptionCode::TypeError, makeString("Unsupported texture view format: "_s, *error) };
    }

    if (!isSupportedContextFormat(configuration.format))
        return Exception { ExceptionCode::TypeError, "GPUCanvasContext.configure: Unsupported context format."_s };

    if (configuration.toneMapping.mode != GPUCanvasToneMappingMode::Standard) {
#if ENABLE(HDR_FOR_WEBGPU)
        RefPtr scriptExecutionContext = canvasBase().scriptExecutionContext();
        if (!scriptExecutionContext || !scriptExecutionContext->settingsValues().webGPUHDREnabled)
            configuration.toneMapping.mode = GPUCanvasToneMappingMode::Standard;
#else
        configuration.toneMapping.mode = GPUCanvasToneMappingMode::Standard;
#endif
    }

    auto textureFormat = computeTextureFormat(configuration.format, configuration.toneMapping.mode);
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    // Only use RGBA16F when CALayer HDR is needed.
    m_layerContentsDisplayDelegate->setContentsFormat(textureFormat != WebGPU::TextureFormat::Rgba16float ? ContentsFormat::RGBA8 : ContentsFormat::RGBA16F);
#endif

#if HAVE(SUPPORT_HDR_DISPLAY)
    m_currentEDRHeadroom = 0.f;
    m_suppressEDR = false;
#endif // HAVE(SUPPORT_HDR_DISPLAY)
    auto renderBuffers = m_compositorIntegration->recreateRenderBuffers(m_width, m_height, toWebCoreColorSpace(configuration.colorSpace, configuration.toneMapping), configuration.alphaMode == GPUCanvasAlphaMode::Premultiplied ? WebCore::AlphaPremultiplication::Premultiplied : WebCore::AlphaPremultiplication::Unpremultiplied, textureFormat, is<OffscreenCanvas>(canvasBase()) ? 1 : 3, configuration.device->backing());
    // FIXME: This ASSERT() is wrong. It's totally possible for the IPC to the GPU process to timeout if the GPUP is busy, and return nothing here.
    ASSERT(!renderBuffers.isEmpty());

    bool reportValidationErrors = !dueToReshape;
    if (!m_presentationContext->configure(configuration, m_width, m_height, reportValidationErrors))
        return Exception { ExceptionCode::InvalidStateError, "GPUCanvasContext.configure: Unable to configure."_s };

    m_layerContentsDisplayDelegate->setOpaque(configuration.alphaMode == GPUCanvasAlphaMode::Opaque);
    m_configuration = {
        configuration.device,
        configuration.format,
        configuration.usage,
        configuration.viewFormats,
        configuration.colorSpace,
        configuration.toneMapping,
        configuration.alphaMode,
        WTF::move(renderBuffers),
        0,
    };
    return { };
}

ExceptionOr<void> GPUCanvasContextCocoa::configure(GPUCanvasConfiguration&& configuration)
{
    return configure(WTF::move(configuration), false);
}

void GPUCanvasContextCocoa::unconfigure()
{
    m_presentationContext->unconfigure();
    m_configuration = std::nullopt;
    m_currentTexture = nullptr;
    ASSERT(!isConfigured());
}

std::optional<GPUCanvasConfiguration> GPUCanvasContextCocoa::getConfiguration() const
{
    std::optional<GPUCanvasConfiguration> configuration;
    if (m_configuration) {
        configuration.emplace(GPUCanvasConfiguration {
            m_configuration->device,
            m_configuration->format,
            m_configuration->usage,
            m_configuration->viewFormats,
            m_configuration->colorSpace,
            m_configuration->toneMapping,
            m_configuration->compositingAlphaMode,
        });
    }

    return configuration;
}

ExceptionOr<Ref<GPUTexture>> GPUCanvasContextCocoa::getCurrentTexture()
{
    if (!isConfigured())
        return Exception { ExceptionCode::InvalidStateError, "GPUCanvasContextCocoa::getCurrentTexture: canvas is not configured"_s };

    RefPtr currentTexture = m_currentTexture;
    if (currentTexture)
        return currentTexture.releaseNonNull();

    markContextChangedAndNotifyCanvasObservers();
    m_currentTexture = m_presentationContext->getCurrentTexture(m_configuration->frameCount);
    currentTexture = m_currentTexture;
    return currentTexture.releaseNonNull();
}

PixelFormat GPUCanvasContextCocoa::pixelFormat() const
{
#if ENABLE(PIXEL_FORMAT_RGBA16F)
    if (m_configuration)
        return m_configuration->toneMapping.mode == GPUCanvasToneMappingMode::Extended ? PixelFormat::RGBA16F : PixelFormat::BGRA8;
#endif
    return PixelFormat::BGRX8;
}

bool GPUCanvasContextCocoa::isOpaque() const
{
    if (m_configuration)
        return m_configuration->compositingAlphaMode == GPUCanvasAlphaMode::Opaque;
    return true;
}

DestinationColorSpace GPUCanvasContextCocoa::colorSpace() const
{
    if (!m_configuration)
        return DestinationColorSpace::SRGB();

    return toWebCoreColorSpace(m_configuration->colorSpace, m_configuration->toneMapping);
}

RefPtr<GraphicsLayerContentsDisplayDelegate> GPUCanvasContextCocoa::layerContentsDisplayDelegate()
{
    return m_layerContentsDisplayDelegate.ptr();
}

void GPUCanvasContextCocoa::present(uint32_t frameIndex)
{
    if (!m_configuration)
        return;

    m_compositingResultsNeedsUpdating = false;
    m_configuration->frameCount = (m_configuration->frameCount + 1) % m_configuration->renderBuffers.size();
    if (RefPtr currentTexture = m_currentTexture)
        currentTexture->destroy();
    m_currentTexture = nullptr;
    m_presentationContext->present(frameIndex);
}

void GPUCanvasContextCocoa::prepareForDisplay()
{
    if (!isConfigured())
        return;

    ASSERT(m_configuration->frameCount < m_configuration->renderBuffers.size());

    auto frameIndex = m_configuration->frameCount;
    m_compositorIntegration->prepareForDisplay(frameIndex, [weakThis = WeakPtr { *this }, frameIndex] {
        RefPtr protectedThis = weakThis.get();
        if (!protectedThis)
            return;
        if (frameIndex >= protectedThis->m_configuration->renderBuffers.size())
            return;
        protectedThis->m_layerContentsDisplayDelegate->setDisplayBuffer(protectedThis->m_configuration->renderBuffers[frameIndex]);
        protectedThis->present(frameIndex);
    });
}

void GPUCanvasContextCocoa::markContextChangedAndNotifyCanvasObservers()
{
    m_compositingResultsNeedsUpdating = true;
    markCanvasChanged();
}


} // namespace WebCore
