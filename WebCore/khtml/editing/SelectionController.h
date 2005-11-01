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
#include "xml/dom_position.h"
#include "text_granularity.h"
#include "misc/shared.h"

class KHTMLPart;
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
    void setExtent(const VisiblePosition &);
    void setBaseAndExtent(const VisiblePosition &base, const VisiblePosition &extent);

    void setBase(const Position &pos, EAffinity affinity);
    void setExtent(const Position &pos, EAffinity affinity);
    void setBaseAndExtent(const Position &base, EAffinity baseAffinity, const Position &extent, EAffinity extentAffinity);

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

    SharedPtr<DOM::RangeImpl> toRange() const;

    void debugPosition() const;
    void debugRenderer(khtml::RenderObject *r, bool selected) const;

    friend class ::KHTMLPart;

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
    void showTree() const;
#endif

private:
    enum EPositionType { START, END, BASE, EXTENT };

    void init(EAffinity affinity);
    void validate(ETextGranularity granularity = CHARACTER);
    void adjustExtentForEditableContent();

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
    QPoint m_caretPositionOnLayout;
    
    bool m_baseIsStart : 1;       // true if base node is before the extent node
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
