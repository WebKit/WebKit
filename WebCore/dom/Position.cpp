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

#include "CSSComputedStyleDeclaration.h"
#include "CString.h"
#include "CharacterNames.h"
#include "Document.h"
#include "Element.h"
#include "HTMLNames.h"
#include "Logging.h"
#include "PositionIterator.h"
#include "RenderBlock.h"
#include "Text.h"
#include "TextIterator.h"
#include "htmlediting.h"
#include "visible_units.h"
#include <stdio.h>
  
namespace WebCore {

using namespace HTMLNames;

static Node *nextRenderedEditable(Node *node)
{
    while (1) {
        node = node->nextEditable();
        if (!node)
            return 0;
        RenderObject* renderer = node->renderer();
        if (!renderer)
            continue;
        if (renderer->inlineBoxWrapper() || renderer->isText() && static_cast<RenderText*>(renderer)->firstTextBox())
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
        RenderObject* renderer = node->renderer();
        if (!renderer)
            continue;
        if (renderer->inlineBoxWrapper() || renderer->isText() && static_cast<RenderText*>(renderer)->firstTextBox())
            return node;
    }
    return 0;
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
    Element* elem = element();
    if (!elem)
        return 0;
    return WebCore::computedStyle(elem);
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

// Whether or not [node, 0] and [node, maxDeepOffset(node)] are their own VisiblePositions.
// If true, adjacent candidates are visually distinct.
// FIXME: Disregard nodes with renderers that have no height, as we do in isCandidate.
// FIXME: Share code with isCandidate, if possible.
static bool endsOfNodeAreVisuallyDistinctPositions(Node* node)
{
    if (!node || !node->renderer())
        return false;
        
    if (!node->renderer()->isInline())
        return true;
        
    // Don't include inline tables.
    if (node->hasTagName(tableTag))
        return false;
    
    // There is a VisiblePosition inside an empty inline-block container.
    return node->renderer()->isReplaced() && canHaveChildrenForEditing(node) && node->renderer()->height() != 0 && !node->firstChild();
}

static Node* enclosingVisualBoundary(Node* node)
{
    while (node && !endsOfNodeAreVisuallyDistinctPositions(node))
        node = node->parentNode();
        
    return node;
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

// This function and downstream() are used for moving back and forth between visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is collapsible, there are two candidates 
// that map to the VisiblePosition between 'b' and the space.  This function will return the left candidate 
// and downstream() will return the right one.
// Also, upstream() will return [boundary, 0] for any of the positions from [boundary, 0] to the first candidate
// in boundary, where endsOfNodeAreVisuallyDistinctPositions(boundary) is true.
Position Position::upstream() const
{
    Node* startNode = node();
    if (!startNode)
        return Position();
    
    // iterate backward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary(startNode);
    PositionIterator lastVisible = *this;
    PositionIterator currentPos = lastVisible;
    bool startEditable = startNode->isContentEditable();
    Node* lastNode = startNode;
    for (; !currentPos.atStart(); currentPos.decrement()) {
        Node* currentNode = currentPos.node();
        
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing isContentEditable().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->isContentEditable();
            if (startEditable != currentEditable)
                break;
            lastNode = currentNode;
        }

        // If we've moved to a position that is visually disinct, return the last saved position. There 
        // is code below that terminates early if we're *about* to move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible;

        // skip position in unrendered or invisible node
        RenderObject* renderer = currentNode->renderer();
        if (!renderer || renderer->style()->visibility() != VISIBLE)
            continue;
                 
        // track last visible streamer position
        if (isStreamer(currentPos))
            lastVisible = currentPos;
        
        // Don't move past a position that is visually distinct.  We could rely on code above to terminate and 
        // return lastVisible on the next iteration, but we terminate early to avoid doing a nodeIndex() call.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentPos.atStartOfNode())
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
            InlineTextBox* lastTextBox = textRenderer->lastTextBox();
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset <= box->start() + box->len()) {
                    if (textOffset > box->start())
                        return currentPos;
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len() + 1)
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || otherBox->object() == textRenderer && static_cast<InlineTextBox*>(otherBox)->start() > textOffset)
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || otherBox->object() == textRenderer && static_cast<InlineTextBox*>(otherBox)->start() > textOffset)
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos;
            }
        }
    }

    return lastVisible;
}

// This function and upstream() are used for moving back and forth between visually equivalent candidates.
// For example, for the text node "foo     bar" where whitespace is collapsible, there are two candidates 
// that map to the VisiblePosition between 'b' and the space.  This function will return the right candidate 
// and upstream() will return the left one.
// Also, downstream() will return the last position in the last atomic node in boundary for all of the positions
// in boundary after the last candidate, where endsOfNodeAreVisuallyDistinctPositions(boundary).
Position Position::downstream() const
{
    Node* startNode = node();
    if (!startNode)
        return Position();

    // iterate forward from there, looking for a qualified position
    Node* boundary = enclosingVisualBoundary(startNode);
    PositionIterator lastVisible = *this;
    PositionIterator currentPos = lastVisible;
    bool startEditable = startNode->isContentEditable();
    Node* lastNode = startNode;
    for (; !currentPos.atEnd(); currentPos.increment()) {   
        Node* currentNode = currentPos.node();
        
        // Don't check for an editability change if we haven't moved to a different node,
        // to avoid the expense of computing isContentEditable().
        if (currentNode != lastNode) {
            // Don't change editability.
            bool currentEditable = currentNode->isContentEditable();
            if (startEditable != currentEditable)
                break;
            lastNode = currentNode;
        }

        // stop before going above the body, up into the head
        // return the last visible streamer position
        if (currentNode->hasTagName(bodyTag) && currentPos.atEndOfNode())
            break;
            
        // Do not move to a visually distinct position.
        if (endsOfNodeAreVisuallyDistinctPositions(currentNode) && currentNode != boundary)
            return lastVisible;
        // Do not move past a visually disinct position.
        // Note: The first position after the last in a node whose ends are visually distinct
        // positions will be [boundary->parentNode(), originalBlock->nodeIndex() + 1].
        if (boundary && boundary->parentNode() == currentNode)
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
            InlineTextBox* lastTextBox = textRenderer->lastTextBox();
            for (InlineTextBox* box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
                if (textOffset <= box->end()) {
                    if (textOffset >= box->start())
                        return currentPos;
                    continue;
                }

                if (box == lastTextBox || textOffset != box->start() + box->len())
                    continue;

                // The text continues on the next line only if the last text box is not on this line and
                // none of the boxes on this line have a larger start offset.

                bool continuesOnNextLine = true;
                InlineBox* otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->nextLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || otherBox->object() == textRenderer && static_cast<InlineTextBox*>(otherBox)->start() >= textOffset)
                        continuesOnNextLine = false;
                }

                otherBox = box;
                while (continuesOnNextLine) {
                    otherBox = otherBox->prevLeafChild();
                    if (!otherBox)
                        break;
                    if (otherBox == lastTextBox || otherBox->object() == textRenderer && static_cast<InlineTextBox*>(otherBox)->start() >= textOffset)
                        continuesOnNextLine = false;
                }

                if (continuesOnNextLine)
                    return currentPos;
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

    int ignoredCaretOffset;
    InlineBox* b1;
    getInlineBoxAndOffset(DOWNSTREAM, b1, ignoredCaretOffset);
    InlineBox* b2;
    pos.getInlineBoxAndOffset(DOWNSTREAM, b2, ignoredCaretOffset);

    LOG(Editing, "renderer:               %p [%p]\n", renderer, b1);
    LOG(Editing, "thisRenderedOffset:         %d\n", thisRenderedOffset);
    LOG(Editing, "posRenderer:            %p [%p]\n", posRenderer, b2);
    LOG(Editing, "posRenderedOffset:      %d\n", posRenderedOffset);
    LOG(Editing, "node min/max:           %d:%d\n", caretMinOffset(node()), caretMaxRenderedOffset(node()));
    LOG(Editing, "pos node min/max:       %d:%d\n", caretMinOffset(pos.node()), caretMaxRenderedOffset(pos.node()));
    LOG(Editing, "----------------------------------------------------------------------\n");

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
Position Position::trailingWhitespacePosition(EAffinity, bool considerNonCollapsibleWhitespace) const
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

void Position::getInlineBoxAndOffset(EAffinity affinity, InlineBox*& inlineBox, int& caretOffset) const
{
    TextDirection primaryDirection = LTR;
    for (RenderObject* r = node()->renderer(); r; r = r->parent()) {
        if (r->isBlockFlow()) {
            primaryDirection = r->style()->direction();
            break;
        }
    }
    getInlineBoxAndOffset(affinity, primaryDirection, inlineBox, caretOffset);
}

static bool isNonTextLeafChild(RenderObject* object)
{
    if (object->firstChild())
        return false;
    if (object->isText())
        return false;
    return true;
}

static InlineTextBox* searchAheadForBetterMatch(RenderObject* renderer)
{
    InlineTextBox* match = 0;
    int minOffset = INT_MAX;
    RenderBlock* container = renderer->containingBlock();
    RenderObject* next = renderer;
    while ((next = next->nextInPreOrder(container))) {
        if (next->isRenderBlock())
            break;
        if (next->isBR())
            break;
        if (isNonTextLeafChild(next))
            break;
        if (next->isText()) {
            for (InlineTextBox* box = static_cast<RenderText*>(next)->firstTextBox(); box; box = box->nextTextBox()) {
                int caretMinOffset = box->caretMinOffset();
                if (caretMinOffset < minOffset) {
                    match = box;
                    minOffset = caretMinOffset;
                }
            }
        }
    }
    return match;
}

void Position::getInlineBoxAndOffset(EAffinity affinity, TextDirection primaryDirection, InlineBox*& inlineBox, int& caretOffset) const
{
    caretOffset = offset();
    RenderObject* renderer = node()->renderer();
    if (!renderer->isText()) {
        inlineBox = renderer->inlineBoxWrapper();
        if (!inlineBox || caretOffset > inlineBox->caretMinOffset() && caretOffset < inlineBox->caretMaxOffset())
            return;
    } else {
        RenderText* textRenderer = static_cast<RenderText*>(renderer);

        InlineTextBox* box;
        InlineTextBox* candidate = 0;

        for (box = textRenderer->firstTextBox(); box; box = box->nextTextBox()) {
            int caretMinOffset = box->caretMinOffset();
            int caretMaxOffset = box->caretMaxOffset();

            if (caretOffset < caretMinOffset || caretOffset > caretMaxOffset || caretOffset == caretMaxOffset && box->isLineBreak())
                continue;

            if (caretOffset > caretMinOffset && caretOffset < caretMaxOffset) {
                inlineBox = box;
                return;
            }

            if ((caretOffset == caretMinOffset) ^ (affinity == UPSTREAM))
                break;

            candidate = box;
        }
        if (candidate && !box && affinity == DOWNSTREAM) {
            box = searchAheadForBetterMatch(textRenderer);
            if (box)
                caretOffset = box->caretMinOffset();
        }
        inlineBox = box ? box : candidate;
    }

    if (!inlineBox)
        return;

    unsigned char level = inlineBox->bidiLevel();

    if (inlineBox->direction() == primaryDirection) {
        if (caretOffset == inlineBox->caretRightmostOffset()) {
            InlineBox* nextBox = inlineBox->nextLeafChild();
            if (!nextBox || nextBox->bidiLevel() >= level)
                return;

            level = nextBox->bidiLevel();
            InlineBox* prevBox = inlineBox;
            do {
                prevBox = prevBox->prevLeafChild();
            } while (prevBox && prevBox->bidiLevel() > level);

            if (prevBox && prevBox->bidiLevel() == level)   // For example, abc FED 123 ^ CBA
                return;

            // For example, abc 123 ^ CBA
            while (InlineBox* nextBox = inlineBox->nextLeafChild()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        } else {
            InlineBox* prevBox = inlineBox->prevLeafChild();
            if (!prevBox || prevBox->bidiLevel() >= level)
                return;

            level = prevBox->bidiLevel();
            InlineBox* nextBox = inlineBox;
            do {
                nextBox = nextBox->nextLeafChild();
            } while (nextBox && nextBox->bidiLevel() > level);

            if (nextBox && nextBox->bidiLevel() == level)
                return;

            while (InlineBox* prevBox = inlineBox->prevLeafChild()) {
                if (prevBox->bidiLevel() < level)
                    break;
                inlineBox = prevBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        }
        return;
    }

    if (caretOffset == inlineBox->caretLeftmostOffset()) {
        InlineBox* prevBox = inlineBox->prevLeafChild();
        if (!prevBox || prevBox->bidiLevel() < level) {
            // Left edge of a secondary run. Set to the right edge of the entire run.
            while (InlineBox* nextBox = inlineBox->nextLeafChild()) {
                if (nextBox->bidiLevel() < level)
                    break;
                inlineBox = nextBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        } else if (prevBox->bidiLevel() > level) {
            // Right edge of a "tertiary" run. Set to the left edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->prevLeafChild()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        }
    } else {
        InlineBox* nextBox = inlineBox->nextLeafChild();
        if (!nextBox || nextBox->bidiLevel() < level) {
            // Right edge of a secondary run. Set to the left edge of the entire run.
            while (InlineBox* prevBox = inlineBox->prevLeafChild()) {
                if (prevBox->bidiLevel() < level)
                    break;
                inlineBox = prevBox;
            }
            caretOffset = inlineBox->caretLeftmostOffset();
        } else if (nextBox->bidiLevel() > level) {
            // Left edge of a "tertiary" run. Set to the right edge of that run.
            while (InlineBox* tertiaryBox = inlineBox->nextLeafChild()) {
                if (tertiaryBox->bidiLevel() <= level)
                    break;
                inlineBox = tertiaryBox;
            }
            caretOffset = inlineBox->caretRightmostOffset();
        }
    }
}

void Position::debugPosition(const char* msg) const
{
    if (isNull())
        fprintf(stderr, "Position [%s]: null\n", msg);
    else
        fprintf(stderr, "Position [%s]: %s [%p] at %d\n", msg, node()->nodeName().utf8().data(), node(), offset());
}

#ifndef NDEBUG

void Position::formatForDebugger(char* buffer, unsigned length) const
{
    String result;
    
    if (isNull())
        result = "<null>";
    else {
        char s[1024];
        result += "offset ";
        result += String::number(offset());
        result += " of ";
        node()->formatForDebugger(s, sizeof(s));
        result += s;
    }
          
    strncpy(buffer, result.utf8().data(), length - 1);
}

void Position::showTreeForThis() const
{
    if (node())
        node()->showTreeForThis();
}

#endif

Position startPosition(const Range* r)
{
    return r ? r->startPosition() : Position();
}

Position endPosition(const Range* r)
{
    return r ? r->endPosition() : Position();
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
