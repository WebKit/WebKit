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
  
#include "selection.h"

#include <qevent.h>
#include <qpainter.h>
#include <qrect.h>

#include "dom/dom_node.h"
#include "dom/dom_string.h"
#include "khtml_part.h"
#include "khtmlview.h"
#include "misc/htmltags.h"
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"
#include "visible_position.h"
#include "visible_units.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_positioniterator.h"
#include "xml/dom_textimpl.h"
#include "xml/dom2_rangeimpl.h"

#if APPLE_CHANGES
#include "KWQAssertions.h"
#else
#define ASSERT(assertion) assert(assertion)
#endif

#define EDIT_DEBUG 0

using DOM::DOMString;
using DOM::ElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Position;
using DOM::Range;
using DOM::RangeImpl;
using DOM::StayInBlock;

namespace khtml {

static Selection selectionForLine(const Position &position, EAffinity affinity);

Selection::Selection()
{
    init();
}

Selection::Selection(const Position &pos)
    : m_base(pos), m_extent(pos)
{
    init();
    validate();
}

Selection::Selection(const Range &r)
    : m_base(startPosition(r)), m_extent(endPosition(r))
{
    init();
    validate();
}

Selection::Selection(const Position &base, const Position &extent)
    : m_base(base), m_extent(extent)
{
    init();
    validate();
}

Selection::Selection(const VisiblePosition &base, const VisiblePosition &extent)
    : m_base(base.position()), m_extent(extent.position())
{
    init();
    validate();
}

Selection::Selection(const Selection &o)
    : m_base(o.m_base), m_extent(o.m_extent)
    , m_start(o.m_start), m_end(o.m_end)
    , m_state(o.m_state), m_affinity(o.m_affinity)
    , m_baseIsStart(o.m_baseIsStart)
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
        m_expectedVisibleRect = o.m_expectedVisibleRect;
    }
}

void Selection::init()
{
    m_state = NONE; 
    m_affinity = UPSTREAM;
    m_baseIsStart = true;
    m_needsLayout = true;
    m_modifyBiasSet = false;
}

Selection &Selection::operator=(const Selection &o)
{
    m_base = o.m_base;
    m_extent = o.m_extent;
    m_start = o.m_start;
    m_end = o.m_end;

    m_state = o.m_state;
    m_affinity = o.m_affinity;

    m_baseIsStart = o.m_baseIsStart;
    m_needsLayout = o.m_needsLayout;
    m_modifyBiasSet = o.m_modifyBiasSet;
    
    // Only copy the coordinates over if the other object
    // has had a layout, otherwise keep the current
    // coordinates. This prevents drawing artifacts from
    // remaining when the caret is painted and then moves,
    // and the old rectangle needs to be repainted.
    if (!m_needsLayout) {
        m_caretRect = o.m_caretRect;
        m_expectedVisibleRect = o.m_expectedVisibleRect;
    }
    
    return *this;
}

void Selection::setAffinity(EAffinity affinity)
{
    if (affinity == m_affinity)
        return;
        
    m_affinity = affinity;
    setNeedsLayout();
}

void Selection::modifyAffinity(EAlter alter, EDirection dir, ETextGranularity granularity)
{
    switch (granularity) {
        case CHARACTER:
        case WORD:
            m_affinity = DOWNSTREAM;
            break;
        case PARAGRAPH:
        case LINE:
            if (dir == BACKWARD || dir == LEFT)
                m_affinity = UPSTREAM;
            break;
        case PARAGRAPH_BOUNDARY:
        case DOCUMENT_BOUNDARY:
            // These granularities should not change affinity.
            break;
        case LINE_BOUNDARY: {
            // When extending, leave affinity unchanged.
            if (alter == MOVE) {
                switch (dir) {
                    case FORWARD:
                    case RIGHT:
                        m_affinity = UPSTREAM;
                        break;
                    case BACKWARD:
                    case LEFT:
                        m_affinity = DOWNSTREAM;
                        break;
                }
            }
            break;
        }
    }
    setNeedsLayout();
}

void Selection::moveTo(const Range &r)
{
    m_affinity = UPSTREAM;
    m_base = startPosition(r);
    m_extent = endPosition(r);
    validate();
}

void Selection::moveTo(const Selection &o)
{
    m_affinity = UPSTREAM;
    m_base = o.m_start;
    m_extent = o.m_end;
    validate();
}

void Selection::moveTo(const Position &pos)
{
    m_affinity = UPSTREAM;
    m_base = pos;
    m_extent = pos;
    validate();
}

void Selection::moveTo(const Position &base, const Position &extent)
{
    m_affinity = UPSTREAM;
    m_base = base;
    m_extent = extent;
    validate();
}

void Selection::setModifyBias(EAlter alter, EDirection direction)
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

VisiblePosition Selection::modifyExtendingRightForward(ETextGranularity granularity)
{
    VisiblePosition pos(m_extent);
    switch (granularity) {
        case CHARACTER:
            pos = pos.next();
            break;
        case WORD:
            pos = nextWordPosition(pos);
            break;
        case PARAGRAPH:
            pos = nextParagraphPosition(pos, m_affinity, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE:
            pos = nextLinePosition(pos, m_affinity, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE_BOUNDARY:
            pos = VisiblePosition(selectionForLine(m_end, m_affinity).end());
            break;
        case PARAGRAPH_BOUNDARY:
            pos = endOfParagraph(VisiblePosition(m_end));
            break;
        case DOCUMENT_BOUNDARY: {
            NodeImpl *de = m_start.node()->getDocument()->documentElement();
            pos = VisiblePosition(de, de ? de->childNodeCount() : 0);
            break;
        }
    }
    return pos;
}

VisiblePosition Selection::modifyMovingRightForward(ETextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CHARACTER:
            if (isRange()) 
                pos = VisiblePosition(m_end);
            else
                pos = VisiblePosition(m_extent).next();
            break;
        case WORD:
            pos = nextWordPosition(VisiblePosition(m_extent));
            break;
        case PARAGRAPH:
            pos = nextParagraphPosition(VisiblePosition(m_end), m_affinity, xPosForVerticalArrowNavigation(END, isRange()));
            break;
        case LINE: {
            // This somewhat complicated code is needed to handle the case where there is a
            // whole line selected (like when the user clicks at the start of a line and hits shift+down-arrow),
            // and then hits an (unshifted) down arrow. Since the whole-line selection considers its
            // ending point to be the start of the next line, it may be necessary to juggle the 
            // position to use as the VisiblePosition to pass to nextLinePosition(). If this juggling
            // is not done, you can wind up skipping a line. See these two bugs for more information:
            // <rdar://problem/3875618> REGRESSION (Mail): Hitting down arrow with full line selected skips line (br case)
            // <rdar://problem/3875641> REGRESSION (Mail): Hitting down arrow with full line selected skips line (div case)
            if (isCaret()) {
                pos = VisiblePosition(m_end);
            }
            else if (isRange()) {
                Position p(m_end.upstream());
                if (p.node()->id() == ID_BR)
                    pos = VisiblePosition(Position(p.node(), 0));
                else
                    pos = VisiblePosition(p);
            }
            pos = nextLinePosition(pos, m_affinity, xPosForVerticalArrowNavigation(END, isRange()));
            break;
        }
        case LINE_BOUNDARY:
            pos = VisiblePosition(selectionForLine(m_end, m_affinity).end());
            break;
        case PARAGRAPH_BOUNDARY:
            pos = endOfParagraph(VisiblePosition(m_end));
            break;
        case DOCUMENT_BOUNDARY: {
            NodeImpl *de = m_start.node()->getDocument()->documentElement();
            pos = VisiblePosition(de, de ? de->childNodeCount() : 0);
            break;
        }
    }
    return pos;
}

VisiblePosition Selection::modifyExtendingLeftBackward(ETextGranularity granularity)
{
    VisiblePosition pos(m_extent);
    switch (granularity) {
        case CHARACTER:
            pos = pos.previous();
            break;
        case WORD:
            pos = previousWordPosition(pos);
            break;
        case PARAGRAPH:
            pos = previousParagraphPosition(pos, m_affinity, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE:
            pos = previousLinePosition(pos, m_affinity, xPosForVerticalArrowNavigation(EXTENT));
            break;
        case LINE_BOUNDARY:
            pos = VisiblePosition(selectionForLine(m_start, m_affinity).start());
            break;
        case PARAGRAPH_BOUNDARY:
            pos = startOfParagraph(VisiblePosition(m_start));
            break;
        case DOCUMENT_BOUNDARY:
            pos = VisiblePosition(m_start.node()->getDocument()->documentElement(), 0);
            break;
    }
    return pos;
}

VisiblePosition Selection::modifyMovingLeftBackward(ETextGranularity granularity)
{
    VisiblePosition pos;
    switch (granularity) {
        case CHARACTER:
            if (isRange()) 
                pos = VisiblePosition(m_start);
            else
                pos = VisiblePosition(m_extent).previous();
            break;
        case WORD:
            pos = previousWordPosition(VisiblePosition(m_extent));
            break;
        case PARAGRAPH:
            pos = previousParagraphPosition(VisiblePosition(m_start), m_affinity, xPosForVerticalArrowNavigation(START, isRange()));
            break;
        case LINE:
            pos = previousLinePosition(VisiblePosition(m_start), m_affinity, xPosForVerticalArrowNavigation(START, isRange()));
            break;
        case LINE_BOUNDARY:
            pos = VisiblePosition(selectionForLine(m_start, m_affinity).start());
            break;
        case PARAGRAPH_BOUNDARY:
            pos = startOfParagraph(VisiblePosition(m_start));
            break;
        case DOCUMENT_BOUNDARY:
            pos = VisiblePosition(m_start.node()->getDocument()->documentElement(), 0);
            break;
    }
    return pos;
}

bool Selection::modify(EAlter alter, EDirection dir, ETextGranularity granularity)
{
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

    // Save and restore affinity here before calling setAffinity. 
    // The moveTo() and setExtent() calls reset affinity and this 
    // is undesirable here.
    EAffinity savedAffinity = m_affinity;
    switch (alter) {
        case MOVE:
            moveTo(pos.deepEquivalent());
            break;
        case EXTEND:
            setExtent(pos.deepEquivalent());
            break;
    }
    m_affinity = savedAffinity;
    modifyAffinity(alter, dir, granularity);

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

bool Selection::modify(EAlter alter, int verticalDistance)
{
    if (verticalDistance == 0)
        return false;

    bool up = verticalDistance < 0;
    if (up)
        verticalDistance = -verticalDistance;

    m_affinity = UPSTREAM;
    setModifyBias(alter, up ? BACKWARD : FORWARD);

    VisiblePosition pos;
    int xPos = 0; /* initialized only to make compiler happy */

    switch (alter) {
        case MOVE:
            pos = VisiblePosition(up ? m_start : m_end);
            xPos = xPosForVerticalArrowNavigation(up ? START : END, isRange());
            break;
        case EXTEND:
            pos = VisiblePosition(m_extent);
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
        next = (up ? previousLinePosition : nextLinePosition)(p, m_affinity, xPos);
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
            moveTo(result.deepEquivalent());
            break;
        case EXTEND:
            setExtent(result.deepEquivalent());
            break;
    }

    return true;
}

bool Selection::expandUsingGranularity(ETextGranularity granularity)
{
    if (isNone())
        return false;
    validate(granularity);
    return true;
}

int Selection::xPosForVerticalArrowNavigation(EPositionType type, bool recalc) const
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

    KHTMLPart *part = pos.node()->getDocument()->part();
    if (!part)
        return x;
        
    if (recalc || part->xPosForVerticalArrowNavigation() == KHTMLPart::NoXPosForVerticalArrowNavigation) {
        switch (m_affinity) {
            case DOWNSTREAM:
                pos = VisiblePosition(pos).downstreamDeepEquivalent();
                break;
            case UPSTREAM:
                pos = VisiblePosition(pos).deepEquivalent();
                break;
        }
        x = pos.node()->renderer()->caretRect(pos.offset(), m_affinity).x();
        part->setXPosForVerticalArrowNavigation(x);
    }
    else {
        x = part->xPosForVerticalArrowNavigation();
    }
    return x;
}

void Selection::clear()
{
    m_affinity = UPSTREAM;
    m_base.clear();
    m_extent.clear();
    validate();
}

void Selection::setBase(const Position &pos)
{
    m_affinity = UPSTREAM;
    m_base = pos;
    validate();
}

void Selection::setExtent(const Position &pos)
{
    m_affinity = UPSTREAM;
    m_extent = pos;
    validate();
}

void Selection::setBaseAndExtent(const Position &base, const Position &extent)
{
    m_affinity = UPSTREAM;
    m_base = base;
    m_extent = extent;
    validate();
}

void Selection::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

Range Selection::toRange() const
{
    if (isNone())
        return Range();

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
        s = m_start.upstream(StayInBlock).equivalentRangeCompliantPosition();
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
        s = m_start.downstream(StayInBlock);
        e = m_end.upstream(StayInBlock);
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

    // Use this roundabout way of creating the Range in order to have defined behavior
    // when there is a DOM exception.
    int exceptionCode = 0;
    Range result(s.node()->getDocument());
    RangeImpl *handle = result.handle();
    ASSERT(handle);
    handle->setStart(s.node(), s.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range start from Selection: %d", exceptionCode);
        return Range();
    }
    handle->setEnd(e.node(), e.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range end from Selection: %d", exceptionCode);
        return Range();
    }
    return result;
}

void Selection::layout()
{
    if (isNone() || !m_start.node()->inDocument() || !m_end.node()->inDocument()) {
        m_caretRect = QRect();
        m_expectedVisibleRect = QRect();
        return;
    }

    m_start.node()->getDocument()->updateRendering();
        
    if (isCaret()) {
        Position pos = m_start;
        switch (m_affinity) {
            case DOWNSTREAM:
                pos = VisiblePosition(m_start).downstreamDeepEquivalent();
                break;
            case UPSTREAM:
                pos = VisiblePosition(m_start).deepEquivalent();
                break;
        }
        if (pos.isNotNull()) {
            ASSERT(pos.node()->renderer());
            m_caretRect = pos.node()->renderer()->caretRect(pos.offset(), m_affinity);
            m_expectedVisibleRect = m_caretRect;
        }
        else {
            m_caretRect = QRect();
            m_expectedVisibleRect = QRect();
        }
    }
    else {
        // Calculate which position to use based on whether the base is the start.
        // We want the position, start or end, that was calculated using the extent. 
        // This makes the selection follow the extent position while scrolling as a 
        // result of arrow navigation. 
        //
        // Note: no need to get additional help from VisiblePosition. The m_start and
        // m_end positions should already be visible, and we're only interested in 
        // a rectangle for m_expectedVisibleRect, hence affinity is not a factor
        // like it is when drawing a caret.
        //
        Position pos = m_baseIsStart ? m_end : m_start;
        ASSERT(pos.node()->renderer()); 
        m_expectedVisibleRect = pos.node()->renderer()->caretRect(pos.offset(), m_affinity);
        m_caretRect = QRect();
    }

    m_needsLayout = false;
}

QRect Selection::caretRect() const
{
    if (m_needsLayout) {
        const_cast<Selection *>(this)->layout();
    }

    return m_caretRect;
}

QRect Selection::expectedVisibleRect() const
{
    if (m_needsLayout) {
        const_cast<Selection *>(this)->layout();
    }

    return m_expectedVisibleRect;
}

QRect Selection::caretRepaintRect() const
{
    // FIXME: Add one pixel of slop on each side to make sure we don't leave behind artifacts.
    QRect r = caretRect();
    if (r.isEmpty())
        return QRect();
    return QRect(r.left() - 1, r.top() - 1, r.width() + 2, r.height() + 2);
}

void Selection::needsCaretRepaint()
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

void Selection::paintCaret(QPainter *p, const QRect &rect)
{
    if (m_state != CARET)
        return;

    if (m_needsLayout)
        layout();

    if (m_caretRect.isValid())
        p->fillRect(m_caretRect & rect, QBrush());
}

void Selection::validate(ETextGranularity granularity)
{
    // Move the selection to rendered positions, if possible.
    Position originalBase(m_base);
    bool baseAndExtentEqual = m_base == m_extent;
    bool updatedLayout = false;
    if (m_base.isNotNull()) {
        m_base.node()->getDocument()->updateLayout();
        updatedLayout = true;
        m_base = VisiblePosition(m_base).deepEquivalent();
        if (baseAndExtentEqual)
            m_extent = m_base;
    }
    if (m_extent.isNotNull() && !baseAndExtentEqual) {
        if (!updatedLayout)
            m_extent.node()->getDocument()->updateLayout();
        m_extent = VisiblePosition(m_extent).deepEquivalent();
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
        m_baseIsStart = true;
    }
    else if (m_base.isNull()) {
        m_base = m_extent;
        m_baseIsStart = true;
    }
    else if (m_extent.isNull()) {
        m_extent = m_base;
        m_baseIsStart = true;
    }
    else {
        m_baseIsStart = RangeImpl::compareBoundaryPoints(m_base.node(), m_base.offset(), m_extent.node(), m_extent.offset()) <= 0;
    }

    m_start.clear();
    m_end.clear();

    // calculate the correct start and end positions
    switch (granularity) {
        case CHARACTER:
            if (m_baseIsStart) {
                m_start = m_base;
                m_end = m_extent;
            } else {
                m_start = m_extent;
                m_end = m_base;
            }
            break;
        case WORD:
            if (m_baseIsStart) {
                m_end = endOfWord(VisiblePosition(m_extent)).deepEquivalent();
                // If at the end of the document, expand to the left.
                EWordSide side = (m_end == m_extent) ? LeftWordIfOnBoundary : RightWordIfOnBoundary;
                m_start = startOfWord(VisiblePosition(m_base), side).deepEquivalent();
            } else {
                m_start = startOfWord(VisiblePosition(m_extent)).deepEquivalent();
                m_end = endOfWord(VisiblePosition(m_base)).deepEquivalent();
            }
            break;
        case LINE:
        case LINE_BOUNDARY: {
            Selection baseSelection = *this;
            Selection extentSelection = *this;
            Selection baseLine = selectionForLine(m_base, m_affinity);
            if (baseLine.isCaretOrRange()) {
                baseSelection = baseLine;
            }
            Selection extentLine = selectionForLine(m_extent, m_affinity);
            if (extentLine.isCaretOrRange()) {
                extentSelection = extentLine;
            }
            if (m_baseIsStart) {
                m_start = baseSelection.m_start;
                m_end = extentSelection.m_end;
            } else {
                m_start = extentSelection.m_start;
                m_end = baseSelection.m_end;
            }
            break;
        }
        case PARAGRAPH:
            if (m_baseIsStart) {
                m_start = startOfParagraph(VisiblePosition(m_base)).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_extent), IncludeLineBreak).deepEquivalent();
            } else {
                m_start = startOfParagraph(VisiblePosition(m_extent)).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_base), IncludeLineBreak).deepEquivalent();
            }
            break;
        case DOCUMENT_BOUNDARY: {
            NodeImpl *de = m_start.node()->getDocument()->documentElement();
            m_start = VisiblePosition(de, 0).deepEquivalent();
            m_end = VisiblePosition(de, de ? de->childNodeCount() : 0).deepEquivalent();
            break;
        }
        case PARAGRAPH_BOUNDARY:
            if (m_baseIsStart) {
                m_start = startOfParagraph(VisiblePosition(m_base)).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_extent)).deepEquivalent();
            } else {
                m_start = startOfParagraph(VisiblePosition(m_extent)).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_base)).deepEquivalent();
            }
            break;
    }

    // adjust the state
    if (m_start.isNull()) {
        ASSERT(m_end.isNull());
        m_state = NONE;
    }
    else if (m_start == m_end || m_start.upstream(StayInBlock) == m_end.upstream(StayInBlock)) {
        m_state = CARET;
    }
    else {
        m_state = RANGE;
        // "Constrain" the selection to be the smallest equivalent range of nodes.
        // This is a somewhat arbitrary choice, but experience shows that it is
        // useful to make to make the selection "canonical" (if only for
        // purposes of comparing selections). This is an ideal point of the code
        // to do this operation, since all selection changes that result in a RANGE 
        // come through here before anyone uses it.
        m_start = m_start.downstream(StayInBlock);
        m_end = m_end.upstream(StayInBlock);
    }

    m_needsLayout = true;
    
#if EDIT_DEBUG
    debugPosition();
#endif
}

static Position startOfFirstRunAt(RenderObject *renderNode, int y)
{
    for (RenderObject *n = renderNode; n; n = n->nextSibling()) {
        if (n->isText()) {
            RenderText *textRenderer = static_cast<RenderText *>(n);
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                int absx, absy;
                n->absolutePosition(absx, absy);
                int top = absy + box->root()->topOverflow();
                if (top == y)
                    return Position(textRenderer->element(), box->m_start);
            }
        }
        
        Position position = startOfFirstRunAt(n->firstChild(), y);
        if (position.isNotNull())
            return position;
    }
    
    return Position();
}

static Position endOfLastRunAt(RenderObject *renderNode, int y)
{
    RenderObject *n = renderNode;
    if (!n)
        return Position();
    if (RenderObject *parent = n->parent())
        n = parent->lastChild();
    
    while (1) {
        Position position = endOfLastRunAt(n->firstChild(), y);
        if (position.isNotNull())
            return position;
        
        if (n->isText() && !n->isBR()) {
            RenderText *textRenderer = static_cast<RenderText *>(n);
            for (InlineTextBox* box = textRenderer->lastTextBox(); box; box = box->prevTextBox()) {
                int absx, absy;
                n->absolutePosition(absx, absy);
                int top = absy + box->root()->topOverflow();
                if (top == y)
                    return Position(textRenderer->element(), box->m_start + box->m_len);
            }
        }
        
        if (n == renderNode)
            return Position();
        
        n = n->previousSibling();
    }
}

static Selection selectionForLine(const Position &position, EAffinity affinity)
{
    NodeImpl *node = position.node();
    if (!node || !node->renderer())
        return Selection();
    
    QRect rect = node->renderer()->caretRect(position.offset(), affinity);
    int selectionPointY = rect.y();
    
    // Go up to first non-inline element.
    RenderObject *renderNode = node->renderer();
    while (renderNode && renderNode->isInline())
        renderNode = renderNode->parent();
    renderNode = renderNode->firstChild();
    
    // Look for all the first child in the block that is on the same line
    // as the selection point.
    Position start = startOfFirstRunAt(renderNode, selectionPointY);
    if (start.isNull())
        return Selection();

    // Look for all the last child in the block that is on the same line
    // as the selection point.
    Position end = endOfLastRunAt(renderNode, selectionPointY);
    if (end.isNull())
        return Selection();
    
    return Selection(start, end);
}

void Selection::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        ElementImpl *element = static_cast<ElementImpl *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->tagName().string().latin1());
    }
    else if (r->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(r);
        if (textRenderer->stringLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        QString text = DOMString(textRenderer->string()).string();
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

void Selection::debugPosition() const
{
    if (!m_start.node())
        return;

    //static int context = 5;
    
    //RenderObject *r = 0;

    fprintf(stderr, "Selection =================\n");

    if (m_start == m_end) {
        Position pos = m_start;
        Position upstream = pos.upstream();
        Position downstream = pos.downstream();
        fprintf(stderr, "upstream:   %s %p:%d\n", getTagName(upstream.node()->id()).string().latin1(), upstream.node(), upstream.offset());
        fprintf(stderr, "pos:        %s %p:%d\n", getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "downstream: %s %p:%d\n", getTagName(downstream.node()->id()).string().latin1(), downstream.node(), downstream.offset());
    }
    else {
        Position pos = m_start;
        Position upstream = pos.upstream();
        Position downstream = pos.downstream();
        fprintf(stderr, "upstream:   %s %p:%d\n", getTagName(upstream.node()->id()).string().latin1(), upstream.node(), upstream.offset());
        fprintf(stderr, "start:      %s %p:%d\n", getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "downstream: %s %p:%d\n", getTagName(downstream.node()->id()).string().latin1(), downstream.node(), downstream.offset());
        fprintf(stderr, "-----------------------------------\n");
        pos = m_end;
        upstream = pos.upstream();
        downstream = pos.downstream();
        fprintf(stderr, "upstream:   %s %p:%d\n", getTagName(upstream.node()->id()).string().latin1(), upstream.node(), upstream.offset());
        fprintf(stderr, "end:        %s %p:%d\n", getTagName(pos.node()->id()).string().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "downstream: %s %p:%d\n", getTagName(downstream.node()->id()).string().latin1(), downstream.node(), downstream.offset());
        fprintf(stderr, "-----------------------------------\n");
    }
          
#if 0
    int back = 0;
    r = m_start.node()->renderer();
    for (int i = 0; i < context; i++, back++) {
        if (r->previousRenderer())
            r = r->previousRenderer();
        else
            break;
    }
    for (int i = 0; i < back; i++) {
        debugRenderer(r, false);
        r = r->nextRenderer();
    }


    fprintf(stderr, "\n");

    if (m_start.node() == m_end.node())
        debugRenderer(m_start.node()->renderer(), true);
    else
        for (r = m_start.node()->renderer(); r && r != m_end.node()->renderer(); r = r->nextRenderer())
            debugRenderer(r, true);
    
    fprintf(stderr, "\n");
    
    r = m_end.node()->renderer();
    for (int i = 0; i < context; i++) {
        if (r->nextRenderer()) {
            r = r->nextRenderer();
            debugRenderer(r, false);
        }
        else
            break;
    }
#endif

    fprintf(stderr, "================================\n");
}

#ifndef NDEBUG
#define FormatBufferSize 1024
void Selection::formatForDebugger(char *buffer, unsigned length) const
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
          
    strncpy(buffer, result.string().latin1(), length - 1);
}
#undef FormatBufferSize
#endif

} // namespace DOM
