/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
  
#include "config.h"
#include "SelectionController.h"

#include "DeleteSelectionCommand.h"
#include "Document.h"
#include "Editor.h"
#include "Element.h"
#include "EventNames.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameTree.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "HTMLInputElement.h"
#include "HTMLNames.h"
#include "HitTestRequest.h"
#include "HitTestResult.h"
#include "Page.h"
#include "Range.h"
#include "RenderView.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "visible_units.h"

#define EDIT_DEBUG 0

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

const int NoXPosForVerticalArrowNavigation = INT_MIN;

SelectionController::SelectionController(Frame* frame, bool isDragCaretController)
    : m_needsLayout(true)
    , m_lastChangeWasHorizontalExtension(false)
    , m_frame(frame)
    , m_isDragCaretController(isDragCaretController)
    , m_isCaretBlinkingSuspended(false)
    , m_xPosForVerticalArrowNavigation(NoXPosForVerticalArrowNavigation)
{
}

void SelectionController::moveTo(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(pos.deepEquivalent(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const VisiblePosition &base, const VisiblePosition &extent, bool userTriggered)
{
    setSelection(Selection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(pos, affinity), true, true, userTriggered);
}

void SelectionController::moveTo(const Range *r, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(startPosition(r), endPosition(r), affinity), true, true, userTriggered);
}

void SelectionController::moveTo(const Position &base, const Position &extent, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(base, extent, affinity), true, true, userTriggered);
}

void SelectionController::setSelection(const Selection& s, bool closeTyping, bool clearTypingStyle, bool userTriggered)
{
    if (m_isDragCaretController) {
        invalidateCaretRect();
        m_sel = s;
        m_needsLayout = true;
        invalidateCaretRect();
        return;
    }
    if (!m_frame) {
        m_sel = s;
        return;
    }
    
    if (s.base().node() && s.base().node()->document() != m_frame->document()) {
        s.base().node()->document()->frame()->selectionController()->setSelection(s, closeTyping, clearTypingStyle, userTriggered);
        return;
    }
    
    if (closeTyping)
        TypingCommand::closeTyping(m_frame->editor()->lastEditCommand());

    if (clearTypingStyle) {
        m_frame->clearTypingStyle();
        m_frame->editor()->setRemovedAnchor(0);
    }
        
    if (m_sel == s)
        return;
    
    Selection oldSelection = m_sel;

    m_sel = s;
    
    m_needsLayout = true;
    
    if (!s.isNone())
        m_frame->setFocusedNodeIfNeeded();
    
    m_frame->selectionLayoutChanged();
    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation;
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(userTriggered);
    m_frame->respondToChangedSelection(oldSelection, closeTyping);
    if (userTriggered)
        m_frame->revealCaret(RenderLayer::gAlignToEdgeIfNeeded);

    notifyAccessibilityForSelectionChange();
}

static bool removingNodeRemovesPosition(Node* node, const Position& position)
{
    if (!position.node())
        return false;
        
    if (position.node() == node || position.node()->isDescendantOf(node))
        return true;
    
    Node* shadowAncestorNode = position.node()->shadowAncestorNode();
    return shadowAncestorNode && shadowAncestorNode->isDescendantOf(node);
}

void SelectionController::nodeWillBeRemoved(Node *node)
{
    if (isNone())
        return;
    
    bool baseRemoved = removingNodeRemovesPosition(node, m_sel.base());
    bool extentRemoved = removingNodeRemovesPosition(node, m_sel.extent());
    bool startRemoved = removingNodeRemovesPosition(node, m_sel.start());
    bool endRemoved = removingNodeRemovesPosition(node, m_sel.end());
    
    bool clearRenderTreeSelection = false;
    bool clearDOMTreeSelection = false;

    if (startRemoved || endRemoved) {
        // FIXME: When endpoints are removed, we should just alter the selection, instead of blowing it away.
        clearRenderTreeSelection = true;
        clearDOMTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        if (m_sel.isBaseFirst()) {
            m_sel.setBase(m_sel.start());
            m_sel.setExtent(m_sel.end());
        } else {
            m_sel.setBase(m_sel.start());
            m_sel.setExtent(m_sel.end());
        }
    // FIXME: This could be more efficient if we had an isNodeInRange function on Ranges.
    } else if (Range::compareBoundaryPoints(m_sel.start(), Position(node, 0)) == -1 &&
               Range::compareBoundaryPoints(m_sel.end(), Position(node, 0)) == 1) {
        // If we did nothing here, when this node's renderer was destroyed, the rect that it 
        // occupied would be invalidated, but, selection gaps that change as a result of 
        // the removal wouldn't be invalidated.
        // FIXME: Don't do so much unnecessary invalidation.
        clearRenderTreeSelection = true;
    }

    if (clearRenderTreeSelection) {
        RefPtr<Document> document = m_sel.start().node()->document();
        document->updateRendering();
        if (RenderView* view = static_cast<RenderView*>(document->renderer()))
            view->clearSelection();
    }

    if (clearDOMTreeSelection)
        setSelection(Selection(), false, false);
}

void SelectionController::willBeModified(EAlteration alter, EDirection direction)
{
    switch (alter) {
        case MOVE:
            m_lastChangeWasHorizontalExtension = false;
            break;
        case EXTEND:
            if (!m_lastChangeWasHorizontalExtension) {
                m_lastChangeWasHorizontalExtension = true;
                Position start = m_sel.start();
                Position end = m_sel.end();
                switch (direction) {
                    // FIXME: right for bidi?
                    case RIGHT:
                    case FORWARD:
                        m_sel.setBase(start);
                        m_sel.setExtent(end);
                        break;
                    case LEFT:
                    case BACKWARD:
                        m_sel.setBase(end);
                        m_sel.setExtent(start);
                        break;
                }
            }
            break;
    }
}

VisiblePosition SelectionController::modifyExtendingRightForward(TextGranularity granularity)
{
    VisiblePosition pos(m_sel.extent(), m_sel.affinity());
    switch (granularity) {
        case CharacterGranularity:
            pos = pos.next(true);
            break;
        case WordGranularity:
            pos = nextWordPosition(pos);
            break;
        case SentenceGranularity:
            pos = nextSentencePosition(pos);
            break;
        case LineGranularity:
            pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case ParagraphGranularity:
            pos = nextParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case SentenceBoundary:
            pos = endOfSentence(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = endOfLine(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
    }
    
    return pos;
}

VisiblePosition SelectionController::modifyMovingRightForward(TextGranularity granularity)
{
    VisiblePosition pos;
    // FIXME: Stay in editable content for the less common granularities.
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).next(true);
            break;
        case WordGranularity:
            pos = nextWordPosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case SentenceGranularity:
            pos = nextSentencePosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case LineGranularity: {
            // down-arrowing from a range selection that ends at the start of a line needs
            // to leave the selection at that line start (no need to call nextLinePosition!)
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (!isRange() || !isStartOfLine(pos))
                pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(START));
            break;
        }
        case ParagraphGranularity:
            pos = nextParagraphPosition(VisiblePosition(m_sel.end(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
            break;
        case SentenceBoundary:
            pos = endOfSentence(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = endOfLine(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(VisiblePosition(m_sel.end(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.end(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
            
    }
    return pos;
}

VisiblePosition SelectionController::modifyExtendingLeftBackward(TextGranularity granularity)
{
    VisiblePosition pos(m_sel.extent(), m_sel.affinity());
        
    // Extending a selection backward by word or character from just after a table selects
    // the table.  This "makes sense" from the user perspective, esp. when deleting.
    // It was done here instead of in VisiblePosition because we want VPs to iterate
    // over everything.
    switch (granularity) {
        case CharacterGranularity:
            pos = pos.previous(true);
            break;
        case WordGranularity:
            pos = previousWordPosition(pos);
            break;
        case SentenceGranularity:
            pos = previousSentencePosition(pos);
            break;
        case LineGranularity:
            pos = previousLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case SentenceBoundary:
            pos = startOfSentence(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = startOfLine(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingLeftBackward(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            else
                pos = VisiblePosition(m_sel.extent(), m_sel.affinity()).previous(true);
            break;
        case WordGranularity:
            pos = previousWordPosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case SentenceGranularity:
            pos = previousSentencePosition(VisiblePosition(m_sel.extent(), m_sel.affinity()));
            break;
        case LineGranularity:
            pos = previousLinePosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(VisiblePosition(m_sel.start(), m_sel.affinity()), xPosForVerticalArrowNavigation(START));
            break;
        case SentenceBoundary:
            pos = startOfSentence(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case LineBoundary:
            pos = startOfLine(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(VisiblePosition(m_sel.start(), m_sel.affinity()));
            break;
        case DocumentBoundary:
            pos = VisiblePosition(m_sel.start(), m_sel.affinity());
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

bool SelectionController::modify(const String &alterString, const String &directionString, const String &granularityString, bool userTriggered)
{
    String alterStringLower = alterString.lower();
    EAlteration alter;
    if (alterStringLower == "extend")
        alter = EXTEND;
    else if (alterStringLower == "move")
        alter = MOVE;
    else 
        return false;
    
    String directionStringLower = directionString.lower();
    EDirection direction;
    if (directionStringLower == "forward")
        direction = FORWARD;
    else if (directionStringLower == "backward")
        direction = BACKWARD;
    else if (directionStringLower == "left")
        direction = LEFT;
    else if (directionStringLower == "right")
        direction = RIGHT;
    else
        return false;
        
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
        return false;
                
    return modify(alter, direction, granularity, userTriggered);
}

bool SelectionController::modify(EAlteration alter, EDirection dir, TextGranularity granularity, bool userTriggered)
{
    if (userTriggered) {
        SelectionController trialSelectionController;
        trialSelectionController.setLastChangeWasHorizontalExtension(m_lastChangeWasHorizontalExtension);
        trialSelectionController.setSelection(m_sel);
        trialSelectionController.modify(alter, dir, granularity, false);

        bool change = m_frame->shouldChangeSelection(trialSelectionController.selection());
        if (!change)
            return false;
    }

    if (m_frame)
        m_frame->setSelectionGranularity(granularity);
    
    willBeModified(alter, dir);

    VisiblePosition pos;
    switch (dir) {
        // EDIT FIXME: These need to handle bidi
        case RIGHT:
        case FORWARD:
            if (alter == EXTEND)
                pos = modifyExtendingRightForward(granularity);
            else
                pos = modifyMovingRightForward(granularity);
            break;
        case LEFT:
        case BACKWARD:
            if (alter == EXTEND)
                pos = modifyExtendingLeftBackward(granularity);
            else
                pos = modifyMovingLeftBackward(granularity);
            break;
    }

    if (pos.isNull())
        return false;
    
    // Some of the above operations set an xPosForVerticalArrowNavigation.
    // Setting a selection will clear it, so save it to possibly restore later.
    // Note: the START position type is arbitrary because it is unused, it would be
    // the requested position type if there were no xPosForVerticalArrowNavigation set.
    int x = xPosForVerticalArrowNavigation(START);

    switch (alter) {
        case MOVE:
            moveTo(pos, userTriggered);
            break;
        case EXTEND:
            setExtent(pos, userTriggered);
            break;
    }
    
    if (granularity == LineGranularity || granularity == ParagraphGranularity)
        m_xPosForVerticalArrowNavigation = x;

    if (userTriggered) {
        // User modified selection change also sets the granularity back to character.
        // NOTE: The one exception is that we need to keep word granularity to
        // preserve smart delete behavior when extending by word (e.g. double-click),
        // then shift-option-right arrow, then delete needs to smart delete, per TextEdit.
        if (!(alter == EXTEND && granularity == WordGranularity && m_frame->selectionGranularity() == WordGranularity))
            m_frame->setSelectionGranularity(CharacterGranularity);
    }

    setNeedsLayout();

    return true;
}

// FIXME: Maybe baseline would be better?
static bool caretY(const VisiblePosition &c, int &y)
{
    Position p = c.deepEquivalent();
    Node *n = p.node();
    if (!n)
        return false;
    RenderObject *r = p.node()->renderer();
    if (!r)
        return false;
    IntRect rect = r->caretRect(p.offset());
    if (rect.isEmpty())
        return false;
    y = rect.y() + rect.height() / 2;
    return true;
}

bool SelectionController::modify(EAlteration alter, int verticalDistance, bool userTriggered)
{
    if (verticalDistance == 0)
        return false;

    if (userTriggered) {
        SelectionController trialSelectionController;
        trialSelectionController.setSelection(m_sel);
        trialSelectionController.modify(alter, verticalDistance, false);

        bool change = m_frame->shouldChangeSelection(trialSelectionController.selection());
        if (!change)
            return false;
    }

    bool up = verticalDistance < 0;
    if (up)
        verticalDistance = -verticalDistance;

    willBeModified(alter, up ? BACKWARD : FORWARD);

    VisiblePosition pos;
    int xPos = 0;
    switch (alter) {
        case MOVE:
            pos = VisiblePosition(up ? m_sel.start() : m_sel.end(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(up ? START : END);
            m_sel.setAffinity(up ? UPSTREAM : DOWNSTREAM);
            break;
        case EXTEND:
            pos = VisiblePosition(m_sel.extent(), m_sel.affinity());
            xPos = xPosForVerticalArrowNavigation(EXTENT);
            m_sel.setAffinity(DOWNSTREAM);
            break;
    }

    int startY;
    if (!caretY(pos, startY))
        return false;
    if (up)
        startY = -startY;
    int lastY = startY;

    VisiblePosition result;
    VisiblePosition next;
    for (VisiblePosition p = pos; ; p = next) {
        next = (up ? previousLinePosition : nextLinePosition)(p, xPos);
        if (next.isNull() || next == p)
            break;
        int nextY;
        if (!caretY(next, nextY))
            break;
        if (up)
            nextY = -nextY;
        if (nextY - startY > verticalDistance)
            break;
        if (nextY >= lastY) {
            lastY = nextY;
            result = next;
        }
    }

    if (result.isNull())
        return false;

    switch (alter) {
        case MOVE:
            moveTo(result, userTriggered);
            break;
        case EXTEND:
            setExtent(result, userTriggered);
            break;
    }

    if (userTriggered)
        m_frame->setSelectionGranularity(CharacterGranularity);

    return true;
}

bool SelectionController::expandUsingGranularity(TextGranularity granularity)
{
    if (isNone())
        return false;

    m_sel.expandUsingGranularity(granularity);
    m_needsLayout = true;
    return true;
}

int SelectionController::xPosForVerticalArrowNavigation(EPositionType type)
{
    int x = 0;

    if (isNone())
        return x;

    Position pos;
    switch (type) {
        case START:
            pos = m_sel.start();
            break;
        case END:
            pos = m_sel.end();
            break;
        case BASE:
            pos = m_sel.base();
            break;
        case EXTENT:
            pos = m_sel.extent();
            break;
    }

    Frame *frame = pos.node()->document()->frame();
    if (!frame)
        return x;
        
    if (m_xPosForVerticalArrowNavigation == NoXPosForVerticalArrowNavigation) {
        pos = VisiblePosition(pos, m_sel.affinity()).deepEquivalent();
        // VisiblePosition creation can fail here if a node containing the selection becomes visibility:hidden
        // after the selection is created and before this function is called.
        x = pos.isNotNull() ? pos.node()->renderer()->caretRect(pos.offset(), m_sel.affinity()).x() : 0;
        m_xPosForVerticalArrowNavigation = x;
    }
    else
        x = m_xPosForVerticalArrowNavigation;
        
    return x;
}

void SelectionController::clear()
{
    setSelection(Selection());
}

void SelectionController::setBase(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(pos.deepEquivalent(), m_sel.extent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setExtent(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(Selection(m_sel.base(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setBase(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(pos, m_sel.extent(), affinity), true, true, userTriggered);
}

void SelectionController::setExtent(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(Selection(m_sel.base(), pos, affinity), true, true, userTriggered);
}

void SelectionController::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

String SelectionController::type() const
{
    if (isNone())
        return "None";
    else if (isCaret())
        return "Caret";
    else
        return "Range";
}

// These methods are accessible via JS (and are not used internally), so they must return valid DOM positions.
Node* SelectionController::baseNode() const
{
    Position base = rangeCompliantEquivalent(m_sel.base());
    return base.node();
}

int SelectionController::baseOffset() const
{
    Position base = rangeCompliantEquivalent(m_sel.base());
    return base.offset();
}

Node* SelectionController::extentNode() const
{
    Position extent = rangeCompliantEquivalent(m_sel.extent());
    return extent.node();
}

int SelectionController::extentOffset() const
{
    Position extent = rangeCompliantEquivalent(m_sel.extent());
    return extent.offset();
}

Node* SelectionController::anchorNode() const
{
    Position anchor = m_sel.isBaseFirst() ? m_sel.start() : m_sel.end();
    anchor = rangeCompliantEquivalent(anchor);
    return anchor.node();
}

int SelectionController::anchorOffset() const
{
    Position anchor = m_sel.isBaseFirst() ? m_sel.start() : m_sel.end();
    anchor = rangeCompliantEquivalent(anchor);
    return anchor.offset();
}

Node* SelectionController::focusNode() const
{
    Position focus = m_sel.isBaseFirst() ? m_sel.end() : m_sel.start();
    focus = rangeCompliantEquivalent(focus);
    return focus.node();
}

int SelectionController::focusOffset() const
{
    Position focus = m_sel.isBaseFirst() ? m_sel.end() : m_sel.start();
    focus = rangeCompliantEquivalent(focus);
    return focus.offset();
}

String SelectionController::toString() const
{
    return String(plainText(m_sel.toRange().get()));
}

PassRefPtr<Range> SelectionController::getRangeAt(int index, ExceptionCode& ec) const
{
    if (index < 0 || index >= rangeCount()) {
        ec = INDEX_SIZE_ERR;
        return 0;
    }   
    return m_sel.toRange();
}

void SelectionController::removeAllRanges()
{
    clear();
}

// Adds r to the currently selected range.
void SelectionController::addRange(const Range* r)
{
    if (!r)
        return;
    
    if (isNone()) {
        setSelection(Selection(r));
        return;
    }

    RefPtr<Range> range = m_sel.toRange();
    ExceptionCode ec = 0;
    if (r->compareBoundaryPoints(Range::START_TO_START, range.get(), ec) == -1) {
        // We don't support discontiguous selection. We don't do anything if r and range don't intersect.
        if (r->compareBoundaryPoints(Range::END_TO_START, range.get(), ec) > -1) {
            if (r->compareBoundaryPoints(Range::END_TO_END, range.get(), ec) == -1)
                // The original range and r intersect.
                setSelection(Selection(r->startPosition(), range->endPosition(), DOWNSTREAM));
            else
                // r contains the original range.
                setSelection(Selection(r));
        }
    } else {
        // We don't support discontiguous selection. We don't do anything if r and range don't intersect.
        if (r->compareBoundaryPoints(Range::START_TO_END, range.get(), ec) < 1) {
            if (r->compareBoundaryPoints(Range::END_TO_END, range.get(), ec) == -1)
                // The original range contains r.
                setSelection(Selection(range.get()));
            else
                // The original range and r intersect.
                setSelection(Selection(range->startPosition(), r->endPosition(), DOWNSTREAM));
        }
    }
}

void SelectionController::setBaseAndExtent(Node *baseNode, int baseOffset, Node *extentNode, int extentOffset, ExceptionCode& ec)
{
    if (baseOffset < 0 || extentOffset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    VisiblePosition visibleBase = VisiblePosition(baseNode, baseOffset, DOWNSTREAM);
    VisiblePosition visibleExtent = VisiblePosition(extentNode, extentOffset, DOWNSTREAM);
    
    moveTo(visibleBase, visibleExtent);
}

void SelectionController::setPosition(Node *node, int offset, ExceptionCode& ec)
{
    if (offset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::collapse(Node *node, int offset, ExceptionCode& ec)
{
    if (offset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::collapseToEnd()
{
    moveTo(VisiblePosition(m_sel.end(), DOWNSTREAM));
}

void SelectionController::collapseToStart()
{
    moveTo(VisiblePosition(m_sel.start(), DOWNSTREAM));
}

void SelectionController::empty()
{
    moveTo(VisiblePosition());
}

void SelectionController::extend(Node *node, int offset, ExceptionCode& ec)
{
    if (offset < 0) {
        ec = INDEX_SIZE_ERR;
        return;
    }
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::layout()
{
    if (isNone() || !m_sel.start().node()->inDocument() || !m_sel.end().node()->inDocument()) {
        m_caretRect = IntRect();
        m_caretPositionOnLayout = IntPoint();
        return;
    }

    m_sel.start().node()->document()->updateRendering();
    
    m_caretRect = IntRect();
    m_caretPositionOnLayout = IntPoint();
        
    if (isCaret()) {
        Position pos = m_sel.start();
        pos = VisiblePosition(m_sel.start(), m_sel.affinity()).deepEquivalent();
        if (pos.isNotNull()) {
            ASSERT(pos.node()->renderer());
            m_caretRect = pos.node()->renderer()->caretRect(pos.offset(), m_sel.affinity());
            
            int x, y;
            pos.node()->renderer()->absolutePositionForContent(x, y);
            m_caretPositionOnLayout = IntPoint(x, y);
        }
    }

    m_needsLayout = false;
}

IntRect SelectionController::caretRect() const
{
    if (m_needsLayout)
        const_cast<SelectionController *>(this)->layout();
    
    IntRect caret = m_caretRect;

    if (m_sel.start().node() && m_sel.start().node()->renderer()) {
        int x, y;
        m_sel.start().node()->renderer()->absolutePositionForContent(x, y);
        caret.move(IntPoint(x, y) - m_caretPositionOnLayout);
    }

    return caret;
}

static IntRect repaintRectForCaret(IntRect caret)
{
    if (caret.isEmpty())
        return IntRect();
    caret.inflate(1);
    return caret;
}

IntRect SelectionController::caretRepaintRect() const
{
    return repaintRectForCaret(caretRect());
}

bool SelectionController::recomputeCaretRect()
{
    if (!m_frame || !m_frame->document())
        return false;
        
    FrameView* v = m_frame->document()->view();
    if (!v)
        return false;

    if (!m_needsLayout)
        return false;

    IntRect oldRect = m_caretRect;
    m_needsLayout = true;
    IntRect newRect = caretRect();
    if (oldRect == newRect)
        return false;

    v->updateContents(repaintRectForCaret(oldRect), false);
    v->updateContents(repaintRectForCaret(newRect), false);
    return true;
}

void SelectionController::invalidateCaretRect()
{
    if (!isCaret())
        return;

    FrameView* v = m_sel.start().node()->document()->view();
    if (!v)
        return;

    bool caretRectChanged = recomputeCaretRect();

    // EDIT FIXME: This is an unfortunate hack.
    // Basically, we can't trust this layout position since we 
    // can't guarantee that the check to see if we are in unrendered 
    // content will work at this point. We may have to wait for
    // a layout and re-render of the document to happen. So, resetting this
    // flag will cause another caret layout to happen the first time
    // that we try to paint the caret after this call. That one will work since
    // it happens after the document has accounted for any editing
    // changes which may have been done.
    // And, we need to leave this layout here so the caret moves right 
    // away after clicking.
    m_needsLayout = true;

    if (!caretRectChanged)
        v->updateContents(caretRepaintRect(), false);
}

void SelectionController::paintCaret(GraphicsContext *p, const IntRect &rect)
{
    if (! m_sel.isCaret())
        return;

    if (m_needsLayout)
        layout();
        
    IntRect caret = intersection(caretRect(), rect);
    if (!caret.isEmpty()) {
        Color caretColor = Color::black;
        Element* element = rootEditableElement();
        if (element && element->renderer())
            caretColor = element->renderer()->style()->color();

        p->fillRect(caret, caretColor);
    }
}

void SelectionController::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        Element *element = static_cast<Element *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->localName().deprecatedString().latin1());
    }
    else if (r->isText()) {
        RenderText* textRenderer = static_cast<RenderText*>(r);
        if (textRenderer->textLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        DeprecatedString text = String(textRenderer->text()).deprecatedString();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (r->node() == m_sel.start().node())
                offset = m_sel.start().offset();
            else if (r->node() == m_sel.end().node())
                offset = m_sel.end().offset();
                
            int pos;
            InlineTextBox *box = textRenderer->findNextInlineTextBox(offset, pos);
            text = text.mid(box->m_start, box->m_len);
            
            DeprecatedString show;
            int mid = max / 2;
            int caret = 0;
            
            // text is shorter than max
            if (textLength < max) {
                show = text;
                caret = pos;
            }
            
            // too few characters to left
            else if (pos - mid < 0) {
                show = text.left(max - 3) + "...";
                caret = pos;
            }
            
            // enough characters on each side
            else if (pos - mid >= 0 && pos + mid <= textLength) {
                show = "..." + text.mid(pos - mid + 3, max - 6) + "...";
                caret = mid;
            }
            
            // too few characters on right
            else {
                show = "..." + text.right(max - 3);
                caret = pos - (textLength - show.length());
            }
            
            show.replace('\n', ' ');
            show.replace('\r', ' ');
            fprintf(stderr, "==> #text : \"%s\" at offset %d\n", show.latin1(), pos);
            fprintf(stderr, "           ");
            for (int i = 0; i < caret; i++)
                fprintf(stderr, " ");
            fprintf(stderr, "^\n");
        }
        else {
            if ((int)text.length() > max)
                text = text.left(max - 3) + "...";
            else
                text = text.left(max);
            fprintf(stderr, "    #text : \"%s\"\n", text.latin1());
        }
    }
}

bool SelectionController::contains(const IntPoint& point)
{
    Document* document = m_frame->document();
    
    // Treat a collapsed selection like no selection.
    if (!isRange())
        return false;
    if (!document->renderer()) 
        return false;
    
    HitTestRequest request(true, true);
    HitTestResult result(point);
    document->renderer()->layer()->hitTest(request, result);
    Node* innerNode = result.innerNode();
    if (!innerNode || !innerNode->renderer())
        return false;
    
    VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(result.localPoint()));
    if (visiblePos.isNull())
        return false;
        
    if (m_sel.visibleStart().isNull() || m_sel.visibleEnd().isNull())
        return false;
        
    Position start(m_sel.visibleStart().deepEquivalent());
    Position end(m_sel.visibleEnd().deepEquivalent());
    Position p(visiblePos.deepEquivalent());

    return comparePositions(start, p) <= 0 && comparePositions(p, end) <= 0;
}

// Workaround for the fact that it's hard to delete a frame.
// Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
// Can't do this implicitly as part of every setSelection call because in some contexts it might not be good
// for the focus to move to another frame. So instead we call it from places where we are selecting with the
// mouse or the keyboard after setting the selection.
void SelectionController::selectFrameElementInParentIfFullySelected()
{
    // Find the parent frame; if there is none, then we have nothing to do.
    Frame* parent = m_frame->tree()->parent();
    if (!parent)
        return;
    Page* page = m_frame->page();
    if (!page)
        return;

    // Check if the selection contains the entire frame contents; if not, then there is nothing to do.
    if (!isRange())
        return;
    if (!isStartOfDocument(selection().visibleStart()))
        return;
    if (!isEndOfDocument(selection().visibleEnd()))
        return;

    // Get to the <iframe> or <frame> (or even <object>) element in the parent frame.
    Document* doc = m_frame->document();
    if (!doc)
        return;
    Element* ownerElement = doc->ownerElement();
    if (!ownerElement)
        return;
    Node* ownerElementParent = ownerElement->parentNode();
    if (!ownerElementParent)
        return;
        
    // This method's purpose is it to make it easier to select iframes (in order to delete them).  Don't do anything if the iframe isn't deletable.
    if (!ownerElementParent->isContentEditable())
        return;

    // Create compute positions before and after the element.
    unsigned ownerElementNodeIndex = ownerElement->nodeIndex();
    VisiblePosition beforeOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex, SEL_DEFAULT_AFFINITY));
    VisiblePosition afterOwnerElement(VisiblePosition(ownerElementParent, ownerElementNodeIndex + 1, VP_UPSTREAM_IF_POSSIBLE));

    // Focus on the parent frame, and then select from before this element to after.
    Selection newSelection(beforeOwnerElement, afterOwnerElement);
    if (parent->shouldChangeSelection(newSelection)) {
        page->focusController()->setFocusedFrame(parent);
        parent->selectionController()->setSelection(newSelection);
    }
}

void SelectionController::selectAll()
{
    Document* document = m_frame->document();
    if (!document)
        return;
    
    if (document->focusedNode() && document->focusedNode()->canSelectAll()) {
        document->focusedNode()->selectAll();
        return;
    }
    
    Node* root = isContentEditable() ? highestEditableRoot(m_sel.start()) : document->documentElement();
    if (!root)
        return;
    Selection newSelection(Selection::selectionFromContentsOfNode(root));
    if (m_frame->shouldChangeSelection(newSelection))
        setSelection(newSelection);
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(true);
}

void SelectionController::setSelectedRange(Range* range, EAffinity affinity, bool closeTyping, ExceptionCode& ec)
{
    ec = 0;
    
    if (!range) {
        ec = INVALID_STATE_ERR;
        return;
    }
    
    Node* startContainer = range->startContainer(ec);
    if (ec)
        return;

    Node* endContainer = range->endContainer(ec);
    if (ec)
        return;
    
    ASSERT(startContainer);
    ASSERT(endContainer);
    ASSERT(startContainer->document() == endContainer->document());
    
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    // Non-collapsed ranges are not allowed to start at the end of a line that is wrapped,
    // they start at the beginning of the next line instead
    bool collapsed = range->collapsed(ec);
    if (ec)
        return;
    
    int startOffset = range->startOffset(ec);
    if (ec)
        return;

    int endOffset = range->endOffset(ec);
    if (ec)
        return;
    
    // FIXME: Can we provide extentAffinity?
    VisiblePosition visibleStart(startContainer, startOffset, collapsed ? affinity : DOWNSTREAM);
    VisiblePosition visibleEnd(endContainer, endOffset, SEL_DEFAULT_AFFINITY);
    setSelection(Selection(visibleStart, visibleEnd), closeTyping);
}

bool SelectionController::isInPasswordField() const
{
    Node* startNode = start().node();
    if (!startNode)
        return false;

    startNode = startNode->shadowAncestorNode();
    if (!startNode)
        return false;

    if (!startNode->hasTagName(inputTag))
        return false;
    
    return static_cast<HTMLInputElement*>(startNode)->inputType() == HTMLInputElement::PASSWORD;
}

bool SelectionController::isInsideNode() const
{
    Node* startNode = start().node();
    if (!startNode)
        return false;
    return !isTableElement(startNode) && !editingIgnoresContent(startNode);
}
  
#ifndef NDEBUG

void SelectionController::formatForDebugger(char* buffer, unsigned length) const
{
    m_sel.formatForDebugger(buffer, length);
}

void SelectionController::showTreeForThis() const
{
    m_sel.showTreeForThis();
}

#endif

}

#ifndef NDEBUG

void showTree(const WebCore::SelectionController& sel)
{
    sel.showTreeForThis();
}

void showTree(const WebCore::SelectionController* sel)
{
    if (sel)
        sel->showTreeForThis();
}

#endif
