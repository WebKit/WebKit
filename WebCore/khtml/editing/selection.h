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

#ifndef __khtml_selection_h__
#define __khtml_selection_h__

class KHTMLPart;
class QPainter;
class QRect;

namespace DOM {
    class DOMPosition;
    class NodeImpl;
    class Range;
};

namespace khtml {
    class RenderObject;
}

class KHTMLSelection
{
public:
    KHTMLSelection();
    KHTMLSelection(DOM::NodeImpl *node, long offset);
    KHTMLSelection(const DOM::DOMPosition &);
    KHTMLSelection(DOM::NodeImpl *startNode, long startOffset, DOM::NodeImpl *endNode, long endOffset);
    KHTMLSelection(const KHTMLSelection &);
    ~KHTMLSelection();

	enum EState { NONE, CARET, RANGE };
	enum ETextElement { CHARACTER, WORD, LINE };
	enum EDirection { FORWARD, BACKWARD };
	enum EAlter { MOVE, EXTEND };

	EState state() const { return m_state; }

    void moveTo(DOM::NodeImpl *node, long offset);
    void moveTo(const DOM::Range &);
    void moveTo(const DOM::DOMPosition &);
    void moveTo(const KHTMLSelection &);
    void moveTo(DOM::NodeImpl *baseNode, long baseOffset, DOM::NodeImpl *extentNode, long extentOffset);
    bool modify(EAlter, EDirection, ETextElement);
    void expandToElement(ETextElement);
    void clear();

    bool moveToRenderedContent();
    
    void setBase(DOM::NodeImpl *node, long offset);
    void setExtent(DOM::NodeImpl *node, long offset);

    DOM::NodeImpl *baseNode() const { return m_baseNode; }
    long baseOffset() const { return m_baseOffset; }

    DOM::NodeImpl *extentNode() const { return m_extentNode; }
    long extentOffset() const { return m_extentOffset; }

    DOM::NodeImpl *startNode() const { return m_startNode; }
    long startOffset() const { return m_startOffset; }

    DOM::NodeImpl *endNode() const { return m_endNode; }
    long endOffset() const { return m_endOffset; }

    DOM::DOMPosition previousCharacterPosition() const;
    static DOM::DOMPosition previousCharacterPosition(const DOM::DOMPosition &from);
    DOM::DOMPosition nextCharacterPosition() const;
    static DOM::DOMPosition nextCharacterPosition(const DOM::DOMPosition &from);
        
    void setNeedsLayout(bool flag=true);
    
    bool isEmpty() const;
    DOM::Range toRange() const;

    
    void debugPosition() const;
    void debugRenderer(khtml::RenderObject *r, bool selected) const;

    KHTMLSelection &operator=(const KHTMLSelection &o);
    
    friend bool operator==(const KHTMLSelection &a, const KHTMLSelection &b);
    friend bool operator!=(const KHTMLSelection &a, const KHTMLSelection &b);
    
    friend class KHTMLPart;

private:
    void init();
    void validate(ETextElement expandTo=CHARACTER);

    void layoutCaret();
    void needsCaretRepaint();
    QRect getRepaintRect();
    void paintCaret(QPainter *p, const QRect &rect);

	void setBaseNode(DOM::NodeImpl *);
	void setBaseOffset(long);
	void setExtentNode(DOM::NodeImpl *);
	void setExtentOffset(long);

	void setStartNode(DOM::NodeImpl *);
	void setStartOffset(long);
	void setEndNode(DOM::NodeImpl *);
	void setEndOffset(long);

    bool inRenderedContent(const DOM::DOMPosition &);
    bool nodeIsBeforeNode(DOM::NodeImpl *n1, DOM::NodeImpl *n2);

    void calculateStartAndEnd(ETextElement select=CHARACTER);
    
    DOM::NodeImpl *m_baseNode;    // base node for the selection
    long m_baseOffset;            // offset into base node where selection is
    DOM::NodeImpl *m_extentNode;  // extent node for the selection
    long m_extentOffset;          // offset into extent node where selection is

    DOM::NodeImpl *m_startNode;   // start node for the selection (read-only)
    long m_startOffset;           // offset into start node where selection is (read-only)
    DOM::NodeImpl *m_endNode;     // end node for the selection (read-only)
    long m_endOffset;             // offset into end node where selection is (read-only)

	EState m_state;               // the state of the selection

	int m_caretX;
	int m_caretY;
	int m_caretSize;

	bool m_baseIsStart : 1;       // true if base node is before the extent node
	bool m_needsCaretLayout : 1;  // true if the caret position needs to be calculated
};


inline bool operator==(const KHTMLSelection &a, const KHTMLSelection &b)
{
    return a.startNode() == b.startNode() && a.startOffset() == b.startOffset() &&
        a.endNode() == b.endNode() && a.endOffset() == b.endOffset();
}

inline bool operator!=(const KHTMLSelection &a, const KHTMLSelection &b)
{
    return !(a == b);
}

#endif