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
#include "xml/dom_nodeimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include <KWQAssertions.h>
#include <CoreServices/CoreServices.h>
#endif

using DOM::DocumentImpl;
using DOM::DOMPosition;
using DOM::DOMString;
using DOM::Node;
using DOM::NodeImpl;
using DOM::Range;
using DOM::TextImpl;
using khtml::InlineTextBox;
using khtml::InlineTextBoxArray;
using khtml::RenderObject;
using khtml::RenderText;

enum { CARET_BLINK_FREQUENCY = 500 };

#if APPLE_CHANGES
static void findWordBoundary(QChar *chars, int len, int position, int *start, int *end);
static bool firstRunAt(RenderObject *renderNode, int y, NodeImpl *&startNode, long &startOffset);
static bool lastRunAt(RenderObject *renderNode, int y, NodeImpl *&endNode, long &endOffset);
static bool startAndEndLineNodesIncludingNode(DOM::NodeImpl *node, int offset, KHTMLSelection &selection);
#endif


KHTMLSelection::KHTMLSelection() 
	: QObject(),
	  m_part(0),
	  m_baseNode(0), m_baseOffset(0), m_extentNode(0), m_extentOffset(0),
	  m_startNode(0), m_startOffset(0), m_endNode(0), m_endOffset(0),
	  m_state(NONE), m_caretBlinkTimer(0),
      m_baseIsStart(true), m_caretBlinks(true), m_caretPaint(false), 
      m_visible(false), m_startEndValid(false)
{
}

KHTMLSelection::KHTMLSelection(const KHTMLSelection &o)
	: QObject(),
	  m_part(o.m_part),
	  m_baseNode(0), m_baseOffset(0), m_extentNode(0), m_extentOffset(0),
	  m_startNode(0), m_startOffset(0), m_endNode(0), m_endOffset(0)
{
    setBase(o.baseNode(), o.baseOffset());
    setExtent(o.extentNode(), o.extentOffset());
    setStart(o.startNode(), o.startOffset());
    setEnd(o.endNode(), o.endOffset());

	m_state = o.m_state;
	m_caretBlinkTimer = o.m_caretBlinkTimer;
	m_baseIsStart = o.m_baseIsStart;
	m_caretBlinks = o.m_caretBlinks;
	m_caretPaint = true;
	m_visible = o.m_visible;
    m_startEndValid = true;
}

KHTMLSelection::~KHTMLSelection()
{
    if (m_baseNode)
        m_baseNode->deref();
    if (m_extentNode)
        m_extentNode->deref();
}

KHTMLSelection &KHTMLSelection::operator=(const KHTMLSelection &o)
{
    m_part = o.m_part;
    
    setBase(o.baseNode(), o.baseOffset());
    setExtent(o.extentNode(), o.extentOffset());
    setStart(o.startNode(), o.startOffset());
    setEnd(o.endNode(), o.endOffset());

	m_state = o.m_state;
	m_caretBlinkTimer = o.m_caretBlinkTimer;
	m_baseIsStart = o.m_baseIsStart;
	m_caretBlinks = o.m_caretBlinks;
	m_caretPaint = true;
	m_visible = o.m_visible;
    m_startEndValid = true;
    return *this;
}

void KHTMLSelection::setSelection(DOM::NodeImpl *node, long offset)
{
	setBaseNode(node);
	setExtentNode(node);
	setBaseOffset(offset);
	setExtentOffset(offset);
	update();
}

void KHTMLSelection::setSelection(const DOM::Range &r)
{
	setSelection(r.startContainer().handle(), r.startOffset(), 
		r.endContainer().handle(), r.endOffset());
}

void KHTMLSelection::setSelection(const DOM::DOMPosition &pos)
{
	setSelection(pos.node(), pos.offset());
}

void KHTMLSelection::setSelection(DOM::NodeImpl *baseNode, long baseOffset, DOM::NodeImpl *extentNode, long extentOffset)
{
	setBaseNode(baseNode);
	setExtentNode(extentNode);
	setBaseOffset(baseOffset);
	setExtentOffset(extentOffset);
	update();
}

void KHTMLSelection::setBase(DOM::NodeImpl *node, long offset)
{
	setBaseNode(node);
	setBaseOffset(offset);
	update();
}

void KHTMLSelection::setExtent(DOM::NodeImpl *node, long offset)
{
	setExtentNode(node);
	setExtentOffset(offset);
	update();
}

bool KHTMLSelection::alterSelection(EAlter alter, EDirection dir, ETextElement elem)
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
        setSelection(pos.node(), pos.offset());
    else // alter == EXTEND
        setExtent(pos.node(), pos.offset());
    
    return true;
}

void KHTMLSelection::clearSelection()
{
	setBaseNode(0);
	setExtentNode(0);
	setBaseOffset(0);
	setExtentOffset(0);
	update();
}

NodeImpl *KHTMLSelection::startNode() const
{ 
    return m_startNode;
}

long KHTMLSelection::startOffset() const
{ 
    return m_startOffset;
}

NodeImpl *KHTMLSelection::endNode() const 
{
    return m_endNode;
}

long KHTMLSelection::endOffset() const 
{ 
    return m_endOffset;
}

void KHTMLSelection::setVisible(bool flag)
{
    m_visible = flag;
    update();
}

void KHTMLSelection::invalidate()
{
    update();
}

void KHTMLSelection::update()
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

    // update start and end
    m_startEndValid = false;
    calculateStartAndEnd();
    
    // update the blink timer
    if (m_caretBlinkTimer >= 0)
        killTimer(m_caretBlinkTimer);
    if (m_visible && m_state == CARET && m_caretBlinks)
        m_caretBlinkTimer = startTimer(CARET_BLINK_FREQUENCY);
    else
        m_caretBlinkTimer = -1;

    // short-circuit if not visible
    if (!m_visible) {
        if (m_caretPaint) {
            m_caretPaint = false;
            repaint();
        }
        return;
    }

    // short-circuit if not CARET state
	if (m_state != CARET)
		return;

    // calculate the new caret rendering position
    int oldX = m_caretX;   
    int oldY = m_caretY;   
    int oldSize = m_caretSize;
    
    int newX = 0;
    int newY = 0;
    int newSize = 0;
    
    NodeImpl *node = startNode();
    if (node && node->renderer()) {
        int w;
        node->renderer()->caretPos(startOffset(), true, newX, newY, w, newSize);
    }

    // repaint the old position if necessary
    // prevents two carets from ever being drawn
    if (m_caretPaint && (oldX != newX || oldY != newY || oldSize != newSize)) {
        repaint();
    }

    // update caret rendering position
    m_caretX = newX;
    m_caretY = newY;
    m_caretSize = newSize;

    // paint the caret if it is visible
    if (m_visible && m_caretSize != 0) {
        m_caretPaint = true;
        repaint();
    }
}

bool KHTMLSelection::isEmpty() const
{
    return m_baseNode == 0 && m_extentNode == 0;
}

#ifdef APPLE_CHANGES
void KHTMLSelection::paint(QPainter *p, const QRect &rect) const
{
    if (!m_caretPaint || m_state != CARET)
        return;

    QRect pos(m_caretX, m_caretY, 1, m_caretSize);
    if (pos.intersects(rect)) {
        QPen pen = p->pen();
        pen.setStyle(SolidLine);
        pen.setColor(Qt::black);
        pen.setWidth(1);
        p->setPen(pen);
        p->drawLine(pos.left(), pos.top(), pos.left(), pos.bottom());
    }
}
#endif

void KHTMLSelection::setPart(KHTMLPart *part)
{
    m_part = part;
}

void KHTMLSelection::timerEvent(QTimerEvent *e)
{
    if (e->timerId() == m_caretBlinkTimer && m_visible) {
        m_caretPaint = !m_caretPaint;
        repaint();
    }
}

void KHTMLSelection::repaint(bool immediate) const
{
    KHTMLView *v = m_part->view();
    if (!v)
        return;
    // EDIT FIXME: fudge a bit to make sure we don't leave behind artifacts
    v->updateContents(m_caretX - 1, m_caretY - 1, 3, m_caretSize + 2, immediate);
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

void KHTMLSelection::setStart(DOM::NodeImpl *node, long offset)
{
    setStartNode(node);
    setStartOffset(offset);
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

void KHTMLSelection::setEnd(DOM::NodeImpl *node, long offset)
{
    setEndNode(node);
    setEndOffset(offset);
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

void KHTMLSelection::expandSelection(ETextElement select)
{
    m_startEndValid = false;
    calculateStartAndEnd(select);
}

void KHTMLSelection::calculateStartAndEnd(ETextElement select)
{
    if (m_startEndValid)
        return;

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
    if (select == CHARACTER) {
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
    else if (select == WORD) {
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
    else {  // select == LINE
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

	// update the state
	if (!m_startNode && !m_endNode)
		m_state = NONE;
	if (m_startNode == m_endNode && m_startOffset == m_endOffset)
		m_state = CARET;
	else
		m_state = RANGE;
    
    m_startEndValid = true;
}

DOMPosition KHTMLSelection::nextCharacterPosition()
{
    DOMPosition result;
	NodeImpl *node = endNode();
	long offset = endOffset();
    long desiredOffset = offset + 1;

    if (!node)
        return result;
    
    //
    // Look in this renderer
    //
    RenderObject *renderer = node->renderer();
    if (renderer->isText()) {
        RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer);
        InlineTextBoxArray runs = textRenderer->inlineTextBoxes();
        unsigned i = 0;
        for (i = 0; i < runs.count(); i++) {
            long start = runs[i]->m_start;
            long end = runs[i]->m_start + runs[i]->m_len;
            if (desiredOffset > end) {
                // Skip this node.
                // It is too early in the text runs to be involved.
                continue;
            }
            else if (desiredOffset >= start && 
                (desiredOffset < end || (desiredOffset == end && i + 1 == runs.count() && !renderer->nextEditable())) ||
                (desiredOffset == end && textRenderer->precedesLineBreak() && !textRenderer->followsLineBreak())) {
                // Desired offset is in this node.
                // Either it is:
                // 1. at or after the start and before, but not at the end
                // 2. at the end of a text run and is immediately followed by a line break
                //    but does not precede a line break
                // 3. at the end of the editable content of the document
                return DOMPosition(renderer->element(), desiredOffset);
            }
            else if (desiredOffset <= start) {
                // The offset we're looking for is before this node
                // this means the offset must be in text that is
                // not rendered. Just return the start of the node.
                return DOMPosition(renderer->element(), start);
            }
        }
    }
    else if (desiredOffset < renderer->caretMaxOffset() || (desiredOffset == renderer->caretMaxOffset() && !renderer->nextEditable())) {
        return DOMPosition(node, desiredOffset);
    }

    //
    // Look in next renderer(s)
    //
    renderer = renderer->nextEditable();
    while (renderer) {
        if (renderer->isText()) {
            RenderText *textRenderer = static_cast<khtml::RenderText *>(renderer);
            InlineTextBoxArray runs = textRenderer->inlineTextBoxes();
            if (runs.count())
                return DOMPosition(renderer->element(), runs[0]->m_start);
        }
        else {
            return DOMPosition(renderer->element(), renderer->caretMinOffset());
        }
        renderer = renderer->nextEditable();
    }

    result = DOMPosition(node, offset);
    return result;
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
    n = n1->parentNode()->firstChild();
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
            InlineTextBoxArray runs = textRenderer->inlineTextBoxes();
            for (unsigned i = 0; i != runs.count(); i++) {
                if (runs[i]->m_y == y) {
                    startNode = textRenderer->element();
                    startOffset = runs[i]->m_start;
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
            InlineTextBoxArray runs = textRenderer->inlineTextBoxes();
            for (int i = (int)runs.count()-1; i >= 0; i--) {
                if (runs[i]->m_y == y) {
                    endNode = textRenderer->element();
                    endOffset = runs[i]->m_start + runs[i]->m_len;
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
        
        selection.setSelection(startNode, startOffset, endNode, endOffset);
        
        return true;
    }
    return false;
}

#endif
