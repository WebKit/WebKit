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
#include "ElementIterator.h"
#include "HTMLHtmlElement.h"
#include "RenderBox.h"
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

ScrollOffset ScrollAnchoringController::computeOffset(RenderBox& candidate)
{
    // TODO: investigate this for zoom/rtl
    // make sure this works for subscrollers
    return ScrollOffset(FloatPoint(candidate.absoluteBoundingBoxRect().location() - m_frameView.layoutViewportRect().location()));
}

void ScrollAnchoringController::chooseAnchorElement()
{
    // TODO: fully implement anchor node selection algorithm in spec
    // (specifically excluded subtrees and priority nodes)
    // https://drafts.csswg.org/css-scroll-anchoring/#anchor-node-selection

    auto* document = m_frameView.frame().document();
    if (!document)
        return;

    Element* candidateAnchor = nullptr;
    auto layoutViewport = m_frameView.layoutViewportRect();

    // TODO: iterate from the current scroller not document root
    for (auto& element : descendantsOfType<Element>(document->rootNode())) {
        if (auto renderer = element.renderBox()) {
            if (renderer->style().overflowAnchor() == OverflowAnchor::None || &element == document->bodyOrFrameset() || is<HTMLHtmlElement>(&element))
                continue;
            auto boxRect = renderer->absoluteBoundingBoxRect();
            if (layoutViewport.intersects(boxRect))
                candidateAnchor = &element;
            if (layoutViewport.contains(boxRect))
                break;
        }
    }
    if (!candidateAnchor)
        return;
    m_anchorElement = candidateAnchor;
    m_lastOffsetForAnchorElement = computeOffset(*m_anchorElement->renderBox());
}

void ScrollAnchoringController::updateAnchorElement()
{
    // TODO: check owning scroller scroll position
    if (frameView().scrollPosition().isZero() || m_isQueuedForScrollPositionUpdate)
        return;

    if (!m_anchorElement || !m_anchorElement->renderBox()) {
        chooseAnchorElement();
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
    auto renderBox = m_anchorElement->renderBox();
    if (!renderBox) {
        invalidateAnchorElement();
        return;
    }
    FloatSize adjustment = computeOffset(*renderBox) - m_lastOffsetForAnchorElement;
    if (!adjustment.isZero()) {
        auto newScrollPosition = frameView().scrollPosition() + IntPoint(adjustment.width(), adjustment.height());
        auto options = ScrollPositionChangeOptions::createProgrammatic();
        options.originalScrollDelta = adjustment;
        m_frameView.setScrollPosition(newScrollPosition, options);
    }
}

} // namespace WebCore
