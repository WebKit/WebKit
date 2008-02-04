/**
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "Range.h"

#include "Document.h"
#include "DocumentFragment.h"
#include "ExceptionCode.h"
#include "HTMLElement.h"
#include "HTMLNames.h"
#include "ProcessingInstruction.h"
#include "RangeException.h"
#include "RenderBlock.h"
#include "Text.h"
#include "TextIterator.h"
#include "markup.h"
#include "visible_units.h"

namespace WebCore {

using namespace std;
using namespace HTMLNames;

#ifndef NDEBUG
class RangeCounter {
public:
    static unsigned count;
    ~RangeCounter()
    {
        if (count)
            fprintf(stderr, "LEAK: %u Range\n", count);
    }
};
unsigned RangeCounter::count = 0;
static RangeCounter rangeCounter;
#endif

Range::Range(Document* ownerDocument)
    : m_ownerDocument(ownerDocument)
    , m_startContainer(ownerDocument)
    , m_startOffset(0)
    , m_endContainer(ownerDocument)
    , m_endOffset(0)
    , m_detached(false)
{
#ifndef NDEBUG
    ++RangeCounter::count;
#endif
}

Range::Range(Document* ownerDocument,
              Node* startContainer, int startOffset,
              Node* endContainer, int endOffset)
    : m_ownerDocument(ownerDocument)
    , m_startContainer(ownerDocument)
    , m_startOffset(0)
    , m_endContainer(ownerDocument)
    , m_endOffset(0)
    , m_detached(false)
{
#ifndef NDEBUG
    ++RangeCounter::count;
#endif
    // Simply setting the containers and offsets directly would not do any of the checking
    // that setStart and setEnd do, so we must call those functions.
    ExceptionCode ec = 0;
    setStart(startContainer, startOffset, ec);
    ASSERT(ec == 0);
    setEnd(endContainer, endOffset, ec);
    ASSERT(ec == 0);
}

Range::Range(Document* ownerDocument, const Position& start, const Position& end)
    : m_ownerDocument(ownerDocument)
    , m_startContainer(ownerDocument)
    , m_startOffset(0)
    , m_endContainer(ownerDocument)
    , m_endOffset(0)
    , m_detached(false)
{
#ifndef NDEBUG
    ++RangeCounter::count;
#endif
    // Simply setting the containers and offsets directly would not do any of the checking
    // that setStart and setEnd do, so we must call those functions.
    ExceptionCode ec = 0;
    setStart(start.node(), start.offset(), ec);
    ASSERT(ec == 0);
    setEnd(end.node(), end.offset(), ec);
    ASSERT(ec == 0);
}

Range::~Range()
{
#ifndef NDEBUG
    --RangeCounter::count;
#endif
}

Node *Range::startContainer(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return m_startContainer.get();
}

int Range::startOffset(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return m_startOffset;
}

Node *Range::endContainer(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return m_endContainer.get();
}

int Range::endOffset(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return m_endOffset;
}

Node *Range::commonAncestorContainer(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    Node *com = commonAncestorContainer(m_startContainer.get(), m_endContainer.get());
    if (!com) //  should never happen
        ec = WRONG_DOCUMENT_ERR;
    return com;
}

Node *Range::commonAncestorContainer(Node *containerA, Node *containerB)
{
    Node *parentStart;

    for (parentStart = containerA; parentStart; parentStart = parentStart->parentNode()) {
        Node *parentEnd = containerB;
        while (parentEnd && (parentStart != parentEnd))
            parentEnd = parentEnd->parentNode();
        if (parentStart == parentEnd)
            break;
    }
        
    return parentStart;
}

bool Range::collapsed(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return (m_startContainer == m_endContainer && m_startOffset == m_endOffset);
}

void Range::setStart( Node *refNode, int offset, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }

    ec = 0;
    checkNodeWOffset(refNode, offset, ec);
    if (ec)
        return;

    m_startContainer = refNode;
    m_startOffset = offset;

    // check if different root container
    Node* endRootContainer = m_endContainer.get();
    while (endRootContainer->parentNode())
        endRootContainer = endRootContainer->parentNode();
    Node* startRootContainer = m_startContainer.get();
    while (startRootContainer->parentNode())
        startRootContainer = startRootContainer->parentNode();
    if (startRootContainer != endRootContainer)
        collapse(true, ec);
    // check if new start after end
    else if (compareBoundaryPoints(m_startContainer.get(), m_startOffset, m_endContainer.get(), m_endOffset) > 0)
        collapse(true, ec);
}

void Range::setEnd( Node *refNode, int offset, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }

    ec = 0;
    checkNodeWOffset(refNode, offset, ec);
    if (ec)
        return;

    m_endContainer = refNode;
    m_endOffset = offset;

    // check if different root container
    Node* endRootContainer = m_endContainer.get();
    while (endRootContainer->parentNode())
        endRootContainer = endRootContainer->parentNode();
    Node* startRootContainer = m_startContainer.get();
    while (startRootContainer->parentNode())
        startRootContainer = startRootContainer->parentNode();
    if (startRootContainer != endRootContainer)
        collapse(false, ec);
    // check if new end before start
    if (compareBoundaryPoints(m_startContainer.get(), m_startOffset, m_endContainer.get(), m_endOffset) > 0)
        collapse(false, ec);
}

void Range::collapse( bool toStart, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (toStart) {  // collapse to start
        m_endContainer = m_startContainer;
        m_endOffset = m_startOffset;
    } else {        // collapse to end
        m_startContainer = m_endContainer;
        m_startOffset = m_endOffset;
    }
}

bool Range::isPointInRange(Node* refNode, int offset, ExceptionCode& ec)
{
    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return false;
    }

    if (m_detached && refNode->attached()) {
        ec = INVALID_STATE_ERR;
        return false;
    }

    if (!m_detached && !refNode->attached()) {
        // firefox doesn't throw an exception for this case; it returns false
        return false;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return false;
    }

    ec = 0;
    checkNodeWOffset(refNode, offset, ec);
    if (ec)
        return false;

    // point is not before the start and not after the end
    if ((compareBoundaryPoints(refNode, offset, m_startContainer.get(), m_startOffset) != -1) && 
        (compareBoundaryPoints(refNode, offset, m_endContainer.get(), m_endOffset) != 1))
        return true;
    else
        return false;
}

short Range::comparePoint(Node* refNode, int offset, ExceptionCode& ec)
{
    // http://developer.mozilla.org/en/docs/DOM:range.comparePoint
    // This method returns -1, 0 or 1 depending on if the point described by the 
    // refNode node and an offset within the node is before, same as, or after the range respectively.

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    if (m_detached && refNode->attached()) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    if (!m_detached && !refNode->attached()) {
        // firefox doesn't throw an exception for this case; it returns -1
        return -1;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    ec = 0;
    checkNodeWOffset(refNode, offset, ec);
    if (ec)
        return 0;

    // compare to start, and point comes before
    if (compareBoundaryPoints(refNode, offset, m_startContainer.get(), m_startOffset) == -1)
        return -1;

    // compare to end, and point comes after
    else if (compareBoundaryPoints(refNode, offset, m_endContainer.get(), m_endOffset) == 1)
        return 1;
    
    // point is in the middle of this range, or on the boundary points
    else
        return 0;
}

Range::CompareResults Range::compareNode(Node* refNode, ExceptionCode& ec)
{
    // http://developer.mozilla.org/en/docs/DOM:range.compareNode
    // This method returns 0, 1, 2, or 3 based on if the node is before, after,
    // before and after(surrounds), or inside the range, respectively

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return NODE_BEFORE;
    }
    
    if (m_detached && refNode->attached()) {
        ec = INVALID_STATE_ERR;
        return NODE_BEFORE;
    }

    if (!m_detached && !refNode->attached()) {
        // firefox doesn't throw an exception for this case; it returns 0
        return NODE_BEFORE;
    }

    if (refNode->document() != m_ownerDocument) {
        // firefox doesn't throw an exception for this case; it returns 0
        return NODE_BEFORE;
    }

    Node* parentNode = refNode->parentNode();
    unsigned nodeIndex = refNode->nodeIndex();
    
    if (!parentNode) {
        // if the node is the top document we should return NODE_BEFORE_AND_AFTER
        // but we throw to match firefox behavior
        ec = NOT_FOUND_ERR;
        return NODE_BEFORE;
    }

    if (comparePoint(parentNode, nodeIndex, ec) == -1) { // starts before
        if (comparePoint(parentNode, nodeIndex + 1, ec) == 1) // ends after the range
            return NODE_BEFORE_AND_AFTER;
        return NODE_BEFORE; // ends before or in the range
    } else { // starts at or after the range start
        if (comparePoint(parentNode, nodeIndex + 1, ec) == 1) // ends after the range
            return NODE_AFTER;
        return NODE_INSIDE; // ends inside the range
    }
}


short Range::compareBoundaryPoints(CompareHow how, const Range *sourceRange, ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    if (!sourceRange) {
        ec = NOT_FOUND_ERR;
        return 0;
    }

    ec = 0;
    Node *thisCont = commonAncestorContainer(ec);
    if (ec)
        return 0;
    Node *sourceCont = sourceRange->commonAncestorContainer(ec);
    if (ec)
        return 0;

    if (thisCont->document() != sourceCont->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    Node *thisTop = thisCont;
    Node *sourceTop = sourceCont;
    while (thisTop->parentNode())
        thisTop = thisTop->parentNode();
    while (sourceTop->parentNode())
        sourceTop = sourceTop->parentNode();
    if (thisTop != sourceTop) { // in different DocumentFragments
        ec = WRONG_DOCUMENT_ERR;
        return 0;
    }

    switch(how)
    {
    case START_TO_START:
        return compareBoundaryPoints( m_startContainer.get(), m_startOffset,
                                      sourceRange->startContainer(ec), sourceRange->startOffset(ec) );
        break;
    case START_TO_END:
        return compareBoundaryPoints( m_startContainer.get(), m_startOffset,
                                      sourceRange->endContainer(ec), sourceRange->endOffset(ec) );
        break;
    case END_TO_END:
        return compareBoundaryPoints( m_endContainer.get(), m_endOffset,
                                      sourceRange->endContainer(ec), sourceRange->endOffset(ec) );
        break;
    case END_TO_START:
        return compareBoundaryPoints( m_endContainer.get(), m_endOffset,
                                      sourceRange->startContainer(ec), sourceRange->startOffset(ec) );
        break;
    default:
        ec = SYNTAX_ERR;
        return 0;
    }
}

short Range::compareBoundaryPoints( Node *containerA, int offsetA, Node *containerB, int offsetB )
{
    ASSERT(containerA && containerB);
    if (!containerA)
        return -1;
    if (!containerB)
        return 1;
    // see DOM2 traversal & range section 2.5

    // case 1: both points have the same container
    if(containerA == containerB) {
        if (offsetA == offsetB)
            return 0;           // A is equal to B
        if (offsetA < offsetB)
            return -1;          // A is before B
        else
            return 1;           // A is after B
    }

    // case 2: node C (container B or an ancestor) is a child node of A
    Node *c = containerB;
    while (c && c->parentNode() != containerA)
        c = c->parentNode();
    if (c) {
        int offsetC = 0;
        Node *n = containerA->firstChild();
        while (n != c && offsetC < offsetA) {
            offsetC++;
            n = n->nextSibling();
        }

        if (offsetA <= offsetC)
            return -1;              // A is before B
        else
            return 1;               // A is after B
    }

    // case 3: node C (container A or an ancestor) is a child node of B
    c = containerA;
    while (c && c->parentNode() != containerB)
        c = c->parentNode();
    if (c) {
        int offsetC = 0;
        Node *n = containerB->firstChild();
        while (n != c && offsetC < offsetB) {
            offsetC++;
            n = n->nextSibling();
        }

        if(offsetC < offsetB)
            return -1;              // A is before B
        else
            return 1;               // A is after B
    }

    // case 4: containers A & B are siblings, or children of siblings
    // ### we need to do a traversal here instead
    Node *cmnRoot = commonAncestorContainer(containerA,containerB);
    if (!cmnRoot)
        return 0;
    Node *childA = containerA;
    while (childA && childA->parentNode() != cmnRoot)
        childA = childA->parentNode();
    if (!childA)
        childA = cmnRoot;
    Node *childB = containerB;
    while (childB && childB->parentNode() != cmnRoot)
        childB = childB->parentNode();
    if (!childB)
        childB = cmnRoot;

    if (childA == childB)
        return 0; // A is equal to B

    Node *n = cmnRoot->firstChild();
    while (n) {
        if (n == childA)
            return -1; // A is before B
        if (n == childB)
            return 1; // A is after B
        n = n->nextSibling();
    }

    // Should never reach this point.
    ASSERT(0);
    return 0;
}

short Range::compareBoundaryPoints( const Position &a, const Position &b )
{
    return compareBoundaryPoints(a.node(), a.offset(), b.node(), b.offset());
}

bool Range::boundaryPointsValid() const
{
    return m_startContainer && m_endContainer && compareBoundaryPoints(m_startContainer.get(), m_startOffset, m_endContainer.get(), m_endOffset) <= 0;
}

void Range::deleteContents(ExceptionCode& ec) {
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    ec = 0;
    checkDeleteExtract(ec);
    if (ec)
        return;

    processContents(DELETE_CONTENTS,ec);
}

bool Range::intersectsNode(Node* refNode, ExceptionCode& ec)
{
    // http://developer.mozilla.org/en/docs/DOM:range.intersectsNode
    // Returns a bool if the node intersects the range.

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return false;
    }
    
    if (m_detached && refNode->attached() ||
        !m_detached && !refNode->attached() ||
        refNode->document() != m_ownerDocument)
        // firefox doesn't throw an exception for these case; it returns false
        return false;

    Node* parentNode = refNode->parentNode();
    unsigned nodeIndex = refNode->nodeIndex();
    
    if (!parentNode) {
        // if the node is the top document we should return NODE_BEFORE_AND_AFTER
        // but we throw to match firefox behavior
        ec = NOT_FOUND_ERR;
        return false;
    }

    if (comparePoint(parentNode, nodeIndex, ec) == -1 && // starts before start
        comparePoint(parentNode, nodeIndex + 1, ec) == -1) { // ends before start
        return false;
    } else if(comparePoint(parentNode, nodeIndex, ec) == 1 && // starts after end
              comparePoint(parentNode, nodeIndex + 1, ec) == 1) { // ends after end
        return false;
    }
    
    return true;    //all other cases
}

PassRefPtr<DocumentFragment> Range::processContents ( ActionType action, ExceptionCode& ec)
{
    // ### when mutation events are implemented, we will have to take into account
    // situations where the tree is being transformed while we delete - ugh!

    // ### perhaps disable node deletion notification for this range while we do this?

    RefPtr<DocumentFragment> fragment;
    if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
        fragment = new DocumentFragment(m_ownerDocument.get());
    
    ec = 0;
    if (collapsed(ec))
        return fragment.release();
    if (ec)
        return 0;

    Node *cmnRoot = commonAncestorContainer(ec);
    if (ec)
        return 0;

    // what is the highest node that partially selects the start of the range?
    Node *partialStart = 0;
    if (m_startContainer != cmnRoot) {
        partialStart = m_startContainer.get();
        while (partialStart->parentNode() != cmnRoot)
            partialStart = partialStart->parentNode();
    }

    // what is the highest node that partially selects the end of the range?
    Node *partialEnd = 0;
    if (m_endContainer != cmnRoot) {
        partialEnd = m_endContainer.get();
        while (partialEnd->parentNode() != cmnRoot)
            partialEnd = partialEnd->parentNode();
    }

    // Simple case: the start and end containers are the same. We just grab
    // everything >= start offset and < end offset
    if (m_startContainer == m_endContainer) {
        if(m_startContainer->nodeType() == Node::TEXT_NODE ||
           m_startContainer->nodeType() == Node::CDATA_SECTION_NODE ||
           m_startContainer->nodeType() == Node::COMMENT_NODE) {

            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<CharacterData> c = static_pointer_cast<CharacterData>(m_startContainer->cloneNode(true));
                c->deleteData(m_endOffset, c->length() - m_endOffset, ec);
                c->deleteData(0, m_startOffset, ec);
                fragment->appendChild(c.release(), ec);
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
                static_cast<CharacterData*>(m_startContainer.get())->deleteData(m_startOffset,m_endOffset-m_startOffset,ec);
                m_startContainer->document()->updateLayout();
            }
        }
        else if (m_startContainer->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<ProcessingInstruction> c = static_pointer_cast<ProcessingInstruction>(m_startContainer->cloneNode(true));
                c->setData(c->data().substring(m_startOffset, m_endOffset - m_startOffset), ec);
                fragment->appendChild(c.release(), ec);
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
                ProcessingInstruction* pi= static_cast<ProcessingInstruction*>(m_startContainer.get());
                String data(pi->data());
                data.remove(m_startOffset, m_endOffset - m_startOffset);
                pi->setData(data, ec);
            }
        }
        else {
            Node *n = m_startContainer->firstChild();
            unsigned i;
            for (i = 0; n && i < m_startOffset; i++) // skip until m_startOffset
                n = n->nextSibling();
            while (n && i < m_endOffset) { // delete until m_endOffset
                Node *next = n->nextSibling();
                if (action == EXTRACT_CONTENTS)
                    fragment->appendChild(n,ec); // will remove n from it's parent
                else if (action == CLONE_CONTENTS)
                    fragment->appendChild(n->cloneNode(true),ec);
                else
                    m_startContainer->removeChild(n,ec);
                n = next;
                i++;
            }
        }
        if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
            collapse(true,ec);
        return fragment.release();
    }

    // Complex case: Start and end containers are different.
    // There are three possiblities here:
    // 1. Start container == cmnRoot (End container must be a descendant)
    // 2. End container == cmnRoot (Start container must be a descendant)
    // 3. Neither is cmnRoot, they are both descendants
    //
    // In case 3, we grab everything after the start (up until a direct child
    // of cmnRoot) into leftContents, and everything before the end (up until
    // a direct child of cmnRoot) into rightContents. Then we process all
    // cmnRoot children between leftContents and rightContents
    //
    // In case 1 or 2, we skip either processing of leftContents or rightContents,
    // in which case the last lot of nodes either goes from the first or last
    // child of cmnRoot.
    //
    // These are deleted, cloned, or extracted (i.e. both) depending on action.

    RefPtr<Node> leftContents;
    if (m_startContainer != cmnRoot) {
        // process the left-hand side of the range, up until the last ancestor of
        // m_startContainer before cmnRoot
        if(m_startContainer->nodeType() == Node::TEXT_NODE ||
           m_startContainer->nodeType() == Node::CDATA_SECTION_NODE ||
           m_startContainer->nodeType() == Node::COMMENT_NODE) {

            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<CharacterData> c = static_pointer_cast<CharacterData>(m_startContainer->cloneNode(true));
                c->deleteData(0, m_startOffset, ec);
                leftContents = c.release();
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
                static_cast<CharacterData*>(m_startContainer.get())->deleteData(
                    m_startOffset, static_cast<CharacterData*>(m_startContainer.get())->length() - m_startOffset, ec);
                m_startContainer->document()->updateLayout();
            }
        }
        else if (m_startContainer->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<ProcessingInstruction> c = static_pointer_cast<ProcessingInstruction>(m_startContainer->cloneNode(true));
                c->setData(c->data().substring(m_startOffset), ec);
                leftContents = c.release();
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
                ProcessingInstruction* pi= static_cast<ProcessingInstruction*>(m_startContainer.get());
                String data(pi->data());
                pi->setData(data.left(m_startOffset), ec);
            }
        }
        else {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
                leftContents = m_startContainer->cloneNode(false);
            Node *n = m_startContainer->firstChild();
            for (unsigned i = 0; n && i < m_startOffset; i++) // skip until m_startOffset
                n = n->nextSibling();
            while (n) { // process until end
                Node *next = n->nextSibling();
                if (action == EXTRACT_CONTENTS)
                    leftContents->appendChild(n,ec); // will remove n from m_startContainer
                else if (action == CLONE_CONTENTS)
                    leftContents->appendChild(n->cloneNode(true),ec);
                else
                    m_startContainer->removeChild(n,ec);
                n = next;
            }
        }

        Node *leftParent = m_startContainer->parentNode();
        Node *n = m_startContainer->nextSibling();
        for (; leftParent != cmnRoot; leftParent = leftParent->parentNode()) {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<Node> leftContentsParent = leftParent->cloneNode(false);
                leftContentsParent->appendChild(leftContents,ec);
                leftContents = leftContentsParent;
            }

            Node *next;
            for (; n; n = next) {
                next = n->nextSibling();
                if (action == EXTRACT_CONTENTS)
                    leftContents->appendChild(n,ec); // will remove n from leftParent
                else if (action == CLONE_CONTENTS)
                    leftContents->appendChild(n->cloneNode(true),ec);
                else
                    leftParent->removeChild(n,ec);
            }
            n = leftParent->nextSibling();
        }
    }

    RefPtr<Node> rightContents = 0;
    if (m_endContainer != cmnRoot) {
        // delete the right-hand side of the range, up until the last ancestor of
        // m_endContainer before cmnRoot
        if(m_endContainer->nodeType() == Node::TEXT_NODE ||
           m_endContainer->nodeType() == Node::CDATA_SECTION_NODE ||
           m_endContainer->nodeType() == Node::COMMENT_NODE) {

            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<CharacterData> c = static_pointer_cast<CharacterData>(m_endContainer->cloneNode(true));
                c->deleteData(m_endOffset, static_cast<CharacterData*>(m_endContainer.get())->length() - m_endOffset, ec);
                rightContents = c;
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
                static_cast<CharacterData*>(m_endContainer.get())->deleteData(0, m_endOffset, ec);
                m_startContainer->document()->updateLayout();
            }
        }
        else if (m_endContainer->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<ProcessingInstruction> c = static_pointer_cast<ProcessingInstruction>(m_endContainer->cloneNode(true));
                c->setData(c->data().left(m_endOffset), ec);
                rightContents = c.release();
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
                ProcessingInstruction* pi= static_cast<ProcessingInstruction*>(m_endContainer.get());
                pi->setData(pi->data().substring(m_endOffset), ec);
            }
        }
        else {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
                rightContents = m_endContainer->cloneNode(false);
            Node *n = m_endContainer->firstChild();
            if (n && m_endOffset) {
                for (unsigned i = 0; i+1 < m_endOffset; i++) { // skip to m_endOffset
                    Node *next = n->nextSibling();
                    if (!next)
                        break;
                    n = next;
                }
                Node *prev;
                for (; n; n = prev) {
                    prev = n->previousSibling();
                    if (action == EXTRACT_CONTENTS)
                        rightContents->insertBefore(n,rightContents->firstChild(),ec); // will remove n from it's parent
                    else if (action == CLONE_CONTENTS)
                        rightContents->insertBefore(n->cloneNode(true),rightContents->firstChild(),ec);
                    else
                        m_endContainer->removeChild(n,ec);
                }
            }
        }

        Node *rightParent = m_endContainer->parentNode();
        Node *n = m_endContainer->previousSibling();
        for (; rightParent != cmnRoot; rightParent = rightParent->parentNode()) {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                RefPtr<Node> rightContentsParent = rightParent->cloneNode(false);
                rightContentsParent->appendChild(rightContents,ec);
                rightContents = rightContentsParent;
            }

            Node *prev;
            for (; n; n = prev) {
                prev = n->previousSibling();
                if (action == EXTRACT_CONTENTS)
                    rightContents->insertBefore(n,rightContents->firstChild(),ec); // will remove n from it's parent
                else if (action == CLONE_CONTENTS)
                    rightContents->insertBefore(n->cloneNode(true),rightContents->firstChild(),ec);
                else
                    rightParent->removeChild(n,ec);
            }
            n = rightParent->previousSibling();
        }
    }

    // delete all children of cmnRoot between the start and end container

    Node *processStart; // child of cmnRooot
    if (m_startContainer == cmnRoot) {
        unsigned i;
        processStart = m_startContainer->firstChild();
        for (i = 0; i < m_startOffset; i++)
            processStart = processStart->nextSibling();
    }
    else {
        processStart = m_startContainer.get();
        while (processStart->parentNode() != cmnRoot)
            processStart = processStart->parentNode();
        processStart = processStart->nextSibling();
    }
    Node *processEnd; // child of cmnRooot
    if (m_endContainer == cmnRoot) {
        unsigned i;
        processEnd = m_endContainer->firstChild();
        for (i = 0; i < m_endOffset; i++)
            processEnd = processEnd->nextSibling();
    }
    else {
        processEnd = m_endContainer.get();
        while (processEnd->parentNode() != cmnRoot)
            processEnd = processEnd->parentNode();
    }

    // Now add leftContents, stuff in between, and rightContents to the fragment
    // (or just delete the stuff in between)

    if ((action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) && leftContents)
      fragment->appendChild(leftContents,ec);

    Node *next;
    Node *n;
    if (processStart) {
        for (n = processStart; n && n != processEnd; n = next) {
            next = n->nextSibling();

            if (action == EXTRACT_CONTENTS)
                fragment->appendChild(n,ec); // will remove from cmnRoot
            else if (action == CLONE_CONTENTS)
                fragment->appendChild(n->cloneNode(true),ec);
            else
                cmnRoot->removeChild(n,ec);
        }
    }

    if ((action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) && rightContents)
      fragment->appendChild(rightContents,ec);

    // collapse to the proper position - see spec section 2.6
    if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
        if (!partialStart && !partialEnd)
            collapse(true,ec);
        else if (partialStart) {
            m_startContainer = partialStart->parentNode();
            m_endContainer = partialStart->parentNode();
            m_startOffset = m_endOffset = partialStart->nodeIndex()+1;
        }
        else if (partialEnd) {
            m_startContainer = partialEnd->parentNode();
            m_endContainer = partialEnd->parentNode();
            m_startOffset = m_endOffset = partialEnd->nodeIndex();
        }
    }
    return fragment.release();
}


PassRefPtr<DocumentFragment> Range::extractContents(ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    ec = 0;
    checkDeleteExtract(ec);
    if (ec)
        return 0;

    return processContents(EXTRACT_CONTENTS,ec);
}

PassRefPtr<DocumentFragment> Range::cloneContents( int &ec  )
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return processContents(CLONE_CONTENTS,ec);
}

void Range::insertNode(PassRefPtr<Node> newNode, ExceptionCode& ec)
{
    ec = 0;

    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!newNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container of either boundary-point of
    // the Range is read-only.
    if (containedByReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // WRONG_DOCUMENT_ERR: Raised if newParent and the container of the start of the Range were
    // not created from the same document.
    if (newNode->document() != m_startContainer->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }


    // HIERARCHY_REQUEST_ERR: Raised if the container of the start of the Range is of a type that
    // does not allow children of the type of newNode or if newNode is an ancestor of the container.

    // an extra one here - if a text node is going to split, it must have a parent to insert into
    if (m_startContainer->nodeType() == Node::TEXT_NODE && !m_startContainer->parentNode()) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }

    // In the case where the container is a text node, we check against the container's parent, because
    // text nodes get split up upon insertion.
    Node *checkAgainst;
    if (m_startContainer->nodeType() == Node::TEXT_NODE)
        checkAgainst = m_startContainer->parentNode();
    else
        checkAgainst = m_startContainer.get();

    if (newNode->nodeType() == Node::DOCUMENT_FRAGMENT_NODE) {
        // check each child node, not the DocumentFragment itself
        Node *c;
        for (c = newNode->firstChild(); c; c = c->nextSibling()) {
            if (!checkAgainst->childTypeAllowed(c->nodeType())) {
                ec = HIERARCHY_REQUEST_ERR;
                return;
            }
        }
    } else {
        if (!checkAgainst->childTypeAllowed(newNode->nodeType())) {
            ec = HIERARCHY_REQUEST_ERR;
            return;
        }
    }

    for (Node *n = m_startContainer.get(); n; n = n->parentNode()) {
        if (n == newNode) {
            ec = HIERARCHY_REQUEST_ERR;
            return;
        }
    }

    // INVALID_NODE_TYPE_ERR: Raised if newNode is an Attr, Entity, Notation, or Document node.
    if (newNode->nodeType() == Node::ATTRIBUTE_NODE ||
        newNode->nodeType() == Node::ENTITY_NODE ||
        newNode->nodeType() == Node::NOTATION_NODE ||
        newNode->nodeType() == Node::DOCUMENT_NODE) {
        ec = RangeException::INVALID_NODE_TYPE_ERR;
        return;
    }

    unsigned endOffsetDelta = 0;
    if (m_startContainer->nodeType() == Node::TEXT_NODE ||
        m_startContainer->nodeType() == Node::CDATA_SECTION_NODE) {
        RefPtr<Text> newText = static_cast<Text*>(m_startContainer.get())->splitText(m_startOffset, ec);
        if (ec)
            return;

        if (m_startContainer == m_endContainer)
            endOffsetDelta = -m_startOffset;

        m_startContainer->parentNode()->insertBefore(newNode, newText.get(), ec);
        if (ec)
            return;
        m_endContainer = newText;
    } else {
        if (m_startContainer == m_endContainer) {
            bool isFragment = newNode->nodeType() == Node::DOCUMENT_FRAGMENT_NODE;
            endOffsetDelta = isFragment ? newNode->childNodeCount() : 1;
        }

        m_startContainer->insertBefore(newNode, m_startContainer->childNode(m_startOffset), ec);
        if (ec)
            return;
    }
    m_endOffset += endOffsetDelta;
}

String Range::toString(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return String();
    }

    Vector<UChar> result;

    Node* pastEnd = pastEndNode();
    for (Node* n = startNode(); n != pastEnd; n = n->traverseNextNode()) {
        if (n->nodeType() == Node::TEXT_NODE || n->nodeType() == Node::CDATA_SECTION_NODE) {
            String data = static_cast<CharacterData*>(n)->data();
            unsigned length = data.length();
            unsigned start = (n == m_startContainer) ? min(m_startOffset, length) : 0;
            unsigned end = (n == m_endContainer) ? min(max(start, m_endOffset), length) : length;
            result.append(data.characters() + start, end - start);
        }
    }

    return String::adopt(result);
}

String Range::toHTML() const
{
    return createMarkup(this);
}

String Range::text() const
{
    if (m_detached)
        return String();

    // We need to update layout, since plainText uses line boxes in the render tree.
    // FIXME: As with innerText, we'd like this to work even if there are no render objects.
    m_startContainer->document()->updateLayout();

    // FIXME: Maybe DOMRange constructor should take const DOMRange*; if it did we would not need this const_cast.
    return plainText(const_cast<Range *>(this));
}

PassRefPtr<DocumentFragment> Range::createContextualFragment(const String &html, ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    Node* htmlElement = m_startContainer->isHTMLElement() ? m_startContainer.get() : m_startContainer->parentNode();
    if (!htmlElement || !htmlElement->isHTMLElement()) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    RefPtr<DocumentFragment> fragment = static_cast<HTMLElement*>(htmlElement)->createContextualFragment(html);
    if (!fragment) {
        ec = NOT_SUPPORTED_ERR;
        return 0;
    }

    return fragment.release();
}


void Range::detach(ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    m_startContainer = 0;
    m_endContainer = 0;
    m_detached = true;
}

bool Range::isDetached() const
{
    return m_detached;
}

void Range::checkNodeWOffset(Node* n, int offset, ExceptionCode& ec) const
{
    if (offset < 0)
        ec = INDEX_SIZE_ERR;
        // no return here

    switch (n->nodeType()) {
        case Node::ENTITY_NODE:
        case Node::NOTATION_NODE:
        case Node::DOCUMENT_TYPE_NODE:
            ec = RangeException::INVALID_NODE_TYPE_ERR;
            break;
        case Node::TEXT_NODE:
        case Node::COMMENT_NODE:
        case Node::CDATA_SECTION_NODE:
            if ((unsigned)offset > static_cast<CharacterData*>(n)->length())
                ec = INDEX_SIZE_ERR;
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
            if ((unsigned)offset > static_cast<ProcessingInstruction*>(n)->data().length())
                ec = INDEX_SIZE_ERR;
            break;
        default:
            if ((unsigned)offset > n->childNodeCount())
                ec = INDEX_SIZE_ERR;
            break;
    }
}

void Range::checkNodeBA( Node *n, ExceptionCode& ec) const
{
    // INVALID_NODE_TYPE_ERR: Raised if the root container of refNode is not an
    // Attr, Document or DocumentFragment node or part of a shadow DOM tree
    // or if refNode is a Document, DocumentFragment, Attr, Entity, or Notation node.
    Node *root = n;
    while (root->parentNode())
        root = root->parentNode();
    if (!(root->nodeType() == Node::ATTRIBUTE_NODE ||
          root->nodeType() == Node::DOCUMENT_NODE ||
          root->nodeType() == Node::DOCUMENT_FRAGMENT_NODE ||
          root->isShadowNode())) {
        ec = RangeException::INVALID_NODE_TYPE_ERR;
        return;
    }

    if( n->nodeType() == Node::DOCUMENT_NODE ||
        n->nodeType() == Node::DOCUMENT_FRAGMENT_NODE ||
        n->nodeType() == Node::ATTRIBUTE_NODE ||
        n->nodeType() == Node::ENTITY_NODE ||
        n->nodeType() == Node::NOTATION_NODE )
        ec = RangeException::INVALID_NODE_TYPE_ERR;

}

PassRefPtr<Range> Range::cloneRange(ExceptionCode& ec) const
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return 0;
    }

    return new Range(m_ownerDocument.get(), m_startContainer.get(), m_startOffset, m_endContainer.get(), m_endOffset);
}

void Range::setStartAfter( Node *refNode, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }

    ec = 0;
    checkNodeBA( refNode, ec );
    if (ec)
        return;

    setStart(refNode->parentNode(), refNode->nodeIndex() + 1, ec);
}

void Range::setEndBefore( Node *refNode, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }

    ec = 0;
    checkNodeBA(refNode, ec);
    if (ec)
        return;

    setEnd(refNode->parentNode(), refNode->nodeIndex(), ec);
}

void Range::setEndAfter( Node *refNode, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }

    ec = 0;
    checkNodeBA(refNode, ec);
    if (ec)
        return;

    setEnd(refNode->parentNode(), refNode->nodeIndex() + 1, ec);

}

void Range::selectNode( Node *refNode, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // INVALID_NODE_TYPE_ERR: Raised if an ancestor of refNode is an Entity, Notation or
    // DocumentType node or if refNode is a Document, DocumentFragment, Attr, Entity, or Notation
    // node.
    Node *anc;
    for (anc = refNode->parentNode(); anc; anc = anc->parentNode()) {
        if (anc->nodeType() == Node::ENTITY_NODE ||
            anc->nodeType() == Node::NOTATION_NODE ||
            anc->nodeType() == Node::DOCUMENT_TYPE_NODE) {

            ec = RangeException::INVALID_NODE_TYPE_ERR;
            return;
        }
    }

    if (refNode->nodeType() == Node::DOCUMENT_NODE ||
        refNode->nodeType() == Node::DOCUMENT_FRAGMENT_NODE ||
        refNode->nodeType() == Node::ATTRIBUTE_NODE ||
        refNode->nodeType() == Node::ENTITY_NODE ||
        refNode->nodeType() == Node::NOTATION_NODE) {

        ec = RangeException::INVALID_NODE_TYPE_ERR;
        return;
    }

    ec = 0;
    setStartBefore( refNode, ec );
    if (ec)
        return;
    setEndAfter( refNode, ec );
}

void Range::selectNodeContents( Node *refNode, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // INVALID_NODE_TYPE_ERR: Raised if refNode or an ancestor of refNode is an Entity, Notation
    // or DocumentType node.
    Node *n;
    for (n = refNode; n; n = n->parentNode()) {
        if (n->nodeType() == Node::ENTITY_NODE ||
            n->nodeType() == Node::NOTATION_NODE ||
            n->nodeType() == Node::DOCUMENT_TYPE_NODE) {

            ec = RangeException::INVALID_NODE_TYPE_ERR;
            return;
        }
    }

    m_startContainer = refNode;
    m_startOffset = 0;
    m_endContainer = refNode;
    m_endOffset = refNode->offsetInCharacters() ? refNode->maxCharacterOffset() : refNode->childNodeCount();
}

void Range::surroundContents(PassRefPtr<Node> passNewParent, ExceptionCode& ec)
{
    RefPtr<Node> newParent = passNewParent;

    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!newParent) {
        ec = NOT_FOUND_ERR;
        return;
    }

    // INVALID_NODE_TYPE_ERR: Raised if node is an Attr, Entity, DocumentType, Notation,
    // Document, or DocumentFragment node.
    if( newParent->nodeType() == Node::ATTRIBUTE_NODE ||
        newParent->nodeType() == Node::ENTITY_NODE ||
        newParent->nodeType() == Node::NOTATION_NODE ||
        newParent->nodeType() == Node::DOCUMENT_TYPE_NODE ||
        newParent->nodeType() == Node::DOCUMENT_NODE ||
        newParent->nodeType() == Node::DOCUMENT_FRAGMENT_NODE) {
        ec = RangeException::INVALID_NODE_TYPE_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container of either boundary-point of
    // the Range is read-only.
    if (containedByReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // WRONG_DOCUMENT_ERR: Raised if newParent and the container of the start of the Range were
    // not created from the same document.
    if (newParent->document() != m_startContainer->document()) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }

    // Raise a HIERARCHY_REQUEST_ERR if m_startContainer doesn't accept children like newParent.
    Node* parentOfNewParent = m_startContainer.get();
    // If m_startContainer is a textNode, it will be split and it will be its parent that will 
    // need to accept newParent.
    if (parentOfNewParent->isTextNode())
        parentOfNewParent = parentOfNewParent->parentNode();
    if (!parentOfNewParent->childTypeAllowed(newParent->nodeType())) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }
    
    if (m_startContainer == newParent || m_startContainer->isDescendantOf(newParent.get())) {
        ec = HIERARCHY_REQUEST_ERR;
        return;
    }

    // ### check if node would end up with a child node of a type not allowed by the type of node

    // BAD_BOUNDARYPOINTS_ERR: Raised if the Range partially selects a non-text node.
    if (!m_startContainer->offsetInCharacters()) {
        if (m_startOffset > 0 && m_startOffset < m_startContainer->childNodeCount()) {
            ec = RangeException::BAD_BOUNDARYPOINTS_ERR;
            return;
        }
    }
    if (!m_endContainer->offsetInCharacters()) {
        if (m_endOffset > 0 && m_endOffset < m_endContainer->childNodeCount()) {
            ec = RangeException::BAD_BOUNDARYPOINTS_ERR;
            return;
        }
    }

    ec = 0;
    while (Node* n = newParent->firstChild()) {
        newParent->removeChild(n, ec);
        if (ec)
            return;
    }
    RefPtr<DocumentFragment> fragment = extractContents(ec);
    if (ec)
        return;
    insertNode(newParent, ec);
    if (ec)
        return;
    newParent->appendChild(fragment.release(), ec);
    if (ec)
        return;
    selectNode(newParent.get(), ec);
}

void Range::setStartBefore( Node *refNode, ExceptionCode& ec)
{
    if (m_detached) {
        ec = INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        ec = NOT_FOUND_ERR;
        return;
    }

    if (refNode->document() != m_ownerDocument) {
        ec = WRONG_DOCUMENT_ERR;
        return;
    }

    ec = 0;
    checkNodeBA(refNode, ec);
    if (ec)
        return;

    setStart(refNode->parentNode(), refNode->nodeIndex(), ec);
}

void Range::checkDeleteExtract(ExceptionCode& ec)
{
    if (!commonAncestorContainer(ec) || ec)
        return;
        
    Node *pastEnd = pastEndNode();
    for (Node *n = startNode(); n != pastEnd; n = n->traverseNextNode()) {
        if (n->isReadOnlyNode()) {
            ec = NO_MODIFICATION_ALLOWED_ERR;
            return;
        }
        if (n->nodeType() == Node::DOCUMENT_TYPE_NODE) { // ### is this for only directly under the DF, or anywhere?
            ec = HIERARCHY_REQUEST_ERR;
            return;
        }
    }

    if (containedByReadOnly()) {
        ec = NO_MODIFICATION_ALLOWED_ERR;
        return;
    }
}

bool Range::containedByReadOnly() const
{
    Node *n;
    for (n = m_startContainer.get(); n; n = n->parentNode()) {
        if (n->isReadOnlyNode())
            return true;
    }
    for (n = m_endContainer.get(); n; n = n->parentNode()) {
        if (n->isReadOnlyNode())
            return true;
    }
    return false;
}

Position Range::startPosition() const
{
    return Position(m_startContainer.get(), m_startOffset);
}

Position Range::endPosition() const
{
    return Position(m_endContainer.get(), m_endOffset);
}

Node *Range::startNode() const
{
    if (!m_startContainer)
        return 0;
    if (m_startContainer->offsetInCharacters())
        return m_startContainer.get();
    Node *child = m_startContainer->childNode(m_startOffset);
    if (child)
        return child;
    if (m_startOffset == 0)
        return m_startContainer.get();
    return m_startContainer->traverseNextSibling();
}

Position Range::editingStartPosition() const
{
    // This function is used by range style computations to avoid bugs like:
    // <rdar://problem/4017641> REGRESSION (Mail): you can only bold/unbold a selection starting from end of line once
    // It is important to skip certain irrelevant content at the start of the selection, so we do not wind up 
    // with a spurious "mixed" style.
    
    VisiblePosition visiblePosition(m_startContainer.get(), m_startOffset, VP_DEFAULT_AFFINITY);
    if (visiblePosition.isNull())
        return Position();

    ExceptionCode ec = 0;
    // if the selection is a caret, just return the position, since the style
    // behind us is relevant
    if (collapsed(ec))
        return visiblePosition.deepEquivalent();

    // if the selection starts just before a paragraph break, skip over it
    if (isEndOfParagraph(visiblePosition))
        return visiblePosition.next().deepEquivalent().downstream();

    // otherwise, make sure to be at the start of the first selected node,
    // instead of possibly at the end of the last node before the selection
    return visiblePosition.deepEquivalent().downstream();
}

Node *Range::pastEndNode() const
{
    if (!m_startContainer || !m_endContainer)
        return 0;
    if (m_endContainer->offsetInCharacters())
        return m_endContainer->traverseNextSibling();
    Node *child = m_endContainer->childNode(m_endOffset);
    if (child)
        return child;
    return m_endContainer->traverseNextSibling();
}

IntRect Range::boundingBox()
{
    IntRect result;
    Vector<IntRect> rects;
    addLineBoxRects(rects);
    const size_t n = rects.size();
    for (size_t i = 0; i < n; ++i)
        result.unite(rects[i]);
    return result;
}

void Range::addLineBoxRects(Vector<IntRect>& rects, bool useSelectionHeight)
{
    if (!m_startContainer || !m_endContainer)
        return;

    RenderObject* start = m_startContainer->renderer();
    RenderObject* end = m_endContainer->renderer();
    if (!start || !end)
        return;

    RenderObject* stop = end->nextInPreOrderAfterChildren();
    for (RenderObject* r = start; r && r != stop; r = r->nextInPreOrder()) {
        // only ask leaf render objects for their line box rects
        if (!r->firstChild()) {
            int startOffset = r == start ? m_startOffset : 0;
            int endOffset = r == end ? m_endOffset : UINT_MAX;
            r->addLineBoxRects(rects, startOffset, endOffset, useSelectionHeight);
        }
    }
}

#ifndef NDEBUG
#define FormatBufferSize 1024
void Range::formatForDebugger(char *buffer, unsigned length) const
{
    String result;
    String s;
    
    if (!m_startContainer || !m_endContainer)
        result = "<empty>";
    else {
        char s[FormatBufferSize];
        result += "from offset ";
        result += String::number(m_startOffset);
        result += " of ";
        m_startContainer->formatForDebugger(s, FormatBufferSize);
        result += s;
        result += " to offset ";
        result += String::number(m_endOffset);
        result += " of ";
        m_endContainer->formatForDebugger(s, FormatBufferSize);
        result += s;
    }
          
    strncpy(buffer, result.deprecatedString().latin1(), length - 1);
}
#undef FormatBufferSize
#endif

bool operator==(const Range &a, const Range &b)
{
    if (&a == &b)
        return true;
    // Not strictly legal C++, but in practice this can happen, and works fine with GCC.
    if (!&a || !&b)
        return false;
    bool ad = a.isDetached();
    bool bd = b.isDetached();
    if (ad && bd)
        return true;
    if (ad || bd)
        return false;
    int exception = 0;
    return a.startContainer(exception) == b.startContainer(exception)
        && a.endContainer(exception) == b.endContainer(exception)
        && a.startOffset(exception) == b.startOffset(exception)
        && a.endOffset(exception) == b.endOffset(exception);
}

PassRefPtr<Range> rangeOfContents(Node* node)
{
    ASSERT(node);
    RefPtr<Range> range = new Range(node->document());
    int exception = 0;
    range->selectNodeContents(node, exception);
    return range.release();
}

}
