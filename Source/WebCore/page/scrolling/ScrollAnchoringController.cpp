/*
* Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "ScrollAnchoringController.h"
#include "ElementChildIteratorInlines.h"
#include "ElementIterator.h"
#include "HTMLHtmlElement.h"
#include "Logging.h"
#include "RenderBox.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

void ScrollAnchoringController::invalidateAnchorElement()
{
    if (!m_midUpdatingScrollPositionForAnchorElement) {
        m_anchorElement = nullptr;
        m_lastOffsetForAnchorElement = { };
        m_isQueuedForScrollPositionUpdate = false;
        m_frameView.queueScrollableAreaForScrollAnchoringUpdate(m_frameView);
    }
}

ScrollOffset ScrollAnchoringController::computeOffset(RenderObject& candidate)
{
    // TODO: investigate this for zoom/rtl
    // make sure this works for subscrollers
    return ScrollOffset(FloatPoint(candidate.absoluteBoundingBoxRect().location() - m_frameView.layoutViewportRect().location()));
}

static RefPtr<Element> anchorElementForPriorityCandidate(Element* element)
{
    while (element) {
        if (auto renderer = element->renderer()) {
            if (!renderer->isAnonymousBlock() && (!renderer->isInline() || renderer->isAtomicInlineLevelBox()))
                return element;
        }
        element = element->parentElement();
    }
    return nullptr;
}

bool ScrollAnchoringController::didFindPriorityCandidate(Document& document)
{
    auto viablePriorityCandidateForElement = [](Element* element) -> RefPtr<Element> {
        RefPtr candidateElement = anchorElementForPriorityCandidate(element);
        if (!candidateElement)
            return nullptr;

        LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::viablePriorityCandidateForElement()");
        RefPtr iterElement = candidateElement;

        // TODO: iterate up to owning scroller
        while (iterElement) {
            if (auto renderer = element->renderer()) {
                if (renderer->style().overflowAnchor() == OverflowAnchor::None)
                    return nullptr;
            }
            iterElement = iterElement->parentElement();
        }
        return iterElement;
    };

    // TODO: need to check if focused element is text editable
    // TODO: need to figure out how to get element that is the current find-in-page element (look into FindController)
    if (RefPtr priorityCandidate = viablePriorityCandidateForElement(document.focusedElement())) {
        m_anchorElement = priorityCandidate;
        m_lastOffsetForAnchorElement = computeOffset(*m_anchorElement->renderer());
        return true;
    }
    return false;
}

CandidateExaminationResult ScrollAnchoringController::examineCandidate(Element& element)
{
    auto layoutViewport = m_frameView.layoutViewportRect();
    auto* document = m_frameView.frame().document();

    if (auto renderer = element.renderer()) {
        // TODO: we need to think about position: absolute
        // TODO: figure out how to get scrollable area for renderer to check if it is maintaining scroll anchor
        if (renderer->style().overflowAnchor() == OverflowAnchor::None || renderer->isStickilyPositioned() || renderer->isFixedPositioned() || renderer->isPseudoElement() || renderer->isAnonymousBlock())
            return CandidateExaminationResult::Exclude;
        if (&element == document->bodyOrFrameset() || is<HTMLHtmlElement>(&element) || (renderer->isInline() && !renderer->isAtomicInlineLevelBox()))
            return CandidateExaminationResult::Skip;
        auto boxRect = renderer->absoluteBoundingBoxRect();
        if (!boxRect.width() || !boxRect.height())
            return CandidateExaminationResult::Skip;
        if (layoutViewport.contains(boxRect))
            return CandidateExaminationResult::Select;
        if (layoutViewport.intersects(boxRect))
            return CandidateExaminationResult::Descend;
    }
    return CandidateExaminationResult::Skip;
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, CandidateExaminationResult result)
{
    switch (result) {
    case CandidateExaminationResult::Exclude:
        ts << "Exclude";
        break;
    case CandidateExaminationResult::Select:
        ts << "Select";
        break;
    case CandidateExaminationResult::Descend:
        ts << "Descend";
        break;
    case CandidateExaminationResult::Skip:
        ts << "Skip";
        break;
    }
    return ts;
}
#endif

Element* ScrollAnchoringController::findAnchorElementRecursive(Element* element)
{
    if (!element)
        return nullptr;

    auto result = examineCandidate(*element);
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::findAnchorElementRecursive() element: "<< *element<<" examination result: " << result);

    switch (result) {
    case CandidateExaminationResult::Select:
        return element;
    case CandidateExaminationResult::Exclude:
        return nullptr;
    case CandidateExaminationResult::Skip:
    case CandidateExaminationResult::Descend: {
        for (auto& child : childrenOfType<Element>(*element)) {
            if (auto* anchorElement = findAnchorElementRecursive(&child))
                return anchorElement;
        }
        break;
    }
    }
    if (result == CandidateExaminationResult::Skip)
        return nullptr;
    return element;
}

void ScrollAnchoringController::chooseAnchorElement(Document& document)
{
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::chooseAnchorElement() starting findAnchorElementRecursive: ");

    if (didFindPriorityCandidate(document))
        return;

    RefPtr<Element> anchorElement;

    // TODO: iterate from owning scroller not root
    if (!m_anchorElement) {
        anchorElement = findAnchorElementRecursive(document.documentElement());
        if (!anchorElement)
            return;
    }

    m_anchorElement = anchorElement;
    m_lastOffsetForAnchorElement = computeOffset(*m_anchorElement->renderer());
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::chooseAnchorElement() found anchor node: " << *anchorElement << " offset: " << computeOffset(*m_anchorElement->renderer()));
}

void ScrollAnchoringController::updateAnchorElement()
{
    // TODO: check owning scroller scroll position
    if (frameView().scrollPosition().isZero() || m_isQueuedForScrollPositionUpdate)
        return;

    RefPtr document = m_frameView.frame().document();
    if (!document)
        return;

    if (m_anchorElement && !m_anchorElement->renderer())
        invalidateAnchorElement();

    if (!m_anchorElement) {
        chooseAnchorElement(*document);
        if (!m_anchorElement)
            return;
    }
    m_isQueuedForScrollPositionUpdate = true;
    m_frameView.queueScrollableAreaForScrollAnchoringUpdate(m_frameView);
}

void ScrollAnchoringController::adjustScrollPositionForAnchoring()
{
    SetForScope midUpdatingScrollPositionForAnchorElement(m_midUpdatingScrollPositionForAnchorElement, true);
    auto queued = std::exchange(m_isQueuedForScrollPositionUpdate, false);
    if (!m_anchorElement || !queued)
        return;
    auto renderBox = m_anchorElement->renderer();
    if (!renderBox) {
        invalidateAnchorElement();
        return;
    }
    FloatSize adjustment = computeOffset(*renderBox) - m_lastOffsetForAnchorElement;
    if (!adjustment.isZero()) {
        auto newScrollPosition = frameView().scrollPosition() + IntPoint(adjustment.width(), adjustment.height());
        LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::updateScrollPosition() for frame: " << m_frameView << " adjusting from: " << frameView().scrollPosition() << " to: " << newScrollPosition);
        auto options = ScrollPositionChangeOptions::createProgrammatic();
        options.originalScrollDelta = adjustment;
        m_frameView.setScrollPosition(newScrollPosition, options);
    }
}

} // namespace WebCore
