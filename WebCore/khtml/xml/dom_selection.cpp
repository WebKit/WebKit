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
  
#include "khtml_selection.h"

#include "khtml_part.h"
#include "khtmlview.h"
#include "qevent.h"
#include "qpainter.h"
#include "qrect.h"
#include "dom/dom2_range.h"
#include "dom/dom_node.h"
#include "dom/dom_position.h"
#include "dom/dom_string.h"
#include "rendering/render_object.h"
#include "rendering/render_style.h"
#include "rendering/render_text.h"
#include "xml/dom_docimpl.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include <KWQAssertions.h>
#include <CoreServices/CoreServices.h>

#define EDIT_DEBUG 0
#endif

using DOM::DocumentImpl;
using DOM::DOMPosition;
using DOM::DOMString;
using DOM::ElementImpl;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Range;
using DOM::TextImpl;
using khtml::InlineTextBox;
using khtml::RenderObject;
using khtml::RenderText;

#if APPLE_CHANGES
static void findWordBoundary(QChar *chars, int len, int position, int *start, int *end);
static bool firstRunAt(RenderObject *renderNode, int y, NodeImpl *&startNode, long &startOffset);
static bool lastRunAt(RenderObject *renderNode, int y, NodeImpl *&endNode, long &endOffset);
static bool startAndEndLineNodesIncludingNode(DOM::NodeImpl *node, int offset, KHTMLSelection &selection);
#endif


KHTMLSelection::KHTMLSelection()
{
    init();
}

KHTMLSelection::KHTMLSelection(NodeImpl *node, long offset)
{
    init();

	setBaseNode(node);
	setExtentNode(node);
	setBaseOffset(offset);
	setExtentOffset(offset);

    validate();
}

KHTMLSelection::KHTMLSelection(const DOM::DOMPosition &pos)
{
    init();

	setBaseNode(pos.node());
	setExtentNode(pos.node());
	setBaseOffset(pos.offset());
	setExtentOffset(pos.offset());

    validate();
}

KHTMLSelection::KHTMLSelection(NodeImpl *baseNode, long baseOffset, NodeImpl *endNode, long endOffset)
{
    init();

	setBaseNode(baseNode);
	setExtentNode(endNode);
	setBaseOffset(baseOffset);
	setExtentOffset(endOffset);

    validate();
}

KHTMLSelection::KHTMLSelection(const KHTMLSelection &o)
{
    init();
    
	setBaseNode(o.baseNode());
	setExtentNode(o.extentNode());
	setBaseOffset(o.baseOffset());
	setExtentOffset(o.extentOffset());

	setStartNode(o.startNode());
	setEndNode(o.endNode());
	setStartOffset(o.startOffset());
	setEndOffset(o.endOffset());

    m_state = o.m_state;

    m_baseIsStart = o.m_baseIsStart;
    m_needsCaretLayout = o.m_needsCaretLayout;

    // Only copy the coordinates over if the other object
    // has had a layout, otherwise keep the current
    // coordinates. This prevents drawing artifacts from
    // remaining when the caret is painted and then moves,
    // and the old rectangle needs to be repainted.
    if (!m_needsCaretLayout) {
        m_caretX = o.m_caretX;
        m_caretY = o.m_caretY;
        m_caretSize = o.m_caretSize;
    }
}

void KHTMLSelection::init()
{
    m_baseNode = 0;
    m_baseOffset = 0;
    m_extentNode = 0; 
    m_extentOffset = 0;
    m_startNode = 0;
    m_startOffset = 0;
    m_endNode = 0;
    m_endOffset = 0;
    m_state = NONE; 
    m_caretX = 0;
    m_caretY = 0;
    m_caretSize = 0;
    m_baseIsStart = true;
    m_needsCaretLayout = true;
}

KHTMLSelection::~KHTMLSelection()
{
    if (m_baseNode)
        m_baseNode->deref();
    if (m_extentNode)
        m_extentNode->deref();
    if (m_startNode)
        m_startNode->deref();
    if (m_endNode)
        m_endNode->deref();
}

KHTMLSelection &KHTMLSelection::operator=(const KHTMLSelection &o)
{
	setBaseNode(o.baseNode());
	setExtentNode(o.extentNode());
	setBaseOffset(o.baseOffset());
	setExtentOffset(o.extentOffset());

	setStartNode(o.startNode());
	setEndNode(o.endNode());
	setStartOffset(o.startOffset());
	setEndOffset(o.endOffset());

    m_state = o.m_state;

    m_baseIsStart = o.m_baseIsStart;
    m_needsCaretLayout = o.m_needsCaretLayout;
    
    // Only copy the coordinates over if the other object
    // has had a layout, otherwise keep the current
    // coordinates. This prevents drawing artifacts from
    // remaining when the caret is painted and then moves,
    // and the old rectangle needs to be repainted.
    if (!m_needsCaretLayout) {
        m_caretX = o.m_caretX;
        m_caretY = o.m_caretY;
        m_caretSize = o.m_caretSize;
    }
    
    return *this;
}

void KHTMLSelection::moveTo(DOM::NodeImpl *node, long offset)
{
    moveTo(node, offset, node, offset);
}

void KHTMLSelection::moveTo(const DOM::Range &r)
{
	moveTo(r.startContainer().handle(), r.startOffset(), 
		r.endContainer().handle(), r.endOffset());
}

void KHTMLSelection::moveTo(const DOM::DOMPosition &pos)
{
	moveTo(pos.node(), pos.offset());
}

void KHTMLSelection::moveTo(const KHTMLSelection &o)
{
	moveTo(o.baseNode(), o.baseOffset(), o.extentNode(), o.extentOffset());
}

void KHTMLSelection::moveTo(DOM::NodeImpl *baseNode, long baseOffset, DOM::NodeImpl *extentNode, long extentOffset)
{
	setBaseNode(baseNode);
	setExtentNode(extentNode);
	setBaseOffset(baseOffset);
	setExtentOffset(extentOffset);
	validate();
}

bool KHTMLSelection::modify(EAlter alter, EDirection dir, ETextElement elem)
{
    DOMPosition pos;
    
    switch (dir) {
        case FORWARD:
            switch (elem) {
                case CHARACTER:
                    pos = nextCharacterPosition();
                    break;
                case WORD:
                    break;
                case LINE:
                    break;
            }
            break;
        case BACKWARD:
            switch (elem) {
                case CHARACTER:
                    pos = previousCharacterPosition();
                    break;
                case WORD:
                    break;
                case LINE:
                    break;
            }
            break;
    }
    
    if (pos.isEmpty())
        return false;
    
    if (alter == MOVE)
        moveTo(pos.node(), pos.offset());
    else // alter == EXTEND
        setExtent(pos.node(), pos.offset());
    
    return true;
}

void KHTMLSelection::expandToElement(ETextElement select)
{
    validate(select);
}

void KHTMLSelection::clear()
{
	setBaseNode(0);
	setExtentNode(0);
	setBaseOffset(0);
	setExtentOffset(0);
	validate();
}

void KHTMLSelection::setBase(DOM::NodeImpl *node, long offset)
{
	setBaseNode(node);
	setBaseOffset(offset);
	validate();
}

void KHTMLSelection::setExtent(DOM::NodeImpl *node, long offset)
{
	setExtentNode(node);
	setExtentOffset(offset);
	validate();
}

void KHTMLSelection::setNeedsLayout(bool flag)
{
    m_needsCaretLayout = flag;
}

bool KHTMLSelection::isEmpty() const
{
    return m_baseNode == 0 && m_extentNode == 0;
}

Range KHTMLSelection::toRange() const
{
    if (isEmpty())
        return Range();

    return Range(Node(startNode()), startOffset(), Node(endNode()), endOffset());
}

void KHTMLSelection::layoutCaret()
{
    if (isEmpty() || !startNode()->renderer()) {
        m_caretX = m_caretY = m_caretSize = 0;
    }
    else {
        int w;
        startNode()->renderer()->caretPos(startOffset(), true, m_caretX, m_caretY, w, m_caretSize);
    }

    m_needsCaretLayout = false;
}

QRect KHTMLSelection::getRepaintRect()
{
    // EDIT FIXME: fudge a bit to make sure we don't leave behind artifacts
    return QRect(m_caretX - 1, m_caretY - 1, 3, m_caretSize + 2);
}

void KHTMLSelection::needsCaretRepaint()
{
    if (isEmpty())
        return;

    if (!startNode()->getDocument())
        return;

    KHTMLView *v = startNode()->getDocument()->view();
    if (!v)
        return;

    if (m_needsCaretLayout) {
        // repaint old position and calculate new position
        v->updateContents(getRepaintRect(), false);
        layoutCaret();
        
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
        m_needsCaretLayout = true;
    }
    v->updateContents(getRepaintRect(), false);
}

void KHTMLSelection::paintCaret(QPainter *p, const QRect &rect)
{
    if (isEmpty())
        return;

    if (m_state != CARET)
        return;

    if (m_needsCaretLayout) {
        DOMPosition pos = DOMPosition(startNode(), startOffset());
        if (!inRenderedContent(pos)) {
            moveToRenderedContent();
        }
        layoutCaret();
    }

    QRect caretRect(m_caretX, m_caretY, 1, m_caretSize);
    if (caretRect.intersects(rect)) {
        QPen pen = p->pen();
        pen.setStyle(Qt::SolidLine);
        pen.setColor(Qt::black);
        pen.setWidth(1);
        p->setPen(pen);
        p->drawLine(caretRect.left(), caretRect.top(), caretRect.left(), caretRect.bottom());
    }
}

void KHTMLSelection::setBaseNode(DOM::NodeImpl *node)
{
	if (m_baseNode == node)
		return;

	if (m_baseNode)
		m_baseNode->deref();
	
	m_baseNode = node;
	
	if (m_baseNode)
		m_baseNode->ref();
}

void KHTMLSelection::setBaseOffset(long offset)
{
	m_baseOffset = offset;
}

void KHTMLSelection::setExtentNode(DOM::NodeImpl *node)
{
	if (m_extentNode == node)
		return;

	if (m_extentNode)
		m_extentNode->deref();
	
	m_extentNode = node;
	
	if (m_extentNode)
		m_extentNode->ref();
}
	
void KHTMLSelection::setExtentOffset(long offset)
{
	m_extentOffset = offset;
}

void KHTMLSelection::setStartNode(DOM::NodeImpl *node)
{
	if (m_startNode == node)
		return;

	if (m_startNode)
		m_startNode->deref();
	
	m_startNode = node;
	
	if (m_startNode)
		m_startNode->ref();
}

void KHTMLSelection::setStartOffset(long offset)
{
	m_startOffset = offset;
}

void KHTMLSelection::setEndNode(DOM::NodeImpl *node)
{
	if (m_endNode == node)
		return;

	if (m_endNode)
		m_endNode->deref();
	
	m_endNode = node;
	
	if (m_endNode)
		m_endNode->ref();
}
	
void KHTMLSelection::setEndOffset(long offset)
{
	m_endOffset = offset;
}

void KHTMLSelection::validate(ETextElement expandTo)
{
    // make sure we do not have a dangling start or end
	if (!m_baseNode && !m_extentNode) {
        setBaseOffset(0);
        setExtentOffset(0);
        m_baseIsStart = true;
    }
	else if (!m_baseNode) {
		setBaseNode(m_extentNode);
		setBaseOffset(m_extentOffset);
        m_baseIsStart = true;
	}
	else if (!m_extentNode) {
		setExtentNode(m_baseNode);
		setExtentOffset(m_baseOffset);
        m_baseIsStart = true;
	}
    else {
        // adjust m_baseIsStart as needed
        if (m_baseNode == m_extentNode) {
            if (m_baseOffset > m_extentOffset)
                m_baseIsStart = false;
            else 
                m_baseIsStart = true;
        }
        else if (nodeIsBeforeNode(m_baseNode, m_extentNode))
            m_baseIsStart = true;
        else
            m_baseIsStart = false;
    }

    // calculate the correct start and end positions
#if !APPLE_CHANGES
    if (m_baseIsStart) {
        setStartNode(m_baseNode);
        setStartOffset(m_baseOffset);
        setEndNode(m_extentNode);
        setEndOffset(m_extentOffset);
    }
    else {
        setStartNode(m_extentNode);
        setStartOffset(m_extentOffset);
        setEndNode(m_baseNode);
        setEndOffset(m_baseOffset);
    }
#else
    if (expandTo == CHARACTER) {
        if (m_baseIsStart) {
            setStartNode(m_baseNode);
            setStartOffset(m_baseOffset);
            setEndNode(m_extentNode);
            setEndOffset(m_extentOffset);
        }
        else {
            setStartNode(m_extentNode);
            setStartOffset(m_extentOffset);
            setEndNode(m_baseNode);
            setEndOffset(m_baseOffset);
        }
    }
    else if (expandTo == WORD) {
        int baseStartOffset = m_baseOffset;
        int baseEndOffset = m_baseOffset;
        int extentStartOffset = m_extentOffset;
        int extentEndOffset = m_extentOffset;
        if (m_baseNode && (m_baseNode->nodeType() == Node::TEXT_NODE || m_baseNode->nodeType() == Node::CDATA_SECTION_NODE)) {
            DOMString t = m_baseNode->nodeValue();
            QChar *chars = t.unicode();
            uint len = t.length();
            findWordBoundary(chars, len, m_baseOffset, &baseStartOffset, &baseEndOffset);
        }
        if (m_extentNode && (m_extentNode->nodeType() == Node::TEXT_NODE || m_extentNode->nodeType() == Node::CDATA_SECTION_NODE)) {
            DOMString t = m_extentNode->nodeValue();
            QChar *chars = t.unicode();
            uint len = t.length();
            findWordBoundary(chars, len, m_extentOffset, &extentStartOffset, &extentEndOffset);
        }
        if (m_baseIsStart) {
            setStartNode(m_baseNode);
            setStartOffset(baseStartOffset);
            setEndNode(m_extentNode);
            setEndOffset(extentEndOffset);
        }
        else {
            setStartNode(m_extentNode);
            setStartOffset(extentStartOffset);
            setEndNode(m_baseNode);
            setEndOffset(baseEndOffset);
        }
    }
    else {  // expandTo == LINE
        KHTMLSelection baseSelection = *this;
        KHTMLSelection extentSelection = *this;
        if (m_baseNode && (m_baseNode->nodeType() == Node::TEXT_NODE || m_baseNode->nodeType() == Node::CDATA_SECTION_NODE)) {
            if (startAndEndLineNodesIncludingNode(m_baseNode, m_baseOffset, baseSelection)) {
                setStartNode(baseSelection.baseNode());
                setStartOffset(baseSelection.baseOffset());
                setEndNode(baseSelection.extentNode());
                setEndOffset(baseSelection.extentOffset());
            }
        }
        if (m_extentNode && (m_extentNode->nodeType() == Node::TEXT_NODE || m_extentNode->nodeType() == Node::CDATA_SECTION_NODE)) {
            if (startAndEndLineNodesIncludingNode(m_extentNode, m_extentOffset, extentSelection)) {
                setStartNode(extentSelection.baseNode());
                setStartOffset(extentSelection.baseOffset());
                setEndNode(extentSelection.extentNode());
                setEndOffset(extentSelection.extentOffset());
            }
        }
        if (m_baseIsStart) {
            setStartNode(baseSelection.startNode());
            setStartOffset(baseSelection.startOffset());
            setEndNode(extentSelection.endNode());
            setEndOffset(extentSelection.endOffset());
        }
        else {
            setStartNode(extentSelection.startNode());
            setStartOffset(extentSelection.startOffset());
            setEndNode(baseSelection.endNode());
            setEndOffset(baseSelection.endOffset());
        }
    }
#endif  // APPLE_CHANGES

	// adjust the state
	if (!m_startNode && !m_endNode)
		m_state = NONE;
	else if (m_startNode == m_endNode && m_startOffset == m_endOffset)
		m_state = CARET;
	else
		m_state = RANGE;

    m_needsCaretLayout = true;
    
#if EDIT_DEBUG
    debugPosition();
#endif
}

DOMPosition KHTMLSelection::previousCharacterPosition() const
{
    return previousCharacterPosition(DOMPosition(startNode(), startOffset()));
}

DOMPosition KHTMLSelection::previousCharacterPosition(const DOMPosition &from)
{
    if (!from.node())
        return from;

	NodeImpl *node = from.node();
	long offset = from.offset() - 1;

    //
    // Look in this renderer
    //
    RenderObject *renderer = node->renderer();
    if (renderer->isText()) {
        if (!renderer->isBR()) {
            RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer);
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                long start = box->m_start;
                long end = box->m_start + box->m_len;
                if (offset > end) {
                    // Skip this node.
                    // It is too early in the text runs to be involved.
                    continue;
                }
                else if (offset >= start) {
                    // Offset is in this node, return the start
                    return DOMPosition(node, offset);
                }
            }
        }
    }
    else {
        // Offset is in this node, if:
        // 1. greater than the min offset and less than or equal to the max caret offset
        // 2. at the start of the editable content of the document
        if ((offset > renderer->caretMinOffset() && offset <= renderer->caretMaxOffset()) || 
            (offset == renderer->caretMinOffset() && !renderer->previousEditable()))
            return DOMPosition(node, offset);
    }

    //
    // Look in previous renderer(s)
    //
    renderer = renderer->previousEditable();
    while (renderer) {
        // Offset is in this node, if:
        // 1. it is a BR which follows a line break
        // 2. it is an element with content
    	if (renderer->isBR()) {
			if (renderer->followsLineBreak())
				return DOMPosition(renderer->element(), renderer->caretMinOffset());
    	}
    	else {
    		if (renderer->isText()) {
    			 RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer);
    			 if (!textRenderer->lastTextBox()) 
    			 	continue;
    		}
            offset = renderer->caretMaxOffset();
            if (renderer->nextEditable() && !renderer->precedesLineBreak())
                offset--;
            assert(offset >= 0);
            return DOMPosition(renderer->element(), offset);
    	}
        renderer = renderer->previousEditable();
    }

    // can't move the position
    return from;
}


DOMPosition KHTMLSelection::nextCharacterPosition() const
{
    return nextCharacterPosition(DOMPosition(endNode(), endOffset()));
}

DOMPosition KHTMLSelection::nextCharacterPosition(const DOMPosition &from)
{
    if (!from.node())
        return DOMPosition();

 	NodeImpl *node = from.node();
 	long offset = from.offset() + 1;

    //
    // Look in this renderer
    //
    RenderObject *renderer = node->renderer();
    if (renderer->isText()) {
        if (!renderer->isBR()) {
            RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer);
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                long start = box->m_start;
                long end = box->m_start + box->m_len;
                if (offset > end) {
                    // Skip this node.
                    // It is too early in the text runs to be involved.
                    continue;
                }
                else if (offset >= start) {
                    // Offset is in this node, if:
                    // Either it is:
                    // 1. at or after the start and before, but not at the end
                    // 2. at the end of a text run and is immediately followed by a line break
                    // 3. at the end of the editable content of the document
                    if (offset < end)
                        return DOMPosition(node, offset);
                    else if (offset == end && renderer->precedesLineBreak())
                        return DOMPosition(node, offset);
                    else if (offset == end && !box->nextTextBox() && !renderer->nextEditable())
                        return DOMPosition(node, offset);
                }
                else if (offset < start) {
                    // The offset we're looking for is before this node
                    // this means the offset must be in content that is
                    // not rendered. Just return the start of the node.
                    return DOMPosition(node, start);
                }
            }
        }
    }
    else {
        // Offset is in this node, if:
        // 1. before the max caret offset
        // 2. equal to the max caret offset and is immediately preceded by a line break
        // 3. at the end of the editable content of the document
        if (offset < renderer->caretMaxOffset() ||
            (offset == renderer->caretMaxOffset() && renderer->precedesLineBreak()) ||
            (offset == renderer->caretMaxOffset() && !renderer->nextEditable()))
                return DOMPosition(node, offset);
    }

    //
    // Look in next renderer(s)
    //
    renderer = renderer->nextEditable();
    while (renderer) {
		// Offset is in this node, if:
		// 1. it is a BR which follows a line break
		// 2. it is a text element with content
        // 3. it is a non-text element with content
		if (renderer->isBR()) {
			if (renderer->followsLineBreak())
				return DOMPosition(renderer->element(), renderer->caretMinOffset());
		}
		else if (renderer->isText()) {
            RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer);
            if (textRenderer->firstTextBox())
                return DOMPosition(renderer->element(), textRenderer->firstTextBox()->m_start);
        }
        else {
            return DOMPosition(renderer->element(), renderer->caretMinOffset());
        }
        renderer = renderer->nextEditable();
    }

    // can't move the position
    return from;
}

bool KHTMLSelection::moveToRenderedContent()
{
    if (isEmpty())
        return false;
        
    if (m_state != CARET)
        return false;

    DOMPosition pos = DOMPosition(startNode(), startOffset());
    if (inRenderedContent(pos))
        return true;
        
    // not currently rendered, try moving to next
    DOMPosition next = nextCharacterPosition(pos);
    if (next != pos) {
        moveTo(next);
        return true;
    }

    // could not be moved to next, try prev
    DOMPosition prev = previousCharacterPosition(pos);
    if (prev != pos) {
        moveTo(prev);
        return true;
    }
    
    return false;
}

bool KHTMLSelection::inRenderedContent(const DOMPosition &pos)
{
    if (pos.isEmpty())
        return false;
        
 	long offset = pos.offset();

    RenderObject *renderer = pos.node()->renderer();
    if (!renderer)
        return false;
    
    if (renderer->isText() && !renderer->isBR()) {
        RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer);
        for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            if (offset >= box->m_start && offset <= box->m_start + box->m_len) {
                return true;
            }
            else if (offset < box->m_start) {
                // The offset we're looking for is before this node
                // this means the offset must be in content that is
                // not rendered. Return false.
                return false;
            }
        }
    }
    else if (offset >= renderer->caretMinOffset() && offset <= renderer->caretMaxOffset()) {
        return true;
    }
    
    return false;
}
 
bool KHTMLSelection::nodeIsBeforeNode(NodeImpl *n1, NodeImpl *n2) 
{
	if (!n1 || !n2) 
		return true;
 
 	if (n1 == n2)
 		return true;
 
 	bool result = false;
    int n1Depth = 0;
    int n2Depth = 0;

    // First we find the depths of the two nodes in the tree (n1Depth, n2Depth)
    DOM::NodeImpl *n = n1;
    while (n->parentNode()) {
        n = n->parentNode();
        n1Depth++;
    }
    n = n2;
    while (n->parentNode()) {
        n = n->parentNode();
        n2Depth++;
    }
    // Climb up the tree with the deeper node, until both nodes have equal depth
    while (n2Depth > n1Depth) {
        n2 = n2->parentNode();
        n2Depth--;
    }
    while (n1Depth > n2Depth) {
        n1 = n1->parentNode();
        n1Depth--;
    }
    // Climb the tree with both n1 and n2 until they have the same parent
    while (n1->parentNode() != n2->parentNode()) {
        n1 = n1->parentNode();
        n2 = n2->parentNode();
    }
    // Iterate through the parent's children until n1 or n2 is found
    n = n1->parentNode() ? n1->parentNode()->firstChild() : n1->firstChild();
    while (n) {
        if (n == n1) {
            result = true;
            break;
        }
        else if (n == n2) {
            result = false;
            break;
        }
        n = n->nextSibling();
    }
	return result;
}

#if APPLE_CHANGES

static void findWordBoundary(QChar *chars, int len, int position, int *start, int *end)
{
    TextBreakLocatorRef breakLocator;
    OSStatus status = UCCreateTextBreakLocator(NULL, 0, kUCTextBreakWordMask, &breakLocator);
    if (status == noErr) {
        UniCharArrayOffset startOffset, endOffset;
        status = UCFindTextBreak(breakLocator, kUCTextBreakWordMask, 0, (const UniChar *)chars, len, position, &endOffset);
        if (status == noErr) {
            status = UCFindTextBreak(breakLocator, kUCTextBreakWordMask, kUCTextBreakGoBackwardsMask, (const UniChar *)chars, len, position, &startOffset);
        }
        UCDisposeTextBreakLocator(&breakLocator);
        if (status == noErr) {
            *start = startOffset;
            *end = endOffset;
            return;
        }
    }
    
    // If Carbon fails (why would it?), do a simple space/punctuation boundary check.
    if (chars[position].isSpace()) {
        int pos = position;
        while (chars[pos].isSpace() && pos >= 0)
            pos--;
        *start = pos+1;
        pos = position;
        while (chars[pos].isSpace() && pos < (int)len)
            pos++;
        *end = pos;
    } else if (chars[position].isPunct()) {
        int pos = position;
        while (chars[pos].isPunct() && pos >= 0)
            pos--;
        *start = pos+1;
        pos = position;
        while (chars[pos].isPunct() && pos < (int)len)
            pos++;
        *end = pos;
    } else {
        int pos = position;
        while (!chars[pos].isSpace() && !chars[pos].isPunct() && pos >= 0)
            pos--;
        *start = pos+1;
        pos = position;
        while (!chars[pos].isSpace() && !chars[pos].isPunct() && pos < (int)len)
            pos++;
        *end = pos;
    }
}

static bool firstRunAt(RenderObject *renderNode, int y, NodeImpl *&startNode, long &startOffset)
{
    for (RenderObject *n = renderNode; n; n = n->nextSibling()) {
        if (n->isText()) {
            RenderText *textRenderer = static_cast<khtml::RenderText *>(n);
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (box->m_y == y) {
                    startNode = textRenderer->element();
                    startOffset = box->m_start;
                    return true;
                }
            }
        }
        
        if (firstRunAt(n->firstChild(), y, startNode, startOffset)) {
            return true;
        }
    }
    
    return false;
}

static bool lastRunAt(RenderObject *renderNode, int y, NodeImpl *&endNode, long &endOffset)
{
    RenderObject *n = renderNode;
    if (!n) {
        return false;
    }
    RenderObject *next;
    while ((next = n->nextSibling())) {
        n = next;
    }
    
    while (1) {
        if (lastRunAt(n->firstChild(), y, endNode, endOffset)) {
            return true;
        }
    
        if (n->isText()) {
            RenderText *textRenderer =  static_cast<khtml::RenderText *>(n);
            for (InlineTextBox* box = textRenderer->lastTextBox(); box; box = box->prevTextBox()) {
                if (box->m_y == y) {
                    endNode = textRenderer->element();
                    endOffset = box->m_start + box->m_len;
                    return true;
                }
            }
        }
        
        if (n == renderNode) {
            return false;
        }
        
        n = n->previousSibling();
    }
}

static bool startAndEndLineNodesIncludingNode(DOM::NodeImpl *node, int offset, KHTMLSelection &selection)
{
    if (node && (node->nodeType() == Node::TEXT_NODE || node->nodeType() == Node::CDATA_SECTION_NODE)) {
        int pos;
        int selectionPointY;
        RenderText *renderer = static_cast<RenderText *>(node->renderer());
        InlineTextBox * run = renderer->findNextInlineTextBox( offset, pos );
        DOMString t = node->nodeValue();
        
        if (!run)
            return false;
            
        selectionPointY = run->m_y;
        
        // Go up to first non-inline element.
        khtml::RenderObject *renderNode = renderer;
        while (renderNode && renderNode->isInline())
            renderNode = renderNode->parent();
        
        renderNode = renderNode->firstChild();
        
        DOM::NodeImpl *startNode = 0;
        DOM::NodeImpl *endNode = 0;
        long startOffset;
        long endOffset;
        
        // Look for all the first child in the block that is on the same line
        // as the selection point.
        if (!firstRunAt (renderNode, selectionPointY, startNode, startOffset))
            return false;
    
        // Look for all the last child in the block that is on the same line
        // as the selection point.
        if (!lastRunAt (renderNode, selectionPointY, endNode, endOffset))
            return false;
        
        selection.moveTo(startNode, startOffset, endNode, endOffset);
        
        return true;
    }
    return false;
}

void KHTMLSelection::debugRenderer(RenderObject *r, bool selected) const
{
    if (r->node()->isElementNode()) {
        ElementImpl *element = static_cast<ElementImpl *>(r->node());
        fprintf(stderr, "%s%s\n", selected ? "==> " : "    ", element->tagName().string().latin1());
    }
    else if (r->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(r);
        if (textRenderer->stringLength() == 0 || !textRenderer->firstTextBox()) {
            fprintf(stderr, "%s#text (empty)\n", selected ? "==> " : "    ");
            return;
        }
        
        static const int max = 36;
        QString text = DOMString(textRenderer->string()).string();
        int textLength = text.length();
        if (selected) {
            int offset = 0;
            if (r->node() == startNode())
                offset = startOffset();
            else if (r->node() == endNode())
                offset = endOffset();
                
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
            
            show = show.replace("\n", " ");
            show = show.replace("\r", " ");
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

void KHTMLSelection::debugPosition() const
{
    if (!startNode())
        return;

    static int context = 5;
    
    RenderObject *r = 0;

    fprintf(stderr, "KHTMLSelection =================\n");
    
    int back = 0;
    r = startNode()->renderer();
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

    if (startNode() == endNode())
        debugRenderer(startNode()->renderer(), true);
    else
        for (r = startNode()->renderer(); r && r != endNode()->renderer(); r = r->nextRenderer())
            debugRenderer(r, true);
    
    fprintf(stderr, "\n");
    
    r = endNode()->renderer();
    for (int i = 0; i < context; i++) {
        if (r->nextRenderer()) {
            r = r->nextRenderer();
            debugRenderer(r, false);
        }
        else
            break;
    }

    fprintf(stderr, "================================\n");
}

#endif
