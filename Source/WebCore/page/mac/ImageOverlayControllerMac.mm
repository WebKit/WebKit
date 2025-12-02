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
#import "ImageOverlayController.h"

#if PLATFORM(MAC)

#import "Chrome.h"
#import "ChromeClient.h"
#import "ContainerNodeInlines.h"
#import "DataDetection.h"
#import "DataDetectionResultsStorage.h"
#import "DataDetectorElementInfo.h"
#import "DocumentView.h"
#import "ElementInlines.h"
#import "FrameDestructionObserverInlines.h"
#import "GraphicsLayer.h"
#import "GraphicsLayerClient.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "ImageOverlay.h"
#import "ImageOverlayDataDetectionResultIdentifier.h"
#import "IntRect.h"
#import "LocalFrameInlines.h"
#import "LocalFrameView.h"
#import "Page.h"
#import "PlatformMouseEvent.h"
#import "ShadowRoot.h"
#import "SimpleRange.h"
#import "TypedElementDescendantIteratorInlines.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/HashSet.h>
#import <wtf/OptionSet.h>
#import <wtf/RefPtr.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <pal/mac/DataDetectorsSoftLink.h>

namespace WebCore {

void ImageOverlayController::updateDataDetectorHighlights(const HTMLElement& overlayHost)
{
    if (!ImageOverlay::hasOverlay(overlayHost)) {
        ASSERT_NOT_REACHED();
        clearDataDetectorHighlights();
        return;
    }

    Vector<Ref<HTMLElement>> dataDetectorResultElements;
    for (auto& child : descendantsOfType<HTMLElement>(*overlayHost.protectedUserAgentShadowRoot())) {
        if (ImageOverlay::isDataDetectorResult(child) && child.renderer())
            dataDetectorResultElements.append(child);
    }

    HashSet<Ref<HTMLElement>> dataDetectorResultElementsWithHighlights;
    for (auto& containerAndHighlight : m_dataDetectorContainersAndHighlights) {
        if (containerAndHighlight.first)
            dataDetectorResultElementsWithHighlights.add(*containerAndHighlight.first);
    }

    if (dataDetectorResultElementsWithHighlights == dataDetectorResultElements)
        return;

    RefPtr mainFrameView = m_page->mainFrame().virtualView();
    if (!mainFrameView)
        return;

    RefPtr frameView = overlayHost.document().view();
    if (!frameView)
        return;

    m_activeDataDetectorHighlight = nullptr;
    m_dataDetectorContainersAndHighlights = WTF::map(dataDetectorResultElements, [&](auto& element) {
        CGRect elementBounds = element->renderer()->absoluteBoundingBoxRect();
        elementBounds.origin = mainFrameView->windowToContents(frameView->contentsToWindow(roundedIntPoint(elementBounds.origin)));

        // FIXME: We should teach DataDetectorHighlight to render quads instead of always falling back to axis-aligned bounding rects.
        auto highlight = adoptCF(PAL::softLink_DataDetectors_DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection(nullptr, &elementBounds, 1, mainFrameView->visibleContentRect(), static_cast<DDHighlightStyle>(DDHighlightStyleBubbleStandard) | static_cast<DDHighlightStyle>(DDHighlightStyleStandardIconArrow), YES, NSWritingDirectionNatural, NO, YES, 0));
        return ContainerAndHighlight { element, DataDetectorHighlight::createForImageOverlay(*this, WTFMove(highlight), *makeRangeSelectingNode(element.get())) };
    });
}

bool ImageOverlayController::platformHandleMouseEvent(const PlatformMouseEvent& event)
{
    RefPtr mainFrameView = m_page->mainFrame().virtualView();
    if (!mainFrameView)
        return false;

    RefPtr<HTMLElement> activeDataDetectorElement;
    auto previousActiveHighlight = std::exchange(m_activeDataDetectorHighlight, nullptr);
    auto mousePositionInContents = mainFrameView->windowToContents(event.position());
    bool mouseIsOverActiveDataDetectorHighlightButton = false;

    for (auto& [element, highlight] : m_dataDetectorContainersAndHighlights) {
        if (!element)
            continue;

        Boolean isOverButton = NO;
        if (!PAL::softLink_DataDetectors_DDHighlightPointIsOnHighlight(highlight->highlight(), mousePositionInContents, &isOverButton))
            continue;

        mouseIsOverActiveDataDetectorHighlightButton = isOverButton;
        m_activeDataDetectorHighlight = highlight.copyRef();
        activeDataDetectorElement = element.get();
        break;
    }

    if (previousActiveHighlight != m_activeDataDetectorHighlight) {
        if (previousActiveHighlight)
            previousActiveHighlight->fadeOut();

        if (m_activeDataDetectorHighlight) {
            m_overlay->layer().addChild(m_activeDataDetectorHighlight->layer());
            m_activeDataDetectorHighlight->fadeIn();
        }
    }

    if (event.type() == PlatformEvent::Type::MousePressed && mouseIsOverActiveDataDetectorHighlightButton)
        return handleDataDetectorAction(*activeDataDetectorElement, flooredIntPoint(mousePositionInContents));

    return false;
}

bool ImageOverlayController::handleDataDetectorAction(const HTMLElement& element, const IntPoint& locationInContents)
{
    RefPtr frame = element.document().frame();
    if (!frame)
        return false;

    RefPtr frameView = element.document().view();
    if (!frameView)
        return false;

    auto identifierValue = parseInteger<uint64_t>(element.attributeWithoutSynchronization(HTMLNames::x_apple_data_detectors_resultAttr));
    if (!identifierValue || !*identifierValue)
        return false;

    auto identifier = ObjectIdentifier<ImageOverlayDataDetectionResultIdentifierType>(*identifierValue);

    auto* dataDetectionResults = frame->dataDetectionResultsIfExists();
    if (!dataDetectionResults)
        return false;

    auto dataDetectionResult = retainPtr(dataDetectionResults->imageOverlayDataDetectionResult(identifier));
    if (!dataDetectionResult)
        return false;

    auto* renderer = element.renderer();
    if (!renderer)
        return false;

    protectedPage()->chrome().client().handleClickForDataDetectionResult({ WTFMove(dataDetectionResult), frameView->contentsToWindow(renderer->absoluteBoundingBoxRect()) }, frameView->contentsToWindow(locationInContents));
    return true;
}

void ImageOverlayController::clearDataDetectorHighlights()
{
    m_hostElementForDataDetectors = nullptr;
    m_dataDetectorContainersAndHighlights.clear();
    m_activeDataDetectorHighlight = nullptr;
}

void ImageOverlayController::textRecognitionResultsChanged(HTMLElement& element)
{
    if (m_hostElementForDataDetectors != &element)
        return;

    clearDataDetectorHighlights();
    uninstallPageOverlayIfNeeded();
}

bool ImageOverlayController::hasActiveDataDetectorHighlightForTesting() const
{
    return !!m_activeDataDetectorHighlight;
}

void ImageOverlayController::elementUnderMouseDidChange(LocalFrame& frame, Element* elementUnderMouse)
{
    if (m_activeDataDetectorHighlight)
        return;

    if (!elementUnderMouse && m_hostElementForDataDetectors && frame.document() != &m_hostElementForDataDetectors->document())
        return;

    if (!elementUnderMouse || !ImageOverlay::isInsideOverlay(*elementUnderMouse)) {
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    RefPtr imageOverlayHost = dynamicDowncast<HTMLElement>(elementUnderMouse->shadowHost());
    if (!imageOverlayHost) {
        ASSERT_NOT_REACHED();
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    if (!ImageOverlay::hasOverlay(*imageOverlayHost)) {
        ASSERT_NOT_REACHED();
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    if (m_hostElementForDataDetectors == imageOverlayHost.get())
        return;

    updateDataDetectorHighlights(*imageOverlayHost);

    if (m_dataDetectorContainersAndHighlights.isEmpty()) {
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    m_hostElementForDataDetectors = imageOverlayHost.releaseNonNull();
    installPageOverlayIfNeeded();
}

#pragma mark - DataDetectorHighlightClient

#if ENABLE(DATA_DETECTION)

void ImageOverlayController::scheduleRenderingUpdate(OptionSet<RenderingUpdateStep> requestedSteps)
{
    protectedPage()->scheduleRenderingUpdate(requestedSteps);
}

float ImageOverlayController::deviceScaleFactor() const
{
    return protectedPage()->deviceScaleFactor();
}

RefPtr<GraphicsLayer> ImageOverlayController::createGraphicsLayer(GraphicsLayerClient& client)
{
    return GraphicsLayer::create(protectedPage()->chrome().client().graphicsLayerFactory(), client);
}

#endif

} // namespace WebCore

#endif // PLATFORM(MAC)
