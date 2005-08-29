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
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"
#include "visible_position.h"
#include "visible_units.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_nodeimpl.h"
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
using DOM::NodeImpl;
using DOM::Position;
using DOM::Range;
using DOM::RangeImpl;

namespace khtml {

Selection::Selection()
{
    init(DOWNSTREAM);
}

Selection::Selection(const Position &pos, EAffinity affinity)
    : m_base(pos), m_extent(pos)
{
    init(affinity);
    validate();
}

Selection::Selection(const RangeImpl *r, EAffinity baseAffinity, EAffinity extentAffinity)
    : m_base(startPosition(r)), m_extent(endPosition(r))
{
    init(baseAffinity);
    validate();
}

Selection::Selection(const Position &base, EAffinity baseAffinity, const Position &extent, EAffinity extentAffinity)
    : m_base(base), m_extent(extent)
{
    init(baseAffinity);
    validate();
}

Selection::Selection(const VisiblePosition &visiblePos)
    : m_base(visiblePos.position()), m_extent(visiblePos.position())
{
    init(visiblePos.affinity());
    validate();
}

Selection::Selection(const VisiblePosition &base, const VisiblePosition &extent)
    : m_base(base.position()), m_extent(extent.position())
{
    init(base.affinity());
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

void Selection::init(EAffinity affinity)
{
    // FIXME: set extentAffinity
    m_state = NONE; 
    m_baseIsStart = true;
    m_affinity = affinity;
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

void Selection::moveTo(const VisiblePosition &pos)
{
    // FIXME: use extentAffinity
    m_affinity = pos.affinity();
    m_base = pos.deepEquivalent();
    m_extent = pos.deepEquivalent();
    validate();
}

void Selection::moveTo(const VisiblePosition &base, const VisiblePosition &extent)
{
    // FIXME: use extentAffinity
    m_affinity = base.affinity();
    m_base = base.deepEquivalent();
    m_extent = extent.deepEquivalent();
    validate();
}

void Selection::moveTo(const Selection &o)
{
    // FIXME: copy extentAffinity
    m_affinity = o.m_affinity;
    m_base = o.m_start;
    m_extent = o.m_end;
    validate();
}

void Selection::moveTo(const Position &pos, EAffinity affinity)
{
    // FIXME: use extentAffinity
    m_affinity = affinity;
    m_base = pos;
    m_extent = pos;
    validate();
}

void Selection::moveTo(const RangeImpl *r, EAffinity baseAffinity, EAffinity extentAffinity)
{
    // FIXME: use extentAffinity
    m_affinity = baseAffinity;
    m_base = startPosition(r);
    m_extent = endPosition(r);
    validate();
}

void Selection::moveTo(const Position &base, EAffinity baseAffinity, const Position &extent, EAffinity extentAffinity)
{
    // FIXME: use extentAffinity
    m_affinity = baseAffinity;
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

VisiblePosition Selection::modifyMovingRightForward(ETextGranularity granularity)
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

VisiblePosition Selection::modifyExtendingLeftBackward(ETextGranularity granularity)
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

VisiblePosition Selection::modifyMovingLeftBackward(ETextGranularity granularity)
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

bool Selection::modify(EAlter alter, int verticalDistance)
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
                pos = VisiblePosition(pos, m_affinity).downstreamDeepEquivalent();
                break;
            case UPSTREAM:
                pos = VisiblePosition(pos, m_affinity).deepEquivalent();
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
    m_affinity = SEL_DEFAULT_AFFINITY;
    m_base.clear();
    m_extent.clear();
    validate();
}

void Selection::setBase(const VisiblePosition &pos)
{
    m_affinity = pos.affinity();
    m_base = pos.deepEquivalent();
    validate();
}

void Selection::setExtent(const VisiblePosition &pos)
{
    // FIXME: Support extentAffinity
    m_extent = pos.deepEquivalent();
    validate();
}

void Selection::setBaseAndExtent(const VisiblePosition &base, const VisiblePosition &extent)
{
    // FIXME: Support extentAffinity
    m_affinity = base.affinity();
    m_base = base.deepEquivalent();
    m_extent = extent.deepEquivalent();
    validate();
}


void Selection::setBase(const Position &pos, EAffinity baseAffinity)
{
    m_affinity = baseAffinity;
    m_base = pos;
    validate();
}

void Selection::setExtent(const Position &pos, EAffinity extentAffinity)
{
    // FIXME: Support extentAffinity for real
    m_affinity = extentAffinity;
    m_extent = pos;
    validate();
}

void Selection::setBaseAndExtent(const Position &base, EAffinity baseAffinity, const Position &extent, EAffinity extentAffinity)
{
    // FIXME: extentAffinity
    m_affinity = baseAffinity;
    m_base = base;
    m_extent = extent;
    validate();
}

void Selection::setNeedsLayout(bool flag)
{
    m_needsLayout = flag;
}

SharedPtr<RangeImpl> Selection::toRange() const
{
    if (isNone())
        return SharedPtr<RangeImpl>();

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
    SharedPtr<RangeImpl> result(new RangeImpl(s.node()->docPtr()));
    result->setStart(s.node(), s.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range start from Selection: %d", exceptionCode);
        return SharedPtr<RangeImpl>();
    }
    result->setEnd(e.node(), e.offset(), exceptionCode);
    if (exceptionCode) {
        ERROR("Exception setting Range end from Selection: %d", exceptionCode);
        return SharedPtr<RangeImpl>();
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
                pos = VisiblePosition(m_start, m_affinity).downstreamDeepEquivalent();
                break;
            case UPSTREAM:
                pos = VisiblePosition(m_start, m_affinity).deepEquivalent();
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
        m_base = VisiblePosition(m_base, m_affinity).deepEquivalent();
        if (baseAndExtentEqual)
            m_extent = m_base;
    }
    if (m_extent.isNotNull() && !baseAndExtentEqual) {
        if (!updatedLayout)
            m_extent.node()->getDocument()->updateLayout();
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
        case WORD: {
            // General case: Select the word the caret is positioned inside of, or at the start of (RightWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a soft-wrapped line or the last word in
            // the document, select that last word (LeftWordIfOnBoundary).
            // Edge case: If the caret is after the last word in a paragraph, select from the the end of the
            // last word to the line break (also RightWordIfOnBoundary);
            VisiblePosition start = m_baseIsStart ? VisiblePosition(m_base, m_affinity)   : VisiblePosition(m_extent, m_affinity);
            VisiblePosition end   = m_baseIsStart ? VisiblePosition(m_extent, m_affinity) : VisiblePosition(m_base, m_affinity);
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
        case LINE:
        case LINE_BOUNDARY:
            if (m_baseIsStart) {
                m_start = startOfLine(VisiblePosition(m_base, m_affinity)).deepEquivalent();
                m_end = endOfLine(VisiblePosition(m_extent, m_affinity), IncludeLineBreak).deepEquivalent();
            } else {
                m_start = startOfLine(VisiblePosition(m_extent, m_affinity)).deepEquivalent();
                m_end = endOfLine(VisiblePosition(m_base, m_affinity), IncludeLineBreak).deepEquivalent();
            }
            break;
        case PARAGRAPH:
            if (m_baseIsStart) {
                VisiblePosition pos(m_base, m_affinity);
                if (isStartOfLine(pos) && isEndOfDocument(pos))
                    pos = pos.previous();
                m_start = startOfParagraph(pos).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_extent, m_affinity), IncludeLineBreak).deepEquivalent();
            } else {
                m_start = startOfParagraph(VisiblePosition(m_extent, m_affinity)).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_base, m_affinity), IncludeLineBreak).deepEquivalent();
            }
            break;
        case DOCUMENT_BOUNDARY:
            m_start = startOfDocument(VisiblePosition(m_base, m_affinity)).deepEquivalent();
            m_end = endOfDocument(VisiblePosition(m_base, m_affinity)).deepEquivalent();
            break;
        case PARAGRAPH_BOUNDARY:
            if (m_baseIsStart) {
                m_start = startOfParagraph(VisiblePosition(m_base, m_affinity)).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_extent, m_affinity)).deepEquivalent();
            } else {
                m_start = startOfParagraph(VisiblePosition(m_extent, m_affinity)).deepEquivalent();
                m_end = endOfParagraph(VisiblePosition(m_base, m_affinity)).deepEquivalent();
            }
            break;
    }

    // adjust the state
    if (m_start.isNull()) {
        ASSERT(m_end.isNull());
        m_state = NONE;
    }
    else if (m_start == m_end || m_start.upstream() == m_end.upstream()) {
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
        m_start = m_start.downstream();
        m_end = m_end.upstream();
    }

    m_needsLayout = true;
    
#if EDIT_DEBUG
    debugPosition();
#endif
}

void Selection::debugRenderer(RenderObject *r, bool selected) const
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

void Selection::debugPosition() const
{
    if (!m_start.node())
        return;

    //static int context = 5;
    
    //RenderObject *r = 0;

    fprintf(stderr, "Selection =================\n");

    if (m_start == m_end) {
        Position pos = m_start;
        fprintf(stderr, "pos:        %s %p:%ld\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
    }
    else {
        Position pos = m_start;
        fprintf(stderr, "start:      %s %p:%ld\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
        fprintf(stderr, "-----------------------------------\n");
        pos = m_end;
        fprintf(stderr, "end:        %s %p:%ld\n", pos.node()->nodeName().qstring().latin1(), pos.node(), pos.offset());
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
          
    strncpy(buffer, result.qstring().latin1(), length - 1);
}
#undef FormatBufferSize

void Selection::showTree() const
{
    if (m_start.node())
        m_start.node()->showTreeAndMark(m_start.node(), "S", m_end.node(), "E");
}
#endif

} // namespace DOM
