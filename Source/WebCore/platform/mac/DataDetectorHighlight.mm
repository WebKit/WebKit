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

#import "config.h"
#import "DataDetectorHighlight.h"

#if ENABLE(DATA_DETECTION) && PLATFORM(MAC)

#import "Chrome.h"
#import "ChromeClient.h"
#import "FloatRect.h"
#import "GraphicsContext.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerFactory.h"
#import "ImageBuffer.h"
#import "Page.h"
#import <wtf/Seconds.h>
#import <pal/mac/DataDetectorsSoftLink.h>

namespace WebCore {

constexpr Seconds highlightFadeAnimationDuration = 300_ms;
constexpr double highlightFadeAnimationFrameRate = 30;

Ref<DataDetectorHighlight> DataDetectorHighlight::createForSelection(DataDetectorHighlightClient& client, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new DataDetectorHighlight(client, DataDetectorHighlight::Type::Selection, WTFMove(ddHighlight), { WTFMove(range) }));
}

Ref<DataDetectorHighlight> DataDetectorHighlight::createForTelephoneNumber(DataDetectorHighlightClient& client, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new DataDetectorHighlight(client, DataDetectorHighlight::Type::TelephoneNumber, WTFMove(ddHighlight), { WTFMove(range) }));
}

Ref<DataDetectorHighlight> DataDetectorHighlight::createForImageOverlay(DataDetectorHighlightClient& client, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new DataDetectorHighlight(client, DataDetectorHighlight::Type::ImageOverlay, WTFMove(ddHighlight), { WTFMove(range) }));
}

#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
Ref<DataDetectorHighlight> DataDetectorHighlight::createForPDFSelection(DataDetectorHighlightClient& client, RetainPtr<DDHighlightRef>&& ddHighlight)
{
    return adoptRef(*new DataDetectorHighlight(client, DataDetectorHighlight::Type::PDFSelection, WTFMove(ddHighlight), { }));
}
#endif

DataDetectorHighlight::DataDetectorHighlight(DataDetectorHighlightClient& client, Type type, RetainPtr<DDHighlightRef>&& ddHighlight, std::optional<SimpleRange>&& range)
    : m_client(client)
    , m_range(WTFMove(range))
    , m_graphicsLayer(client.createGraphicsLayer(*this).releaseNonNull())
    , m_type(type)
    , m_fadeAnimationTimer(*this, &DataDetectorHighlight::fadeAnimationTimerFired)
{
    ASSERT(ddHighlight);
    ASSERT(isRangeSupportingType() == m_range.has_value());

    m_graphicsLayer->setDrawsContent(true);

    setHighlight(ddHighlight.get());

    layer().setOpacity(0);
}

DataDetectorHighlight::~DataDetectorHighlight()
{
    invalidate();
}

void DataDetectorHighlight::setHighlight(DDHighlightRef highlight)
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return;

    if (!m_client)
        return;

    m_highlight = highlight;

    if (!m_highlight)
        return;

    CGRect highlightBoundingRect = PAL::softLink_DataDetectors_DDHighlightGetBoundingRect(m_highlight.get());
    m_graphicsLayer->setPosition(FloatPoint(highlightBoundingRect.origin));
    m_graphicsLayer->setSize(FloatSize(highlightBoundingRect.size));

    m_graphicsLayer->setNeedsDisplay();
}

void DataDetectorHighlight::invalidate()
{
    m_fadeAnimationTimer.stop();
    layer().removeFromParent();
    m_client = nullptr;
}

void DataDetectorHighlight::notifyFlushRequired(const GraphicsLayer*)
{
    if (RefPtr client = m_client.get())
        client->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

void DataDetectorHighlight::paintContents(const GraphicsLayer&, GraphicsContext& graphicsContext, const FloatRect&, OptionSet<GraphicsLayerPaintBehavior>)
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return;

    if (!highlight())
        return;

    CGRect highlightBoundingRect = PAL::softLink_DataDetectors_DDHighlightGetBoundingRect(protectedHighlight().get());
    highlightBoundingRect.origin = CGPointZero;

    auto imageBuffer = graphicsContext.createImageBuffer(FloatSize(highlightBoundingRect.size), deviceScaleFactor(), DestinationColorSpace::SRGB(), graphicsContext.renderingMode(), RenderingMethod::Local);
    if (!imageBuffer)
        return;

    RetainPtr cgContext = imageBuffer->context().platformContext();

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr highlightLayer = PAL::softLink_DataDetectors_DDHighlightGetLayerWithContext(protectedHighlight().get(), cgContext.get());
ALLOW_DEPRECATED_DECLARATIONS_END

    CGContextDrawLayerInRect(cgContext.get(), highlightBoundingRect, highlightLayer.get());

    graphicsContext.drawConsumingImageBuffer(WTFMove(imageBuffer), highlightBoundingRect);
}

float DataDetectorHighlight::deviceScaleFactor() const
{
    RefPtr client = m_client.get();
    return client ? client->deviceScaleFactor() : 1;
}

bool DataDetectorHighlight::isRangeSupportingType() const
{
#if ENABLE(UNIFIED_PDF_DATA_DETECTION)
    static constexpr OptionSet rangeSupportingHighlightTypes {
        DataDetectorHighlight::Type::TelephoneNumber,
        DataDetectorHighlight::Type::Selection,
        DataDetectorHighlight::Type::ImageOverlay,
    };

    return rangeSupportingHighlightTypes.contains(m_type);
#endif
    return true;
}

const SimpleRange& DataDetectorHighlight::range() const
{
    ASSERT(isRangeSupportingType());

    return *m_range;
}

void DataDetectorHighlight::fadeAnimationTimerFired()
{
    float animationProgress = (WallTime::now() - m_fadeAnimationStartTime) / highlightFadeAnimationDuration;
    animationProgress = std::min<float>(animationProgress, 1.0);

    float opacity = (m_fadeAnimationState == FadeAnimationState::FadingIn) ? animationProgress : 1 - animationProgress;
    layer().setOpacity(opacity);

    if (animationProgress == 1.0) {
        m_fadeAnimationTimer.stop();

        bool wasFadingOut = m_fadeAnimationState == FadeAnimationState::FadingOut;
        m_fadeAnimationState = FadeAnimationState::NotAnimating;

        if (wasFadingOut)
            didFinishFadeOutAnimation();
    }
}

void DataDetectorHighlight::dismissImmediately()
{
    layer().setOpacity(0);

    if (m_fadeAnimationTimer.isActive())
        m_fadeAnimationTimer.stop();

    m_fadeAnimationState = FadeAnimationState::NotAnimating;
    didFinishFadeOutAnimation();
}

void DataDetectorHighlight::fadeIn()
{
    if (m_fadeAnimationState == FadeAnimationState::FadingIn && m_fadeAnimationTimer.isActive())
        return;

    m_fadeAnimationState = FadeAnimationState::FadingIn;
    startFadeAnimation();
}

void DataDetectorHighlight::fadeOut()
{
    if (m_fadeAnimationState == FadeAnimationState::FadingOut && m_fadeAnimationTimer.isActive())
        return;

    m_fadeAnimationState = FadeAnimationState::FadingOut;
    startFadeAnimation();
}

void DataDetectorHighlight::startFadeAnimation()
{
    m_fadeAnimationStartTime = WallTime::now();
    m_fadeAnimationTimer.startRepeating(1_s / highlightFadeAnimationFrameRate);
}

void DataDetectorHighlight::didFinishFadeOutAnimation()
{
    RefPtr client = m_client.get();
    if (!client)
        return;

    if (client->activeHighlight() == this)
        return;

    layer().removeFromParent();
}

bool areEquivalent(const DataDetectorHighlight* a, const DataDetectorHighlight* b)
{
    if (a == b)
        return true;

    if (!a || !b)
        return false;

    if (a->type() != b->type())
        return false;

    return !a->isRangeSupportingType() || a->range() == b->range();
}

} // namespace WebCore

#endif // ENABLE(DATA_DETECTION) && PLATFORM(MAC)
