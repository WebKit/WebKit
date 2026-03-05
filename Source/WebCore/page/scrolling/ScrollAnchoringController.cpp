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

#include "ContainerNodeInlines.h"
#include "Document.h"
#include "Editing.h"
#include "FloatRect.h"
#include "LegacyRenderSVGModelObject.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "NodeInlines.h"
#include "RenderBlockFlow.h"
#include "RenderBox.h"
#include "RenderBoxInlines.h"
#include "RenderElementInlines.h"
#include "RenderIterator.h"
#include "RenderLayerScrollableArea.h"
#include "RenderObjectInlines.h"
#include "RenderSVGModelObject.h"
#include "RenderText.h"
#include "RenderView.h"
#include <wtf/SetForScope.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ScrollAnchoringController);

ScrollAnchoringController::ScrollAnchoringController(ScrollableArea& owningScroller)
    : m_owningScrollableArea(owningScroller)
{
}

ScrollAnchoringController::~ScrollAnchoringController()
{
    invalidate();
}

bool ScrollAnchoringController::shouldMaintainScrollAnchor() const
{
    CheckedPtr scrollerBox = scrollableAreaBox();
    if (!scrollerBox)
        return false;

    // FIXME: Writing modes: only check the block direction?
    if (!scrollerBox->hasPotentiallyScrollableOverflow())
        return false;

    if (scrollerBox->style().overflowAnchor() == OverflowAnchor::None)
        return false;

    // FIXME: Writing modes: only check the block direction.
    if (!m_owningScrollableArea->scrollOffset().y())
        return false;

    return true;
}

void ScrollAnchoringController::scrollPositionDidChange()
{
    if (m_isUpdatingScrollPositionForAnchoring)
        return;

    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::scrollPositionChanged() to " << m_owningScrollableArea->scrollPosition() << " - clearing scroll anchor");
    clearAnchor();
    updateScrollableAreaRegistration();
}

void ScrollAnchoringController::scrollerDidLayout()
{
    updateScrollableAreaRegistration();
}

void ScrollAnchoringController::updateScrollableAreaRegistration()
{
    Ref frameView = this->frameView();
    if (shouldMaintainScrollAnchor())
        frameView->addScrollableAreaForScrollAnchoring(m_owningScrollableArea);
    else
        frameView->removeScrollableAreaForScrollAnchoring(m_owningScrollableArea);
}

LocalFrameView& ScrollAnchoringController::frameView() const
{
    if (CheckedPtr renderLayerScrollableArea = dynamicDowncast<RenderLayerScrollableArea>(m_owningScrollableArea.get()))
        return renderLayerScrollableArea->layer().renderer().view().frameView();

    return downcast<LocalFrameView>(downcast<ScrollView>(m_owningScrollableArea));
}

RenderBox* ScrollAnchoringController::scrollableAreaBox() const
{
    if (auto* renderLayerScrollableArea = dynamicDowncast<RenderLayerScrollableArea>(m_owningScrollableArea.get()))
        return renderLayerScrollableArea->layer().renderBox();

    if (RefPtr frameView = dynamicDowncast<LocalFrameView>(downcast<ScrollView>(m_owningScrollableArea.get())))
        return frameView->renderView();

    return nullptr;
}

void ScrollAnchoringController::clearAnchor(bool includeAncestors)
{
    if (m_isUpdatingScrollPositionForAnchoring)
        return;

    m_anchorObject = nullptr;
    m_lastAnchorOffset = { };

    if (includeAncestors) {
        if (is<ScrollView>(m_owningScrollableArea))
            return;

        CheckedPtr scrollerBox = scrollableAreaBox();
        if (!scrollerBox)
            return;

        for (CheckedPtr layer = scrollerBox->layer(); layer; layer = layer->parent()) {
            if (CheckedPtr scrollableArea = layer->scrollableArea()) {
                if (CheckedPtr controller = scrollableArea->scrollAnchoringController())
                    controller->clearAnchor(false);
            }
        }
    }
}

void ScrollAnchoringController::invalidate()
{
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " invalidateAnchorElement() invalidating anchor for frame: " << frameView() << " for scroller: " << m_owningScrollableArea);

    m_anchorObject = nullptr;
    m_lastAnchorOffset = { };

    if (m_isQueuedForScrollPositionUpdate) {
        m_isQueuedForScrollPositionUpdate = false;
        frameView().dequeueScrollableAreaForScrollAnchoringUpdate(m_owningScrollableArea);
    }
}

static FloatRect candidateLocalRectForAnchoring(RenderObject& renderer)
{
    if (CheckedPtr box = dynamicDowncast<RenderBox>(renderer)) {
        auto anchoringBounds = box->borderBoxRect();
        if (!box->hasNonVisibleOverflow())
            anchoringBounds.shiftMaxYEdgeTo(box->layoutOverflowRect().maxY());

        return anchoringBounds;
    }

    if (CheckedPtr text = dynamicDowncast<RenderText>(renderer))
        return text->linesBoundingBox();

    if (CheckedPtr text = dynamicDowncast<RenderText>(renderer))
        return text->linesBoundingBox();

    if (is<LegacyRenderSVGModelObject>(renderer))
        return renderer.decoratedBoundingBox();

    if (is<RenderSVGModelObject>(renderer))
        return renderer.decoratedBoundingBox();

    return { };
}

auto ScrollAnchoringController::computeScrollerRelativeRects(RenderObject& candidate) const -> Rects
{
    // FIXME: This needs to handle writing modes and zooming.
    CheckedPtr scrollerBox = scrollableAreaBox();
    if (!scrollerBox)
        return { };

    auto localAnchoringRect = candidateLocalRectForAnchoring(candidate);
    LOG_WITH_STREAM(ScrollAnchoring, stream << "computeScrollerRelativeRects - candidate " << candidate << " localAnchoringRect " << localAnchoringRect);

    if (scrollerBox->isRenderView()) {
        RefPtr frameView = dynamicDowncast<LocalFrameView>(downcast<ScrollView>(m_owningScrollableArea.get()));
        if (!frameView)
            return { };

        // This is not affected by page scale.
        auto scrollViewport = frameView->visualViewportRect();

        RefPtr documentElement = frameView->frame().document() ? frameView->frame().document()->documentElement() : nullptr;
        if (!documentElement)
            return { };

        CheckedPtr docRenderer = documentElement->renderBox();
        if (!docRenderer)
            return { };

        scrollViewport.contract(docRenderer->scrollPaddingForViewportRect(scrollViewport));

        return {
            // Map to the RenderView to exclude page scale.
            .boundsRelativeToScrolledContent = candidate.localToContainerQuad(localAnchoringRect, dynamicDowncast<RenderView>(*scrollerBox)).boundingBox(),
            .scrollerContentsVisibleRect = scrollViewport
        };
    }

    auto scrollerRect = LayoutRect { m_owningScrollableArea->visibleContentRect() };
    scrollerRect.contract(scrollerBox->scrollPaddingForViewportRect(scrollerRect));

    // FIXME: Check for writing modes.
    // FIXME: This really needs to compute bounds relative to the padding box.
    auto boundsInScrollerContentCoordinates = candidate.localToContainerQuad(localAnchoringRect, scrollerBox.get()).boundingBox();
    boundsInScrollerContentCoordinates.moveBy(m_owningScrollableArea->scrollPosition());

    return {
        .boundsRelativeToScrolledContent = boundsInScrollerContentCoordinates,
        .scrollerContentsVisibleRect = scrollerRect
    };
}

FloatPoint ScrollAnchoringController::computeOffsetFromOwningScroller(RenderObject& candidate) const
{
    auto rects = computeScrollerRelativeRects(candidate);
    return toFloatPoint(rects.boundsRelativeToScrolledContent.location() - rects.scrollerContentsVisibleRect.location());
}

void ScrollAnchoringController::notifyChildHadSuppressingStyleChange(RenderElement& renderer)
{
    CheckedPtr scrollerBox = scrollableAreaBox();

#if LOG_DISABLED
    UNUSED_PARAM(renderer);
#else
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " notifyChildHadSuppressingStyleChange() for " << *scrollerBox << " for " << renderer);
#endif

    scrollerBox->setScrollAnchoringSuppressionStyleChanged(true);
}

static CheckedPtr<RenderElement> priorityCandidateForElement(Element* element)
{
    while (element) {
        if (CheckedPtr renderer = element->renderer()) {
            if (!renderer->isAnonymousBlock() && (!renderer->isInline() || renderer->isAtomicInlineLevelBox()))
                return renderer;
        }
        SUPPRESS_UNCOUNTED_LOCAL element = element->parentElement();
    }
    return nullptr;
}

// https://drafts.csswg.org/css-scroll-anchoring/#anchor-priority-candidates
bool ScrollAnchoringController::findPriorityCandidate(Document& document)
{
    CheckedPtr scrollerBox = scrollableAreaBox();
    if (!scrollerBox)
        return false;

    RefPtr focusedElement = document.focusedElement();
    if (focusedElement && isEditableNode(*focusedElement)) {
        if (CheckedPtr candidate = priorityCandidateForElement(focusedElement)) {
            auto status = examinePriorityCandidate(*candidate);
            if (isViableStatus(status)) {
                m_anchorObject = *candidate;
                // FIXME: Look inside the candidate?
                return true;
            }
        }
    }

    // FIXME: Find in page match

    return false;
}

static bool candidateMayMoveWithScroller(RenderElement& renderer, RenderBox& scrollerBox)
{
    if (renderer.isStickilyPositioned() || renderer.isFixedPositioned())
        return false;

    CheckedPtr scrollerBlock = dynamicDowncast<RenderBlock>(scrollerBox);
    if (!scrollerBlock->isContainingBlockAncestorFor(renderer))
        return false;

    return true;
}

AnchorSearchStatus ScrollAnchoringController::examinePriorityCandidate(RenderElement& renderer) const
{
    CheckedPtr scrollerBox = scrollableAreaBox();

    CheckedPtr<RenderElement> ancestor = &renderer;
    while (ancestor && ancestor != scrollerBox.get()) {
        if (ancestor->style().overflowAnchor() == OverflowAnchor::None)
            return AnchorSearchStatus::Exclude;

        if (!candidateMayMoveWithScroller(*ancestor, *scrollerBox))
            return AnchorSearchStatus::Exclude;

        ancestor = ancestor->parent();
    }

    if (!ancestor)
        return AnchorSearchStatus::Exclude;

    return examineAnchorCandidate(*ancestor);
}

static bool NODELETE overflowAnchorProhibitsAnchoring(const RenderElement& object, const RenderBox& scrollingAncestor)
{
    for (CheckedPtr renderer = &object; renderer; renderer = renderer->parent()) {
        if (renderer->style().overflowAnchor() == OverflowAnchor::None)
            return true;

        if (renderer == &scrollingAncestor)
            break;
    }

    return false;
}

// https://drafts.csswg.org/css-scroll-anchoring/#candidate-examination
AnchorSearchStatus ScrollAnchoringController::examineAnchorCandidate(RenderObject& candidate) const
{
    CheckedPtr scrollerBox = scrollableAreaBox();

    if (&candidate == scrollerBox.get())
        return AnchorSearchStatus::Continue;

    bool isScrollableWithAnchor = false;

    if (CheckedPtr renderer = dynamicDowncast<RenderElement>(candidate)) {
        if (overflowAnchorProhibitsAnchoring(*renderer, *scrollerBox))
            return AnchorSearchStatus::Exclude;

        if (candidate.isBR())
            return AnchorSearchStatus::Exclude;

        if (candidate.isAnonymous())
            return AnchorSearchStatus::Continue;

        if (!candidateMayMoveWithScroller(*renderer, *scrollerBox))
            return AnchorSearchStatus::Exclude;

        isScrollableWithAnchor = [&]() {
            CheckedPtr candidateBox = dynamicDowncast<RenderBox>(candidate);
            if (candidateBox && candidateBox->canBeScrolledAndHasScrollableArea() && candidateBox->hasLayer()) {
                if (CheckedPtr scrollableArea = downcast<RenderLayerModelObject>(candidate).layer()->scrollableArea()) {
                    CheckedPtr controller = scrollableArea->scrollAnchoringController();
                    if (controller && controller->shouldMaintainScrollAnchor())
                        return true;
                }
            }
            return false;
        }();
    }

    auto shouldDescendIntoObjectWithEmptyLayoutOverflow = [](RenderObject& candidate) {
        if (!is<RenderElement>(candidate))
            return false;

        if (candidate.isInline())
            return true;

        if (CheckedPtr blockFlow = dynamicDowncast<RenderBlockFlow>(candidate))
            return blockFlow->subtreeContainsFloats();

        return false;
    };

    auto rects = computeScrollerRelativeRects(candidate);
    if (rects.boundsRelativeToScrolledContent.isEmpty()) {
        if (shouldDescendIntoObjectWithEmptyLayoutOverflow(candidate))
            return AnchorSearchStatus::Continue;

        return AnchorSearchStatus::Exclude;
    }

    if (rects.scrollerContentsVisibleRect.contains(rects.boundsRelativeToScrolledContent))
        return AnchorSearchStatus::Choose;

    // This takes scroll padding into account
    auto intersectionRect = intersection(rects.boundsRelativeToScrolledContent, rects.scrollerContentsVisibleRect);
    LOG_WITH_STREAM(ScrollAnchoring, stream << " bounds in scrolled content " << rects.boundsRelativeToScrolledContent << " scroller viewport " << rects.scrollerContentsVisibleRect << " intersectionRect " << intersectionRect);

    if (intersectionRect.isEmpty())
        return AnchorSearchStatus::Exclude;

    return isScrollableWithAnchor ? AnchorSearchStatus::Choose : AnchorSearchStatus::Constrain;
}

#if !LOG_DISABLED
static TextStream& operator<<(TextStream& ts, AnchorSearchStatus result)
{
    switch (result) {
    case AnchorSearchStatus::Exclude:
        ts << "Exclude"_s;
        break;
    case AnchorSearchStatus::Continue:
        ts << "Continue"_s;
        break;
    case AnchorSearchStatus::Constrain:
        ts << "Constrain"_s;
        break;
    case AnchorSearchStatus::Choose:
        ts << "Choose"_s;
        break;
    }
    return ts;
}
#endif

// For each absolutely-positioned element A whose containing block is N...
AnchorSearchStatus ScrollAnchoringController::findAnchorInOutOfFlowObjects(RenderObject& candidate)
{
    CheckedPtr block = dynamicDowncast<RenderBlock>(candidate);
    if (!block)
        return AnchorSearchStatus::Exclude;

    auto* outOfFlowBoxes = block->outOfFlowBoxes();
    if (!outOfFlowBoxes)
        return AnchorSearchStatus::Exclude;

    for (CheckedRef outOfFlowBox : *outOfFlowBoxes) {
        auto status = findAnchorRecursive(outOfFlowBox.ptr());
        if (isViableStatus(status))
            return status;
    }

    return AnchorSearchStatus::Exclude;
}

AnchorSearchStatus ScrollAnchoringController::findAnchorRecursive(RenderObject* object)
{
    if (!object)
        return AnchorSearchStatus::Exclude;

    if (!object->everHadLayout())
        return AnchorSearchStatus::Exclude;

    auto status = examineAnchorCandidate(*object);
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " findAnchorRecursive() element: " << *object <<" examination result: " << status);

    if (isViableStatus(status))
        m_anchorObject = *object;

    if (status == AnchorSearchStatus::Choose || status == AnchorSearchStatus::Exclude)
        return status;

    CheckedPtr renderElement = dynamicDowncast<RenderElement>(*object);
    if (!renderElement)
        return AnchorSearchStatus::Exclude;

    for (CheckedPtr child = renderElement->firstChild(); child; child = child->nextSibling()) {
        auto childStatus = findAnchorRecursive(child.get());
        if (childStatus == AnchorSearchStatus::Choose)
            return childStatus;

        if (childStatus == AnchorSearchStatus::Constrain) {
            // FIXME: Do something better in fragmented contexts?
            return childStatus;
        }
    }

    auto outOfFlowStatus = findAnchorInOutOfFlowObjects(*object);
    if (isViableStatus(outOfFlowStatus))
        return outOfFlowStatus;

    return status;
}

// https://drafts.csswg.org/css-scroll-anchoring/#anchor-node-selection
void ScrollAnchoringController::chooseAnchorElement(Document& document)
{
    bool foundPriorityObject = findPriorityCandidate(document);

    if (!foundPriorityObject)
        findAnchorRecursive(scrollableAreaBox());

    if (!m_anchorObject) {
        LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " chooseAnchorElement() failed to find anchor");
        return;
    }

    m_lastAnchorOffset = computeOffsetFromOwningScroller(*m_anchorObject);
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController::chooseAnchorElement() found anchor: " << *m_anchorObject << " offset: " << m_lastAnchorOffset);
}

// https://drafts.csswg.org/css-scroll-anchoring/#suppression-triggers
bool ScrollAnchoringController::anchoringSuppressedByStyleChange() const
{
    if (!m_anchorObject)
        return false;

    CheckedPtr scrollerBox = scrollableAreaBox();

    // ...any element in the path from the anchor node to the scrollable element (or document), inclusive of both.
    // m_anchorObject can be a RenderText, but that will never have scrollAnchoringSuppressionStyleChanged() set.
    if (CheckedPtr renderer = dynamicDowncast<RenderElement>(*m_anchorObject); renderer && renderer->scrollAnchoringSuppressionStyleChanged())
        return true;

    for (CheckedPtr renderer = m_anchorObject->parent(); renderer; renderer = renderer->parent()) {
        if (renderer->scrollAnchoringSuppressionStyleChanged())
            return true;

        if (renderer == scrollerBox)
            break;
    }

    return false;
}

void ScrollAnchoringController::updateBeforeLayout()
{
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " on " << *scrollableAreaBox() << " updateBeforeLayout() - scroll offset " << m_owningScrollableArea->scrollOffset() << " queued " << m_isQueuedForScrollPositionUpdate);

    if (m_isQueuedForScrollPositionUpdate) {
        m_anchoringSuppressedByStyleChange |= anchoringSuppressedByStyleChange();
        return;
    }

    auto scrollOffset = m_owningScrollableArea->scrollOffset();
    // FIXME: Writing modes.
    if (!scrollOffset.y()) {
        clearAnchor();
        return;
    }

    if (!m_anchorObject) {
        RefPtr document = frameView().frame().document();
        if (!document)
            return;

        chooseAnchorElement(*document);
        if (!m_anchorObject) {
            LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " updateBeforeLayout() - did not find anchor");
            return;
        }
    }

    m_anchoringSuppressedByStyleChange = anchoringSuppressedByStyleChange();

    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " updateBeforeLayout() - anchor " << *m_anchorObject << " offset " << m_lastAnchorOffset << " suppressedByStyleChange " << m_anchoringSuppressedByStyleChange);

    frameView().queueScrollableAreaForScrollAnchoringUpdate(m_owningScrollableArea);
    m_isQueuedForScrollPositionUpdate = true;
}

// https://drafts.csswg.org/css-scroll-anchoring/#scroll-adjustment
void ScrollAnchoringController::adjustScrollPositionForAnchoring()
{
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " adjustScrollPositionForAnchoring() - anchor " << m_anchorObject << " offset " << m_lastAnchorOffset << " suppressedByStyleChange  " << m_anchoringSuppressedByStyleChange << " in scroll event " << !!m_inScrollEventCount << " in suppression scope " << !!m_suppressionCount);

    // FIXME: Test for running animated scrolls?
    if (m_inScrollEventCount || m_suppressionCount) {
        m_anchoringSuppressedByStyleChange = false;
        clearAnchor();
        return;
    }

    auto queued = std::exchange(m_isQueuedForScrollPositionUpdate, false);
    if (!m_anchorObject || !queued)
        return;

    if (m_anchoringSuppressedByStyleChange) {
        m_anchoringSuppressedByStyleChange = false;
        clearAnchor();
        return;
    }

    SetForScope midUpdatingScrollPositionForAnchorElement(m_isUpdatingScrollPositionForAnchoring, true);

    auto currentOffset = computeOffsetFromOwningScroller(*m_anchorObject);
    auto adjustment = currentOffset - m_lastAnchorOffset;
    if (adjustment.isZero())
        return;

    // FIXME: Handle content-visibility.

    CheckedPtr scrollerBox = scrollableAreaBox();
    if (scrollerBox->isRenderView()) {
        auto pageScale = frameView().frame().frameScaleFactor();
        adjustment.scale(pageScale);
    }

    auto currentPosition = m_owningScrollableArea->scrollPosition();
    auto newScrollPosition = currentPosition + roundedIntSize(adjustment);
    RELEASE_LOG(ScrollAnchoring, "ScrollAnchoringController::adjustScrollPositionForAnchoring() is main frame: %d, is main scroller: %d, adjusting from (%d, %d) to (%d, %d)",  frameView().frame().isMainFrame(), !m_owningScrollableArea->isRenderLayer(), currentPosition.x(), currentPosition.y(), newScrollPosition.x(), newScrollPosition.y());
    LOG_WITH_STREAM(ScrollAnchoring, stream << "ScrollAnchoringController " << this << " adjustScrollPositionForAnchoring() for scroller element: " << ValueOrNull(scrollableAreaBox()) << " anchor: " << *m_anchorObject << " adjusting from " << currentPosition << " to " << newScrollPosition);

    auto options = ScrollPositionChangeOptions::createProgrammatic();
    options.originalScrollDelta = adjustment;
    options.interruptsAnimation = ScrollInterruptsAnimation::No;

    auto revealScope = ScrollbarRevealBehaviorScope(m_owningScrollableArea.get(), ScrollbarRevealBehavior::DontReveal);
    auto scrollTypeScope = ScrollTypeScope(m_owningScrollableArea.get(), ScrollType::Programmatic);

    // FIXME: This has to explicitly not stop animated scrolls (use ImplicitDeltaUpdate).
    if (!m_owningScrollableArea->requestScrollToPosition(newScrollPosition, options))
        m_owningScrollableArea->scrollToPositionWithoutAnimation(newScrollPosition);
}

void ScrollAnchoringController::willDispatchScrollEvent()
{
    ++m_inScrollEventCount;
}

void ScrollAnchoringController::didDispatchScrollEvent()
{
    ASSERT(m_inScrollEventCount);
    --m_inScrollEventCount;
}

void ScrollAnchoringController::startSuppressingScrollAnchoring()
{
    ++m_suppressionCount;
}

void ScrollAnchoringController::stopSuppressingScrollAnchoring()
{
    ASSERT(m_suppressionCount);
    --m_suppressionCount;
}

} // namespace WebCore
