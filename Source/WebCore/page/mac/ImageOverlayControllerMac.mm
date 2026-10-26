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
    for (Ref child : descendantsOfType<HTMLElement>(*overlayHost.userAgentShadowRoot())) {
        if (ImageOverlay::isDataDetectorResult(child) && child->renderer())
            dataDetectorResultElements.append(child);
    }

    HashSet<Ref<HTMLElement>> dataDetectorResultElementsWithHighlights;
    for (auto& containerAndHighlight : m_dataDetectorContainersAndHighlights) {
        if (containerAndHighlight.first)
            dataDetectorResultElementsWithHighlights.add(*containerAndHighlight.first);
    }

    if (dataDetectorResultElementsWithHighlights == dataDetectorResultElements)
        return;

    RefPtr rootFrame { overlayHost.document().rootFrame() };
    RefPtr rootView { rootFrame ? rootFrame->view() : nullptr };
    if (!rootView)
        return;

    RefPtr frameView = overlayHost.document().view();
    if (!frameView)
        return;

    m_activeDataDetectorHighlight = nullptr;
    m_dataDetectorContainersAndHighlights = WTF::map(dataDetectorResultElements, [&](auto& element) {
        CGRect elementBounds = protect(element->renderer())->absoluteBoundingBoxRect();
        elementBounds.origin = rootView->windowToContents(frameView->contentsToWindow(roundedIntPoint(elementBounds.origin)));

        // FIXME: We should teach DataDetectorHighlight to render quads instead of always falling back to axis-aligned bounding rects.
        RetainPtr highlight { adoptCF(PAL::softLink_DataDetectors_DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection(nullptr, &elementBounds, 1, DataDetection::visibleContentRectForHighlights(*rootView), static_cast<DDHighlightStyle>(DDHighlightStyleBubbleStandard) | static_cast<DDHighlightStyle>(DDHighlightStyleStandardIconArrow), YES, NSWritingDirectionNatural, NO, YES, 0)) };
        return ContainerAndHighlight { element, DataDetectorHighlight::createForImageOverlay(*this, WTF::move(highlight), *makeRangeSelectingNode(element.get())) };
    });
}

bool ImageOverlayController::platformHandleMouseEvent(PageOverlay& overlay, const PlatformMouseEvent& event)
{
    RefPtr rootFrame { overlay.associatedFrame() };
    RefPtr rootView { rootFrame ? rootFrame->view() : nullptr };
    if (!rootView || rootFrame != rootFrameForHost(m_hostElementForDataDetectors))
        return false;

    RefPtr<HTMLElement> activeDataDetectorElement;
    auto previousActiveHighlight = std::exchange(m_activeDataDetectorHighlight, nullptr);
    auto mousePositionInContents = rootView->windowToContents(event.position());
    bool mouseIsOverActiveDataDetectorHighlightButton = false;

    for (auto& [element, highlight] : m_dataDetectorContainersAndHighlights) {
        if (!element)
            continue;

        Boolean isOverButton = NO;
        if (!PAL::softLink_DataDetectors_DDHighlightPointIsOnHighlight(protect(highlight->highlight()), mousePositionInContents, &isOverButton))
            continue;

        mouseIsOverActiveDataDetectorHighlightButton = isOverButton;
        m_activeDataDetectorHighlight = highlight.copyRef();
        activeDataDetectorElement = element.get();
        break;
    }

    if (previousActiveHighlight != m_activeDataDetectorHighlight) {
        if (previousActiveHighlight)
            previousActiveHighlight->fadeOut();

        if (RefPtr activeDataDetectorHighlight = m_activeDataDetectorHighlight) {
            protect(overlay.layer())->addChild(activeDataDetectorHighlight->layer());
            activeDataDetectorHighlight->fadeIn();
        }
    }

    if (event.type() == PlatformEvent::Type::MousePressed && mouseIsOverActiveDataDetectorHighlightButton)
        return handleDataDetectorAction(*activeDataDetectorElement, flooredIntPoint(event.position()));

    return false;
}

bool ImageOverlayController::handleDataDetectorAction(const HTMLElement& element, const IntPoint& locationInWindow)
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

    CheckedPtr renderer = element.renderer();
    if (!renderer)
        return false;

    m_page->chrome().client().handleClickForDataDetectionResult(frame->frameID(), { WTF::move(dataDetectionResult), frameView->contentsToMainFrameView(renderer->absoluteBoundingBoxRect()) }, frameView->contentsToMainFrameView(frameView->windowToContents(locationInWindow)));
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
    updateOverlays();
}

bool LEGACY_NODELETE ImageOverlayController::hasActiveDataDetectorHighlightForTesting() const
{
    return !!m_activeDataDetectorHighlight;
}

void ImageOverlayController::elementUnderMouseDidChange(LocalFrame& frame, Element* elementUnderMouse)
{
    Ref rootFrame { frame.rootFrame() };
    RefPtr hostRootFrame { rootFrameForHost(m_hostElementForDataDetectors) };

    if (m_activeDataDetectorHighlight && hostRootFrame && rootFrame->tree().isDescendantOf(hostRootFrame.get()))
        return;

    if (!elementUnderMouse && m_hostElementForDataDetectors && frame.document() != &m_hostElementForDataDetectors->document())
        return;

    if (!elementUnderMouse || !ImageOverlay::isInsideOverlay(*elementUnderMouse)) {
        if (hostRootFrame && hostRootFrame != rootFrame.ptr() && hostRootFrame->tree().isDescendantOf(rootFrame.ptr()))
            return;

        m_hostElementForDataDetectors = nullptr;
        updateOverlays();
        return;
    }

    RefPtr imageOverlayHost = dynamicDowncast<HTMLElement>(elementUnderMouse->shadowHost());
    if (!imageOverlayHost) {
        ASSERT_NOT_REACHED();
        m_hostElementForDataDetectors = nullptr;
        updateOverlays();
        return;
    }

    if (!ImageOverlay::hasOverlay(*imageOverlayHost)) {
        ASSERT_NOT_REACHED();
        m_hostElementForDataDetectors = nullptr;
        updateOverlays();
        return;
    }

    if (m_hostElementForDataDetectors == imageOverlayHost.get())
        return;

    updateDataDetectorHighlights(*imageOverlayHost);

    if (m_dataDetectorContainersAndHighlights.isEmpty()) {
        m_hostElementForDataDetectors = nullptr;
        updateOverlays();
        return;
    }

    m_hostElementForDataDetectors = imageOverlayHost.releaseNonNull();
    updateOverlays();
}

void ImageOverlayController::mouseDidLeaveLocalRoot(LocalFrame& rootFrame)
{
    if (rootFrameForHost(m_hostElementForDataDetectors) != &rootFrame)
        return;

    clearDataDetectorHighlights();
    updateOverlays();
}

#pragma mark - DataDetectorHighlightClient

#if ENABLE(DATA_DETECTION)

void ImageOverlayController::scheduleRenderingUpdate(OptionSet<RenderingUpdateStep> requestedSteps)
{
    protect(m_page)->scheduleRenderingUpdate(requestedSteps);
}

float ImageOverlayController::deviceScaleFactor() const
{
    return m_page->deviceScaleFactor();
}

RefPtr<GraphicsLayer> ImageOverlayController::createGraphicsLayer(GraphicsLayerClient& client)
{
    return GraphicsLayer::create(m_page->chrome().client().graphicsLayerFactory(), client);
}

#endif

} // namespace WebCore

#endif // PLATFORM(MAC)
