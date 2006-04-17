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

#include "IntRect.h"
#include "Selection.h"
#include "Range.h"

namespace WebCore {

class Frame;
class GraphicsContext;
class RenderObject;
class VisiblePosition;

class SelectionController
{
public:
    enum EAlter { MOVE, EXTEND };
    enum EDirection { FORWARD, BACKWARD, RIGHT, LEFT };
#define SEL_DEFAULT_AFFINITY DOWNSTREAM

    SelectionController();
    SelectionController(const Selection&);
    SelectionController(const Range*, EAffinity);
    SelectionController(const VisiblePosition&);
    SelectionController(const VisiblePosition&, const VisiblePosition&);
    SelectionController(const Position&, EAffinity);
    SelectionController(const Position&, const Position&, EAffinity);
    SelectionController(const SelectionController&);
    
    SelectionController& operator=(const SelectionController&);
    SelectionController& operator=(const VisiblePosition& r) { moveTo(r); return *this; }

    Element* rootEditableElement() const { return selection().rootEditableElement(); }
    bool isContentEditable() const { return selection().isContentEditable(); }
    bool isContentRichlyEditable() const { return selection().isContentRichlyEditable(); }

    void moveTo(const Range*, EAffinity);
    void moveTo(const VisiblePosition&);
    void moveTo(const VisiblePosition&, const VisiblePosition&);
    void moveTo(const Position&, EAffinity);
    void moveTo(const Position&, const Position&, EAffinity);
    void moveTo(const SelectionController&);

    const Selection& selection() const { return m_sel; }
    void setSelection(const Selection&);

    Selection::EState state() const { return m_sel.state(); }

    EAffinity affinity() const { return m_sel.affinity(); }

    bool modify(EAlter, EDirection, TextGranularity);
    bool modify(EAlter, int verticalDistance);
    bool expandUsingGranularity(TextGranularity);

    void clear();

    void setBase(const VisiblePosition& );
    void setBase(const Position&, EAffinity);
    void setExtent(const VisiblePosition&);
    void setExtent(const Position&, EAffinity);

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

    PassRefPtr<Range> toRange() const { return m_sel.toRange(); }

    void debugRenderer(RenderObject*, bool selected) const;

    friend class Frame;
    
    Frame* frame() const;
    
    void nodeWillBeRemoved(Node*);

    // Safari Selection Object API
    Node* baseNode() const { return m_sel.base().node(); }
    Node* extentNode() const { return m_sel.extent().node(); }
    int baseOffset() const { return m_sel.base().offset(); }
    int extentOffset() const { return m_sel.extent().offset(); }
    String type() const;
    void setBaseAndExtent(Node* baseNode, int baseOffset, Node* extentNode, int extentOffset);
    void setPosition(Node*, int offset);
    bool modify(const String& alterString, const String& directionString, const String& granularityString);
    
    // Mozilla Selection Object API
    // In FireFox, anchor/focus are the equal to the start/end of the selection,
    // but reflect the direction in which the selection was made by the user.  That does
    // not mean that they are base/extent, since the base/extent don't reflect
    // expansion.
    Node* anchorNode() const { return m_sel.isBaseFirst() ? m_sel.start().node() : m_sel.end().node(); }
    int anchorOffset() const { return m_sel.isBaseFirst() ? m_sel.start().offset() : m_sel.end().offset(); }
    Node* focusNode() const { return m_sel.isBaseFirst() ? m_sel.end().node() : m_sel.start().node(); }
    int focusOffset() const { return m_sel.isBaseFirst() ? m_sel.end().offset() : m_sel.start().offset(); }
    bool isCollapsed() const { return !isRange(); }
    String toString() const;
    void collapse(Node*, int offset);
    void collapseToEnd();
    void collapseToStart();
    void extend(Node*, int offset);
    PassRefPtr<Range> getRangeAt(int index) const;
    //void deleteFromDocument();
    //bool containsNode(Node *node, bool entirelyContained);
    //int rangeCount() const;
    //void addRange(const Range *);
    //void selectAllChildren(const Node *);
    //void removeRange(const Range *);
    //void removeAllRanges();
    
    // Microsoft Selection Object API
    void empty();
    //void clear();
    //TextRange *createRange();

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif

private:
    enum EPositionType { START, END, BASE, EXTENT };

    VisiblePosition modifyExtendingRightForward(TextGranularity);
    VisiblePosition modifyMovingRightForward(TextGranularity);
    VisiblePosition modifyExtendingLeftBackward(TextGranularity);
    VisiblePosition modifyMovingLeftBackward(TextGranularity);

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
};

inline bool operator==(const SelectionController& a, const SelectionController& b)
{
    return a.start() == b.start() && a.end() == b.end() && a.affinity() == b.affinity();
}

inline bool operator!=(const SelectionController& a, const SelectionController& b)
{
    return !(a == b);
}

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::SelectionController&);
void showTree(const WebCore::SelectionController*);
#endif

#endif // KHTML_EDITING_SELECTIONCONTROLLER_H
