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
#include "LocalFrameView.h"
#include "Logging.h"
#include "RenderBox.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderView.h"
#include "TypedElementDescendantIteratorInlines.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

ScrollAnchoringController::ScrollAnchoringController(ScrollableArea& owningScroller)
    : m_owningScrollableArea(owningScroller)
{ }

ScrollAnchoringController::~ScrollAnchoringController()
{
    invalidateAnchorElement();
}

LocalFrameView& ScrollAnchoringController::frameView()
{
    if (is<RenderLayerScrollableArea>(m_owningScrollableArea))
        return downcast<RenderLayerScrollableArea>(m_owningScrollableArea).layer().renderer().view().frameView();
    return downcast<LocalFrameView>(downcast<ScrollView>(m_owningScrollableArea));
}

static bool elementIsScrollableArea(const Element& element, const ScrollableArea& scrollableArea)
{
    return element.renderBox() && element.renderBox()->layer() && element.renderBox()->layer()->scrollableArea() == &scrollableArea;
}

static Element* elementForScrollableArea(ScrollableArea& scrollableArea)
{
    if (is<RenderLayerScrollableArea>(scrollableArea))
        return downcast<RenderLayerScrollableArea>(scrollableArea).layer().renderer().element();
    if (auto* document = downcast<LocalFrameView>(downcast<ScrollView>(scrollableArea)).frame().document())
        return document->documentElement();
    return nullptr;
}

void ScrollAnchoringController::invalidateAnchorElement()
{
    if (m_midUpdatingScrollPositionForAnchorElement)
        return;
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::invalidateAnchorElement() invalidating anchor for frame: " << frameView() << " for scroller: " << m_owningScrollableArea);
    if (!m_anchorElement) {
        if (auto* element = elementForScrollableArea(m_owningScrollableArea)) {
            if (auto* renderer = element->renderer()) {
                auto* scrollAnchoringControllerForScrollableArea = RenderObject::searchParentChainForScrollAnchoringController(*renderer);
                if (scrollAnchoringControllerForScrollableArea && scrollAnchoringControllerForScrollableArea->isInScrollAnchoringAncestorChain(*renderer))
                    scrollAnchoringControllerForScrollableArea->invalidateAnchorElement();
            }
        }
    }
    m_anchorElement = nullptr;
    m_lastOffsetForAnchorElement = { };
    m_isQueuedForScrollPositionUpdate = false;
    frameView().dequeueScrollableAreaForScrollAnchoringUpdate(m_owningScrollableArea);
}

static IntRect boundingRectForScrollableArea(ScrollableArea& scrollableArea)
{
    if (is<RenderLayerScrollableArea>(scrollableArea))
        return downcast<RenderLayerScrollableArea>(scrollableArea).layer().renderer().absoluteBoundingBoxRect();

    return IntRect(downcast<LocalFrameView>(downcast<ScrollView>(scrollableArea)).layoutViewportRect());
}

FloatPoint ScrollAnchoringController::computeOffsetFromOwningScroller(RenderObject& candidate)
{
    // TODO: investigate this for zoom/rtl
    return FloatPoint(candidate.absoluteBoundingBoxRect().location() - boundingRectForScrollableArea(m_owningScrollableArea).location());
}

void ScrollAnchoringController::notifyChildHadSuppressingStyleChange()
{
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::notifyChildHadSuppressingStyleChange() for scroller: " << m_owningScrollableArea);

    m_shouldSuppressScrollPositionUpdate = true;
}

bool ScrollAnchoringController::isInScrollAnchoringAncestorChain(const RenderObject& object)
{
    RefPtr iterElement = m_anchorElement.get();
    while (iterElement) {
        if (auto* renderer = iterElement->renderer()) {
            LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::isInScrollAnchoringAncestorChain() checking for : " <<object << " current Element: " << *iterElement);
            if (&object == renderer)
                return true;
        }
        if (iterElement && elementIsScrollableArea(*iterElement, m_owningScrollableArea))
            break;
        iterElement = iterElement->parentElementInComposedTree();
    }
    return false;
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

static ScrollAnchoringController* scrollAnchoringControllerForElement(Element& element)
{
    if (auto renderer = element.renderer()) {
        if (renderer->hasLayer()) {
            if (auto layer = downcast<RenderLayerModelObject>(*renderer).layer()) {
                if (auto scrollableArea = layer->scrollableArea())
                    return scrollableArea->scrollAnchoringController();
            }
        }
    }
    return nullptr;
}

// This function ensures that each element in the chain from the priorityCandidateElement to the parentController are viable according to
// ScrollAnchoringController::examineAnchorCandidate and that none of them are maintaining an anchor element.
static bool canIncludeElementInPriorityCandidateChain(Element& element, Element& priorityCandidateElement, ScrollAnchoringController& parentController)
{
    auto candidateResult = parentController.examineAnchorCandidate(element);
    auto elementsController = scrollAnchoringControllerForElement(element);
    return !(candidateResult == CandidateExaminationResult::Exclude || (&element == &priorityCandidateElement && candidateResult == CandidateExaminationResult::Skip) || (elementsController && elementsController->anchorElement()));
}

bool ScrollAnchoringController::didFindPriorityCandidate(Document& document)
{
    auto viablePriorityCandidateForElement = [this](Element* element) -> RefPtr<Element> {
        RefPtr candidateElement = anchorElementForPriorityCandidate(element);
        if (!candidateElement || candidateElement == elementForScrollableArea(m_owningScrollableArea))
            return nullptr;

        RefPtr iterElement = candidateElement;

        while (iterElement && iterElement.get() != elementForScrollableArea(m_owningScrollableArea)) {
            if (!canIncludeElementInPriorityCandidateChain(*iterElement, *candidateElement, *this))
                return nullptr;
            iterElement = iterElement->parentElement();
        }

        // Ensure that candidateElement is a child of m_owningScrollableArea
        if (iterElement.get() != elementForScrollableArea(m_owningScrollableArea))
            return nullptr;
        return candidateElement;
    };

    // TODO: need to check if focused element is text editable
    // TODO: need to figure out how to get element that is the current find-in-page element (look into FindController)
    if (RefPtr priorityCandidate = viablePriorityCandidateForElement(document.focusedElement())) {
        m_anchorElement = priorityCandidate;
        m_lastOffsetForAnchorElement = computeOffsetFromOwningScroller(*m_anchorElement->renderer());
        LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::viablePriorityCandidateForElement() found priority candidate: " << *priorityCandidate << " for element: " << ValueOrNull(elementForScrollableArea(m_owningScrollableArea)));
        return true;
    }
    return false;
}

static bool absolutePositionedElementOutsideScroller(RenderElement& renderer, ScrollableArea& scroller)
{
    if (is<RenderLayerScrollableArea>(scroller) && renderer.hasLayer()) {
        if (auto* layerForRenderer = downcast<RenderLayerModelObject>(renderer).layer())
            return !layerForRenderer->ancestorLayerIsInContainingBlockChain(downcast<RenderLayerScrollableArea>(scroller).layer());
    }
    return false;
}

static bool canDescendIntoElement(Element& element)
{
    if (auto* scrollAnchoringController = scrollAnchoringControllerForElement(element))
        return !scrollAnchoringController->anchorElement();
    return false;
}

CandidateExaminationResult ScrollAnchoringController::examineAnchorCandidate(Element& element)
{
    if (elementForScrollableArea(m_owningScrollableArea) && elementForScrollableArea(m_owningScrollableArea)->identifier() == element.identifier())
        return CandidateExaminationResult::Skip;

    auto containingRect = boundingRectForScrollableArea(m_owningScrollableArea);
    auto* document = frameView().frame().document();

    if (auto* element = elementForScrollableArea(m_owningScrollableArea)) {
        if (auto* box = element->renderBox()) {
            LayoutRect paddedLayerBounds(containingRect);
            paddedLayerBounds.contract(box->scrollPaddingForViewportRect(paddedLayerBounds));
            LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::examineAnchorCandidate() contracted rect: "<< IntRect(paddedLayerBounds));
            containingRect = IntRect(paddedLayerBounds);
        }
    }

    auto isExcludedSubtree = [this](RenderElement* renderer, bool intersects) {
        return renderer->style().overflowAnchor() == OverflowAnchor::None || renderer->isStickilyPositioned() || renderer->isFixedPositioned() || renderer->isPseudoElement() || renderer->isAnonymousBlock() || (renderer->isAbsolutelyPositioned() && absolutePositionedElementOutsideScroller(*renderer, m_owningScrollableArea)) || (!intersects && renderer->style().containsPaint());
    };

    if (auto renderer = element.renderer()) {
        // TODO: figure out how to get scrollable area for renderer to check if it is maintaining scroll anchor
        auto boxRect = renderer->absoluteBoundingBoxRect();
        bool intersects = containingRect.intersects(boxRect);

        if (isExcludedSubtree(renderer, intersects))
            return CandidateExaminationResult::Exclude;
        if (&element == document->bodyOrFrameset() || is<HTMLHtmlElement>(&element) || (renderer->isInline() && !renderer->isAtomicInlineLevelBox()))
            return CandidateExaminationResult::Skip;
        if (!boxRect.width() || !boxRect.height())
            return CandidateExaminationResult::Skip;
        if (containingRect.contains(boxRect))
            return CandidateExaminationResult::Select;
        auto isScrollingNode = false;
        if (auto* renderBox = dynamicDowncast<RenderBox>(renderer))
            isScrollingNode = renderBox->hasPotentiallyScrollableOverflow();
        if (intersects)
            return !isScrollingNode || canDescendIntoElement(element) ? CandidateExaminationResult::Descend : CandidateExaminationResult::Select;
        if (isScrollingNode)
            return CandidateExaminationResult::Exclude;
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

    auto result = examineAnchorCandidate(*element);
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
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::chooseAnchorElement() starting findAnchorElementRecursive: for element: " << ValueOrNull(elementForScrollableArea(m_owningScrollableArea)));

    if (didFindPriorityCandidate(document))
        return;

    RefPtr<Element> anchorElement;

    if (!m_anchorElement) {
        anchorElement = findAnchorElementRecursive(elementForScrollableArea(m_owningScrollableArea));
        if (!anchorElement)
            return;
    }

    m_anchorElement = anchorElement;
    m_lastOffsetForAnchorElement = computeOffsetFromOwningScroller(*m_anchorElement->renderer());
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::chooseAnchorElement() found anchor node: " << *anchorElement << " offset: " << computeOffsetFromOwningScroller(*m_anchorElement->renderer()));
}

void ScrollAnchoringController::updateAnchorElement()
{
    if (m_owningScrollableArea.scrollPosition().isZero() || m_isQueuedForScrollPositionUpdate || frameView().layoutContext().layoutPhase() != LocalFrameViewLayoutContext::LayoutPhase::OutsideLayout)
        return;

    RefPtr document = frameView().frame().document();
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
    frameView().queueScrollableAreaForScrollAnchoringUpdate(m_owningScrollableArea);
}

void ScrollAnchoringController::adjustScrollPositionForAnchoring()
{
    auto queued = std::exchange(m_isQueuedForScrollPositionUpdate, false);
    auto suppressed = std::exchange(m_shouldSuppressScrollPositionUpdate, false);
    if (!m_anchorElement || !queued)
        return;
    auto* renderer = m_anchorElement->renderer();
    if (!renderer || suppressed) {
        invalidateAnchorElement();
        updateAnchorElement();
        if (suppressed)
            LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::updateScrollPosition() suppressing scroll adjustment for frame: " << frameView() << " for scroller: " << m_owningScrollableArea);
        return;
    }
    SetForScope midUpdatingScrollPositionForAnchorElement(m_midUpdatingScrollPositionForAnchorElement, true);

    FloatSize adjustment = computeOffsetFromOwningScroller(*renderer) - m_lastOffsetForAnchorElement;
    if (!adjustment.isZero()) {
        if (m_owningScrollableArea.isUserScrollInProgress()) {
            invalidateAnchorElement();
            updateAnchorElement();
            return;
        }
        auto newScrollPosition = m_owningScrollableArea.scrollPosition() + IntPoint(adjustment.width(), adjustment.height());
        RELEASE_LOG(ScrollAnchoring, "ScrollAnchoringController::updateScrollPosition() is main frame: %d, is main scroller: %d, adjusting from: (%d, %d) to: (%d, %d)",  frameView().frame().isMainFrame(), !m_owningScrollableArea.isRenderLayer(), m_owningScrollableArea.scrollPosition().x(), m_owningScrollableArea.scrollPosition().y(), newScrollPosition.x(), newScrollPosition.y());
        LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::updateScrollPosition() for scroller element: " << ValueOrNull(elementForScrollableArea(m_owningScrollableArea)) << " anchor node: " << *m_anchorElement << "adjusting from: " << m_owningScrollableArea.scrollPosition() << " to: " << newScrollPosition);

        auto options = ScrollPositionChangeOptions::createProgrammatic();
        options.originalScrollDelta = adjustment;
        auto oldScrollType = m_owningScrollableArea.currentScrollType();
        m_owningScrollableArea.setCurrentScrollType(ScrollType::Programmatic);
        if (!m_owningScrollableArea.requestScrollToPosition(newScrollPosition, options))
            m_owningScrollableArea.scrollToPositionWithoutAnimation(newScrollPosition);
        m_owningScrollableArea.setCurrentScrollType(oldScrollType);
    }
}

} // namespace WebCore
