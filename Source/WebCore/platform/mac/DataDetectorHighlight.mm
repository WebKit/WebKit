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

Ref<DataDetectorHighlight> DataDetectorHighlight::createForSelection(Page& page, DataDetectorHighlightClient& client, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new DataDetectorHighlight(page, client, DataDetectorHighlight::Type::Selection, WTFMove(ddHighlight), WTFMove(range)));
}

Ref<DataDetectorHighlight> DataDetectorHighlight::createForTelephoneNumber(Page& page, DataDetectorHighlightClient& client, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new DataDetectorHighlight(page, client, DataDetectorHighlight::Type::TelephoneNumber, WTFMove(ddHighlight), WTFMove(range)));
}

Ref<DataDetectorHighlight> DataDetectorHighlight::createForImageOverlay(Page& page, DataDetectorHighlightClient& client, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
{
    return adoptRef(*new DataDetectorHighlight(page, client, DataDetectorHighlight::Type::ImageOverlay, WTFMove(ddHighlight), WTFMove(range)));
}

DataDetectorHighlight::DataDetectorHighlight(Page& page, DataDetectorHighlightClient& client, Type type, RetainPtr<DDHighlightRef>&& ddHighlight, SimpleRange&& range)
    : m_client(client)
    , m_page(page)
    , m_range(WTFMove(range))
    , m_graphicsLayer(GraphicsLayer::create(page.chrome().client().graphicsLayerFactory(), *this))
    , m_type(type)
    , m_fadeAnimationTimer(*this, &DataDetectorHighlight::fadeAnimationTimerFired)
{
    ASSERT(ddHighlight);

    m_graphicsLayer->setDrawsContent(true);

    setHighlight(ddHighlight.get());

    layer().setOpacity(0);
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
    m_page = nullptr;
}

void DataDetectorHighlight::notifyFlushRequired(const GraphicsLayer*)
{
    if (!m_page)
        return;

    m_page->scheduleRenderingUpdate(RenderingUpdateStep::LayerFlush);
}

void DataDetectorHighlight::paintContents(const GraphicsLayer*, GraphicsContext& graphicsContext, const FloatRect&, GraphicsLayerPaintBehavior)
{
    if (!PAL::isDataDetectorsFrameworkAvailable())
        return;

    CGRect highlightBoundingRect = PAL::softLink_DataDetectors_DDHighlightGetBoundingRect(highlight());
    highlightBoundingRect.origin = CGPointZero;

    auto imageBuffer = graphicsContext.createImageBuffer(FloatSize(highlightBoundingRect.size), deviceScaleFactor(), DestinationColorSpace::SRGB(), graphicsContext.renderingMode(), RenderingMethod::Local);
    if (!imageBuffer)
        return;

    CGContextRef cgContext = imageBuffer->context().platformContext();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGLayerRef highlightLayer = PAL::softLink_DataDetectors_DDHighlightGetLayerWithContext(highlight(), cgContext);
    ALLOW_DEPRECATED_DECLARATIONS_END

    CGContextDrawLayerInRect(cgContext, highlightBoundingRect, highlightLayer);

    graphicsContext.drawConsumingImageBuffer(WTFMove(imageBuffer), highlightBoundingRect);
}

float DataDetectorHighlight::deviceScaleFactor() const
{
    if (!m_page)
        return 1;

    return m_page->deviceScaleFactor();
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
    if (!m_client)
        return;

    if (m_client->activeHighlight() == this)
        return;

    layer().removeFromParent();
}

bool areEquivalent(const DataDetectorHighlight* a, const DataDetectorHighlight* b)
{
    if (a == b)
        return true;

    if (!a || !b)
        return false;

    return a->type() == b->type() && a->range() == b->range();
}

} // namespace WebCore

#endif // ENABLE(DATA_DETECTION) && PLATFORM(MAC)
