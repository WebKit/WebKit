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

#ifndef KHTML_EDITING_SELECTIONCONTROLLER_H
#define KHTML_EDITING_SELECTIONCONTROLLER_H

#include "EventListener.h"
#include "IntRect.h"
#include "Selection.h"
#include "dom2_rangeimpl.h"

namespace WebCore {

class Frame;
class GraphicsContext;
class RenderObject;
class VisiblePosition;
class SelectionController;

class MutationListener : public EventListener
{
public:
    MutationListener() { m_selectionController = 0; }
    MutationListener(SelectionController *s) { m_selectionController = s; }
    SelectionController *selectionController() const { return m_selectionController; }
    void setSelectionController(SelectionController *s) { m_selectionController = s; }
    
    virtual void handleEvent(EventImpl*, bool isWindowEvent);
    
private:
    SelectionController *m_selectionController;
};

class SelectionController
{
public:
    enum EAlter { MOVE, EXTEND };
    enum EDirection { FORWARD, BACKWARD, RIGHT, LEFT };
#define SEL_DEFAULT_AFFINITY DOWNSTREAM

    SelectionController();
    SelectionController(const Selection &sel);
    SelectionController(const RangeImpl *, EAffinity affinity);
    SelectionController(const VisiblePosition &);
    SelectionController(const VisiblePosition &, const VisiblePosition &);
    SelectionController(const Position &, EAffinity affinity);
    SelectionController(const Position &, const Position &, EAffinity);
    SelectionController(const SelectionController &);
    
    ~SelectionController();

    SelectionController &operator=(const SelectionController &o);
    SelectionController &operator=(const VisiblePosition &r) { moveTo(r); return *this; }

    void moveTo(const RangeImpl *, EAffinity affinity);
    void moveTo(const VisiblePosition &);
    void moveTo(const VisiblePosition &, const VisiblePosition &);
    void moveTo(const Position &, EAffinity);
    void moveTo(const Position &, const Position &, EAffinity);
    void moveTo(const SelectionController &);

    const Selection &selection() const { return m_sel; }
    void setSelection(const Selection &);

    Selection::EState state() const { return m_sel.state(); }

    EAffinity affinity() const { return m_sel.affinity(); }

    bool modify(EAlter, EDirection, ETextGranularity);
    bool modify(EAlter, int verticalDistance);
    bool expandUsingGranularity(ETextGranularity);

    void clear();

    void setBase(const VisiblePosition &);
    void setBase(const Position &pos, EAffinity affinity);
    void setExtent(const VisiblePosition &);
    void setExtent(const Position &pos, EAffinity affinity);

    Position base() const { return m_sel.base(); }
    Position extent() const { return m_sel.extent(); }
    Position start() const { return m_sel.start(); }
    Position end() const { return m_sel.end(); }

    IntRect caretRect() const;
    void setNeedsLayout(bool flag = true);

    void clearModifyBias() { m_modifyBiasSet = false; }
    void setModifyBias(EAlter, EDirection);
    
    bool isNone() const { return m_sel.isNone(); }
    bool isCaret() const { return m_sel.isCaret(); }
    bool isRange() const { return m_sel.isRange(); }
    bool isCaretOrRange() const { return m_sel.isCaretOrRange(); }

    PassRefPtr<RangeImpl> toRange() const { return m_sel.toRange(); }

    void debugRenderer(RenderObject*, bool selected) const;

    friend class Frame;
    
    Frame* frame() const;
    
    void nodeWillBeRemoved(NodeImpl *);

    // Safari Selection Object API
    NodeImpl *baseNode() const { return m_sel.base().node(); }
    NodeImpl *extentNode() const { return m_sel.extent().node(); }
    int baseOffset() const { return m_sel.base().offset(); }
    int extentOffset() const { return m_sel.extent().offset(); }
    DOMString type() const;
    void setBaseAndExtent(NodeImpl *baseNode, int baseOffset, NodeImpl *extentNode, int extentOffset);
    void setPosition(NodeImpl *node, int offset);
    bool modify(const DOMString &alterString, const DOMString &directionString, const DOMString &granularityString);
    
    // Mozilla Selection Object API
    // In FireFox, anchor/focus are the equal to the start/end of the selection,
    // but reflect the direction in which the selection was made by the user.  That does
    // not mean that they are base/extent, since the base/extent don't reflect
    // expansion.
    NodeImpl *anchorNode() const { return m_sel.isBaseFirst() ? m_sel.start().node() : m_sel.end().node(); }
    int anchorOffset() const { return m_sel.isBaseFirst() ? m_sel.start().offset() : m_sel.end().offset(); }
    NodeImpl *focusNode() const { return m_sel.isBaseFirst() ? m_sel.end().node() : m_sel.start().node(); }
    int focusOffset() const { return m_sel.isBaseFirst() ? m_sel.end().offset() : m_sel.start().offset(); }
    bool isCollapsed() const { return !isRange(); }
    DOMString toString() const;
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

    VisiblePosition modifyExtendingRightForward(ETextGranularity);
    VisiblePosition modifyMovingRightForward(ETextGranularity);
    VisiblePosition modifyExtendingLeftBackward(ETextGranularity);
    VisiblePosition modifyMovingLeftBackward(ETextGranularity);

    void layout();
    void needsCaretRepaint();
    void paintCaret(GraphicsContext*, const IntRect &rect);
    IntRect caretRepaintRect() const;

    int xPosForVerticalArrowNavigation(EPositionType, bool recalc = false) const;

    Selection m_sel;

    IntRect m_caretRect;            // caret coordinates, size, and position
    
    // m_caretPositionOnLayout stores the scroll offset on the previous call to SelectionController::layout().
    // When asked for caretRect(), we correct m_caretRect for offset due to scrolling since the last layout().
    // This is faster than doing another layout().
    IntPoint m_caretPositionOnLayout;
    
    bool m_needsLayout : 1;       // true if the caret and expectedVisible rectangles need to be calculated
    bool m_modifyBiasSet : 1;     // true if the selection has been horizontally 
                                  // modified with EAlter::EXTEND
    RefPtr<MutationListener> m_mutationListener;
};

inline bool operator==(const SelectionController &a, const SelectionController &b)
{
    return a.start() == b.start() && a.end() == b.end() && a.affinity() == b.affinity();
}

inline bool operator!=(const SelectionController &a, const SelectionController &b)
{
    return !(a == b);
}

} // namespace WebCore

#endif // KHTML_EDITING_SELECTIONCONTROLLER_H
