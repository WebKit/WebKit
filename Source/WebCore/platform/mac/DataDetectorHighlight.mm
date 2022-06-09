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
#import "GraphicsLayerCA.h"
#import "GraphicsLayerFactory.h"
#import "Page.h"
#import "PlatformCAAnimationCocoa.h"
#import "PlatformCALayer.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/Seconds.h>
#import <pal/mac/DataDetectorsSoftLink.h>

namespace WebCore {

constexpr Seconds highlightFadeAnimationDuration = 300_ms;

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
{
    ASSERT(ddHighlight);

    m_graphicsLayer->setDrawsContent(true);

    setHighlight(ddHighlight.get());

    // Set directly on the PlatformCALayer so that when we leave the 'from' value implicit
    // in our animations, we get the right initial value regardless of flush timing.
    downcast<GraphicsLayerCA>(layer()).platformCALayer()->setOpacity(0);
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

    // FIXME: This needs to be moved into GraphicsContext as a DisplayList-compatible drawing command.
    if (!graphicsContext.hasPlatformContext()) {
        ASSERT_NOT_REACHED();
        return;
    }

    CGContextRef cgContext = graphicsContext.platformContext();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGLayerRef highlightLayer = PAL::softLink_DataDetectors_DDHighlightGetLayerWithContext(highlight(), cgContext);
    ALLOW_DEPRECATED_DECLARATIONS_END
    CGRect highlightBoundingRect = PAL::softLink_DataDetectors_DDHighlightGetBoundingRect(highlight());
    highlightBoundingRect.origin = CGPointZero;

    CGContextDrawLayerInRect(cgContext, highlightBoundingRect, highlightLayer);
}

float DataDetectorHighlight::deviceScaleFactor() const
{
    if (!m_page)
        return 1;

    return m_page->deviceScaleFactor();
}

void DataDetectorHighlight::fadeIn()
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [animation setDuration:highlightFadeAnimationDuration.seconds()];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:false];
    [animation setToValue:@1];

    auto platformAnimation = PlatformCAAnimationCocoa::create(animation.get());
    downcast<GraphicsLayerCA>(layer()).platformCALayer()->addAnimationForKey("FadeHighlightIn", platformAnimation.get());
}

void DataDetectorHighlight::fadeOut()
{
    RetainPtr<CABasicAnimation> animation = [CABasicAnimation animationWithKeyPath:@"opacity"];
    [animation setDuration:highlightFadeAnimationDuration.seconds()];
    [animation setFillMode:kCAFillModeForwards];
    [animation setRemovedOnCompletion:false];
    [animation setToValue:@0];

    [CATransaction begin];
    [CATransaction setCompletionBlock:[protectedSelf = Ref { *this }]() mutable {
        protectedSelf->didFinishFadeOutAnimation();
    }];

    auto platformAnimation = PlatformCAAnimationCocoa::create(animation.get());
    downcast<GraphicsLayerCA>(layer()).platformCALayer()->addAnimationForKey("FadeHighlightOut", platformAnimation.get());
    [CATransaction commit];
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
