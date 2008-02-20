/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include "config.h"
#include "DOMSelection.h"

#include "ExceptionCode.h"
#include "Frame.h"
#include "htmlediting.h"
#include "Node.h"
#include "PlatformString.h"
#include "Range.h"
#include "SelectionController.h"
#include "TextIterator.h"

namespace WebCore {

DOMSelection::DOMSelection(Frame* frame)
    : m_frame(frame)
{
}

Frame* DOMSelection::frame() const
{
    return m_frame;
}

void DOMSelection::disconnectFrame()
{
    m_frame = 0;
}

Node* DOMSelection::anchorNode() const
{
    if (!m_frame)
        return 0;

    const Selection& selection = m_frame->selectionController()->selection();
    Position anchor = selection.isBaseFirst() ? selection.start() : selection.end();
    anchor = rangeCompliantEquivalent(anchor);
    return anchor.node();
}

Node* DOMSelection::baseNode() const
{
    if (!m_frame)
        return 0;
    return rangeCompliantEquivalent(m_frame->selectionController()->selection().base()).node();
}

int DOMSelection::anchorOffset() const
{
    if (!m_frame)
        return 0;

    const Selection& selection = m_frame->selectionController()->selection();
    Position anchor = selection.isBaseFirst() ? selection.start() : selection.end();
    anchor = rangeCompliantEquivalent(anchor);
    return anchor.offset();
}

int DOMSelection::baseOffset() const
{
    if (!m_frame)
        return 0;
    return rangeCompliantEquivalent(m_frame->selectionController()->selection().base()).offset();
}

Node* DOMSelection::focusNode() const
{
    if (!m_frame)
        return 0;

    const Selection& selection = m_frame->selectionController()->selection();
    Position focus = selection.isBaseFirst() ? selection.end() : selection.start();
    focus = rangeCompliantEquivalent(focus);
    return focus.node();
}

Node* DOMSelection::extentNode() const
{
    if (!m_frame)
        return 0;
    return rangeCompliantEquivalent(m_frame->selectionController()->selection().extent()).node();
}

int DOMSelection::focusOffset() const
{
    if (!m_frame)
        return 0;

    const Selection& selection = m_frame->selectionController()->selection();
    Position focus = selection.isBaseFirst() ? selection.end() : selection.start();
    focus = rangeCompliantEquivalent(focus);
    return focus.offset();
}

int DOMSelection::extentOffset() const
{
    if (!m_frame)
        return 0;
    return rangeCompliantEquivalent(m_frame->selectionController()->selection().extent()).offset();
}

bool DOMSelection::isCollapsed() const
{
    if (!m_frame)
        return false;
    return !m_frame->selectionController()->isRange();
}

String DOMSelection::type() const
{
    if (!m_frame)
        return String();

    SelectionController* selectionController = m_frame->selectionController();

    if (selectionController->isNone())
        return "None";
    if (selectionController->isCaret())
        return "Caret";
    return "Range";
}

int DOMSelection::rangeCount() const
{
    if (!m_frame)
        return 0;
    return m_frame->selectionController()->isNone() ? 0 : 1;
}

void DOMSelection::collapse(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    if (offset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    m_frame->selectionController()->moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void DOMSelection::collapseToEnd()
{
    if (!m_frame)
        return;

    const Selection& selection = m_frame->selectionController()->selection();
    m_frame->selectionController()->moveTo(VisiblePosition(selection.end(), DOWNSTREAM));
}

void DOMSelection::collapseToStart()
{
    if (!m_frame)
        return;

    const Selection& selection = m_frame->selectionController()->selection();
    m_frame->selectionController()->moveTo(VisiblePosition(selection.start(), DOWNSTREAM));
}

void DOMSelection::empty()
{
    if (!m_frame)
        return;
    m_frame->selectionController()->moveTo(VisiblePosition());
}

void DOMSelection::setBaseAndExtent(Node* baseNode, int baseOffset, Node* extentNode, int extentOffset, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    if (baseOffset < 0 || extentOffset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    VisiblePosition visibleBase = VisiblePosition(baseNode, baseOffset, DOWNSTREAM);
    VisiblePosition visibleExtent = VisiblePosition(extentNode, extentOffset, DOWNSTREAM);
    
    m_frame->selectionController()->moveTo(visibleBase, visibleExtent);
}

void DOMSelection::setPosition(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;
    if (offset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    m_frame->selectionController()->moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void DOMSelection::modify(const String& alterString, const String& directionString, const String& granularityString)
{
    if (!m_frame)
        return;

    String alterStringLower = alterString.lower();
    SelectionController::EAlteration alter;
    if (alterStringLower == "extend")
        alter = SelectionController::EXTEND;
    else if (alterStringLower == "move")
        alter = SelectionController::MOVE;
    else 
        return;
    
    String directionStringLower = directionString.lower();
    SelectionController::EDirection direction;
    if (directionStringLower == "forward")
        direction = SelectionController::FORWARD;
    else if (directionStringLower == "backward")
        direction = SelectionController::BACKWARD;
    else if (directionStringLower == "left")
        direction = SelectionController::LEFT;
    else if (directionStringLower == "right")
        direction = SelectionController::RIGHT;
    else
        return;
        
    String granularityStringLower = granularityString.lower();
    TextGranularity granularity;
    if (granularityStringLower == "character")
        granularity = CharacterGranularity;
    else if (granularityStringLower == "word")
        granularity = WordGranularity;
    else if (granularityStringLower == "sentence")
        granularity = SentenceGranularity;
    else if (granularityStringLower == "line")
        granularity = LineGranularity;
    else if (granularityStringLower == "paragraph")
        granularity = ParagraphGranularity;
    else if (granularityStringLower == "lineboundary")
        granularity = LineBoundary;
    else if (granularityStringLower == "sentenceboundary")
        granularity = SentenceBoundary;
    else if (granularityStringLower == "paragraphboundary")
        granularity = ParagraphBoundary;
    else if (granularityStringLower == "documentboundary")
        granularity = DocumentBoundary;
    else
        return;

    m_frame->selectionController()->modify(alter, direction, granularity, false);
}

void DOMSelection::extend(Node* node, int offset, ExceptionCode& ec)
{
    if (!m_frame)
        return;

    if (!node) {
        ec = TYPE_MISMATCH_ERR;
        return;
    }
    if (offset < 0 || offset > (node->offsetInCharacters() ? caretMaxOffset(node) : (int)node->childNodeCount())) {
        ec = INDEX_SIZE_ERR;
        return;
    }

    SelectionController* selectionController = m_frame->selectionController();
    selectionController->expandUsingGranularity(CharacterGranularity);
    selectionController->setExtent(VisiblePosition(node, offset, DOWNSTREAM));
}

PassRefPtr<Range> DOMSelection::getRangeAt(int index, ExceptionCode& ec)
{
    if (!m_frame)
        return 0;

    if (index < 0 || index >= rangeCount()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }

    const Selection& selection = m_frame->selectionController()->selection();
    return selection.toRange();
}

void DOMSelection::removeAllRanges()
{
    if (!m_frame)
        return;
    m_frame->selectionController()->clear();
}

void DOMSelection::addRange(Range* r)
{
    if (!m_frame)
        return;
    if (!r)
        return;

    SelectionController* selectionController = m_frame->selectionController();
    
    if (selectionController->isNone()) {
        selectionController->setSelection(Selection(r));
        return;
    }

    RefPtr<Range> range = selectionController->selection().toRange();
    ExceptionCode ec = 0;
    if (r->compareBoundaryPoints(Range::START_TO_START, range.get(), ec) == -1) {
        // We don't support discontiguous selection. We don't do anything if r and range don't intersect.
        if (r->compareBoundaryPoints(Range::END_TO_START, range.get(), ec) > -1) {
            if (r->compareBoundaryPoints(Range::END_TO_END, range.get(), ec) == -1)
                // The original range and r intersect.
                selectionController->setSelection(Selection(r->startPosition(), range->endPosition(), DOWNSTREAM));
            else
                // r contains the original range.
                selectionController->setSelection(Selection(r));
        }
    } else {
        // We don't support discontiguous selection. We don't do anything if r and range don't intersect.
        if (r->compareBoundaryPoints(Range::START_TO_END, range.get(), ec) < 1) {
            if (r->compareBoundaryPoints(Range::END_TO_END, range.get(), ec) == -1)
                // The original range contains r.
                selectionController->setSelection(Selection(range.get()));
            else
                // The original range and r intersect.
                selectionController->setSelection(Selection(range->startPosition(), r->endPosition(), DOWNSTREAM));
        }
    }
}

void DOMSelection::deleteFromDocument()
{
    if (!m_frame)
        return;

    SelectionController* selectionController = m_frame->selectionController();

    if (selectionController->isNone())
        return;

    if (isCollapsed())
        selectionController->modify(SelectionController::EXTEND, SelectionController::BACKWARD, CharacterGranularity);

    RefPtr<Range> selectedRange = selectionController->selection().toRange();

    ExceptionCode ec = 0;
    selectedRange->deleteContents(ec);
    ASSERT(!ec);
    
    setBaseAndExtent(selectedRange->startContainer(ec), selectedRange->startOffset(ec), selectedRange->startContainer(ec), selectedRange->startOffset(ec), ec);
    ASSERT(!ec);
}

bool DOMSelection::containsNode(const Node* n, bool allowPartial) const
{
    if (!m_frame)
        return false;

    SelectionController* selectionController = m_frame->selectionController();

    if (!n || selectionController->isNone())
        return false;

    Node* parentNode = n->parentNode();
    unsigned nodeIndex = n->nodeIndex();
    RefPtr<Range> selectedRange = selectionController->selection().toRange();

    if (!parentNode)
        return false;

    ExceptionCode ec = 0;
    bool nodeFullySelected = Range::compareBoundaryPoints(parentNode, nodeIndex, selectedRange->startContainer(ec), selectedRange->startOffset(ec)) >= 0
        && Range::compareBoundaryPoints(parentNode, nodeIndex + 1, selectedRange->endContainer(ec), selectedRange->endOffset(ec)) <= 0;
    ASSERT(!ec);
    if (nodeFullySelected)
        return true;

    bool nodeFullyUnselected = Range::compareBoundaryPoints(parentNode, nodeIndex, selectedRange->endContainer(ec), selectedRange->endOffset(ec)) > 0
        || Range::compareBoundaryPoints(parentNode, nodeIndex + 1, selectedRange->startContainer(ec), selectedRange->startOffset(ec)) < 0;
    ASSERT(!ec);
    if (nodeFullyUnselected)
        return false;

    return allowPartial || n->isTextNode();
}

void DOMSelection::selectAllChildren(Node* n, ExceptionCode& ec)
{
    if (!n)
        return;

    // This doesn't (and shouldn't) select text node characters.
    setBaseAndExtent(n, 0, n, n->childNodeCount(), ec);
}

String DOMSelection::toString()
{
    if (!m_frame)
        return String();

    return plainText(m_frame->selectionController()->selection().toRange().get());
}

} // namespace WebCore
