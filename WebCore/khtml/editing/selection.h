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

#ifndef __dom_selection_h__
#define __dom_selection_h__

#include "xml/dom_position.h"

class KHTMLPart;
class QPainter;
class QRect;

namespace khtml {
    class RenderObject;
}

namespace DOM {

class NodeImpl;
class Position;
class Range;

class Selection
{
public:
    enum EState { NONE, CARET, RANGE };
    enum EAlter { MOVE, EXTEND };
    enum EDirection { FORWARD, BACKWARD, RIGHT, LEFT };
    enum ETextGranularity { CHARACTER, WORD, LINE, PARAGRAPH };

    // These match the AppKit values for these concepts.
    // From NSTextView.h:
    // NSSelectionAffinityUpstream = 0
    // NSSelectionAffinityDownstream = 1
    enum EAffinity { UPSTREAM = 0, DOWNSTREAM = 1 };

    Selection();
    Selection(const Range &);
    Selection(const Position &);
    Selection(const Position &, const Position &);
    Selection(const Selection &);

    EState state() const { return m_state; }
    EAffinity affinity() const { return m_affinity; }
    void setAffinity(EAffinity);

    void moveTo(const Range &);
    void moveTo(const Selection &);
    void moveTo(const Position &);
    void moveTo(const Position &, const Position &);
    bool modify(EAlter, EDirection, ETextGranularity);
    bool expandUsingGranularity(ETextGranularity);
    void clear();

    bool moveToRenderedContent();
    
    void setBase(const Position &pos);
    void setExtent(const Position &pos);
    void setBaseAndExtent(const Position &base, const Position &extent);
    void setStart(const Position &pos);
    void setEnd(const Position &pos);
    void setStartAndEnd(const Position &start, const Position &end);

    Position base() const { return m_base; }
    Position extent() const { return m_extent; }
    Position start() const { return m_start; }
    Position end() const { return m_end; }

    QRect getRepaintRect() const;
    void setNeedsLayout(bool flag=true);
    void clearModifyBias() { m_modifyBiasSet = false; }
    
    bool isEmpty() const { return state() == NONE; }
    bool notEmpty() const { return !isEmpty(); }
    Range toRange() const;

    void debugPosition() const;
    void debugRenderer(khtml::RenderObject *r, bool selected) const;

    Selection &operator=(const Selection &o);
    Selection &operator=(const Range &r) { moveTo(r); return *this; }
    Selection &operator=(const Position &r) { moveTo(r); return *this; }
    
    friend bool operator==(const Selection &a, const Selection &b);
    friend bool operator!=(const Selection &a, const Selection &b);
    
    friend class KHTMLPart;

private:
    enum EPositionType { START, END, BASE, EXTENT };

    void init();
    void validate(ETextGranularity granularity=CHARACTER);
    void assignBase(const Position &pos) { m_base = pos; }
    void assignExtent(const Position &pos) { m_extent = pos; }
    void assignBaseAndExtent(const Position &base, const Position &extent) { m_base = base; m_extent = extent; }
    void assignStart(const Position &pos) { m_start = pos; }
    void assignEnd(const Position &pos) { m_end = pos; }
    void assignStartAndEnd(const Position &start, const Position &end) { m_start = start; m_end = end; }

    void layoutCaret();
    void needsCaretRepaint();
    void paintCaret(QPainter *p, const QRect &rect);

    bool nodeIsBeforeNode(NodeImpl *n1, NodeImpl *n2) const;
    int xPosForVerticalArrowNavigation(EPositionType, bool recalc=false) const;

    Position m_base;              // base position for the selection
    Position m_extent;            // extent position for the selection
    Position m_start;             // start position for the selection
    Position m_end;               // end position for the selection

    EState m_state;               // the state of the selection
    EAffinity m_affinity;         // the upstream/downstream affinity of the selection

    int m_caretX;                 // caret coordinates and size
    int m_caretY;
    int m_caretSize;

    bool m_baseIsStart : 1;       // true if base node is before the extent node
    bool m_needsCaretLayout : 1;  // true if the caret position needs to be calculated
    bool m_modifyBiasSet : 1;     // true if the selection has been horizontally 
                                  // modified with EAlter::EXTEND
};


inline bool operator==(const Selection &a, const Selection &b)
{
    return a.start() == b.start() && a.end() == b.end();
}

inline bool operator!=(const Selection &a, const Selection &b)
{
    return !(a == b);
}

} // namespace DOM

#endif  // __dom_selection_h__