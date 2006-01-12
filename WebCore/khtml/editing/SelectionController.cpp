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

#include <qevent.h>
#include <qpainter.h>
#include <qrect.h>

#include "dom/dom_node.h"
#include "dom/dom_string.h"
#include "Frame.h"
#include "FrameView.h"
#include "htmlediting.h"
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "InlineTextBox.h"
#include "visible_position.h"
#include "visible_units.h"
#include "DocumentImpl.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_rangeimpl.h"

#include <kxmlcore/Assertions.h>

#define EDIT_DEBUG 0

using DOM::DOMString;
using DOM::ElementImpl;
using DOM::NodeImpl;
using DOM::Position;
using DOM::Range;
using DOM::RangeImpl;

namespace khtml {

SelectionController::SelectionController()
{
    init(DOWNSTREAM);
}

SelectionController::SelectionController(const Position &pos, EAffinity affinity)
    : m_base(pos), m_extent(pos)
{
    init(affinity);
    validate();
}

SelectionController::SelectionController(const RangeImpl *r, EAffinity baseAffinity, EAffinity extentAffinity)
    : m_base(startPosition(r)), m_extent(endPosition(r))
{
    init(baseAffinity);
    validate();
}

SelectionController::SelectionController(const Position &base, EAffinity baseAffinity, const Position &extent, EAffinity extentAffinity)
    : m_base(base), m_extent(extent)
{
    init(baseAffinity);
    validate();
}

SelectionController::SelectionController(const VisiblePosition &visiblePos)
    : m_base(visiblePos.deepEquivalent()), m_extent(visiblePos.deepEquivalent())
{
    init(visiblePos.affinity());
    validate();
}

SelectionController::SelectionController(const VisiblePosition &base, const VisiblePosition &extent)
    : m_base(base.deepEquivalent()), m_extent(extent.deepEquivalent())
{
    init(base.affinity());
    validate();
}

SelectionController::SelectionController(const SelectionController &o)
    : m_base(o.m_base), m_extent(o.m_extent)
    , m_start(o.m_start), m_end(o.m_end)
    , m_state(o.m_state), m_affinity(o.m_affinity)
    , m_baseIsFirst(o.m_baseIsFirst)
    , m_needsLayout(o.m_needsLayout)
    , m_modifyBiasSet(o.m_modifyBiasSet)
{
    // Only copy the coordinates over if the other object
    // has had a layout, otherwise keep the current
    // coordinates. This prevents drawing artifacts from
    // remaining when the caret is painted and then moves,
    // and the old rectangle needs to be repainted.
    if (!m_needsLayout) {
        m_caretRect = o.m_caretRect;
        m_caretPositionOnLayout = o.m_caretPositionOnLayout;
    }
}

void SelectionController::init(EAffinity affinity)
{
    // FIXME: set extentAffinity
    m_state = NONE; 
    m_baseIsFirst = true;
    m_affinity = affinity;
    m_needsLayout = true;
    m_modifyBiasSet = false;
}

SelectionController &SelectionController::operator=(const SelectionController &o)
{
    m_base = o.m_base;
    m_extent = o.m_extent;
    m_start = o.m_start;
    m_end = o.m_end;

    m_state = o.m_state;
    m_affinity = o.m_affinity;

    m_baseIsFirst = o.m_baseIsFirst;
    m_needsLayout = o.m_needsLayout;
    m_modifyBiasSet = o.m_modifyBiasSet;
    
    // Only copy the coordinates over if the other object
    // has had a layout, otherwise keep the current
    // coordinates. This prevents drawing artifacts from
    // remaining when the caret is painted and then moves,
    // and the old rectangle needs to be repainted.
    if (!m_needsLayout) {
        m_caretRect = o.m_caretRect;
        m_caretPositionOnLayout = o.m_caretPositionOnLayout;
    }
    
    return *this;
}

void SelectionController::moveTo(const VisiblePosition &pos)
{
    // FIXME: use extentAffinity
    m_affinity = pos.affinity();
    m_base = pos.deepEquivalent();
    m_extent = pos.deepEquivalent();
    validate();
}

void SelectionController::moveTo(const VisiblePosition &base, const VisiblePosition &extent)
{
    // FIXME: use extentAffinity
    m_affinity = base.affinity();
    m_base = base.deepEquivalent();
    m_extent = extent.deepEquivalent();
    validate();
}

void SelectionController::moveTo(const SelectionController &o)
{
    // FIXME: copy extentAffinity
    m_affinity = o.m_affinity;
    m_base = o.m_start;
    m_extent = o.m_end;
    validate();
}

void SelectionController::moveTo(const Position &pos, EAffinity affinity)
{
    // FIXME: use extentAffinity
    m_affinity = affinity;
    m_base = pos;
    m_extent = pos;
    validate();
}

void SelectionController::moveTo(const RangeImpl *r, EAffinity baseAffinity, EAffinity extentAffinity)
{
    // FIXME: use extentAffinity
    m_affinity = baseAffinity;
    m_base = startPosition(r);
    m_extent = endPosition(r);
    validate();
}

void SelectionController::moveTo(const Position &base, EAffinity baseAffinity, const Position &extent, EAffinity extentAffinity)
{
    // FIXME: use extentAffinity
    m_affinity = baseAffinity;
    m_base = base;
    m_extent = extent;
    validate();
}

void SelectionController::setModifyBias(EAlter alter, EDirection direction)
{
    switch (alter) {
        case MOVE:
            m_modifyBiasSet = false;
            break;
        case EXTEND:
            if (!m_modifyBiasSet) {
                m_modifyBiasSet = true;
                switch (direction) {
                    // FIXME: right for bidi?
                    case RIGHT:
                    case FORWARD:
                        m_base = m_start;
                        m_extent = m_end;
                        break;
                    case LEFT:
                    case BACKWARD:
                        m_base = m_end;
                        m_extent = m_start;
                        break;
                }
            }
            break;
    }
}

VisiblePosition SelectionController::modifyExtendingRightForward(ETextGranularity granularity)
{
    VisiblePosition pos(m_extent, m_affinity);
    switch (granularity) {
        case CHARACTER:
            pos = pos.next();
            break;
        case WORD:
            pos = nextWordPosition(pos);
            break;
        case PARAGRAPH:
            pos = nextParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE:
            pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE_BOUNDARY:
            pos = endOfLine(VisiblePosition(m_end, m_affinity));
            break;
        case PARAGRAPH_BOUNDARY:
            pos = endOfParagraph(VisiblePosition(m_end, m_affinity));
            break;
        case DOCUMENT_BOUNDARY:
            pos = endOfDocument(pos);
            break;
    }
    
    return pos;
}

VisiblePosition SelectionController::modifyMovingRightForward(ETextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CHARACTER:
            if (isRange()) 
                pos = VisiblePosition(m_end, m_affinity);
            else
                pos = VisiblePosition(m_extent, m_affinity).next();
            break;
        case WORD:
            pos = nextWordPosition(VisiblePosition(m_extent, m_affinity));
            break;
        case PARAGRAPH:
            pos = nextParagraphPosition(VisiblePosition(m_end, m_affinity), xPosForVerticalArrowNavigation(END, isRange()));
            break;
        case LINE: {
            // down-arrowing from a range selection that ends at the start of a line needs
            // to leave the selection at that line start (no need to call nextLinePosition!)
            pos = VisiblePosition(m_end, m_affinity);
            if (!isRange() || !isStartOfLine(pos))
                pos = nextLinePosition(pos, xPosForVerticalArrowNavigation(END, isRange()));
            break;
        }
        case LINE_BOUNDARY:
            pos = endOfLine(VisiblePosition(m_end, m_affinity));
            break;
        case PARAGRAPH_BOUNDARY:
            pos = endOfParagraph(VisiblePosition(m_end, m_affinity));
            break;
        case DOCUMENT_BOUNDARY:
            pos = endOfDocument(VisiblePosition(m_end, m_affinity));
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyExtendingLeftBackward(ETextGranularity granularity)
{
    VisiblePosition pos(m_extent, m_affinity);
    switch (granularity) {
        case CHARACTER:
            pos = pos.previous();
            break;
        case WORD:
            pos = previousWordPosition(pos);
            break;
        case PARAGRAPH:
            pos = previousParagraphPosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE:
            pos = previousLinePosition(pos, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE_BOUNDARY:
            pos = startOfLine(VisiblePosition(m_start, m_affinity));
            break;
        case PARAGRAPH_BOUNDARY:
            pos = startOfParagraph(VisiblePosition(m_start, m_affinity));
            break;
        case DOCUMENT_BOUNDARY:
            pos = startOfDocument(pos);
            break;
    }
    return pos;
}

VisiblePosition SelectionController::modifyMovingLeftBackward(ETextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CHARACTER:
            if (isRange()) 
                pos = VisiblePosition(m_start, m_affinity);
            else
                pos = VisiblePosition(m_extent, m_affinity).previous();
            break;
        case WORD:
            pos = previousWordPosition(VisiblePosition(m_extent, m_affinity));
            break;
        case PARAGRAPH:
            pos = previousParagraphPosition(VisiblePosition(m_start, m_affinity), xPosForVerticalArrowNavigation(START, isRange()));
            break;
        case LINE:
            pos = previousLinePosition(VisiblePosition(m_start, m_affinity), xPosForVerticalArrowNavigation(START, isRange()));
            break;
        case LINE_BOUNDARY:
            pos = startOfLine(VisiblePosition(m_start, m_affinity));
            break;
        case PARAGRAPH_BOUNDARY:
            pos = startOfParagraph(VisiblePosition(m_start, m_affinity));
            break;
        case DOCUMENT_BOUNDARY:
            pos = startOfDocument(VisiblePosition(m_start, m_affinity));
            break;
    }
    return pos;
}

bool SelectionController::modify(const DOMString &alterString, const DOMString &directionString, const DOMString &granularityString)
{
    DOMString alterStringLower = alterString.lower();
    EAlter alter;
    if (alterStringLower == "extend")
        alter = EXTEND;
    else if (alterStringLower == "move")
        alter = MOVE;
    else 
        return false;
    
    DOMString directionStringLower = directionString.lower();
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
        
    DOMString granularityStringLower = granularityString.lower();
    ETextGranularity granularity;
    if (granularityStringLower == "character")
        granularity = CHARACTER;
    else if (granularityStringLower == "word")
        granularity = WORD;
    else if (granularityStringLower == "line")
        granularity = LINE;
    else if (granularityStringLower == "paragraph")
        granularity = PARAGRAPH;
    else
        return false;
                
    return modify(alter, direction, granularity);
}

bool SelectionController::modify(EAlter alter, EDirection dir, ETextGranularity granularity)
{
    if (frame())
        frame()->setSelectionGranularity(granularity);
    
    setModifyBias(alter, dir);

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

    switch (alter) {
        case MOVE:
            moveTo(pos);
            break;
        case EXTEND:
            setExtent(pos);
            break;
    }

    setNeedsLayout();

    return true;
}

// FIXME: Maybe baseline would be better?
static bool caretY(const VisiblePosition &c, int &y)
{
    Position p = c.deepEquivalent();
    NodeImpl *n = p.node();
    if (!n)
        return false;
    RenderObject *r = p.node()->renderer();
    if (!r)
        return false;
    QRect rect = r->caretRect(p.offset());
    if (rect.isEmpty())
        return false;
    y = rect.y() + rect.height() / 2;
    return true;
}

bool SelectionController::modify(EAlter alter, int verticalDistance)
{
    if (verticalDistance == 0)
        return false;

    bool up = verticalDistance < 0;
    if (up)
        verticalDistance = -verticalDistance;

    // can dump this UPSTREAM when we have m_extentAffinity
    m_affinity = UPSTREAM;
    setModifyBias(alter, up ? BACKWARD : FORWARD);

    VisiblePosition pos;
    int xPos = 0;
    switch (alter) {
        case MOVE:
            pos = VisiblePosition(up ? m_start : m_end, m_affinity);
            xPos = xPosForVerticalArrowNavigation(up ? START : END, isRange());
            break;
        case EXTEND:
            pos = VisiblePosition(m_extent, m_affinity);
            xPos = xPosForVerticalArrowNavigation(EXTENT);
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
            moveTo(result);
            break;
        case EXTEND:
            setExtent(result);
            break;
    }

    return true;
}

bool SelectionController::expandUsingGranularity(ETextGranularity granularity)
{
    if (isNone())
        return false;
    
    validate(granularity);
    return true;
}

int SelectionController::xPosForVerticalArrowNavigation(EPositionType type, bool recalc) const
{
    int x = 0;

    if (isNone())
        return x;

    Position pos;
    switch (type) {
        case START:
            pos = m_start;
            break;
        case END:
            pos = m_end;
            break;
        case BASE:
            pos = m_base;
            break;
        case EXTENT:
            pos = m_extent;
            break;
    }

    Frame *frame = pos.node()->getDocument()->frame();
    if (!frame)
        return x;
        
    if (recalc || frame->xPosForVerticalArrowNavigation() == Frame::NoXPosForVerticalArrowNavigation) {
        pos = VisiblePosition(pos, m_affinity).deepEquivalent();
        x = pos.node()->renderer()->caretRect(pos.offset(), m_affinity).x();
        frame->setXPosForVerticalArrowNavigation(x);
    }
    else {
        x = frame->xPosForVerticalArrowNavigation();
    }
    return x;
}

void SelectionController::clear()
{
    m_affinity = SEL_DEFAULT_AFFINITY;
    m_base.clear();
    m_extent.clear();
    validate();
}

void SelectionController::setBase(const VisiblePosition &pos)
{
    m_affinity = pos.affinity();
    m_base = pos.deepEquivalent();
    validate();
}

void SelectionController::setExtent(const VisiblePosition &pos)
{
    // FIXME: Support extentAffinity
    m_extent = pos.deepEquivalent();
    validate();
}

void SelectionController::setBase(const Position &pos, EAffinity baseAffinity)
{
    m_affinity = baseAffinity;
    m_base = pos;
    validate();
}

void SelectionController::setExtent(const Position &pos, EAffinity extentAffinity)
{
    // FIXME: Support extentAffinity for real
    m_affinity = extentAffinity;
    m_extent = pos;
    validate();
}

void SelectionController::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

PassRefPtr<RangeImpl> SelectionController::toRange() const
{
    if (isNone())
        return 0;

    // Make sure we have an updated layout since this function is called
    // in the course of running edit commands which modify the DOM.
    // Failing to call this can result in equivalentXXXPosition calls returning
    // incorrect results.
    m_start.node()->getDocument()->updateLayout();

    Position s, e;
    if (isCaret()) {
        // If the selection is a caret, move the range start upstream. This helps us match
        // the conventions of text editors tested, which make style determinations based
        // on the character before the caret, if any. 
        s = m_start.upstream().equivalentRangeCompliantPosition();
        e = s;
    }
    else {
        // If the selection is a range, select the minimum range that encompasses the selection.
        // Again, this is to match the conventions of text editors tested, which make style 
        // determinations based on the first character of the selection. 
        // For instance, this operation helps to make sure that the "X" selected below is the 
        // only thing selected. The range should not be allowed to "leak" out to the end of the 
        // previous text node, or to the beginning of the next text node, each of which has a 
        // different style.
        // 
        // On a treasure map, <b>X</b> marks the spot.
        //                       ^ selected
        //
        ASSERT(isRange());
        s = m_start.downstream();
        e = m_end.upstream();
        if (RangeImpl::compareBoundaryPoints(s.node(), s.offset(), e.node(), e.offset()) > 0) {
            // Make sure the start is before the end.
            // The end can wind up before the start if collapsed whitespace is the only thing selected.
            Position tmp = s;
            s = e;
            e = tmp;
        }
        s = s.equivalentRangeCompliantPosition();
        e = e.equivalentRangeCompliantPosition();
    }

    int exceptionCode = 0;
    PassRefPtr<RangeImpl> result(new RangeImpl(s.node()->getDocument()));
    result->setStart(s.node(), s.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range start from SelectionController: %d", exceptionCode);
        return 0;
    }
    result->setEnd(e.node(), e.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range end from SelectionController: %d", exceptionCode);
        return 0;
    }
    return result;
}

DOMString SelectionController::type() const
{
    if (isNone())
        return DOMString("None");
    else if (isCaret())
        return DOMString("Caret");
    else
        return DOMString("Range");
}

DOMString SelectionController::toString() const
{
    return DOMString(plainText(toRange().get()));
}

PassRefPtr<RangeImpl> SelectionController::getRangeAt(int index) const
{
    return index == 0 ? toRange() : 0;
}

Frame *SelectionController::frame() const
{
    return !isNone() ? m_start.node()->getDocument()->frame() : 0;
}

void SelectionController::setBaseAndExtent(NodeImpl *baseNode, int baseOffset, NodeImpl *extentNode, int extentOffset)
{
    VisiblePosition visibleBase = VisiblePosition(baseNode, baseOffset, DOWNSTREAM);
    VisiblePosition visibleExtent = VisiblePosition(extentNode, extentOffset, DOWNSTREAM);
    
    moveTo(visibleBase, visibleExtent);
}

void SelectionController::setPosition(NodeImpl *node, int offset)
{
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::collapse(NodeImpl *node, int offset)
{
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::collapseToEnd()
{
    moveTo(VisiblePosition(m_end, DOWNSTREAM));
}

void SelectionController::collapseToStart()
{
    moveTo(VisiblePosition(m_start, DOWNSTREAM));
}

void SelectionController::empty()
{
    moveTo(SelectionController());
}

void SelectionController::extend(NodeImpl *node, int offset)
{
    moveTo(VisiblePosition(node, offset, DOWNSTREAM));
}

void SelectionController::layout()
{
    if (isNone() || !m_start.node()->inDocument() || !m_end.node()->inDocument()) {
        m_caretRect = QRect();
        m_caretPositionOnLayout = QPoint();
        return;
    }

    m_start.node()->getDocument()->updateRendering();
    
    m_caretRect = QRect();
    m_caretPositionOnLayout = QPoint();
        
    if (isCaret()) {
        Position pos = m_start;
        pos = VisiblePosition(m_start, m_affinity).deepEquivalent();
        if (pos.isNotNull()) {
            ASSERT(pos.node()->renderer());
            m_caretRect = pos.node()->renderer()->caretRect(pos.offset(), m_affinity);
            
            int x, y;
            pos.node()->renderer()->absolutePosition(x, y);
            m_caretPositionOnLayout = QPoint(x, y);
        }
    }

    m_needsLayout = false;
}

QRect SelectionController::caretRect() const
{
    if (m_needsLayout) {
        const_cast<SelectionController *>(this)->layout();
    }
    
    QRect caret = m_caretRect;
    
    if (m_start.node() && m_start.node()->renderer()) {
        int x, y;
        m_start.node()->renderer()->absolutePosition(x, y);
        QPoint diff = QPoint(x, y) - m_caretPositionOnLayout;
        caret.moveTopLeft(diff);
    }

    return caret;
}

QRect SelectionController::caretRepaintRect() const
{
    // FIXME: Add one pixel of slop on each side to make sure we don't leave behind artifacts.
    QRect r = caretRect();
    if (r.isEmpty())
        return QRect();
    return QRect(r.left() - 1, r.top() - 1, r.width() + 2, r.height() + 2);
}

void SelectionController::needsCaretRepaint()
{
    if (!isCaret())
        return;

    if (!m_start.node()->getDocument())
        return;

    KHTMLView *v = m_start.node()->getDocument()->view();
    if (!v)
        return;

    if (m_needsLayout) {
        // repaint old position and calculate new position
        v->updateContents(caretRepaintRect(), false);
        layout();
        
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
    }
    v->updateContents(caretRepaintRect(), false);
}

void SelectionController::paintCaret(QPainter *p, const QRect &rect)
{
    if (m_state != CARET)
        return;

    if (m_needsLayout)
        layout();
        
    QRect caret = caretRect();

    if (caret.isValid())
        p->fillRect(caret & rect, QBrush());
}

void SelectionController::adjustForEditableContent()
{
    if (m_base.isNull())
        return;

    NodeImpl *baseRoot = m_base.node()->rootEditableElement();
    NodeImpl *startRoot = m_start.node()->rootEditableElement();
    NodeImpl *endRoot = m_end.node()->rootEditableElement();
    
    // The base, start and end are all in the same region.  No adjustment necessary.
    if (baseRoot == startRoot && baseRoot == endRoot)
        return;
    
    // The selection is based in an editable area.  Keep both sides from reaching outside that area.
    if (baseRoot) {
        // If the start is outside the base's editable root, cap it at the start of that editable root.
        if (baseRoot != startRoot) {
            VisiblePosition first(Position(baseRoot, 0));
            m_start = first.deepEquivalent();
        }
        // If the end is outside the base's editable root, cap it at the end of that editable root.
        if (baseRoot != endRoot) {
            VisiblePosition last(Position(baseRoot, maxDeepOffset(baseRoot)));
            m_end = last.deepEquivalent();
        }
    // The selection is based outside editable content.  Keep both sides from reaching into editable content.
    } else {
        // The selection ends in editable content, move backward until non-editable content is reached.
        if (endRoot) {
            VisiblePosition previous;
            do {
                previous = VisiblePosition(Position(endRoot, 0)).previous();
                endRoot = previous.deepEquivalent().node()->rootEditableElement();
            } while (endRoot);
            
            ASSERT(!previous.isNull());
            m_end = previous.deepEquivalent();
        }
        // The selection starts in editable content, move forward until non-editable content is reached.
        if (startRoot) {
            VisiblePosition next;
            do {
                next = VisiblePosition(Position(startRoot, maxDeepOffset(startRoot))).next();
                startRoot = next.deepEquivalent().node()->rootEditableElement();
            } while (startRoot);
            
            ASSERT(!next.isNull());
            m_start = next.deepEquivalent();
        }
    }
    
    // Correct the extent if necessary.
    if (baseRoot != m_extent.node()->rootEditableElement())
        m_extent = m_baseIsFirst ? m_end : m_start;
}

void SelectionController::validate(ETextGranularity granularity)
{
    // Move the selection to rendered positions, if possible.
    Position originalBase(m_base);
    bool baseAndExtentEqual = m_base == m_extent;
    if (m_base.isNotNull()) {
        m_base = VisiblePosition(m_base, m_affinity).deepEquivalent();
        if (baseAndExtentEqual)
            m_extent = m_base;
    }
    if (m_extent.isNotNull() && !baseAndExtentEqual) {
        m_extent = VisiblePosition(m_extent, m_affinity).deepEquivalent();
    }

    // Make sure we do not have a dangling start or end
    if (m_base.isNull() && m_extent.isNull()) {
        // Move the position to the enclosingBlockFlowElement of the original base, if possible.
        // This has the effect of flashing the caret somewhere when a rendered position for
        // the base and extent cannot be found.
        if (originalBase.isNotNull()) {
            Position pos(originalBase.node()->enclosingBlockFlowElement(), 0);
            m_base = pos;
            m_extent = pos;
        }
        else {
            // We have no position to work with. See if the BODY element of the page
            // is contentEditable. If it is, put the caret there.
            //NodeImpl *node = document()
            m_start.clear();
            m_end.clear();
        }
        m_baseIsFirst = true;
    } else if (m_base.isNull()) {
        m_base = m_extent;
        m_baseIsFirst = true;
    } else if (m_extent.isNull()) {
        m_extent = m_base;
        m_baseIsFirst = true;
    } else {
        m_baseIsFirst = RangeImpl::compareBoundaryPoints(m_base.node(), m_base.offset(), m_extent.node(), m_extent.offset()) <= 0;
    }

    if (m_baseIsFirst) {
        m_start = m_base;
        m_end = m_extent;
    } else {
        m_start = m_extent;
        m_end = m_base;
    }
    
    // Expand the selection if requested.
    switch (granularity) {
        case CHARACTER:
            // Don't do any expansion.
            break;
        case WORD: {
            // General case: Select the word the caret is positioned inside of, or at the start of (RightWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a soft-wrapped line or the last word in
            // the document, select that last word (LeftWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a paragraph, select from the the end of the
            // last word to the line break (also RightWordIfOnBoundary);
            VisiblePosition start = VisiblePosition(m_start, m_affinity);
            VisiblePosition end   = VisiblePosition(m_end, m_affinity);
            EWordSide side = RightWordIfOnBoundary;
            if (isEndOfDocument(start) || (isEndOfLine(start) && !isStartOfLine(start) && !isEndOfParagraph(start)))
                side = LeftWordIfOnBoundary;
            m_start = startOfWord(start, side).deepEquivalent();
            side = RightWordIfOnBoundary;
            if (isEndOfDocument(end) || (isEndOfLine(end) && !isStartOfLine(end) && !isEndOfParagraph(end)))
                side = LeftWordIfOnBoundary;
            m_end = endOfWord(end, side).deepEquivalent();
            
            break;
            }
        case LINE: {
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            VisiblePosition end = endOfLine(VisiblePosition(m_end, m_affinity));
            // If the end of this line is at the end of a paragraph, include the space 
            // after the end of the line in the selection.
            if (isEndOfParagraph(end)) {
                VisiblePosition next = end.next();
                if (next.isNotNull())
                    end = next;
            }
            m_end = end.deepEquivalent();
            break;
        }
        case LINE_BOUNDARY:
            m_start = startOfLine(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfLine(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case PARAGRAPH: {
            VisiblePosition pos(m_start, m_affinity);
            if (isStartOfLine(pos) && isEndOfDocument(pos))
                pos = pos.previous();
            m_start = startOfParagraph(pos).deepEquivalent();
            VisiblePosition visibleParagraphEnd = endOfParagraph(VisiblePosition(m_end, m_affinity));
            // Include the space after the end of the paragraph in the selection.
            VisiblePosition startOfNextParagraph = visibleParagraphEnd.next();
            m_end = startOfNextParagraph.isNotNull() ? startOfNextParagraph.deepEquivalent() : visibleParagraphEnd.deepEquivalent();
            break;
        }
        case DOCUMENT_BOUNDARY:
            m_start = startOfDocument(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfDocument(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
        case PARAGRAPH_BOUNDARY:
            m_start = startOfParagraph(VisiblePosition(m_start, m_affinity)).deepEquivalent();
            m_end = endOfParagraph(VisiblePosition(m_end, m_affinity)).deepEquivalent();
            break;
    }
    
    adjustForEditableContent();

    // adjust the state
    if (m_start.isNull()) {
        ASSERT(m_end.isNull());
        m_state = NONE;
    } else if (m_start == m_end || m_start.upstream() == m_end.upstream()) {
        m_state = CARET;
    } else {
        m_state = RANGE;
        // "Constrain" the selection to be the smallest equivalent range of nodes.
        // This is a somewhat arbitrary choice, but experience shows that it is
        // useful to make to make the selection "canonical" (if only for
        // purposes of comparing selections). This is an ideal point of the code
        // to do this operation, since all selection changes that result in a RANGE 
        // come through here before anyone uses it.
        m_start = m_start.downstream();
        m_end = m_end.upstream();
    }

    m_needsLayout = true;
    
#if EDIT_DEBUG
    debugPosition();
#endif
}

void SelectionController::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        ElementImpl *element = static_cast<ElementImpl *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->localName().qstring().latin1());
    }
    else if (r->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(r);
        if (textRenderer->stringLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        QString text = DOMString(textRenderer->string()).qstring();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (r->node() == m_start.node())
                offset = m_start.offset();
            else if (r->node() == m_end.node())
                offset = m_end.offset();
                
            int pos;
            InlineTextBox *box = textRenderer->findNextInlineTextBox(offset, pos);
            text = text.mid(box->m_start, box->m_len);
            
            QString show;
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

void SelectionController::debugPosition() const
{
    if (!m_start.node())
        return;

    //static int context = 5;
    
    //RenderObject *r = 0;

    fprintf(stderr, "SelectionController =================\n");

    if (m_start == m_end) {
        Position pos = m_start;
        fprintf(stderr, "pos:        %s %p:%d\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
    }
    else {
        Position pos = m_start;
        fprintf(stderr, "start:      %s %p:%d\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "-----------------------------------\n");
        pos = m_end;
        fprintf(stderr, "end:        %s %p:%d\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "-----------------------------------\n");
    }
          

    fprintf(stderr, "================================\n");
}

#ifndef NDEBUG
#define FormatBufferSize 1024
void SelectionController::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    if (isNone()) {
        result = "<none>";
    }
    else {
        char s[FormatBufferSize];
        result += "from ";
        m_start.formatForDebugger(s, FormatBufferSize);
        result += s;
        result += " to ";
        m_end.formatForDebugger(s, FormatBufferSize);
        result += s;
    }
          
    strncpy(buffer, result.qstring().latin1(), length - 1);
}
#undef FormatBufferSize

void SelectionController::showTree() const
{
    if (m_start.node())
        m_start.node()->showTreeAndMark(m_start.node(), "S", m_end.node(), "E");
}
#endif

} // namespace DOM
