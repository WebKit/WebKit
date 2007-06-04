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

#ifndef SelectionController_h
#define SelectionController_h

#include "IntRect.h"
#include "Selection.h"
#include "Range.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class Frame;
class GraphicsContext;
class RenderObject;
class VisiblePosition;

class SelectionController : Noncopyable {
public:
    enum EAlteration { MOVE, EXTEND };
    enum EDirection { FORWARD, BACKWARD, RIGHT, LEFT };

    SelectionController(Frame* = 0, bool isDragCaretController = false);

    Element* rootEditableElement() const { return m_sel.rootEditableElement(); }
    bool isContentEditable() const { return m_sel.isContentEditable(); }
    bool isContentRichlyEditable() const { return m_sel.isContentRichlyEditable(); }

    void moveTo(const Range*, EAffinity, bool userTriggered = false);
    void moveTo(const VisiblePosition&, bool userTriggered = false);
    void moveTo(const VisiblePosition&, const VisiblePosition&, bool userTriggered = false);
    void moveTo(const Position&, EAffinity, bool userTriggered = false);
    void moveTo(const Position&, const Position&, EAffinity, bool userTriggered = false);

    const Selection& selection() const { return m_sel; }
    void setSelection(const Selection&, bool closeTyping = true, bool clearTypingStyle = true, bool userTriggered = false);
    void setSelectedRange(Range*, EAffinity, bool closeTyping, ExceptionCode&);
    void selectAll();
    void clear();
    
    // Call this after doing user-triggered selections to make it easy to delete the frame you entirely selected.
    void selectFrameElementInParentIfFullySelected();

    bool contains(const IntPoint&);

    Selection::EState state() const { return m_sel.state(); }

    EAffinity affinity() const { return m_sel.affinity(); }

    bool modify(EAlteration, EDirection, TextGranularity, bool userTriggered = false);
    bool modify(EAlteration, int verticalDistance, bool userTriggered = false);
    bool expandUsingGranularity(TextGranularity);

    void setBase(const VisiblePosition&, bool userTriggered = false);
    void setBase(const Position&, EAffinity, bool userTriggered = false);
    void setExtent(const VisiblePosition&, bool userTriggered = false);
    void setExtent(const Position&, EAffinity, bool userTriggered = false);

    Position base() const { return m_sel.base(); }
    Position extent() const { return m_sel.extent(); }
    Position start() const { return m_sel.start(); }
    Position end() const { return m_sel.end(); }

    IntRect caretRect() const;
    void setNeedsLayout(bool flag = true);

    void setLastChangeWasHorizontalExtension(bool b) { m_lastChangeWasHorizontalExtension = b; }
    void willBeModified(EAlteration, EDirection);
    
    bool isNone() const { return m_sel.isNone(); }
    bool isCaret() const { return m_sel.isCaret(); }
    bool isRange() const { return m_sel.isRange(); }
    bool isCaretOrRange() const { return m_sel.isCaretOrRange(); }
    bool isInPasswordField() const;
    bool isInsideNode() const;
    
    PassRefPtr<Range> toRange() const { return m_sel.toRange(); }

    void debugRenderer(RenderObject*, bool selected) const;
    
    void nodeWillBeRemoved(Node*);

    // Safari Selection Object API
    // These methods return the valid equivalents of internal editing positions.
    Node* baseNode() const;
    Node* extentNode() const;
    int baseOffset() const;
    int extentOffset() const;
    String type() const;
    void setBaseAndExtent(Node* baseNode, int baseOffset, Node* extentNode, int extentOffset, ExceptionCode&);
    void setPosition(Node*, int offset, ExceptionCode&);
    bool modify(const String& alterString, const String& directionString, const String& granularityString, bool userTriggered = false);
    
    // Mozilla Selection Object API
    // In FireFox, anchor/focus are the equal to the start/end of the selection,
    // but reflect the direction in which the selection was made by the user.  That does
    // not mean that they are base/extent, since the base/extent don't reflect
    // expansion.
    // These methods return the valid equivalents of internal editing positions.
    Node* anchorNode() const;
    int anchorOffset() const;
    Node* focusNode() const;
    int focusOffset() const;
    bool isCollapsed() const { return !isRange(); }
    String toString() const;
    void collapse(Node*, int offset, ExceptionCode&);
    void collapseToEnd();
    void collapseToStart();
    void extend(Node*, int offset, ExceptionCode&);
    PassRefPtr<Range> getRangeAt(int index, ExceptionCode&) const;
    int rangeCount() const { return !isNone() ? 1 : 0; }
    void removeAllRanges();
    void addRange(const Range*);
    //void deleteFromDocument();
    //bool containsNode(Node *node, bool entirelyContained);
    //void selectAllChildren(const Node *);
    //void removeRange(const Range *);
    
    // Microsoft Selection Object API
    void empty();
    //void clear();
    //TextRange *createRange();
    
    bool recomputeCaretRect(); // returns true if caret rect moved
    void invalidateCaretRect();
    void paintCaret(GraphicsContext*, const IntRect&);

    // Used to suspend caret blinking while the mouse is down.
    void setCaretBlinkingSuspended(bool suspended) { m_isCaretBlinkingSuspended = suspended; }
    bool isCaretBlinkingSuspended() const { return m_isCaretBlinkingSuspended; }

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
    IntRect caretRepaintRect() const;

    int xPosForVerticalArrowNavigation(EPositionType);
    
#if PLATFORM(MAC)
    void notifyAccessibilityForSelectionChange();
#else
    void notifyAccessibilityForSelectionChange() {};
#endif

    Selection m_sel;

    IntRect m_caretRect;            // caret coordinates, size, and position
    
    // m_caretPositionOnLayout stores the scroll offset on the previous call to SelectionController::layout().
    // When asked for caretRect(), we correct m_caretRect for offset due to scrolling since the last layout().
    // This is faster than doing another layout().
    IntPoint m_caretPositionOnLayout;
    
    bool m_needsLayout : 1;       // true if the caret and expectedVisible rectangles need to be calculated
    bool m_lastChangeWasHorizontalExtension : 1;
    Frame* m_frame;
    bool m_isDragCaretController;

    bool m_isCaretBlinkingSuspended;
    
    int m_xPosForVerticalArrowNavigation;
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

#endif // SelectionController_h
