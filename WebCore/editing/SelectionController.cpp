/*
 * Copyright (C) 2004, 2008, 2009, 2010 Apple Inc. All rights reserved.
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
#include "EventHandler.h"
#include "ExceptionCode.h"
#include "FocusController.h"
#include "FloatQuad.h"
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
#include "RenderTheme.h"
#include "RenderView.h"
#include "SecureTextInput.h"
#include "Settings.h"
#include "TextIterator.h"
#include "TypingCommand.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <stdio.h>
#include <wtf/text/CString.h>

#define EDIT_DEBUG 0

namespace WebCore {

using namespace HTMLNames;

const int NoXPosForVerticalArrowNavigation = INT_MIN;

SelectionController::SelectionController(Frame* frame, bool isDragCaretController)
    : m_frame(frame)
    , m_xPosForVerticalArrowNavigation(NoXPosForVerticalArrowNavigation)
    , m_granularity(CharacterGranularity)
    , m_caretBlinkTimer(this, &SelectionController::caretBlinkTimerFired)
    , m_needsLayout(true)
    , m_absCaretBoundsDirty(true)
    , m_isDragCaretController(isDragCaretController)
    , m_isCaretBlinkingSuspended(false)
    , m_focused(frame && frame->page() && frame->page()->focusController()->focusedFrame() == frame)
    , m_caretVisible(isDragCaretController)
    , m_caretPaint(true)
{
    setIsDirectional(false);
}

void SelectionController::moveTo(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(VisibleSelection(pos.deepEquivalent(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const VisiblePosition &base, const VisiblePosition &extent, bool userTriggered)
{
    setSelection(VisibleSelection(base.deepEquivalent(), extent.deepEquivalent(), base.affinity()), true, true, userTriggered);
}

void SelectionController::moveTo(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(VisibleSelection(pos, affinity), true, true, userTriggered);
}

void SelectionController::moveTo(const Range *r, EAffinity affinity, bool userTriggered)
{
    VisibleSelection selection = r ? VisibleSelection(r->startPosition(), r->endPosition(), affinity) : VisibleSelection(Position(), Position(), affinity);
    setSelection(selection, true, true, userTriggered);
}

void SelectionController::moveTo(const Position &base, const Position &extent, EAffinity affinity, bool userTriggered)
{
    setSelection(VisibleSelection(base, extent, affinity), true, true, userTriggered);
}

void SelectionController::setSelection(const VisibleSelection& s, bool closeTyping, bool clearTypingStyle, bool userTriggered, TextGranularity granularity)
{
    m_granularity = granularity;

    setIsDirectional(false);

    if (m_isDragCaretController) {
        invalidateCaretRect();
        m_selection = s;
        m_needsLayout = true;
        invalidateCaretRect();
        return;
    }
    if (!m_frame) {
        m_selection = s;
        return;
    }

    Node* baseNode = s.base().node();
    Document* document = 0;
    if (baseNode)
        document = baseNode->document();
    
    // <http://bugs.webkit.org/show_bug.cgi?id=23464>: Infinite recursion at SelectionController::setSelection
    // if document->frame() == m_frame we can get into an infinite loop
    if (document && document->frame() != m_frame && document != m_frame->document()) {
        document->frame()->selection()->setSelection(s, closeTyping, clearTypingStyle, userTriggered);
        return;
    }
    
    if (closeTyping)
        TypingCommand::closeTyping(m_frame->editor()->lastEditCommand());

    if (clearTypingStyle)
        m_frame->clearTypingStyle();
        
    if (m_selection == s)
        return;
    
    VisibleSelection oldSelection = m_selection;

    m_selection = s;
    
    m_needsLayout = true;
    
    if (!s.isNone())
        m_frame->setFocusedNodeIfNeeded();
    
    updateAppearance();

    // Always clear the x position used for vertical arrow navigation.
    // It will be restored by the vertical arrow navigation code if necessary.
    m_xPosForVerticalArrowNavigation = NoXPosForVerticalArrowNavigation;
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(userTriggered);
    m_frame->respondToChangedSelection(oldSelection, closeTyping);
    if (userTriggered)
        m_frame->revealSelection(ScrollAlignment::alignToEdgeIfNeeded, true);

    notifyAccessibilityForSelectionChange();
}

static bool removingNodeRemovesPosition(Node* node, const Position& position)
{
    if (!position.node())
        return false;
        
    if (position.node() == node)
        return true;
    
    if (!node->isElementNode())
        return false;
    
    Element* element = static_cast<Element*>(node);
    return element->contains(position.node()) || element->contains(position.node()->shadowAncestorNode());
}

void SelectionController::nodeWillBeRemoved(Node *node)
{
    if (isNone())
        return;
        
    // There can't be a selection inside a fragment, so if a fragment's node is being removed,
    // the selection in the document that created the fragment needs no adjustment.
    if (node && highestAncestor(node)->nodeType() == Node::DOCUMENT_FRAGMENT_NODE)
        return;
    
    bool baseRemoved = removingNodeRemovesPosition(node, m_selection.base());
    bool extentRemoved = removingNodeRemovesPosition(node, m_selection.extent());
    bool startRemoved = removingNodeRemovesPosition(node, m_selection.start());
    bool endRemoved = removingNodeRemovesPosition(node, m_selection.end());
    
    bool clearRenderTreeSelection = false;
    bool clearDOMTreeSelection = false;

    if (startRemoved || endRemoved) {
        // FIXME: When endpoints are removed, we should just alter the selection, instead of blowing it away.
        clearRenderTreeSelection = true;
        clearDOMTreeSelection = true;
    } else if (baseRemoved || extentRemoved) {
        // The base and/or extent are about to be removed, but the start and end aren't.
        // Change the base and extent to the start and end, but don't re-validate the
        // selection, since doing so could move the start and end into the node
        // that is about to be removed.
        if (m_selection.isBaseFirst())
            m_selection.setWithoutValidation(m_selection.start(), m_selection.end());
        else
            m_selection.setWithoutValidation(m_selection.end(), m_selection.start());
    // FIXME: This could be more efficient if we had an isNodeInRange function on Ranges.
    } else if (comparePositions(m_selection.start(), Position(node, 0)) == -1 && comparePositions(m_selection.end(), Position(node, 0)) == 1) {
        // If we did nothing here, when this node's renderer was destroyed, the rect that it 
        // occupied would be invalidated, but, selection gaps that change as a result of 
        // the removal wouldn't be invalidated.
        // FIXME: Don't do so much unnecessary invalidation.
        clearRenderTreeSelection = true;
    }

    if (clearRenderTreeSelection) {
        RefPtr<Document> document = m_selection.start().node()->document();
        document->updateStyleIfNeeded();
        if (RenderView* view = toRenderView(document->renderer()))
            view->clearSelection();
    }

    if (clearDOMTreeSelection)
        setSelection(VisibleSelection(), false, false);
}
    
void SelectionController::setIsDirectional(bool isDirectional)
{
    Settings* settings = m_frame ? m_frame->settings() : 0;
    m_isDirectional = !settings || settings->editingBehavior() != EditingMacBehavior || isDirectional;
}

void SelectionController::willBeModified(EAlteration alter, EDirection direction)
{
    if (alter != EXTEND)
        return;

    Position start = m_selection.start();
    Position end = m_selection.end();

    if (m_isDirectional) {
        // Make base and extent match start and end so we extend the user-visible selection.
        // This only matters for cases where base and extend point to different positions than
        // start and end (e.g. after a double-click to select a word).
        if (m_selection.isBaseFirst()) {
            m_selection.setBase(start);
            m_selection.setExtent(end);            
        } else {
            m_selection.setBase(end);
            m_selection.setExtent(start);
        }
    } else {
        // FIXME: This is probably not correct for right and left when the direction is RTL.
        switch (direction) {
        case RIGHT:
        case FORWARD:
            m_selection.setBase(start);
            m_selection.setExtent(end);
            break;
        case LEFT:
        case BACKWARD:
            m_selection.setBase(end);
            m_selection.setExtent(start);
            break;
        }
    }
}

TextDirection SelectionController::directionOfEnclosingBlock()
{
    Node* n = m_selection.extent().node();
    Node* enclosingBlockNode = enclosingBlock(n);
    if (!enclosingBlockNode)
        return LTR;
    RenderObject* renderer = enclosingBlockNode->renderer();
    if (renderer)
        return renderer->style()->direction();
    return LTR;
}

VisiblePosition SelectionController::positionForPlatform(bool isGetStart) const
{
    Position pos;
    Settings* settings = m_frame ? m_frame->settings() : 0;
    if (settings && settings->editingBehavior() == EditingMacBehavior)
        pos = isGetStart ? m_selection.start() : m_selection.end();
    else {
        // Linux and Windows always extend selections from the extent endpoint.
        // FIXME: VisibleSelection should be fixed to ensure as an invariant that
        // base/extent always point to the same nodes as start/end, but which points
        // to which depends on the value of isBaseFirst. Then this can be changed
        // to just return m_sel.extent().
        pos = m_selection.isBaseFirst() ? m_selection.end() : m_selection.start();
    }
    return VisiblePosition(pos, m_selection.affinity());
}

VisiblePosition SelectionController::startForPlatform() const
{
    return positionForPlatform(true);
}

VisiblePosition SelectionController::endForPlatform() const
{
    return positionForPlatform(false);
}

VisiblePosition SelectionController::modifyExtendingRight(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());

    // The difference between modifyExtendingRight and modifyExtendingForward is:
    // modifyExtendingForward always extends forward logically.
    // modifyExtendingRight behaves the same as modifyExtendingForward except for extending character or word,
    // it extends forward logically if the enclosing block is LTR direction,
    // but it extends backward logically if the enclosing block is RTL direction.
    switch (granularity) {
        case CharacterGranularity:
            if (directionOfEnclosingBlock() == LTR)
                pos = pos.next(true);                
            else
                pos = pos.previous(true);
            break;
        case WordGranularity:
            if (directionOfEnclosingBlock() == LTR)
                pos = nextWordPosition(pos);
            else
                pos = previousWordPosition(pos);
            break;
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            // FIXME: implement all of the above?
            pos = modifyExtendingForward(granularity);
    }
    return pos;
}
        
VisiblePosition SelectionController::modifyExtendingForward(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());
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
            pos = endOfSentence(endForPlatform());
            break;
        case LineBoundary:
            pos = endForPlatform();
            pos.setAffinity(UPSTREAM);
            pos = logicalEndOfLine(pos);
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(endForPlatform());
            break;
        case DocumentBoundary:
            pos = endForPlatform();
            if (isEditablePosition(pos.deepEquivalent()))
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
    }
    
    return pos;
}

VisiblePosition SelectionController::modifyMovingRight(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_selection.end(), m_selection.affinity());
            else
                pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).right(true);
            break;
        case WordGranularity:
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            // FIXME: Implement all of the above.
            pos = modifyMovingForward(granularity);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingForward(TextGranularity granularity)
{
    VisiblePosition pos;
    // FIXME: Stay in editable content for the less common granularities.
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_selection.end(), m_selection.affinity());
            else
                pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).next(true);
            break;
        case WordGranularity:
            pos = nextWordPosition(VisiblePosition(m_selection.extent(), m_selection.affinity()));
            break;
        case SentenceGranularity:
            pos = nextSentencePosition(VisiblePosition(m_selection.extent(), m_selection.affinity()));
            break;
        case LineGranularity: {
            // down-arrowing from a range selection that ends at the start of a line needs
            // to leave the selection at that line start (no need to call nextLinePosition!)
            pos = endForPlatform();
            if (!isRange() || !isStartOfLine(pos))
                pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(START));
            break;
        }
        case ParagraphGranularity:
            pos = nextParagraphPosition(endForPlatform(), xPosForVerticalArrowNavigation(START));
            break;
        case SentenceBoundary:
            pos = endOfSentence(endForPlatform());
            break;
        case LineBoundary:
            pos = logicalEndOfLine(endForPlatform());
            break;
        case ParagraphBoundary:
            pos = endOfParagraph(endForPlatform());
            break;
        case DocumentBoundary:
            pos = endForPlatform();
            if (isEditablePosition(pos.deepEquivalent()))
                pos = endOfEditableContent(pos);
            else
                pos = endOfDocument(pos);
            break;
            
    }
    return pos;
}

VisiblePosition SelectionController::modifyExtendingLeft(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());

    // The difference between modifyExtendingLeft and modifyExtendingBackward is:
    // modifyExtendingBackward always extends backward logically.
    // modifyExtendingLeft behaves the same as modifyExtendingBackward except for extending character or word,
    // it extends backward logically if the enclosing block is LTR direction,
    // but it extends forward logically if the enclosing block is RTL direction.
    switch (granularity) {
        case CharacterGranularity:
            if (directionOfEnclosingBlock() == LTR)
                pos = pos.previous(true);
            else
                pos = pos.next(true);
            break;
        case WordGranularity:
            if (directionOfEnclosingBlock() == LTR)
                pos = previousWordPosition(pos);
            else
                pos = nextWordPosition(pos);
            break;
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            pos = modifyExtendingBackward(granularity);
    }
    return pos;
}
       
VisiblePosition SelectionController::modifyExtendingBackward(TextGranularity granularity)
{
    VisiblePosition pos(m_selection.extent(), m_selection.affinity());

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
            pos = startOfSentence(startForPlatform());
            break;
        case LineBoundary:
            pos = logicalStartOfLine(startForPlatform());
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(startForPlatform());
            break;
        case DocumentBoundary:
            pos = startForPlatform();
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingLeft(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_selection.start(), m_selection.affinity());
            else
                pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).left(true);
            break;
        case WordGranularity:
        case SentenceGranularity:
        case LineGranularity:
        case ParagraphGranularity:
        case SentenceBoundary:
        case LineBoundary:
        case ParagraphBoundary:
        case DocumentBoundary:
            // FIXME: Implement all of the above.
            pos = modifyMovingBackward(granularity);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingBackward(TextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CharacterGranularity:
            if (isRange()) 
                pos = VisiblePosition(m_selection.start(), m_selection.affinity());
            else
                pos = VisiblePosition(m_selection.extent(), m_selection.affinity()).previous(true);
            break;
        case WordGranularity:
            pos = previousWordPosition(VisiblePosition(m_selection.extent(), m_selection.affinity()));
            break;
        case SentenceGranularity:
            pos = previousSentencePosition(VisiblePosition(m_selection.extent(), m_selection.affinity()));
            break;
        case LineGranularity:
            pos = previousLinePosition(startForPlatform(), xPosForVerticalArrowNavigation(START));
            break;
        case ParagraphGranularity:
            pos = previousParagraphPosition(startForPlatform(), xPosForVerticalArrowNavigation(START));
            break;
        case SentenceBoundary:
            pos = startOfSentence(startForPlatform());
            break;
        case LineBoundary:
            pos = logicalStartOfLine(startForPlatform());
            break;
        case ParagraphBoundary:
            pos = startOfParagraph(startForPlatform());
            break;
        case DocumentBoundary:
            pos = startForPlatform();
            if (isEditablePosition(pos.deepEquivalent()))
                pos = startOfEditableContent(pos);
            else 
                pos = startOfDocument(pos);
            break;
    }
    return pos;
}

bool SelectionController::modify(EAlteration alter, EDirection dir, TextGranularity granularity, bool userTriggered)
{
    Settings* settings = m_frame ? m_frame->settings() : 0;
    return modify(alter, dir, granularity, userTriggered, settings);
}
    
static bool isBoundary(TextGranularity granularity)
{
    return granularity == LineBoundary || granularity == ParagraphBoundary || granularity == DocumentBoundary;
}    
    
bool SelectionController::modify(EAlteration alter, EDirection direction, TextGranularity granularity, bool userTriggered, Settings* settings)
{
    if (userTriggered) {
        SelectionController trialSelectionController;
        trialSelectionController.setSelection(m_selection);
        trialSelectionController.setIsDirectional(m_isDirectional);
        trialSelectionController.modify(alter, direction, granularity, false, settings);

        bool change = m_frame->shouldChangeSelection(trialSelectionController.selection());
        if (!change)
            return false;
    }

    willBeModified(alter, direction);

    VisiblePosition position;
    switch (direction) {
        case RIGHT:
            if (alter == MOVE)
                position = modifyMovingRight(granularity);
            else
                position = modifyExtendingRight(granularity);
            break;
        case FORWARD:
            if (alter == EXTEND)
                position = modifyExtendingForward(granularity);
            else
                position = modifyMovingForward(granularity);
            break;
        case LEFT:
            if (alter == MOVE)
                position = modifyMovingLeft(granularity);
            else
                position = modifyExtendingLeft(granularity);
            break;
        case BACKWARD:
            if (alter == EXTEND)
                position = modifyExtendingBackward(granularity);
            else
                position = modifyMovingBackward(granularity);
            break;
    }

    if (position.isNull())
        return false;

    // Some of the above operations set an xPosForVerticalArrowNavigation.
    // Setting a selection will clear it, so save it to possibly restore later.
    // Note: the START position type is arbitrary because it is unused, it would be
    // the requested position type if there were no xPosForVerticalArrowNavigation set.
    int x = xPosForVerticalArrowNavigation(START);

    switch (alter) {
        case MOVE:
            moveTo(position, userTriggered);
            break;
        case EXTEND:
            if (!settings || settings->editingBehavior() != EditingMacBehavior || m_selection.isCaret() || !isBoundary(granularity))
                setExtent(position, userTriggered);
            else {
                // Standard Mac behavior when extending to a boundary is grow the selection rather
                // than leaving the base in place and moving the extent. Matches NSTextView.
                if (direction == FORWARD || direction == RIGHT)
                    setEnd(position, userTriggered);
                else
                    setStart(position, userTriggered);
            }
    }
    
    if (granularity == LineGranularity || granularity == ParagraphGranularity)
        m_xPosForVerticalArrowNavigation = x;

    if (userTriggered)
        m_granularity = CharacterGranularity;


    setNeedsLayout();

    setIsDirectional(alter == EXTEND);

    return true;
}

// FIXME: Maybe baseline would be better?
static bool absoluteCaretY(const VisiblePosition &c, int &y)
{
    IntRect rect = c.absoluteCaretBounds();
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
        trialSelectionController.setSelection(m_selection);
        trialSelectionController.setIsDirectional(m_isDirectional);
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
            pos = VisiblePosition(up ? m_selection.start() : m_selection.end(), m_selection.affinity());
            xPos = xPosForVerticalArrowNavigation(up ? START : END);
            m_selection.setAffinity(up ? UPSTREAM : DOWNSTREAM);
            break;
        case EXTEND:
            pos = VisiblePosition(m_selection.extent(), m_selection.affinity());
            xPos = xPosForVerticalArrowNavigation(EXTENT);
            m_selection.setAffinity(DOWNSTREAM);
            break;
    }

    int startY;
    if (!absoluteCaretY(pos, startY))
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
        if (!absoluteCaretY(next, nextY))
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
        m_granularity = CharacterGranularity;

    setIsDirectional(alter == EXTEND);

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
            pos = m_selection.start();
            break;
        case END:
            pos = m_selection.end();
            break;
        case BASE:
            pos = m_selection.base();
            break;
        case EXTENT:
            pos = m_selection.extent();
            break;
    }

    Frame *frame = pos.node()->document()->frame();
    if (!frame)
        return x;
        
    if (m_xPosForVerticalArrowNavigation == NoXPosForVerticalArrowNavigation) {
        VisiblePosition visiblePosition(pos, m_selection.affinity());
        // VisiblePosition creation can fail here if a node containing the selection becomes visibility:hidden
        // after the selection is created and before this function is called.
        x = visiblePosition.isNotNull() ? visiblePosition.xOffsetForVerticalNavigation() : 0;
        m_xPosForVerticalArrowNavigation = x;
    }
    else
        x = m_xPosForVerticalArrowNavigation;
        
    return x;
}

void SelectionController::clear()
{
    m_granularity = CharacterGranularity;
    setSelection(VisibleSelection());
}

void SelectionController::setStart(const VisiblePosition &pos, bool userTriggered)
{
    if (m_selection.isBaseFirst())
        setBase(pos, userTriggered);
    else
        setExtent(pos, userTriggered);
}

void SelectionController::setEnd(const VisiblePosition &pos, bool userTriggered)
{
    if (m_selection.isBaseFirst())
        setExtent(pos, userTriggered);
    else
        setBase(pos, userTriggered);
}

void SelectionController::setBase(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(VisibleSelection(pos.deepEquivalent(), m_selection.extent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setExtent(const VisiblePosition &pos, bool userTriggered)
{
    setSelection(VisibleSelection(m_selection.base(), pos.deepEquivalent(), pos.affinity()), true, true, userTriggered);
}

void SelectionController::setBase(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(VisibleSelection(pos, m_selection.extent(), affinity), true, true, userTriggered);
}

void SelectionController::setExtent(const Position &pos, EAffinity affinity, bool userTriggered)
{
    setSelection(VisibleSelection(m_selection.base(), pos, affinity), true, true, userTriggered);
}

void SelectionController::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

void SelectionController::layout()
{
    if (isNone() || !m_selection.start().node()->inDocument() || !m_selection.end().node()->inDocument()) {
        m_caretRect = IntRect();
        return;
    }

    m_selection.start().node()->document()->updateStyleIfNeeded();
    
    m_caretRect = IntRect();
        
    if (isCaret()) {
        VisiblePosition pos(m_selection.start(), m_selection.affinity());
        if (pos.isNotNull()) {
            ASSERT(pos.deepEquivalent().node()->renderer());
            
            // First compute a rect local to the renderer at the selection start
            RenderObject* renderer;
            IntRect localRect = pos.localCaretRect(renderer);

            // Get the renderer that will be responsible for painting the caret (which
            // is either the renderer we just found, or one of its containers)
            RenderObject* caretPainter = caretRenderer();

            // Compute an offset between the renderer and the caretPainter
            bool unrooted = false;
            while (renderer != caretPainter) {
                RenderObject* containerObject = renderer->container();
                if (!containerObject) {
                    unrooted = true;
                    break;
                }
                localRect.move(renderer->offsetFromContainer(containerObject, localRect.location()));
                renderer = containerObject;
            }
            
            if (!unrooted)
                m_caretRect = localRect;
            
            m_absCaretBoundsDirty = true;
        }
    }

    m_needsLayout = false;
}

RenderObject* SelectionController::caretRenderer() const
{
    Node* node = m_selection.start().node();
    if (!node)
        return 0;

    RenderObject* renderer = node->renderer();
    if (!renderer)
        return 0;

    // if caretNode is a block and caret is inside it then caret should be painted by that block
    bool paintedByBlock = renderer->isBlockFlow() && caretRendersInsideNode(node);
    return paintedByBlock ? renderer : renderer->containingBlock();
}

IntRect SelectionController::localCaretRect() const
{
    if (m_needsLayout)
        const_cast<SelectionController*>(this)->layout();
    
    return m_caretRect;
}

IntRect SelectionController::absoluteBoundsForLocalRect(const IntRect& rect) const
{
    RenderObject* caretPainter = caretRenderer();
    if (!caretPainter)
        return IntRect();
        
    return caretPainter->localToAbsoluteQuad(FloatRect(rect)).enclosingBoundingBox();
}

IntRect SelectionController::absoluteCaretBounds()
{
    recomputeCaretRect();
    return m_absCaretBounds;
}

static IntRect repaintRectForCaret(IntRect caret)
{
    if (caret.isEmpty())
        return IntRect();
    // Ensure that the dirty rect intersects the block that paints the caret even in the case where
    // the caret itself is just outside the block. See <https://bugs.webkit.org/show_bug.cgi?id=19086>.
    caret.inflateX(1);
    return caret;
}

IntRect SelectionController::caretRepaintRect() const
{
    return absoluteBoundsForLocalRect(repaintRectForCaret(localCaretRect()));
}

bool SelectionController::recomputeCaretRect()
{
    if (!m_frame)
        return false;
        
    FrameView* v = m_frame->document()->view();
    if (!v)
        return false;

    if (!m_needsLayout)
        return false;

    IntRect oldRect = m_caretRect;
    IntRect newRect = localCaretRect();
    if (oldRect == newRect && !m_absCaretBoundsDirty)
        return false;

    IntRect oldAbsCaretBounds = m_absCaretBounds;
    // FIXME: Rename m_caretRect to m_localCaretRect.
    m_absCaretBounds = absoluteBoundsForLocalRect(m_caretRect);
    m_absCaretBoundsDirty = false;
    
    if (oldAbsCaretBounds == m_absCaretBounds)
        return false;
        
    IntRect oldAbsoluteCaretRepaintBounds = m_absoluteCaretRepaintBounds;
    // We believe that we need to inflate the local rect before transforming it to obtain the repaint bounds.
    m_absoluteCaretRepaintBounds = caretRepaintRect();

#if ENABLE(TEXT_CARET)    
    if (RenderView* view = toRenderView(m_frame->document()->renderer())) {
        // FIXME: make caret repainting container-aware.
        view->repaintRectangleInViewAndCompositedLayers(oldAbsoluteCaretRepaintBounds, false);
        if (shouldRepaintCaret(view))
            view->repaintRectangleInViewAndCompositedLayers(m_absoluteCaretRepaintBounds, false);
    }
#endif
    return true;
}

bool SelectionController::shouldRepaintCaret(const RenderView* view) const
{
    ASSERT(view);
    Frame* frame = view->frameView() ? view->frameView()->frame() : 0; // The frame where the selection started.
    bool caretBrowsing = frame && frame->settings() && frame->settings()->caretBrowsingEnabled();
    return (caretBrowsing || isContentEditable());
}

void SelectionController::invalidateCaretRect()
{
    if (!isCaret())
        return;

    Document* d = m_selection.start().node()->document();

    // recomputeCaretRect will always return false for the drag caret,
    // because its m_frame is always 0.
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

    if (!caretRectChanged) {
        RenderView* view = toRenderView(d->renderer());
        if (view && shouldRepaintCaret(view))
            view->repaintRectangleInViewAndCompositedLayers(caretRepaintRect(), false);
    }
}

void SelectionController::paintCaret(GraphicsContext* context, int tx, int ty, const IntRect& clipRect)
{
#if ENABLE(TEXT_CARET)
    if (!m_caretVisible)
        return;
    if (!m_caretPaint)
        return;
    if (!m_selection.isCaret())
        return;

    IntRect drawingRect = localCaretRect();
    drawingRect.move(tx, ty);
    IntRect caret = intersection(drawingRect, clipRect);
    if (caret.isEmpty())
        return;

    Color caretColor = Color::black;
    ColorSpace colorSpace = DeviceColorSpace;
    Element* element = rootEditableElement();
    if (element && element->renderer()) {
        caretColor = element->renderer()->style()->visitedDependentColor(CSSPropertyColor);
        colorSpace = element->renderer()->style()->colorSpace();
    }

    context->fillRect(caret, caretColor, colorSpace);
#else
    UNUSED_PARAM(context);
    UNUSED_PARAM(tx);
    UNUSED_PARAM(ty);
    UNUSED_PARAM(clipRect);
#endif
}

void SelectionController::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        Element *element = static_cast<Element *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->localName().string().utf8().data());
    }
    else if (r->isText()) {
        RenderText* textRenderer = toRenderText(r);
        if (textRenderer->textLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        String text = textRenderer->text();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (r->node() == m_selection.start().node())
                offset = m_selection.start().deprecatedEditingOffset();
            else if (r->node() == m_selection.end().node())
                offset = m_selection.end().deprecatedEditingOffset();
                
            int pos;
            InlineTextBox *box = textRenderer->findNextInlineTextBox(offset, pos);
            text = text.substring(box->start(), box->len());
            
            String show;
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
                show = "..." + text.substring(pos - mid + 3, max - 6) + "...";
                caret = mid;
            }
            
            // too few characters on right
            else {
                show = "..." + text.right(max - 3);
                caret = pos - (textLength - show.length());
            }
            
            show.replace('\n', ' ');
            show.replace('\r', ' ');
            fprintf(stderr, "==> #text : \"%s\" at offset %d\n", show.utf8().data(), pos);
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
            fprintf(stderr, "    #text : \"%s\"\n", text.utf8().data());
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
    
    HitTestRequest request(HitTestRequest::ReadOnly |
                           HitTestRequest::Active);
    HitTestResult result(point);
    document->renderView()->layer()->hitTest(request, result);
    Node* innerNode = result.innerNode();
    if (!innerNode || !innerNode->renderer())
        return false;
    
    VisiblePosition visiblePos(innerNode->renderer()->positionForPoint(result.localPoint()));
    if (visiblePos.isNull())
        return false;
        
    if (m_selection.visibleStart().isNull() || m_selection.visibleEnd().isNull())
        return false;
        
    Position start(m_selection.visibleStart().deepEquivalent());
    Position end(m_selection.visibleEnd().deepEquivalent());
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
    VisibleSelection newSelection(beforeOwnerElement, afterOwnerElement);
    if (parent->shouldChangeSelection(newSelection)) {
        page->focusController()->setFocusedFrame(parent);
        parent->selection()->setSelection(newSelection);
    }
}

void SelectionController::selectAll()
{
    Document* document = m_frame->document();
    
    if (document->focusedNode() && document->focusedNode()->canSelectAll()) {
        document->focusedNode()->selectAll();
        return;
    }
    
    Node* root = 0;
    if (isContentEditable())
        root = highestEditableRoot(m_selection.start());
    else {
        root = shadowTreeRootNode();
        if (!root)
            root = document->documentElement();
    }
    if (!root)
        return;
    VisibleSelection newSelection(VisibleSelection::selectionFromContentsOfNode(root));
    if (m_frame->shouldChangeSelection(newSelection))
        setSelection(newSelection);
    selectFrameElementInParentIfFullySelected();
    m_frame->notifyRendererOfSelectionChange(true);
}

bool SelectionController::setSelectedRange(Range* range, EAffinity affinity, bool closeTyping)
{
    if (!range)
        return false;

    ExceptionCode ec = 0;
    Node* startContainer = range->startContainer(ec);
    if (ec)
        return false;

    Node* endContainer = range->endContainer(ec);
    if (ec)
        return false;
    
    ASSERT(startContainer);
    ASSERT(endContainer);
    ASSERT(startContainer->document() == endContainer->document());
    
    m_frame->document()->updateLayoutIgnorePendingStylesheets();

    // Non-collapsed ranges are not allowed to start at the end of a line that is wrapped,
    // they start at the beginning of the next line instead
    bool collapsed = range->collapsed(ec);
    if (ec)
        return false;
    
    int startOffset = range->startOffset(ec);
    if (ec)
        return false;

    int endOffset = range->endOffset(ec);
    if (ec)
        return false;
    
    // FIXME: Can we provide extentAffinity?
    VisiblePosition visibleStart(startContainer, startOffset, collapsed ? affinity : DOWNSTREAM);
    VisiblePosition visibleEnd(endContainer, endOffset, SEL_DEFAULT_AFFINITY);
    setSelection(VisibleSelection(visibleStart, visibleEnd), closeTyping);
    return true;
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

bool SelectionController::caretRendersInsideNode(Node* node) const
{
    if (!node)
        return false;
    return !isTableElement(node) && !editingIgnoresContent(node);
}

void SelectionController::focusedOrActiveStateChanged()
{
    bool activeAndFocused = isFocusedAndActive();

    // Because RenderObject::selectionBackgroundColor() and
    // RenderObject::selectionForegroundColor() check if the frame is active,
    // we have to update places those colors were painted.
    if (RenderView* view = toRenderView(m_frame->document()->renderer()))
        view->repaintRectangleInViewAndCompositedLayers(enclosingIntRect(m_frame->selectionBounds()));

    // Caret appears in the active frame.
    if (activeAndFocused)
        m_frame->setSelectionFromNone();
    setCaretVisible(activeAndFocused);

    // Update for caps lock state
    m_frame->eventHandler()->capsLockStateMayHaveChanged();

    // Because CSSStyleSelector::checkOneSelector() and
    // RenderTheme::isFocused() check if the frame is active, we have to
    // update style and theme state that depended on those.
    if (Node* node = m_frame->document()->focusedNode()) {
        node->setNeedsStyleRecalc();
        if (RenderObject* renderer = node->renderer())
            if (renderer && renderer->style()->hasAppearance())
                renderer->theme()->stateChanged(renderer, FocusState);
    }

    // Secure keyboard entry is set by the active frame.
    if (m_frame->document()->useSecureKeyboardEntryWhenActive())
        setUseSecureKeyboardEntry(activeAndFocused);
}

void SelectionController::pageActivationChanged()
{
    focusedOrActiveStateChanged();
}

void SelectionController::updateSecureKeyboardEntryIfActive()
{
    if (m_frame->document() && isFocusedAndActive())
        setUseSecureKeyboardEntry(m_frame->document()->useSecureKeyboardEntryWhenActive());
}

void SelectionController::setUseSecureKeyboardEntry(bool enable)
{
    if (enable)
        enableSecureTextInput();
    else
        disableSecureTextInput();
}

void SelectionController::setFocused(bool flag)
{
    if (m_focused == flag)
        return;
    m_focused = flag;

    focusedOrActiveStateChanged();
}

bool SelectionController::isFocusedAndActive() const
{
    return m_focused && m_frame->page() && m_frame->page()->focusController()->isActive();
}

void SelectionController::updateAppearance()
{
    ASSERT(!m_isDragCaretController);

#if ENABLE(TEXT_CARET)
    bool caretRectChanged = recomputeCaretRect();

    bool caretBrowsing = m_frame->settings() && m_frame->settings()->caretBrowsingEnabled();
    bool shouldBlink = m_caretVisible
        && isCaret() && (isContentEditable() || caretBrowsing);

    // If the caret moved, stop the blink timer so we can restart with a
    // black caret in the new location.
    if (caretRectChanged || !shouldBlink)
        m_caretBlinkTimer.stop();

    // Start blinking with a black caret. Be sure not to restart if we're
    // already blinking in the right location.
    if (shouldBlink && !m_caretBlinkTimer.isActive()) {
        if (double blinkInterval = m_frame->page()->theme()->caretBlinkInterval())
            m_caretBlinkTimer.startRepeating(blinkInterval);

        if (!m_caretPaint) {
            m_caretPaint = true;
            invalidateCaretRect();
        }
    }
#endif

    RenderView* view = m_frame->contentRenderer();
    if (!view)
        return;

    VisibleSelection selection = this->selection();

    if (!selection.isRange()) {
        view->clearSelection();
        return;
    }

    // Use the rightmost candidate for the start of the selection, and the leftmost candidate for the end of the selection.
    // Example: foo <a>bar</a>.  Imagine that a line wrap occurs after 'foo', and that 'bar' is selected.   If we pass [foo, 3]
    // as the start of the selection, the selection painting code will think that content on the line containing 'foo' is selected
    // and will fill the gap before 'bar'.
    Position startPos = selection.start();
    Position candidate = startPos.downstream();
    if (candidate.isCandidate())
        startPos = candidate;
    Position endPos = selection.end();
    candidate = endPos.upstream();
    if (candidate.isCandidate())
        endPos = candidate;

    // We can get into a state where the selection endpoints map to the same VisiblePosition when a selection is deleted
    // because we don't yet notify the SelectionController of text removal.
    if (startPos.isNotNull() && endPos.isNotNull() && selection.visibleStart() != selection.visibleEnd()) {
        RenderObject* startRenderer = startPos.node()->renderer();
        RenderObject* endRenderer = endPos.node()->renderer();
        view->setSelection(startRenderer, startPos.deprecatedEditingOffset(), endRenderer, endPos.deprecatedEditingOffset());
    }
}

void SelectionController::setCaretVisible(bool flag)
{
    if (m_caretVisible == flag)
        return;
    clearCaretRectIfNeeded();
    m_caretVisible = flag;
    updateAppearance();
}

void SelectionController::clearCaretRectIfNeeded()
{
#if ENABLE(TEXT_CARET)
    if (!m_caretPaint)
        return;
    m_caretPaint = false;
    invalidateCaretRect();
#endif
}

void SelectionController::caretBlinkTimerFired(Timer<SelectionController>*)
{
#if ENABLE(TEXT_CARET)
    ASSERT(m_caretVisible);
    ASSERT(isCaret());
    bool caretPaint = m_caretPaint;
    if (isCaretBlinkingSuspended() && caretPaint)
        return;
    m_caretPaint = !caretPaint;
    invalidateCaretRect();
#endif
}

#ifndef NDEBUG

void SelectionController::formatForDebugger(char* buffer, unsigned length) const
{
    m_selection.formatForDebugger(buffer, length);
}

void SelectionController::showTreeForThis() const
{
    m_selection.showTreeForThis();
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
