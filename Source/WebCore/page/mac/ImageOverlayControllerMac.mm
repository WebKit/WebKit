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

#import "DataDetection.h"
#import "DataDetectionResultsStorage.h"
#import "DataDetectorElementInfo.h"
#import "FrameView.h"
#import "HTMLElement.h"
#import "HTMLNames.h"
#import "ImageOverlayDataDetectionResultIdentifier.h"
#import "IntRect.h"
#import "Page.h"
#import "PlatformMouseEvent.h"
#import "SimpleRange.h"
#import "TypedElementDescendantIterator.h"
#import <QuartzCore/QuartzCore.h>
#import <wtf/HashSet.h>
#import <wtf/text/StringToIntegerConversion.h>
#import <pal/mac/DataDetectorsSoftLink.h>

namespace WebCore {

void ImageOverlayController::updateDataDetectorHighlights(const HTMLElement& overlayHost)
{
    if (!overlayHost.hasImageOverlay()) {
        ASSERT_NOT_REACHED();
        clearDataDetectorHighlights();
        return;
    }

    Vector<Ref<HTMLElement>> dataDetectorResultElements;
    for (auto& child : descendantsOfType<HTMLElement>(*overlayHost.userAgentShadowRoot())) {
        if (child.isImageOverlayDataDetectorResult() && child.renderer())
            dataDetectorResultElements.append(makeRef(child));
    }

    HashSet<Ref<HTMLElement>> dataDetectorResultElementsWithHighlights;
    for (auto& containerAndHighlight : m_dataDetectorContainersAndHighlights) {
        if (containerAndHighlight.first)
            dataDetectorResultElementsWithHighlights.add(*containerAndHighlight.first);
    }

    if (dataDetectorResultElementsWithHighlights == dataDetectorResultElements)
        return;

    auto mainFrameView = makeRefPtr(m_page->mainFrame().view());
    if (!mainFrameView)
        return;

    auto frameView = makeRefPtr(overlayHost.document().view());
    if (!frameView)
        return;

    m_activeDataDetectorHighlight = nullptr;
    m_dataDetectorContainersAndHighlights.clear();
    m_dataDetectorContainersAndHighlights.reserveInitialCapacity(dataDetectorResultElements.size());

    for (auto& element : dataDetectorResultElements) {
        CGRect elementBounds = element->renderer()->absoluteBoundingBoxRect();
        elementBounds.origin = mainFrameView->windowToContents(frameView->contentsToWindow(roundedIntPoint(elementBounds.origin)));

        // FIXME: We should teach DataDetectorHighlight to render quads instead of always falling back to axis-aligned bounding rects.
#if HAVE(DD_HIGHLIGHT_CREATE_WITH_SCALE)
        auto highlight = adoptCF(PAL::softLink_DataDetectors_DDHighlightCreateWithRectsInVisibleRectWithStyleScaleAndDirection(nullptr, &elementBounds, 1, mainFrameView->visibleContentRect(), DDHighlightStyleBubbleStandard | DDHighlightStyleStandardIconArrow, YES, NSWritingDirectionNatural, NO, YES, 0));
#else
        auto highlight = adoptCF(PAL::softLink_DataDetectors_DDHighlightCreateWithRectsInVisibleRectWithStyleAndDirection(nullptr, &elementBounds, 1, mainFrameView->visibleContentRect(), DDHighlightStyleBubbleStandard | DDHighlightStyleStandardIconArrow, YES, NSWritingDirectionNatural, NO, YES));
#endif
        m_dataDetectorContainersAndHighlights.append({ makeWeakPtr(element.get()), DataDetectorHighlight::createForImageOverlay(*m_page, *this, WTFMove(highlight), *makeRangeSelectingNode(element.get())) });
    }
}

bool ImageOverlayController::platformHandleMouseEvent(const PlatformMouseEvent& event)
{
    auto mainFrameView = makeRefPtr(m_page->mainFrame().view());
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
        activeDataDetectorElement = makeRefPtr(*element);
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

    if (event.type() == PlatformEvent::MousePressed && mouseIsOverActiveDataDetectorHighlightButton)
        return handleDataDetectorAction(*activeDataDetectorElement, mousePositionInContents);

    return false;
}

bool ImageOverlayController::handleDataDetectorAction(const HTMLElement& element, const IntPoint& locationInContents)
{
    if (!m_page)
        return false;

    auto frame = makeRefPtr(element.document().frame());
    if (!frame)
        return false;

    auto frameView = makeRefPtr(element.document().view());
    if (!frameView)
        return false;

    auto identifierValue = parseInteger<uint64_t>(element.attributeWithoutSynchronization(HTMLNames::x_apple_data_detectors_resultAttr));
    if (!identifierValue)
        return false;

    auto identifier = makeObjectIdentifier<ImageOverlayDataDetectionResultIdentifierType>(*identifierValue);
    if (!identifier.isValid())
        return false;

    auto* dataDetectionResults = frame->dataDetectionResultsIfExists();
    if (!dataDetectionResults)
        return false;

    auto dataDetectionResult = retainPtr(dataDetectionResults->imageOverlayDataDetectionResult(identifier));
    if (!dataDetectionResult)
        return false;

    auto* renderer = element.renderer();
    if (!renderer)
        return false;

    m_page->chrome().client().handleClickForDataDetectionResult({ WTFMove(dataDetectionResult), frameView->contentsToWindow(renderer->absoluteBoundingBoxRect()) }, frameView->contentsToWindow(locationInContents));
    return true;
}

void ImageOverlayController::clearDataDetectorHighlights()
{
    m_hostElementForDataDetectors = nullptr;
    m_dataDetectorContainersAndHighlights.clear();
    m_activeDataDetectorHighlight = nullptr;
}

void ImageOverlayController::elementUnderMouseDidChange(Frame& frame, Element* elementUnderMouse)
{
    if (m_activeDataDetectorHighlight)
        return;

    if (!elementUnderMouse && m_hostElementForDataDetectors && frame.document() != &m_hostElementForDataDetectors->document())
        return;

    if (!elementUnderMouse || !HTMLElement::isInsideImageOverlay(*elementUnderMouse)) {
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    auto shadowHost = elementUnderMouse->shadowHost();
    if (!is<HTMLElement>(shadowHost)) {
        ASSERT_NOT_REACHED();
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    auto imageOverlayHost = makeRef(downcast<HTMLElement>(*shadowHost));
    if (!imageOverlayHost->hasImageOverlay()) {
        ASSERT_NOT_REACHED();
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    if (m_hostElementForDataDetectors == imageOverlayHost.ptr())
        return;

    updateDataDetectorHighlights(imageOverlayHost.get());

    if (m_dataDetectorContainersAndHighlights.isEmpty()) {
        m_hostElementForDataDetectors = nullptr;
        uninstallPageOverlayIfNeeded();
        return;
    }

    m_hostElementForDataDetectors = makeWeakPtr(imageOverlayHost.get());
    installPageOverlayIfNeeded();
}

} // namespace WebCore

#endif // PLATFORM(MAC)
