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

#include <qobject.h>

class KHTMLPart;
class KHTMLPartPrivate;
class KHTMLView;
class QPainter;
class QRect;
class QTimerEvent;

namespace DOM {
    class DOMPosition;
    class NodeImpl;
    class Range;
};

class KHTMLSelection : public QObject
{
  Q_OBJECT

public:
    KHTMLSelection();
    KHTMLSelection(const KHTMLSelection &);
    ~KHTMLSelection();

	enum EState { NONE, CARET, RANGE };
	enum ETextElement { CHARACTER, WORD, LINE };
	enum EDirection { FORWARD, BACKWARD };
	enum EAlter { MOVE, EXTEND };

	EState state() const { return m_state; }

    void setSelection(DOM::NodeImpl *node, long offset);
    void setSelection(const DOM::Range &);
    void setSelection(const DOM::DOMPosition &);
    void setSelection(DOM::NodeImpl *baseNode, long baseOffset, DOM::NodeImpl *extentNode, long extentOffset);
    void setBase(DOM::NodeImpl *node, long offset);
    void setExtent(DOM::NodeImpl *node, long offset);
    void expandSelection(ETextElement);
    bool alterSelection(EAlter, EDirection, ETextElement);
    void clearSelection();
    
    DOM::NodeImpl *baseNode() const { return m_baseNode; }
    long baseOffset() const { return m_baseOffset; }

    DOM::NodeImpl *extentNode() const { return m_extentNode; }
    long extentOffset() const { return m_extentOffset; }

    DOM::NodeImpl *startNode() const;
    long startOffset() const;

    DOM::NodeImpl *endNode() const;
    long endOffset() const;

    void setVisible(bool flag=true);
    bool visible() const { return m_visible; }
    
    void invalidate();
    
    bool isEmpty() const;
    
#ifdef APPLE_CHANGES
    void paint(QPainter *p, const QRect &rect) const;
#endif

    KHTMLSelection &operator=(const KHTMLSelection &o);
    
    friend bool operator==(const KHTMLSelection &a, const KHTMLSelection &b);
    friend bool operator!=(const KHTMLSelection &a, const KHTMLSelection &b);
    
    friend class KHTMLPart;

    void dump() {
        fprintf(stderr, "selection: %p:%d ; %p:%d (%p:%d ; %p:%d)\n", 
            m_baseNode, m_baseOffset, m_extentNode, m_extentOffset,
            startNode(), startOffset(), endNode(), endOffset());
    }
    
private:
    void setPart(KHTMLPart *part);

    void update();

    void timerEvent(QTimerEvent *e);
    void repaint(bool immediate=false) const;

	void setBaseNode(DOM::NodeImpl *);
	void setBaseOffset(long);
	void setExtentNode(DOM::NodeImpl *);
	void setExtentOffset(long);

	void setStart(DOM::NodeImpl *, long);
	void setStartNode(DOM::NodeImpl *);
	void setStartOffset(long);
	void setEnd(DOM::NodeImpl *, long);
	void setEndNode(DOM::NodeImpl *);
	void setEndOffset(long);

    bool nodeIsBeforeNode(DOM::NodeImpl *n1, DOM::NodeImpl *n2);

    void calculateStartAndEnd(ETextElement select=CHARACTER);
    
    DOM::DOMPosition nextCharacterPosition();
    
    KHTMLPart *m_part;            // part for this selection

    DOM::NodeImpl *m_baseNode;    // base node for the selection
    long m_baseOffset;            // offset into base node where selection is
    DOM::NodeImpl *m_extentNode;  // extent node for the selection
    long m_extentOffset;          // offset into extent node where selection is

    DOM::NodeImpl *m_startNode;   // start node for the selection (read-only)
    long m_startOffset;           // offset into start node where selection is (read-only)
    DOM::NodeImpl *m_endNode;     // end node for the selection (read-only)
    long m_endOffset;             // offset into end node where selection is (read-only)

	EState m_state;               // the state of the selection

    int m_caretBlinkTimer;        // caret blink frequency timer id
	
	int m_caretX;
	int m_caretY;
	int m_caretSize;

	bool m_baseIsStart : 1;     // true if base node is before the extent node
    bool m_caretBlinks : 1;     // true if caret blinks
    bool m_caretPaint : 1;      // flag used to deal with blinking the caret
    bool m_visible : 1;         // true if selection is to be displayed at all
	bool m_startEndValid : 1;   // true if the start and end are valid
};


inline bool operator==(const KHTMLSelection &a, const KHTMLSelection &b)
{
    return a.baseNode() == b.baseNode() && a.baseOffset() == b.baseOffset() &&
        a.extentNode() == b.extentNode() && a.extentOffset() == b.extentOffset();
}

inline bool operator!=(const KHTMLSelection &a, const KHTMLSelection &b)
{
    return !(a == b);
}

#endif