/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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
#include "ViewGestureGeometryCollector.h"

#include "Logging.h"
#include "MessageSenderInlines.h"
#include "ViewGestureGeometryCollectorMessages.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <WebCore/FontCascade.h>
#include <WebCore/HTMLImageElement.h>
#include <WebCore/HTMLTextFormControlElement.h>
#include <WebCore/HitTestResult.h>
#include <WebCore/ImageDocument.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Range.h>
#include <WebCore/RenderView.h>
#include <WebCore/TextIterator.h>

#if PLATFORM(IOS_FAMILY)
#include "SmartMagnificationControllerMessages.h"
#else
#include "ViewGestureControllerMessages.h"
#endif

namespace WebKit {
using namespace WebCore;

#if PLATFORM(IOS_FAMILY)
static const double minimumScaleDifferenceForZooming = 0.3;
#endif

ViewGestureGeometryCollector::ViewGestureGeometryCollector(WebPage& webPage)
    : m_webPage(webPage)
#if !PLATFORM(IOS_FAMILY)
    , m_renderTreeSizeNotificationThreshold(0)
#endif
{
    WebProcess::singleton().addMessageReceiver(Messages::ViewGestureGeometryCollector::messageReceiverName(), m_webPage.identifier(), *this);
}

ViewGestureGeometryCollector::~ViewGestureGeometryCollector()
{
    WebProcess::singleton().removeMessageReceiver(Messages::ViewGestureGeometryCollector::messageReceiverName(), m_webPage.identifier());
}

void ViewGestureGeometryCollector::dispatchDidCollectGeometryForSmartMagnificationGesture(FloatPoint origin, FloatRect absoluteTargetRect, FloatRect visibleContentRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale)
{
#if PLATFORM(MAC)
    m_webPage.send(Messages::ViewGestureController::DidCollectGeometryForSmartMagnificationGesture(origin, absoluteTargetRect, visibleContentRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale));
#endif
#if PLATFORM(IOS_FAMILY)
    m_webPage.send(Messages::SmartMagnificationController::DidCollectGeometryForSmartMagnificationGesture(origin, absoluteTargetRect, visibleContentRect, fitEntireRect, viewportMinimumScale, viewportMaximumScale));
#endif
}

void ViewGestureGeometryCollector::collectGeometryForSmartMagnificationGesture(FloatPoint gestureLocationInViewCoordinates)
{
    RefPtr frameView = m_webPage.localMainFrameView();
    if (!frameView)
        return;

    FloatRect visibleContentRect = frameView->unobscuredContentRectIncludingScrollbars();

    if (m_webPage.handlesPageScaleGesture())
        return;

    double viewportMinimumScale;
    double viewportMaximumScale;

#if PLATFORM(IOS_FAMILY)
    if (m_webPage.platformPrefersTextLegibilityBasedZoomScaling()) {
        auto textLegibilityScales = computeTextLegibilityScales(viewportMinimumScale, viewportMaximumScale);
        if (!textLegibilityScales) {
            dispatchDidCollectGeometryForSmartMagnificationGesture({ }, { }, { }, false, 0, 0);
            return;
        }

        float targetScale = m_webPage.viewportConfiguration().initialScale();
        float currentScale = m_webPage.pageScaleFactor();
        if (currentScale < textLegibilityScales->first - minimumScaleDifferenceForZooming)
            targetScale = textLegibilityScales->first;
        else if (currentScale < textLegibilityScales->second - minimumScaleDifferenceForZooming)
            targetScale = textLegibilityScales->second;

        FloatRect targetRectInContentCoordinates { gestureLocationInViewCoordinates, FloatSize() };
        targetRectInContentCoordinates.inflate(m_webPage.viewportConfiguration().viewLayoutSize() / (2 * targetScale));

        dispatchDidCollectGeometryForSmartMagnificationGesture(gestureLocationInViewCoordinates, targetRectInContentCoordinates, visibleContentRect, true, viewportMinimumScale, viewportMaximumScale);
        return;
    }
#endif // PLATFORM(IOS_FAMILY)

    IntPoint originInContentsSpace = frameView->windowToContents(roundedIntPoint(gestureLocationInViewCoordinates));
    HitTestResult hitTestResult = HitTestResult(originInContentsSpace);

    if (auto* mainFrame = dynamicDowncast<WebCore::LocalFrame>(m_webPage.mainFrame()))
        mainFrame->document()->hitTest(HitTestRequest(), hitTestResult);

    RefPtr node = hitTestResult.innerNode();
    if (!node) {
        dispatchDidCollectGeometryForSmartMagnificationGesture(FloatPoint(), FloatRect(), FloatRect(), false, 0, 0);
        return;
    }

    bool isReplaced;
    FloatRect absoluteBoundingRect;

    computeZoomInformationForNode(*node, gestureLocationInViewCoordinates, absoluteBoundingRect, isReplaced, viewportMinimumScale, viewportMaximumScale);
    dispatchDidCollectGeometryForSmartMagnificationGesture(gestureLocationInViewCoordinates, absoluteBoundingRect, visibleContentRect, isReplaced, viewportMinimumScale, viewportMaximumScale);
}

#if PLATFORM(IOS_FAMILY)

struct FontSizeAndCount {
    unsigned fontSize;
    unsigned count;
};

std::optional<std::pair<double, double>> ViewGestureGeometryCollector::computeTextLegibilityScales(double& viewportMinimumScale, double& viewportMaximumScale)
{
    static const unsigned fontSizeBinningInterval = 2;
    static const double maximumNumberOfTextRunsToConsider = 200;

    static const double targetLegibilityFontSize = 12;
    static const double textLegibilityScaleRatio = 0.1;
    static const double defaultTextLegibilityZoomScale = 1;

    computeMinimumAndMaximumViewportScales(viewportMinimumScale, viewportMaximumScale);
    if (m_cachedTextLegibilityScales)
        return m_cachedTextLegibilityScales;

    RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(m_webPage.mainFrame());
    if (!localMainFrame)
        return std::nullopt;
    RefPtr document = localMainFrame->document();
    if (!document)
        return std::nullopt;

    document->updateLayout(LayoutOptions::IgnorePendingStylesheets);

    HashSet<Ref<Node>> allTextNodes;
    HashMap<unsigned, unsigned> fontSizeToCountMap;
    unsigned numberOfIterations = 0;
    unsigned totalSampledTextLength = 0;

    for (TextIterator documentTextIterator { makeRangeSelectingNodeContents(*document), TextIteratorBehavior::EntersTextControls }; !documentTextIterator.atEnd(); documentTextIterator.advance()) {
        if (++numberOfIterations >= maximumNumberOfTextRunsToConsider)
            break;

        if (!is<Text>(documentTextIterator.node()))
            continue;

        auto& textNode = downcast<Text>(*documentTextIterator.node());
        auto textLength = textNode.length();
        if (!textLength || !textNode.renderer() || allTextNodes.contains(textNode))
            continue;

        allTextNodes.add(textNode);

        unsigned fontSizeBin = fontSizeBinningInterval * round(textNode.renderer()->style().fontCascade().size() / fontSizeBinningInterval);
        auto entry = fontSizeToCountMap.find(fontSizeBin);
        fontSizeToCountMap.set(fontSizeBin, textLength + (entry == fontSizeToCountMap.end() ? 0 : entry->value));
        totalSampledTextLength += textLength;
    }

    auto sortedFontSizesAndCounts = WTF::map(fontSizeToCountMap, [](auto& entry) {
        return FontSizeAndCount { entry.key, entry.value };
    });

    std::sort(sortedFontSizesAndCounts.begin(), sortedFontSizesAndCounts.end(), [] (auto& first, auto& second) {
        return first.fontSize < second.fontSize;
    });

    double defaultScale = clampTo<double>(defaultTextLegibilityZoomScale, viewportMinimumScale, viewportMaximumScale);
    double textLegibilityScale = defaultScale;
    double currentSampledTextLength = 0;
    for (auto& fontSizeAndCount : sortedFontSizesAndCounts) {
        currentSampledTextLength += fontSizeAndCount.count;
        double ratioOfTextUnderCurrentFontSize = currentSampledTextLength / totalSampledTextLength;
        if (ratioOfTextUnderCurrentFontSize >= textLegibilityScaleRatio) {
            textLegibilityScale = clampTo<double>(targetLegibilityFontSize / fontSizeAndCount.fontSize, viewportMinimumScale, viewportMaximumScale);
            break;
        }
    }

    auto firstTextLegibilityScale = std::min<double>(textLegibilityScale, defaultScale);
    auto secondTextLegibilityScale = std::max<double>(textLegibilityScale, defaultScale);
    if (secondTextLegibilityScale - firstTextLegibilityScale < minimumScaleDifferenceForZooming)
        firstTextLegibilityScale = secondTextLegibilityScale;

    m_cachedTextLegibilityScales.emplace(std::pair<double, double> { firstTextLegibilityScale, secondTextLegibilityScale });
    return m_cachedTextLegibilityScales;
}

#endif // PLATFORM(IOS_FAMILY)

void ViewGestureGeometryCollector::computeZoomInformationForNode(Node& node, FloatPoint& origin, FloatRect& absoluteBoundingRect, bool& isReplaced, double& viewportMinimumScale, double& viewportMaximumScale)
{
    absoluteBoundingRect = node.absoluteBoundingRect(&isReplaced);
    if (node.document().isImageDocument()) {
        if (HTMLImageElement* imageElement = static_cast<ImageDocument&>(node.document()).imageElement()) {
            if (&node != imageElement) {
                absoluteBoundingRect = imageElement->absoluteBoundingRect(&isReplaced);
                FloatPoint newOrigin = origin;
                if (origin.x() < absoluteBoundingRect.x() || origin.x() > absoluteBoundingRect.maxX())
                    newOrigin.setX(absoluteBoundingRect.x() + absoluteBoundingRect.width() / 2);
                if (origin.y() < absoluteBoundingRect.y() || origin.y() > absoluteBoundingRect.maxY())
                    newOrigin.setY(absoluteBoundingRect.y() + absoluteBoundingRect.height() / 2);
                origin = newOrigin;
            }
            isReplaced = true;
        }
    }
    computeMinimumAndMaximumViewportScales(viewportMinimumScale, viewportMaximumScale);
}

void ViewGestureGeometryCollector::computeMinimumAndMaximumViewportScales(double& viewportMinimumScale, double& viewportMaximumScale) const
{
#if PLATFORM(IOS_FAMILY)
    viewportMinimumScale = m_webPage.minimumPageScaleFactor();
    viewportMaximumScale = m_webPage.maximumPageScaleFactor();
#else
    viewportMinimumScale = 0;
    viewportMaximumScale = std::numeric_limits<double>::max();
#endif
}

#if !PLATFORM(IOS_FAMILY)
void ViewGestureGeometryCollector::collectGeometryForMagnificationGesture()
{
    RefPtr frameView = m_webPage.localMainFrameView();
    if (!frameView)
        return;

    FloatRect visibleContentRect = frameView->unobscuredContentRectIncludingScrollbars();
    bool frameHandlesMagnificationGesture = m_webPage.handlesPageScaleGesture();
    m_webPage.send(Messages::ViewGestureController::DidCollectGeometryForMagnificationGesture(visibleContentRect, frameHandlesMagnificationGesture));
}

void ViewGestureGeometryCollector::setRenderTreeSizeNotificationThreshold(uint64_t size)
{
    m_renderTreeSizeNotificationThreshold = size;
    sendDidHitRenderTreeSizeThresholdIfNeeded();
}

void ViewGestureGeometryCollector::sendDidHitRenderTreeSizeThresholdIfNeeded()
{
    if (m_renderTreeSizeNotificationThreshold && m_webPage.renderTreeSize() >= m_renderTreeSizeNotificationThreshold) {
        m_webPage.send(Messages::ViewGestureController::DidHitRenderTreeSizeThreshold());
        m_renderTreeSizeNotificationThreshold = 0;
    }
}
#endif

void ViewGestureGeometryCollector::mainFrameDidLayout()
{
#if PLATFORM(IOS_FAMILY)
    m_cachedTextLegibilityScales.reset();
#else
    sendDidHitRenderTreeSizeThresholdIfNeeded();
#endif
}

} // namespace WebKit

