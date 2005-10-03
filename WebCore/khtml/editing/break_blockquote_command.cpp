/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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
#include "break_blockquote_command.h"

#include "htmlediting.h"
#include "visible_position.h"

#include "htmlnames.h"
#include "xml/dom_elementimpl.h"
#include "xml/dom_textimpl.h"

#if APPLE_CHANGES
#include <kxmlcore/Assertions.h>
#else
#define ASSERT(assertion) assert(assertion)
#endif

using namespace DOM::HTMLNames;

using DOM::DocumentImpl;
using DOM::ElementImpl;
using DOM::NodeImpl;
using DOM::Position;
using DOM::TextImpl;

namespace khtml {

BreakBlockquoteCommand::BreakBlockquoteCommand(DocumentImpl *document)
    : CompositeEditCommand(document), m_breakNode(0)
{
}

BreakBlockquoteCommand::~BreakBlockquoteCommand()
{
    derefNodesInList(clonedNodes);
    if (m_breakNode)
        m_breakNode->deref();
}

void BreakBlockquoteCommand::doApply()
{
    SelectionController selection = endingSelection();
    if (selection.isNone())
        return;
    
    // Delete the current selection.
    Position pos = selection.start();
    EAffinity affinity = selection.startAffinity();
    if (selection.isRange()) {
        deleteSelection(false, false);
        pos = endingSelection().start().upstream();
        affinity = endingSelection().startAffinity();
    }
    
    // Find the top-most blockquote from the start.
    NodeImpl *startNode = pos.node();
    NodeImpl *topBlockquote = 0;
    for (NodeImpl *n = startNode->parentNode(); n; n = n->parentNode()) {
        if (isMailBlockquote(n))
            topBlockquote = n;
    }
    if (!topBlockquote || !topBlockquote->parentNode())
        return;
    
    // Insert a break after the top blockquote.
    m_breakNode = createBreakElement(document());
    m_breakNode->ref();
    insertNodeAfter(m_breakNode, topBlockquote);
    
    if (!isLastVisiblePositionInNode(VisiblePosition(pos, affinity), topBlockquote)) {
        
        NodeImpl *newStartNode = 0;
        // Split at pos if in the middle of a text node.
        if (startNode->isTextNode()) {
            TextImpl *textNode = static_cast<TextImpl *>(startNode);
            bool atEnd = (unsigned)pos.offset() >= textNode->length();
            if (pos.offset() > 0 && !atEnd) {
                splitTextNode(textNode, pos.offset());
                pos = Position(startNode, 0);
            }
            else if (atEnd) {
                newStartNode = startNode->traverseNextNode();
                ASSERT(newStartNode);
            }
        }
        else if (pos.offset() > 0) {
            newStartNode = startNode->traverseNextNode();
            ASSERT(newStartNode);
        }
        
        // If a new start node was determined, find a new top block quote.
        if (newStartNode) {
            startNode = newStartNode;
            for (NodeImpl *n = startNode->parentNode(); n; n = n->parentNode()) {
                if (isMailBlockquote(n))
                    topBlockquote = n;
            }
            if (!topBlockquote || !topBlockquote->parentNode())
                return;
        }
        
        // Build up list of ancestors in between the start node and the top blockquote.
        if (startNode != topBlockquote) {
            for (NodeImpl *n = startNode->parentNode(); n && n != topBlockquote; n = n->parentNode())
                ancestors.prepend(n);
        }                    
        
        // Insert a clone of the top blockquote after the break.
        NodeImpl *clonedBlockquote = topBlockquote->cloneNode(false);
        clonedBlockquote->ref();
        clonedNodes.append(clonedBlockquote);
        insertNodeAfter(clonedBlockquote, m_breakNode);
        
        // Make clones of ancestors in between the start node and the top blockquote.
        NodeImpl *parent = clonedBlockquote;
        for (QPtrListIterator<NodeImpl> it(ancestors); it.current(); ++it) {
            NodeImpl *child = it.current()->cloneNode(false); // shallow clone
            child->ref();
            clonedNodes.append(child);
            appendNode(child, parent);
            parent = child;
        }
        
        // Move the start node and the siblings of the start node.
        bool startIsBR = false;
        if (startNode != topBlockquote) {
            NodeImpl *n = startNode;
            startIsBR = n->hasTagName(brTag);
            if (startIsBR)
                n = n->nextSibling();
            while (n) {
                NodeImpl *next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent);
                n = next;
            }
        }
        
        // Move everything after the start node.
        NodeImpl *leftParent = ancestors.last();
        
        // Insert an extra new line when the start is at the beginning of a line.
        if (!newStartNode && !startIsBR) {
            if (!leftParent)
                leftParent = topBlockquote;
            ElementImpl *b = createBreakElement(document());
            b->ref();
            clonedNodes.append(b);
            appendNode(b, leftParent);
        }        
        
        leftParent = ancestors.last();
        while (leftParent && leftParent != topBlockquote) {
            parent = parent->parentNode();
            NodeImpl *n = leftParent->nextSibling();
            while (n) {
                NodeImpl *next = n->nextSibling();
                removeNode(n);
                appendNode(n, parent);
                n = next;
            }
            leftParent = leftParent->parentNode();
        }
        
        // Make sure the cloned block quote renders.
        addBlockPlaceholderIfNeeded(clonedBlockquote);
    }
    
    // Put the selection right before the break.
    setEndingSelection(Position(m_breakNode, 0), DOWNSTREAM);
    rebalanceWhitespace();
}

} // namespace khtml
