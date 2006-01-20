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

#include "config.h"
#include "dom_position.h"

#include <qstring.h>

#include "css_computedstyle.h"
#include "css_valueimpl.h"
#include "dom_elementimpl.h"
#include "dom2_range.h"
#include "dom2_rangeimpl.h"
#include "dom2_viewsimpl.h"
#include "helper.h"
#include "htmlediting.h"
#include "text_affinity.h"
#include "visible_position.h"
#include "visible_units.h"
#include "RenderBlock.h"
#include "render_flow.h"
#include "render_line.h"
#include "render_style.h"
#include "InlineTextBox.h"
#include "visible_text.h"
#include "htmlnames.h"

#include <kxmlcore/Assertions.h>
#include "KWQLogging.h"

using khtml::EAffinity;
using khtml::InlineBox;
using khtml::InlineTextBox;
using khtml::isAtomicNode;
using khtml::isCollapsibleWhitespace;
using khtml::maxDeepOffset;
using khtml::RenderBlock;
using khtml::RenderFlow;
using khtml::RenderObject;
using khtml::RenderText;
using khtml::RootInlineBox;
using khtml::VISIBLE;
using khtml::VisiblePosition;

namespace DOM {

using namespace HTMLNames;

static NodeImpl *nextRenderedEditable(NodeImpl *node)
{
    while (1) {
        node = node->nextEditable();
        if (!node)
            return 0;
        if (!node->renderer())
            continue;
        if (node->renderer()->inlineBox(0))
            return node;
    }
    return 0;
}

static NodeImpl *previousRenderedEditable(NodeImpl *node)
{
    while (1) {
        node = node->previousEditable();
        if (!node)
            return 0;
        if (!node->renderer())
            continue;
        if (node->renderer()->inlineBox(0))
            return node;
    }
    return 0;
}


Position::Position(NodeImpl *node, int offset) 
    : m_node(node), m_offset(offset) 
{ 
    if (node) {
        node->ref();
    }
}

Position::Position(const Position &o)
    : m_node(o.m_node), m_offset(o.m_offset) 
{
    if (m_node) {
        m_node->ref();
    }
}

Position::~Position()
{
    if (m_node) {
        m_node->deref();
    }
}

Position &Position::operator=(const Position &o)
{
    if (m_node) {
        m_node->deref();
    }
    m_node = o.m_node;
    if (m_node) {
        m_node->ref();
    }

    m_offset = o.m_offset;
    
    return *this;
}

void Position::clear()
{
    if (m_node) {
        m_node->deref();
        m_node = 0;
    }
    m_offset = 0;
}

ElementImpl *Position::element() const
{
    NodeImpl *n;
    for (n = node(); n && !n->isElementNode(); n = n->parentNode())
        ; // empty loop body
    return static_cast<ElementImpl *>(n);
}

CSSComputedStyleDeclarationImpl *Position::computedStyle() const
{
    ElementImpl *elem = element();
    if (!elem)
        return 0;
    return new CSSComputedStyleDeclarationImpl(elem);
}

Position Position::previous(EUsingComposedCharacters usingComposedCharacters) const
{
    NodeImpl *n = node();
    if (!n)
        return *this;
    
    int o = offset();
    assert(o >= 0);

    if (o > 0) {
        NodeImpl *child = n->childNode(o - 1);
        if (child) {
            return Position(child, maxDeepOffset(child));
        }
        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going backward one character at a time is correct.
        //   2) The old offset was a bogus offset like (<br>, 1), and there is no child.
        //      Going from 1 to 0 is correct.
        return Position(n, usingComposedCharacters ? n->previousOffset(o) : o - 1);
    }

    NodeImpl *parent = n->parentNode();
    if (!parent)
        return *this;

    return Position(parent, n->nodeIndex());
}

Position Position::next(EUsingComposedCharacters usingComposedCharacters) const
{
    NodeImpl *n = node();
    if (!n)
        return *this;
    
    int o = offset();
    assert(o >= 0);

    if (o < maxDeepOffset(n)) {
        NodeImpl *child = n->childNode(o);
        if (child) {
            return Position(child, 0);
        }
        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going forward one character at a time is correct.
        //   2) The new offset is a bogus offset like (<br>, 1), and there is no child.
        //      Going from 0 to 1 is correct.
        return Position(n, usingComposedCharacters ? n->nextOffset(o) : o + 1);
    }

    NodeImpl *parent = n->parentNode();
    if (!parent)
        return *this;

    return Position(parent, n->nodeIndex() + 1);
}

bool Position::atStart() const
{
    NodeImpl *n = node();
    if (!n)
        return true;
    
    return offset() <= 0 && n->parent() == 0;
}

bool Position::atEnd() const
{
    NodeImpl *n = node();
    if (!n)
        return true;
    
    return offset() >= maxDeepOffset(n) && n->parent() == 0;
}

int Position::renderedOffset() const
{
    if (!node()->isTextNode())
        return offset();
   
    if (!node()->renderer())
        return offset();
                    
    int result = 0;
    RenderText *textRenderer = static_cast<RenderText *>(node()->renderer());
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        int start = box->m_start;
        int end = box->m_start + box->m_len;
        if (offset() < start)
            return result;
        if (offset() <= end) {
            result += offset() - start;
            return result;
        }
        result += box->m_len;
    }
    return result;
}

// return first preceding DOM position rendered at a different location, or "this"
Position Position::previousCharacterPosition(EAffinity affinity) const
{
    if (isNull())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();

    bool atStartOfLine = isStartOfLine(VisiblePosition(*this, affinity));
    bool rendered = inRenderedContent();
    
    Position currentPos = *this;
    while (!currentPos.atStart()) {
        currentPos = currentPos.previous();

        if (currentPos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atStartOfLine || !rendered) {
            if (currentPos.inRenderedContent())
                return currentPos;
        } else if (rendersInDifferentPosition(currentPos))
            return currentPos;
    }
    
    return *this;
}

// return first following position rendered at a different location, or "this"
Position Position::nextCharacterPosition(EAffinity affinity) const
{
    if (isNull())
        return Position();

    NodeImpl *fromRootEditableElement = node()->rootEditableElement();

    bool atEndOfLine = isEndOfLine(VisiblePosition(*this, affinity));
    bool rendered = inRenderedContent();
    
    Position currentPos = *this;
    while (!currentPos.atEnd()) {
        currentPos = currentPos.next();

        if (currentPos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atEndOfLine || !rendered) {
            if (currentPos.inRenderedContent())
                return currentPos;
        } else if (rendersInDifferentPosition(currentPos))
            return currentPos;
    }
    
    return *this;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
static bool isStreamer(const Position &pos)
{
    if (pos.isNull())
        return true;
        
    if (isAtomicNode(pos.node()))
        return true;
        
    return pos.offset() == 0;
}

// AFAIK no one has a clear, complete definition for this method and how it is used.
// Here is what I have come to understand from re-working the code after fixing PositionIterator
// for <rdar://problem/4103339>.  See also Ken's comments in the header.  Fundamentally, upstream()
// scans backward in the DOM starting at "this" to return the closest previous visible DOM position
// that is either in a text node, or just after a replaced or BR element (btw downstream() also
// considers empty blocks).  The search is limited to the enclosing block.  If the search would
// otherwise have entered into a part of the DOM with a different enclosing block, including a
// nested one, the search stops and this function returns the highest previous visible DOM position
// that is either in an atomic node (i.e. text) or is the end of a non-atomic node.
Position Position::upstream() const
{
    // start at equivalent deep position
    Position start = VisiblePosition::deepEquivalent(*this);
    NodeImpl *startNode = start.node();
    if (!startNode)
        return Position();
    
    // iterate backward from there, looking for a qualified position
    NodeImpl *block = startNode->enclosingBlockFlowOrTableElement();
    Position lastVisible = *this;
    Position currentPos = start;
    RootInlineBox *lastRoot = 0;
    for (; !currentPos.atStart(); currentPos = currentPos.previous()) {
        NodeImpl *currentNode = currentPos.node();
        int currentOffset = currentPos.offset();

        // limit traversal to block or table enclosing the original element
        // NOTE: This includes not going into nested blocks
        if (block != currentNode->enclosingBlockFlowOrTableElement())
            return lastVisible;

        // skip position in unrendered or invisible node
        RenderObject *renderer = currentNode->renderer();
        if (!renderer || renderer->style()->visibility() != VISIBLE)
            continue;

        // Don't move to a position that's on a different line.
        InlineBox *box = renderer->inlineBox(currentPos.offset(), DOWNSTREAM);
        RootInlineBox *currentRoot = box ? box->root() : 0;
        // We consider [br, 1] to be a position that exists visually on the line after the one
        // on which the <br> appears, so treat it specially.
        if (currentRoot && currentNode->hasTagName(brTag) && currentOffset == 1)
            currentRoot = currentRoot->nextRootBox();
        if (lastRoot == 0)
            lastRoot = currentRoot;
        else if (currentRoot && currentRoot != lastRoot)
            return lastVisible;
        
        // track last visible streamer position
        if (isStreamer(currentPos))
            lastVisible = currentPos;
            
        // return position after replaced or BR elements
        // NOTE: caretMaxOffset() can be less than childNodeCount()!!
        // e.g. SELECT and APPLET nodes
        if (renderer->isReplaced() || renderer->isBR()) {
            if (currentOffset >= renderer->caretMaxOffset())
                return Position(currentNode, renderer->caretMaxOffset());
            continue;
        }

        // return current position if it is in rendered text
        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (currentNode != startNode) {
                assert(currentOffset >= renderer->caretMaxOffset());
                return Position(currentNode, renderer->caretMaxOffset());
            }

            if (currentOffset < 0)
                continue;

            uint textOffset = currentOffset;
            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset > box->start() && textOffset <= box->start() + box->len())
                    return currentPos;
                    
                if (box != textRenderer->lastTextBox() && 
                    !box->nextOnLine() && 
                    textOffset == box->start() + box->len() + 1)
                    return currentPos;
            }
        }
    }

    return lastVisible;
}

// AFAIK no one has a clear, complete definition for this method and how it is used.
// Here is what I have come to understand from re-working the code after fixing PositionIterator
// for <rdar://problem/4103339>.  See also Ken's comments in the header.  Fundamentally, downstream()
// scans forward in the DOM starting at "this" to return the first visible DOM position that is
// either in a text node, or just before a replaced, BR element, or empty block flow element (i.e.
// non-text nodes with no children).  The search is limited to the enclosing block.  If the search
// would otherwise have entered into a part of the DOM with a different enclosing block, including a
// nested one, the search stops and this function returns the first previous position that is either
// in an atomic node (i.e. text) or is at offset 0.
Position Position::downstream() const
{
    Position start = VisiblePosition::deepEquivalent(*this);
    NodeImpl *startNode = start.node();
    if (!startNode)
        return Position();

    // iterate forward from there, looking for a qualified position
    NodeImpl *block = startNode->enclosingBlockFlowOrTableElement();
    Position lastVisible = *this;
    Position currentPos = start;
    RootInlineBox *lastRoot = 0;
    for (; !currentPos.atEnd(); currentPos = currentPos.next()) {   
        NodeImpl *currentNode = currentPos.node();
        int currentOffset = currentPos.offset();

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (currentNode->hasTagName(bodyTag) && currentOffset >= (int) currentNode->childNodeCount())
            break;
            
        // limit traversal to block or table enclosing the original element
        // return the last visible streamer position
        // NOTE: This includes not going into nested blocks
        if (block != currentNode->enclosingBlockFlowOrTableElement())
            return lastVisible;

        // skip position in unrendered or invisible node
        RenderObject *renderer = currentNode->renderer();
        if (!renderer || renderer->style()->visibility() != VISIBLE)
            continue;

        // Don't move to a position that's on a different line.
        InlineBox *box = renderer->inlineBox(currentPos.offset(), DOWNSTREAM);
        RootInlineBox *currentRoot = box ? box->root() : 0;
        // We consider [br, 1] to be a position that exists visually on the line after the one
        // on which the <br> appears, so treat it specially.
        if (currentRoot && currentNode->hasTagName(brTag) && currentOffset == 1)
            currentRoot = currentRoot->nextRootBox();
        if (lastRoot == 0)
            lastRoot = currentRoot;
        else if (currentRoot && currentRoot != lastRoot)
            return lastVisible;
        
        // track last visible streamer position
        if (isStreamer(currentPos))
            lastVisible = currentPos;
            
        // if now at a offset 0 of a rendered block flow element...
        //      - return current position if the element has no children (i.e. is a leaf)
        //      - return child node, offset 0, if the first visible child is not a block flow element
        //      - otherwise, skip this position (first visible child is a block, and we will
        //          get there eventually via the iterator)
        if ((currentNode != startNode && renderer->isBlockFlow()) && (currentOffset == 0)) {
            if (!currentNode->firstChild())
                return currentPos;
                
            for (NodeImpl *child = currentNode->firstChild(); child; child = child->nextSibling()) {
                RenderObject *r = child->renderer();
                if (r && r->style()->visibility() == VISIBLE) {
                    if (r->isBlockFlow())
                        break; // break causes continue code below to run.

                    return Position(child, 0);
                }
            }

            continue;
        }

        // return position before replaced or BR elements
        if (renderer->isReplaced() || renderer->isBR()) {
            if (currentOffset <= renderer->caretMinOffset())
                return Position(currentNode, renderer->caretMinOffset());
            continue;
        }

        // return current position if it is in rendered text
        if (renderer->isText() && static_cast<RenderText *>(renderer)->firstTextBox()) {
            if (currentNode != startNode) {
                assert(currentOffset == 0);
                return Position(currentNode, renderer->caretMinOffset());
            }

            if (currentOffset < 0)
                continue;

            uint textOffset = currentOffset;

            RenderText *textRenderer = static_cast<RenderText *>(renderer);
            for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset >= box->start() && textOffset <= box->end())
                    return currentPos;
                
                if (box != textRenderer->lastTextBox() && 
                     !box->nextOnLine() && 
                     textOffset == box->start() + box->len()) {
                    return currentPos;
                }
            }
        }
    }
    
    return lastVisible;
}

Position Position::equivalentRangeCompliantPosition() const
{
    if (isNull())
        return Position();

    // Make sure that 0 <= constrainedOffset <= num kids, otherwise using this Position
    // in DOM calls can result in exceptions.
    int maxOffset = node()->isTextNode() ? static_cast<TextImpl *>(node())->length(): node()->childNodeCount();
    int constrainedOffset = offset() <= 0 ? 0 : kMin(maxOffset, offset());

    if (!node()->parentNode())
        return Position(node(), constrainedOffset);

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return Position(node(), constrainedOffset);
        
    if (!renderer->isReplaced() && !renderer->isBR())
        return Position(node(), constrainedOffset);
    
    int o = offset();
    const NodeImpl *n = node();
    while ((n = n->previousSibling()))
        o++;
    
    // Make sure that 0 <= constrainedOffset <= num kids, as above.
    NodeImpl *parent = node()->parentNode();
    maxOffset = parent->isTextNode() ? static_cast<TextImpl *>(parent)->length(): parent->childNodeCount();
    constrainedOffset = o <= 0 ? 0 : kMin(maxOffset, o);
    return Position(parent, constrainedOffset);
}

bool Position::inRenderedContent() const
{
    if (isNull())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    if (renderer->style()->visibility() != VISIBLE)
        return false;

    // FIXME: This check returns false for a <br> at the end of a line!
    if (renderer->isBR() && static_cast<RenderText *>(renderer)->firstTextBox()) {
        return offset() == 0;
    }
    else if (renderer->isText()) {
        RenderText *textRenderer = static_cast<RenderText *>(renderer);
        for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            if (offset() >= box->m_start && offset() <= box->m_start + box->m_len) {
                return true;
            }
            else if (offset() < box->m_start) {
                // The offset we're looking for is before this node
                // this means the offset must be in content that is
                // not rendered. Return false.
                return false;
            }
        }
    }
    else if (offset() >= renderer->caretMinOffset() && offset() <= renderer->caretMaxOffset()) {
        // return true for replaced elements, for inline flows if they have a line box
        // and for blocks if they are empty
        if (renderer->isReplaced() ||
            (renderer->isInlineFlow() && static_cast<RenderFlow *>(renderer)->firstLineBox()) ||
            (renderer->isBlockFlow() && !renderer->firstChild() && renderer->height()))
            return true;
    }
    
    return false;
}

bool Position::inRenderedText() const
{
    if (isNull() || !node()->isTextNode())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderText *textRenderer = static_cast<RenderText *>(renderer);
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        if (offset() < box->m_start) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered. Return false.
            return false;
        }
        if (offset() >= box->m_start && offset() <= box->m_start + box->m_len)
            return true;
    }
    
    return false;
}

bool Position::isRenderedCharacter() const
{
    if (isNull() || !node()->isTextNode())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderText *textRenderer = static_cast<RenderText *>(renderer);
    for (InlineTextBox *box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
        if (offset() < box->m_start) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered. Return false.
            return false;
        }
        if (offset() >= box->m_start && offset() < box->m_start + box->m_len)
            return true;
    }
    
    return false;
}

bool Position::rendersInDifferentPosition(const Position &pos) const
{
    if (isNull() || pos.isNull())
        return false;

    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    RenderObject *posRenderer = pos.node()->renderer();
    if (!posRenderer)
        return false;

    if (renderer->style()->visibility() != VISIBLE ||
        posRenderer->style()->visibility() != VISIBLE)
        return false;
    
    if (node() == pos.node()) {
        if (node()->hasTagName(brTag))
            return false;

        if (offset() == pos.offset())
            return false;
            
        if (!node()->isTextNode() && !pos.node()->isTextNode()) {
            if (offset() != pos.offset())
                return true;
        }
    }
    
    if (node()->hasTagName(brTag) && pos.inRenderedContent())
        return true;
                
    if (pos.node()->hasTagName(brTag) && inRenderedContent())
        return true;
                
    if (node()->enclosingBlockFlowElement() != pos.node()->enclosingBlockFlowElement())
        return true;

    if (node()->isTextNode() && !inRenderedText())
        return false;

    if (pos.node()->isTextNode() && !pos.inRenderedText())
        return false;

    int thisRenderedOffset = renderedOffset();
    int posRenderedOffset = pos.renderedOffset();

    if (renderer == posRenderer && thisRenderedOffset == posRenderedOffset)
        return false;

    LOG(Editing, "renderer:               %p [%p]\n", renderer, renderer ? renderer->inlineBox(offset()) : 0);
    LOG(Editing, "thisRenderedOffset:         %d\n", thisRenderedOffset);
    LOG(Editing, "posRenderer:            %p [%p]\n", posRenderer, posRenderer ? posRenderer->inlineBox(offset()) : 0);
    LOG(Editing, "posRenderedOffset:      %d\n", posRenderedOffset);
    LOG(Editing, "node min/max:           %d:%d\n", node()->caretMinOffset(), node()->caretMaxRenderedOffset());
    LOG(Editing, "pos node min/max:       %d:%d\n", pos.node()->caretMinOffset(), pos.node()->caretMaxRenderedOffset());
    LOG(Editing, "----------------------------------------------------------------------\n");

    InlineBox *b1 = renderer ? renderer->inlineBox(offset()) : 0;
    InlineBox *b2 = posRenderer ? posRenderer->inlineBox(pos.offset()) : 0;

    if (!b1 || !b2) {
        return false;
    }

    if (b1->root() != b2->root()) {
        return true;
    }

    if (nextRenderedEditable(node()) == pos.node() && 
        thisRenderedOffset == (int)node()->caretMaxRenderedOffset() && posRenderedOffset == 0) {
        return false;
    }
    
    if (previousRenderedEditable(node()) == pos.node() && 
        thisRenderedOffset == 0 && posRenderedOffset == (int)pos.node()->caretMaxRenderedOffset()) {
        return false;
    }

    return true;
}

Position Position::leadingWhitespacePosition(EAffinity affinity, bool considerNonCollapsibleWhitespace) const
{
    if (isNull())
        return Position();
    
    if (upstream().node()->hasTagName(brTag))
        return Position();

    Position prev = previousCharacterPosition(affinity);
    if (prev != *this && prev.node()->inSameContainingBlockFlowElement(node()) && prev.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(prev.node())->data();
        const QChar &c = string[prev.offset()];
        if (considerNonCollapsibleWhitespace ? (c.isSpace() || c.unicode() == 0xa0) : isCollapsibleWhitespace(c))
            return prev;
    }

    return Position();
}

Position Position::trailingWhitespacePosition(EAffinity affinity, bool considerNonCollapsibleWhitespace) const
{
    if (isNull())
        return Position();

    if (node()->isTextNode()) {
        TextImpl *textNode = static_cast<TextImpl *>(node());
        if (offset() < (int)textNode->length()) {
            DOMString string = static_cast<TextImpl *>(node())->data();
            const QChar &c = string[offset()];
            if (considerNonCollapsibleWhitespace ? (c.isSpace() || c.unicode() == 0xa0) : isCollapsibleWhitespace(c))
                return *this;
            return Position();
        }
    }

    if (downstream().node()->hasTagName(brTag))
        return Position();

    Position next = nextCharacterPosition(affinity);
    if (next != *this && next.node()->inSameContainingBlockFlowElement(node()) && next.node()->isTextNode()) {
        DOMString string = static_cast<TextImpl *>(next.node())->data();
        const QChar &c = string[0];
        if (considerNonCollapsibleWhitespace ? (c.isSpace() || c.unicode() == 0xa0) : isCollapsibleWhitespace(c))
            return next;
    }

    return Position();
}

void Position::debugPosition(const char *msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, node()->nodeName().qstring().latin1(), node(), offset());
}

#ifndef NDEBUG
#define FormatBufferSize 1024
void Position::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    if (isNull()) {
        result = "<null>";
    }
    else {
        char s[FormatBufferSize];
        result += "offset ";
        result += QString::number(m_offset);
        result += " of ";
        m_node->formatForDebugger(s, FormatBufferSize);
        result += s;
    }
          
    strncpy(buffer, result.qstring().latin1(), length - 1);
}
#undef FormatBufferSize

void Position::showTree() const
{
    if (m_node)
        m_node->showTree();
}
#endif


Position startPosition(const RangeImpl *r)
{
    if (!r || r->isDetached())
        return Position();
    int exceptionCode;
    return Position(r->startContainer(exceptionCode), r->startOffset(exceptionCode));
}

Position endPosition(const RangeImpl *r)
{
    if (!r || r->isDetached())
        return Position();
    int exceptionCode;
    return Position(r->endContainer(exceptionCode), r->endOffset(exceptionCode));
}

} // namespace DOM
