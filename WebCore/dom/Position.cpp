/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "Position.h"

#include "CharacterNames.h"
#include "Document.h"
#include "Element.h"
#include "Logging.h"
#include "RenderBlock.h"
#include "CSSComputedStyleDeclaration.h"
#include "htmlediting.h"
#include "HTMLNames.h"
#include "PositionIterator.h"
#include "Text.h"
#include "TextIterator.h"
#include "visible_units.h"
  
namespace WebCore {

using namespace HTMLNames;

static Node *nextRenderedEditable(Node *node)
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

static Node *previousRenderedEditable(Node *node)
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

Position::Position(Node* node, int offset) 
    : m_node(node)
    , m_offset(offset) 
{
}

Position::Position(const PositionIterator& it)
    : m_node(it.m_parent)
    , m_offset(it.m_child ? it.m_child->nodeIndex() : (it.m_parent->hasChildNodes() ? maxDeepOffset(it.m_parent) : it.m_offset))
{
}

void Position::clear()
{
    m_node = 0;
    m_offset = 0;
}

Element* Position::documentElement() const
{
    if (Node* n = node())
        if (Element* e = n->document()->documentElement())
            return e;
    return 0;
}

Element *Position::element() const
{
    Node *n;
    for (n = node(); n && !n->isElementNode(); n = n->parentNode())
        ; // empty loop body
    return static_cast<Element *>(n);
}

PassRefPtr<CSSComputedStyleDeclaration> Position::computedStyle() const
{
    Element *elem = element();
    if (!elem)
        return 0;
    return new CSSComputedStyleDeclaration(elem);
}

Position Position::previous(EUsingComposedCharacters usingComposedCharacters) const
{
    Node *n = node();
    if (!n)
        return *this;
    
    int o = offset();
    // FIXME: Negative offsets shouldn't be allowed. We should catch this earlier.
    ASSERT(o >= 0);

    if (o > 0) {
        Node *child = n->childNode(o - 1);
        if (child) {
            return Position(child, maxDeepOffset(child));
        }
        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going backward one character at a time is correct.
        //   2) The old offset was a bogus offset like (<br>, 1), and there is no child.
        //      Going from 1 to 0 is correct.
        return Position(n, usingComposedCharacters ? uncheckedPreviousOffset(n, o) : o - 1);
    }

    Node *parent = n->parentNode();
    if (!parent)
        return *this;

    return Position(parent, n->nodeIndex());
}

Position Position::next(EUsingComposedCharacters usingComposedCharacters) const
{
    Node *n = node();
    if (!n)
        return *this;
    
    int o = offset();
    // FIXME: Negative offsets shouldn't be allowed. We should catch this earlier.
    ASSERT(o >= 0);

    Node* child = n->childNode(o);
    if (child || !n->hasChildNodes() && o < maxDeepOffset(n)) {
        if (child)
            return Position(child, 0);
            
        // There are two reasons child might be 0:
        //   1) The node is node like a text node that is not an element, and therefore has no children.
        //      Going forward one character at a time is correct.
        //   2) The new offset is a bogus offset like (<br>, 1), and there is no child.
        //      Going from 0 to 1 is correct.
        return Position(n, usingComposedCharacters ? uncheckedNextOffset(n, o) : o + 1);
    }

    Node *parent = n->parentNode();
    if (!parent)
        return *this;

    return Position(parent, n->nodeIndex() + 1);
}

int Position::uncheckedPreviousOffset(const Node* n, int current)
{
    return n->renderer() ? n->renderer()->previousOffset(current) : current - 1;
}

int Position::uncheckedNextOffset(const Node* n, int current)
{
    return n->renderer() ? n->renderer()->nextOffset(current) : current + 1;
}

bool Position::atStart() const
{
    Node *n = node();
    if (!n)
        return true;
    
    return offset() <= 0 && n->parent() == 0;
}

bool Position::atEnd() const
{
    Node *n = node();
    if (!n)
        return true;
    
    return n->parent() == 0 && offset() >= maxDeepOffset(n);
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

    Node *fromRootEditableElement = node()->rootEditableElement();

    bool atStartOfLine = isStartOfLine(VisiblePosition(*this, affinity));
    bool rendered = isCandidate();
    
    Position currentPos = *this;
    while (!currentPos.atStart()) {
        currentPos = currentPos.previous();

        if (currentPos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atStartOfLine || !rendered) {
            if (currentPos.isCandidate())
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

    Node *fromRootEditableElement = node()->rootEditableElement();

    bool atEndOfLine = isEndOfLine(VisiblePosition(*this, affinity));
    bool rendered = isCandidate();
    
    Position currentPos = *this;
    while (!currentPos.atEnd()) {
        currentPos = currentPos.next();

        if (currentPos.node()->rootEditableElement() != fromRootEditableElement)
            return *this;

        if (atEndOfLine || !rendered) {
            if (currentPos.isCandidate())
                return currentPos;
        } else if (rendersInDifferentPosition(currentPos))
            return currentPos;
    }
    
    return *this;
}

// upstream() and downstream() want to return positions that are either in a
// text node or at just before a non-text node.  This method checks for that.
static bool isStreamer(const PositionIterator& pos)
{
    if (!pos.node())
        return true;
        
    if (isAtomicNode(pos.node()))
        return true;
        
    return pos.atStartOfNode();
}

// p.upstream() returns the start of the range of positions that map to the same VisiblePosition as P.
Position Position::upstream() const
{
    Node* startNode = node();
    if (!startNode)
        return Position();
    
    // iterate backward from there, looking for a qualified position
    Node* block = enclosingBlock(startNode);
    PositionIterator lastVisible = *this;
    PositionIterator currentPos = lastVisible;
    Node* originalRoot = node()->rootEditableElement();
    for (; !currentPos.atStart(); currentPos.decrement()) {
        Node* currentNode = currentPos.node();
        
        if (currentNode->rootEditableElement() != originalRoot)
            break;

        // Don't enter a new enclosing block flow or table element.  There is code below that
        // terminates early if we're about to leave an enclosing block flow or table element.
        if (block != enclosingBlock(currentNode))
            return lastVisible;

        // skip position in unrendered or invisible node
        RenderObject* renderer = currentNode->renderer();
        if (!renderer || renderer->style()->visibility() != VISIBLE)
            continue;
                 
        // track last visible streamer position
        if (isStreamer(currentPos))
            lastVisible = currentPos;
        
        // Don't leave a block flow or table element.  We could rely on code above to terminate and 
        // return lastVisible on the next iteration, but we terminate early.
        if (currentNode == enclosingBlock(currentNode) && currentPos.atStartOfNode())
            return lastVisible;

        // Return position after tables and nodes which have content that can be ignored.
        if (editingIgnoresContent(currentNode) || isTableElement(currentNode)) {
            if (currentPos.atEndOfNode())
                return Position(currentNode, maxDeepOffset(currentNode));
            continue;
        }

        // return current position if it is in rendered text
        if (renderer->isText() && static_cast<RenderText*>(renderer)->firstTextBox()) {
            if (currentNode != startNode) {
                // This assertion fires in layout tests in the case-transform.html test because
                // of a mix-up between offsets in the text in the DOM tree with text in the
                // render tree which can have a different length due to case transformation.
                // Until we resolve that, disable this so we can run the layout tests!
                //ASSERT(currentOffset >= renderer->caretMaxOffset());
                return Position(currentNode, renderer->caretMaxOffset());
            }

            unsigned textOffset = currentPos.offsetInLeafNode();
            RenderText* textRenderer = static_cast<RenderText*>(renderer);
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
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

// P.downstream() returns the end of the range of positions that map to the same VisiblePosition as P.
Position Position::downstream() const
{
    Node* startNode = node();
    if (!startNode)
        return Position();

    // iterate forward from there, looking for a qualified position
    Node* block = enclosingBlock(startNode);
    PositionIterator lastVisible = *this;
    PositionIterator currentPos = lastVisible;
    Node* originalRoot = node()->rootEditableElement();
    for (; !currentPos.atEnd(); currentPos.increment()) {   
        Node* currentNode = currentPos.node();
        
        if (currentNode->rootEditableElement() != originalRoot)
            break;

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (currentNode->hasTagName(bodyTag) && currentPos.atEndOfNode())
            break;
            
        // Do not enter a new enclosing block flow or table element, and don't leave the original one.
        if (block != enclosingBlock(currentNode))
            return lastVisible;

        // skip position in unrendered or invisible node
        RenderObject* renderer = currentNode->renderer();
        if (!renderer || renderer->style()->visibility() != VISIBLE)
            continue;
            
        // track last visible streamer position
        if (isStreamer(currentPos))
            lastVisible = currentPos;

        // Return position before tables and nodes which have content that can be ignored.
        if (editingIgnoresContent(currentNode) || isTableElement(currentNode)) {
            if (currentPos.offsetInLeafNode() <= renderer->caretMinOffset())
                return Position(currentNode, renderer->caretMinOffset());
            continue;
        }

        // return current position if it is in rendered text
        if (renderer->isText() && static_cast<RenderText*>(renderer)->firstTextBox()) {
            if (currentNode != startNode) {
                ASSERT(currentPos.atStartOfNode());
                return Position(currentNode, renderer->caretMinOffset());
            }

            unsigned textOffset = currentPos.offsetInLeafNode();

            RenderText* textRenderer = static_cast<RenderText*>(renderer);
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
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

bool Position::hasRenderedNonAnonymousDescendantsWithHeight(RenderObject* renderer)
{
    RenderObject* stop = renderer->nextInPreOrderAfterChildren();
    for (RenderObject *o = renderer->firstChild(); o && o != stop; o = o->nextInPreOrder())
        if (o->element() && o->height())
            return true;
            
    return false;
}

bool Position::nodeIsUserSelectNone(Node* node)
{
    return node && node->renderer() && node->renderer()->style()->userSelect() == SELECT_NONE;
}

bool Position::isCandidate() const
{
    if (isNull())
        return false;
        
    RenderObject *renderer = node()->renderer();
    if (!renderer)
        return false;
    
    if (renderer->style()->visibility() != VISIBLE)
        return false;

    if (renderer->isBR())
        return offset() == 0 && !nodeIsUserSelectNone(node()->parent());

    if (renderer->isText())
        return inRenderedText() && !nodeIsUserSelectNone(node());

    if (isTableElement(node()) || editingIgnoresContent(node()))
        return (offset() == 0 || offset() == maxDeepOffset(node())) && !nodeIsUserSelectNone(node()->parent());

    if (!node()->hasTagName(htmlTag) && renderer->isBlockFlow() && !hasRenderedNonAnonymousDescendantsWithHeight(renderer) &&
       (renderer->height() || node()->hasTagName(bodyTag)))
        return offset() == 0 && !nodeIsUserSelectNone(node());
    
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
        if (offset() < box->m_start && !textRenderer->containsReversedText()) {
            // The offset we're looking for is before this node
            // this means the offset must be in content that is
            // not rendered. Return false.
            return false;
        }
        if (box->containsCaretOffset(offset()))
            // Return false for offsets inside composed characters.
            return offset() == 0 || offset() == textRenderer->nextOffset(textRenderer->previousOffset(offset()));
    }
    
    return false;
}

static unsigned caretMaxRenderedOffset(const Node* n)
{
    RenderObject* r = n->renderer();
    if (r)
        return r->caretMaxRenderedOffset();
    
    if (n->isCharacterDataNode())
        return static_cast<const CharacterData*>(n)->length();
    return 1;
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
        if (offset() < box->m_start && !textRenderer->containsReversedText()) {
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
    
    if (node()->hasTagName(brTag) && pos.isCandidate())
        return true;
                
    if (pos.node()->hasTagName(brTag) && isCandidate())
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
    LOG(Editing, "node min/max:           %d:%d\n", caretMinOffset(node()), caretMaxRenderedOffset(node()));
    LOG(Editing, "pos node min/max:       %d:%d\n", caretMinOffset(pos.node()), caretMaxRenderedOffset(pos.node()));
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
        thisRenderedOffset == (int)caretMaxRenderedOffset(node()) && posRenderedOffset == 0) {
        return false;
    }
    
    if (previousRenderedEditable(node()) == pos.node() && 
        thisRenderedOffset == 0 && posRenderedOffset == (int)caretMaxRenderedOffset(pos.node())) {
        return false;
    }

    return true;
}

// This assumes that it starts in editable content.
Position Position::leadingWhitespacePosition(EAffinity affinity, bool considerNonCollapsibleWhitespace) const
{
    ASSERT(isEditablePosition(*this));
    if (isNull())
        return Position();
    
    if (upstream().node()->hasTagName(brTag))
        return Position();

    Position prev = previousCharacterPosition(affinity);
    if (prev != *this && prev.node()->inSameContainingBlockFlowElement(node()) && prev.node()->isTextNode()) {
        String string = static_cast<Text *>(prev.node())->data();
        UChar c = string[prev.offset()];
        if (considerNonCollapsibleWhitespace ? (isSpaceOrNewline(c) || c == noBreakSpace) : isCollapsibleWhitespace(c))
            if (isEditablePosition(prev))
                return prev;
    }

    return Position();
}

// This assumes that it starts in editable content.
Position Position::trailingWhitespacePosition(EAffinity affinity, bool considerNonCollapsibleWhitespace) const
{
    ASSERT(isEditablePosition(*this));
    if (isNull())
        return Position();
    
    VisiblePosition v(*this);
    UChar c = v.characterAfter();
    // The space must not be in another paragraph and it must be editable.
    if (!isEndOfParagraph(v) && v.next(true).isNotNull())
        if (considerNonCollapsibleWhitespace ? (isSpaceOrNewline(c) || c == noBreakSpace) : isCollapsibleWhitespace(c))
            return *this;
    
    return Position();
}

void Position::debugPosition(const char *msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, node()->nodeName().deprecatedString().latin1(), node(), offset());
}

#ifndef NDEBUG

void Position::formatForDebugger(char *buffer, unsigned length) const
{
    String result;
    
    if (isNull())
        result = "<null>";
    else {
        char s[1024];
        result += "offset ";
        result += String::number(m_offset);
        result += " of ";
        m_node->formatForDebugger(s, sizeof(s));
        result += s;
    }
          
    strncpy(buffer, result.deprecatedString().latin1(), length - 1);
}

void Position::showTreeForThis() const
{
    if (m_node)
        m_node->showTreeForThis();
}

#endif

Position startPosition(const Range *r)
{
    if (!r || r->isDetached())
        return Position();
    ExceptionCode ec;
    return Position(r->startContainer(ec), r->startOffset(ec));
}

Position endPosition(const Range *r)
{
    if (!r || r->isDetached())
        return Position();
    ExceptionCode ec;
    return Position(r->endContainer(ec), r->endOffset(ec));
}

} // namespace WebCore

#ifndef NDEBUG

void showTree(const WebCore::Position& pos)
{
    pos.showTreeForThis();
}

void showTree(const WebCore::Position* pos)
{
    if (pos)
        pos->showTreeForThis();
}

#endif
