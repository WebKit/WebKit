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

#ifndef KHTML_EDITING_SELECTION_H
#define KHTML_EDITING_SELECTION_H

#include <qrect.h>
#include <kxmlcore/PassRefPtr.h>
#include "xml/dom_position.h"
#include "text_granularity.h"
#include "misc/shared.h"
#include "editing/visible_text.h"

class Frame;
class QPainter;

namespace khtml {

class RenderObject;
class VisiblePosition;

class SelectionController
{
public:
    enum EState { NONE, CARET, RANGE };
    enum EAlter { MOVE, EXTEND };
    enum EDirection { FORWARD, BACKWARD, RIGHT, LEFT };
#define SEL_DEFAULT_AFFINITY DOWNSTREAM

    typedef DOM::Position Position;
    typedef DOM::RangeImpl RangeImpl;
    typedef DOM::NodeImpl NodeImpl;

    SelectionController();
    SelectionController(const RangeImpl *, EAffinity baseAffinity, EAffinity extentAffinity);
    SelectionController(const VisiblePosition &);
    SelectionController(const VisiblePosition &, const VisiblePosition &);
    SelectionController(const Position &, EAffinity affinity);
    SelectionController(const Position &, EAffinity, const Position &, EAffinity);
    SelectionController(const SelectionController &);

    SelectionController &operator=(const SelectionController &o);
    SelectionController &operator=(const VisiblePosition &r) { moveTo(r); return *this; }

    void moveTo(const RangeImpl *, EAffinity baseAffinity, EAffinity extentAffinity);
    void moveTo(const VisiblePosition &);
    void moveTo(const VisiblePosition &, const VisiblePosition &);
    void moveTo(const Position &, EAffinity);
    void moveTo(const Position &, EAffinity, const Position &, EAffinity);
    void moveTo(const SelectionController &);

    EState state() const { return m_state; }
    
    // FIXME: These should support separate baseAffinity and extentAffinity
    EAffinity startAffinity() const { return m_affinity; }
    EAffinity endAffinity() const { return m_affinity; }
    EAffinity baseAffinity() const { return m_affinity; }
    EAffinity extentAffinity() const { return m_affinity; }

    bool modify(EAlter, EDirection, ETextGranularity);
    bool modify(EAlter, int verticalDistance);
    bool expandUsingGranularity(ETextGranularity);
    void clear();

    void setBase(const VisiblePosition &);
    void setBase(const Position &pos, EAffinity affinity);
    void setExtent(const VisiblePosition &);
    void setExtent(const Position &pos, EAffinity affinity);

    Position base() const { return m_base; }
    Position extent() const { return m_extent; }
    Position start() const { return m_start; }
    Position end() const { return m_end; }

    QRect caretRect() const;
    void setNeedsLayout(bool flag = true);

    void clearModifyBias() { m_modifyBiasSet = false; }
    void setModifyBias(EAlter, EDirection);
    
    bool isNone() const { return state() == NONE; }
    bool isCaret() const { return state() == CARET; }
    bool isRange() const { return state() == RANGE; }
    bool isCaretOrRange() const { return state() != NONE; }

    PassRefPtr<DOM::RangeImpl> toRange() const;

    void debugPosition() const;
    void debugRenderer(khtml::RenderObject *r, bool selected) const;

    friend class ::Frame;
    
    Frame *frame() const;

    // Safari Selection Object API
    NodeImpl *baseNode() const { return m_base.node(); }
    NodeImpl *extentNode() const { return m_extent.node(); }
    int baseOffset() const { return m_base.offset(); }
    int extentOffset() const { return m_extent.offset(); }
    DOM::DOMString type() const;
    void setBaseAndExtent(NodeImpl *baseNode, int baseOffset, NodeImpl *extentNode, int extentOffset);
    void setPosition(NodeImpl *node, int offset);
    bool modify(const DOM::DOMString &alterString, const DOM::DOMString &directionString, const DOM::DOMString &granularityString);
    
    // Mozilla Selection Object API
    // In FireFox, anchor/focus are the equal to the start/end of the selection,
    // but reflect the direction in which the selection was made by the user.  That does
    // not mean that they are base/extent, since the base/extent don't reflect
    // expansion.
    NodeImpl *anchorNode() const { return m_baseIsFirst ? m_start.node() : m_end.node(); }
    int anchorOffset() const { return m_baseIsFirst ? m_start.offset() : m_end.offset(); }
    NodeImpl *focusNode() const { return m_baseIsFirst ? m_end.node() : m_start.node(); }
    int focusOffset() const { return m_baseIsFirst ? m_end.offset() : m_start.offset(); }
    bool isCollapsed() const { return !isRange(); }
    DOM::DOMString toString() const;
    void collapse(NodeImpl *node, int offset);
    void collapseToEnd();
    void collapseToStart();
    void extend(NodeImpl *node, int offset);
    PassRefPtr<RangeImpl> getRangeAt(int index) const;
    //void deleteFromDocument();
    //bool containsNode(NodeImpl *node, bool entirelyContained);
    //int rangeCount() const;
    //void addRange(const RangeImpl *);
    //void selectAllChildren(const NodeImpl *);
    //void removeRange(const RangeImpl *);
    //void removeAllRanges();
    
    // Microsoft Selection Object API
    void empty();
    //void clear();
    //TextRange *createRange();

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
    void showTree() const;
#endif

private:
    enum EPositionType { START, END, BASE, EXTENT };

    void init(EAffinity affinity);
    void validate(ETextGranularity granularity = CHARACTER);
    void adjustForEditableContent();

    VisiblePosition modifyExtendingRightForward(ETextGranularity);
    VisiblePosition modifyMovingRightForward(ETextGranularity);
    VisiblePosition modifyExtendingLeftBackward(ETextGranularity);
    VisiblePosition modifyMovingLeftBackward(ETextGranularity);

    void layout();
    void needsCaretRepaint();
    void paintCaret(QPainter *p, const QRect &rect);
    QRect caretRepaintRect() const;

    int xPosForVerticalArrowNavigation(EPositionType, bool recalc = false) const;

    Position m_base;              // base position for the selection
    Position m_extent;            // extent position for the selection
    Position m_start;             // start position for the selection
    Position m_end;               // end position for the selection

    EState m_state;               // the state of the selection
    EAffinity m_affinity;         // the upstream/downstream affinity of the selection

    QRect m_caretRect;            // caret coordinates, size, and position
    
    // m_caretPositionOnLayout stores the scroll offset on the previous call to SelectionController::layout().
    // When asked for caretRect(), we correct m_caretRect for offset due to scrolling since the last layout().
    // This is faster than doing another layout().
    IntPoint m_caretPositionOnLayout;
    
    bool m_baseIsFirst : 1;       // true if base is before the extent
    bool m_needsLayout : 1;       // true if the caret and expectedVisible rectangles need to be calculated
    bool m_modifyBiasSet : 1;     // true if the selection has been horizontally 
                                  // modified with EAlter::EXTEND
};

inline bool operator==(const SelectionController &a, const SelectionController &b)
{
    return a.start() == b.start() && a.end() == b.end() && a.startAffinity() == b.startAffinity();
}

inline bool operator!=(const SelectionController &a, const SelectionController &b)
{
    return !(a == b);
}

} // namespace khtml

#endif // KHTML_EDITING_SELECTION_H
