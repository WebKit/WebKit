/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
    Selection();
    Selection(NodeImpl *node, long offset);
    Selection(const Position &);
    Selection(const Position &, const Position &);
    Selection(NodeImpl *startNode, long startOffset, NodeImpl *endNode, long endOffset);
    Selection(const Selection &);
    ~Selection();

	enum EState { NONE, CARET, RANGE };
	enum EAlter { MOVE, EXTEND };
	enum EDirection { FORWARD, BACKWARD, RIGHT, LEFT, UP, DOWN };
	enum ETextGranularity { CHARACTER, WORD, LINE };

	EState state() const { return m_state; }

    void moveTo(NodeImpl *node, long offset);
    void moveTo(const Range &);
    void moveTo(const Position &);
    void moveTo(const Selection &);
    void moveTo(NodeImpl *baseNode, long baseOffset, NodeImpl *extentNode, long extentOffset);
    bool modify(EAlter, EDirection, ETextGranularity);
    bool expandUsingGranularity(ETextGranularity);
    void clear();

    bool moveToRenderedContent();
    
    void setBase(NodeImpl *node, long offset);
    void setExtent(NodeImpl *node, long offset);

    NodeImpl *baseNode() const { return m_baseNode; }
    long baseOffset() const { return m_baseOffset; }

    NodeImpl *extentNode() const { return m_extentNode; }
    long extentOffset() const { return m_extentOffset; }

    NodeImpl *startNode() const { return m_startNode; }
    long startOffset() const { return m_startOffset; }

    NodeImpl *endNode() const { return m_endNode; }
    long endOffset() const { return m_endOffset; }

    Position basePosition() const;
    Position extentPosition() const;
    Position startPosition() const;
    Position endPosition() const;

    void setNeedsLayout(bool flag=true);
    void clearModifyBias() { m_modifyBiasSet = false; }
    
    bool isEmpty() const { return state() == NONE; }
    bool notEmpty() const { return !isEmpty(); }
    Range toRange() const;

    
    void debugPosition() const;
    void debugRenderer(khtml::RenderObject *r, bool selected) const;

    Selection &operator=(const Selection &o);
    
    friend bool operator==(const Selection &a, const Selection &b);
    friend bool operator!=(const Selection &a, const Selection &b);
    
    friend class KHTMLPart;

private:
	enum EPositionType { START, END, BASE, EXTENT };

    void init();
    void validate(ETextGranularity granularity=CHARACTER);

    void layoutCaret();
    void needsCaretRepaint();
    QRect getRepaintRect();
    void paintCaret(QPainter *p, const QRect &rect);

	void setBaseNode(NodeImpl *);
	void setBaseOffset(long);
	void setExtentNode(NodeImpl *);
	void setExtentOffset(long);

	void setStartNode(NodeImpl *);
	void setStartOffset(long);
	void setEndNode(NodeImpl *);
	void setEndOffset(long);

    bool nodeIsBeforeNode(NodeImpl *n1, NodeImpl *n2);

    void calculateStartAndEnd(ETextGranularity select=CHARACTER);
    int xPosForVerticalArrowNavigation(EPositionType, bool recalc=false) const;
    
    NodeImpl *m_baseNode;    // base node for the selection
    long m_baseOffset;            // offset into base node where selection is
    NodeImpl *m_extentNode;  // extent node for the selection
    long m_extentOffset;          // offset into extent node where selection is

    NodeImpl *m_startNode;   // start node for the selection (read-only)
    long m_startOffset;           // offset into start node where selection is (read-only)
    NodeImpl *m_endNode;     // end node for the selection (read-only)
    long m_endOffset;             // offset into end node where selection is (read-only)

	EState m_state;               // the state of the selection

	int m_caretX;
	int m_caretY;
	int m_caretSize;

	bool m_baseIsStart : 1;       // true if base node is before the extent node
	bool m_needsCaretLayout : 1;  // true if the caret position needs to be calculated
	bool m_modifyBiasSet : 1;     // true if the selection has been modified with EAlter::EXTEND
};


inline bool operator==(const Selection &a, const Selection &b)
{
    return a.startNode() == b.startNode() && a.startOffset() == b.startOffset() &&
        a.endNode() == b.endNode() && a.endOffset() == b.endOffset();
}

inline bool operator!=(const Selection &a, const Selection &b)
{
    return !(a == b);
}

} // namespace DOM

#endif  // __dom_selection_h__