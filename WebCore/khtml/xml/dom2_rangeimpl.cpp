/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2004 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "dom2_rangeimpl.h"

#include "dom/dom_exception.h"
#include "dom_textimpl.h"
#include "dom_xmlimpl.h"
#include "html/html_elementimpl.h"
#include "editing/markup.h"
#include "editing/visible_position.h"
#include "editing/visible_units.h"
#include "editing/visible_text.h"
#include "xml/dom_position.h"

#include "render_block.h"

using khtml::createMarkup;
using khtml::RenderBlock;
using khtml::RenderObject;
using khtml::VisiblePosition;
using khtml::DOWNSTREAM;
using khtml::isEndOfParagraph;

namespace DOM {

RangeImpl::RangeImpl(DocumentPtr *_ownerDocument)
{
    m_ownerDocument = _ownerDocument;
    m_ownerDocument->ref();
    m_startContainer = _ownerDocument->document();
    m_startContainer->ref();
    m_endContainer = _ownerDocument->document();
    m_endContainer->ref();
    m_startOffset = 0;
    m_endOffset = 0;
    m_detached = false;
}

RangeImpl::RangeImpl(DocumentPtr *_ownerDocument,
              NodeImpl *_startContainer, long _startOffset,
              NodeImpl *_endContainer, long _endOffset)
{
    m_ownerDocument = _ownerDocument;
    m_ownerDocument->ref();
    m_startContainer = _startContainer;
    m_startContainer->ref();
    m_startOffset = _startOffset;
    m_endContainer = _endContainer;
    m_endContainer->ref();
    m_endOffset = _endOffset;
    m_detached = false;
}

RangeImpl::~RangeImpl()
{
    m_ownerDocument->deref();
    int exceptioncode = 0;
    if (!m_detached)
        detach(exceptioncode);
}

NodeImpl *RangeImpl::startContainer(int &exceptioncode) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    return m_startContainer;
}

long RangeImpl::startOffset(int &exceptioncode) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    return m_startOffset;
}

NodeImpl *RangeImpl::endContainer(int &exceptioncode) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    return m_endContainer;
}

long RangeImpl::endOffset(int &exceptioncode) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    return m_endOffset;
}

NodeImpl *RangeImpl::commonAncestorContainer(int &exceptioncode) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    NodeImpl *com = commonAncestorContainer(m_startContainer,m_endContainer);
    if (!com) //  should never happen
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
    return com;
}

NodeImpl *RangeImpl::commonAncestorContainer(NodeImpl *containerA, NodeImpl *containerB)
{
    NodeImpl *parentStart;

    for (parentStart = containerA; parentStart; parentStart = parentStart->parentNode()) {
        NodeImpl *parentEnd = containerB;
        while (parentEnd && (parentStart != parentEnd))
            parentEnd = parentEnd->parentNode();
        if (parentStart == parentEnd)
            break;
    }

    if (!parentStart && containerA->getDocument())
        return containerA->getDocument()->documentElement();
        
    return parentStart;
}

bool RangeImpl::collapsed(int &exceptioncode) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    return (m_startContainer == m_endContainer && m_startOffset == m_endOffset);
}

void RangeImpl::setStart( NodeImpl *refNode, long offset, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    if (refNode->getDocument() != m_ownerDocument->document()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }

    checkNodeWOffset( refNode, offset, exceptioncode );
    if (exceptioncode)
        return;

    setStartContainer(refNode);
    m_startOffset = offset;

    // check if different root container
    NodeImpl *endRootContainer = m_endContainer;
    while (endRootContainer->parentNode())
        endRootContainer = endRootContainer->parentNode();
    NodeImpl *startRootContainer = m_startContainer;
    while (startRootContainer->parentNode())
        startRootContainer = startRootContainer->parentNode();
    if (startRootContainer != endRootContainer)
        collapse(true,exceptioncode);
    // check if new start after end
    else if (compareBoundaryPoints(m_startContainer,m_startOffset,m_endContainer,m_endOffset) > 0)
        collapse(true,exceptioncode);
}

void RangeImpl::setEnd( NodeImpl *refNode, long offset, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    if (refNode->getDocument() != m_ownerDocument->document()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }

    checkNodeWOffset( refNode, offset, exceptioncode );
    if (exceptioncode)
        return;

    setEndContainer(refNode);
    m_endOffset = offset;

    // check if different root container
    NodeImpl *endRootContainer = m_endContainer;
    while (endRootContainer->parentNode())
        endRootContainer = endRootContainer->parentNode();
    NodeImpl *startRootContainer = m_startContainer;
    while (startRootContainer->parentNode())
        startRootContainer = startRootContainer->parentNode();
    if (startRootContainer != endRootContainer)
        collapse(false,exceptioncode);
    // check if new end before start
    if (compareBoundaryPoints(m_startContainer,m_startOffset,m_endContainer,m_endOffset) > 0)
        collapse(false,exceptioncode);
}

void RangeImpl::collapse( bool toStart, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if( toStart )   // collapse to start
    {
        setEndContainer(m_startContainer);
        m_endOffset = m_startOffset;
    }
    else            // collapse to end
    {
        setStartContainer(m_endContainer);
        m_startOffset = m_endOffset;
    }
}

short RangeImpl::compareBoundaryPoints( Range::CompareHow how, const RangeImpl *sourceRange, int &exceptioncode ) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    if (!sourceRange) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return 0;
    }

    NodeImpl *thisCont = commonAncestorContainer(exceptioncode);
    NodeImpl *sourceCont = sourceRange->commonAncestorContainer(exceptioncode);
    if (exceptioncode)
        return 0;

    if (thisCont->getDocument() != sourceCont->getDocument()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return 0;
    }

    NodeImpl *thisTop = thisCont;
    NodeImpl *sourceTop = sourceCont;
    while (thisTop->parentNode())
	thisTop = thisTop->parentNode();
    while (sourceTop->parentNode())
	sourceTop = sourceTop->parentNode();
    if (thisTop != sourceTop) { // in different DocumentFragments
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return 0;
    }

    switch(how)
    {
    case Range::START_TO_START:
        return compareBoundaryPoints( m_startContainer, m_startOffset,
                                      sourceRange->startContainer(exceptioncode), sourceRange->startOffset(exceptioncode) );
        break;
    case Range::START_TO_END:
        return compareBoundaryPoints( m_startContainer, m_startOffset,
                                      sourceRange->endContainer(exceptioncode), sourceRange->endOffset(exceptioncode) );
        break;
    case Range::END_TO_END:
        return compareBoundaryPoints( m_endContainer, m_endOffset,
                                      sourceRange->endContainer(exceptioncode), sourceRange->endOffset(exceptioncode) );
        break;
    case Range::END_TO_START:
        return compareBoundaryPoints( m_endContainer, m_endOffset,
                                      sourceRange->startContainer(exceptioncode), sourceRange->startOffset(exceptioncode) );
        break;
    default:
        exceptioncode = DOMException::SYNTAX_ERR;
        return 0;
    }
}

short RangeImpl::compareBoundaryPoints( NodeImpl *containerA, long offsetA, NodeImpl *containerB, long offsetB )
{
    // see DOM2 traversal & range section 2.5

    // case 1: both points have the same container
    if( containerA == containerB )
    {
        if( offsetA == offsetB )  return 0;    // A is equal to B
        if( offsetA < offsetB )  return -1;    // A is before B
        else  return 1;                        // A is after B
    }

    // case 2: node C (container B or an ancestor) is a child node of A
    NodeImpl *c = containerB;
    while (c && c->parentNode() != containerA)
        c = c->parentNode();
    if (c) {
        int offsetC = 0;
        NodeImpl *n = containerA->firstChild();
        while (n != c && offsetC < offsetA) {
            offsetC++;
            n = n->nextSibling();
        }

        if( offsetA <= offsetC )  return -1;    // A is before B
        else  return 1;                        // A is after B
    }

    // case 3: node C (container A or an ancestor) is a child node of B
    c = containerA;
    while (c && c->parentNode() != containerB)
        c = c->parentNode();
    if (c) {
        int offsetC = 0;
        NodeImpl *n = containerB->firstChild();
        while (n != c && offsetC < offsetB) {
            offsetC++;
            n = n->nextSibling();
        }

        if( offsetC < offsetB )  return -1;    // A is before B
        else  return 1;                        // A is after B
    }

    // case 4: containers A & B are siblings, or children of siblings
    // ### we need to do a traversal here instead
    NodeImpl *cmnRoot = commonAncestorContainer(containerA,containerB);
    NodeImpl *childA = containerA;
    while (childA && childA->parentNode() != cmnRoot)
        childA = childA->parentNode();
    if (!childA)
        childA = cmnRoot;
    NodeImpl *childB = containerB;
    while (childB && childB->parentNode() != cmnRoot)
        childB = childB->parentNode();
    if (!childB)
        childB = cmnRoot;

    if (childA == childB)
        return 0; // A is equal to B

    NodeImpl *n = cmnRoot->firstChild();
    while (n) {
        if (n == childA)
            return -1; // A is before B
        if (n == childB)
            return 1; // A is after B
        n = n->nextSibling();
    }

    // Should never reach this point.
    assert(0);
    return 0;
}

short RangeImpl::compareBoundaryPoints( const Position &a, const Position &b )
{
    return compareBoundaryPoints(a.node(), a.offset(), b.node(), b.offset());
}

bool RangeImpl::boundaryPointsValid(  ) const
{
    return compareBoundaryPoints( m_startContainer, m_startOffset, m_endContainer, m_endOffset ) <= 0;
}

void RangeImpl::deleteContents( int &exceptioncode ) {
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    checkDeleteExtract(exceptioncode);
    if (exceptioncode)
	return;

    processContents(DELETE_CONTENTS,exceptioncode);
}

DocumentFragmentImpl *RangeImpl::processContents ( ActionType action, int &exceptioncode )
{
    // ### when mutation events are implemented, we will have to take into account
    // situations where the tree is being transformed while we delete - ugh!

    // ### perhaps disable node deletion notification for this range while we do this?

    if (collapsed(exceptioncode))
        return 0;
    if (exceptioncode)
        return 0;

    NodeImpl *cmnRoot = commonAncestorContainer(exceptioncode);
    if (exceptioncode)
        return 0;

    // what is the highest node that partially selects the start of the range?
    NodeImpl *partialStart = 0;
    if (m_startContainer != cmnRoot) {
	partialStart = m_startContainer;
	while (partialStart->parentNode() != cmnRoot)
	    partialStart = partialStart->parentNode();
    }

    // what is the highest node that partially selects the end of the range?
    NodeImpl *partialEnd = 0;
    if (m_endContainer != cmnRoot) {
	partialEnd = m_endContainer;
	while (partialEnd->parentNode() != cmnRoot)
	    partialEnd = partialEnd->parentNode();
    }

    DocumentFragmentImpl *fragment = 0;
    if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
        fragment = new DocumentFragmentImpl(m_ownerDocument);

    // Simple case: the start and end containers are the same. We just grab
    // everything >= start offset and < end offset
    if (m_startContainer == m_endContainer) {
        if(m_startContainer->nodeType() == Node::TEXT_NODE ||
           m_startContainer->nodeType() == Node::CDATA_SECTION_NODE ||
           m_startContainer->nodeType() == Node::COMMENT_NODE) {

            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                CharacterDataImpl *c = static_cast<CharacterDataImpl*>(m_startContainer->cloneNode(true));
                c->deleteData(m_endOffset,static_cast<CharacterDataImpl*>(m_startContainer)->length()-m_endOffset,exceptioncode);
                c->deleteData(0,m_startOffset,exceptioncode);
                fragment->appendChild(c,exceptioncode);
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
                static_cast<CharacterDataImpl*>(m_startContainer)->deleteData(m_startOffset,m_endOffset-m_startOffset,exceptioncode);
        }
        else if (m_startContainer->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            // ### operate just on data ?
        }
        else {
            NodeImpl *n = m_startContainer->firstChild();
            unsigned long i;
            for (i = 0; n && i < m_startOffset; i++) // skip until m_startOffset
                n = n->nextSibling();
            while (n && i < m_endOffset) { // delete until m_endOffset
		NodeImpl *next = n->nextSibling();
                if (action == EXTRACT_CONTENTS)
                    fragment->appendChild(n,exceptioncode); // will remove n from it's parent
                else if (action == CLONE_CONTENTS)
                    fragment->appendChild(n->cloneNode(true),exceptioncode);
                else
                    m_startContainer->removeChild(n,exceptioncode);
                n = next;
                i++;
            }
        }
        if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
            collapse(true,exceptioncode);
        return fragment;
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

    NodeImpl *leftContents = 0;
    if (m_startContainer != cmnRoot) {
        // process the left-hand side of the range, up until the last ancestor of
        // m_startContainer before cmnRoot
        if(m_startContainer->nodeType() == Node::TEXT_NODE ||
           m_startContainer->nodeType() == Node::CDATA_SECTION_NODE ||
           m_startContainer->nodeType() == Node::COMMENT_NODE) {

            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                CharacterDataImpl *c = static_cast<CharacterDataImpl*>(m_startContainer->cloneNode(true));
                c->deleteData(0,m_startOffset,exceptioncode);
                leftContents = c;
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
                static_cast<CharacterDataImpl*>(m_startContainer)->deleteData(
                    m_startOffset,static_cast<CharacterDataImpl*>(m_startContainer)->length()-m_startOffset,exceptioncode);
        }
        else if (m_startContainer->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            // ### operate just on data ?
            // leftContents = ...
        }
        else {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
		leftContents = m_startContainer->cloneNode(false);
            NodeImpl *n = m_startContainer->firstChild();
            for (unsigned long i = 0; n && i < m_startOffset; i++) // skip until m_startOffset
                n = n->nextSibling();
            while (n) { // process until end
		NodeImpl *next = n->nextSibling();
                if (action == EXTRACT_CONTENTS)
                    leftContents->appendChild(n,exceptioncode); // will remove n from m_startContainer
                else if (action == CLONE_CONTENTS)
                    leftContents->appendChild(n->cloneNode(true),exceptioncode);
                else
                    m_startContainer->removeChild(n,exceptioncode);
                n = next;
            }
        }

        NodeImpl *leftParent = m_startContainer->parentNode();
        NodeImpl *n = m_startContainer->nextSibling();
        for (; leftParent != cmnRoot; leftParent = leftParent->parentNode()) {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
		NodeImpl *leftContentsParent = leftParent->cloneNode(false);
		leftContentsParent->appendChild(leftContents,exceptioncode);
		leftContents = leftContentsParent;
	    }

            NodeImpl *next;
            for (; n; n = next) {
                next = n->nextSibling();
                if (action == EXTRACT_CONTENTS)
                    leftContents->appendChild(n,exceptioncode); // will remove n from leftParent
                else if (action == CLONE_CONTENTS)
                    leftContents->appendChild(n->cloneNode(true),exceptioncode);
                else
                    leftParent->removeChild(n,exceptioncode);
            }
            n = leftParent->nextSibling();
        }
    }

    NodeImpl *rightContents = 0;
    if (m_endContainer != cmnRoot) {
        // delete the right-hand side of the range, up until the last ancestor of
        // m_endContainer before cmnRoot
        if(m_endContainer->nodeType() == Node::TEXT_NODE ||
           m_endContainer->nodeType() == Node::CDATA_SECTION_NODE ||
           m_endContainer->nodeType() == Node::COMMENT_NODE) {

            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                CharacterDataImpl *c = static_cast<CharacterDataImpl*>(m_endContainer->cloneNode(true));
                c->deleteData(m_endOffset,static_cast<CharacterDataImpl*>(m_endContainer)->length()-m_endOffset,exceptioncode);
                rightContents = c;
            }
            if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
                static_cast<CharacterDataImpl*>(m_endContainer)->deleteData(0,m_endOffset,exceptioncode);
        }
        else if (m_startContainer->nodeType() == Node::PROCESSING_INSTRUCTION_NODE) {
            // ### operate just on data ?
            // rightContents = ...
        }
        else {
	    if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
		rightContents = m_endContainer->cloneNode(false);
            NodeImpl *n = m_endContainer->firstChild();
            if (n && m_endOffset) {
                for (unsigned long i = 0; i+1 < m_endOffset; i++) { // skip to m_endOffset
                    NodeImpl *next = n->nextSibling();
                    if (!next)
                        break;
                    n = next;
                }
                NodeImpl *prev;
                for (; n; n = prev) {
                    prev = n->previousSibling();
                    if (action == EXTRACT_CONTENTS)
                        rightContents->insertBefore(n,rightContents->firstChild(),exceptioncode); // will remove n from it's parent
                    else if (action == CLONE_CONTENTS)
                        rightContents->insertBefore(n->cloneNode(true),rightContents->firstChild(),exceptioncode);
                    else
                        m_endContainer->removeChild(n,exceptioncode);
                }
            }
        }

        NodeImpl *rightParent = m_endContainer->parentNode();
        NodeImpl *n = m_endContainer->previousSibling();
        for (; rightParent != cmnRoot; rightParent = rightParent->parentNode()) {
            if (action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) {
                NodeImpl *rightContentsParent = rightParent->cloneNode(false);
                rightContentsParent->appendChild(rightContents,exceptioncode);
                rightContents = rightContentsParent;
            }

            NodeImpl *prev;
            for (; n; n = prev) {
                prev = n->previousSibling();
                if (action == EXTRACT_CONTENTS)
                    rightContents->insertBefore(n,rightContents->firstChild(),exceptioncode); // will remove n from it's parent
                else if (action == CLONE_CONTENTS)
                    rightContents->insertBefore(n->cloneNode(true),rightContents->firstChild(),exceptioncode);
                else
                    rightParent->removeChild(n,exceptioncode);

            }
            n = rightParent->previousSibling();
        }
    }

    // delete all children of cmnRoot between the start and end container

    NodeImpl *processStart; // child of cmnRooot
    if (m_startContainer == cmnRoot) {
        unsigned long i;
        processStart = m_startContainer->firstChild();
        for (i = 0; i < m_startOffset; i++)
            processStart = processStart->nextSibling();
    }
    else {
        processStart = m_startContainer;
        while (processStart->parentNode() != cmnRoot)
            processStart = processStart->parentNode();
        processStart = processStart->nextSibling();
    }
    NodeImpl *processEnd; // child of cmnRooot
    if (m_endContainer == cmnRoot) {
        unsigned long i;
        processEnd = m_endContainer->firstChild();
        for (i = 0; i < m_endOffset; i++)
            processEnd = processEnd->nextSibling();
    }
    else {
        processEnd = m_endContainer;
        while (processEnd->parentNode() != cmnRoot)
            processEnd = processEnd->parentNode();
    }

    // Now add leftContents, stuff in between, and rightContents to the fragment
    // (or just delete the stuff in between)

    if ((action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) && leftContents)
      fragment->appendChild(leftContents,exceptioncode);

    NodeImpl *next;
    NodeImpl *n;
    if (processStart) {
        for (n = processStart; n && n != processEnd; n = next) {
            next = n->nextSibling();

            if (action == EXTRACT_CONTENTS)
                fragment->appendChild(n,exceptioncode); // will remove from cmnRoot
            else if (action == CLONE_CONTENTS)
                fragment->appendChild(n->cloneNode(true),exceptioncode);
            else
                cmnRoot->removeChild(n,exceptioncode);
        }
    }

    if ((action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) && rightContents)
      fragment->appendChild(rightContents,exceptioncode);

    // collapse to the proper position - see spec section 2.6
    if (action == EXTRACT_CONTENTS || action == DELETE_CONTENTS) {
	if (!partialStart && !partialEnd)
	    collapse(true,exceptioncode);
	else if (partialStart) {
	    setStartContainer(partialStart->parentNode());
	    setEndContainer(partialStart->parentNode());
	    m_startOffset = m_endOffset = partialStart->nodeIndex()+1;
	}
	else if (partialEnd) {
	    setStartContainer(partialEnd->parentNode());
	    setEndContainer(partialEnd->parentNode());
	    m_startOffset = m_endOffset = partialEnd->nodeIndex();
	}
    }
    return fragment;
}


DocumentFragmentImpl *RangeImpl::extractContents( int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    checkDeleteExtract(exceptioncode);
    if (exceptioncode)
	return 0;

    return processContents(EXTRACT_CONTENTS,exceptioncode);
}

DocumentFragmentImpl *RangeImpl::cloneContents( int &exceptioncode  )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    return processContents(CLONE_CONTENTS,exceptioncode);
}

void RangeImpl::insertNode( NodeImpl *newNode, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container of either boundary-point of
    // the Range is read-only.
    if (containedByReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // WRONG_DOCUMENT_ERR: Raised if newParent and the container of the start of the Range were
    // not created from the same document.
    if (newNode->getDocument() != m_startContainer->getDocument()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }


    // HIERARCHY_REQUEST_ERR: Raised if the container of the start of the Range is of a type that
    // does not allow children of the type of newNode or if newNode is an ancestor of the container.

    // an extra one here - if a text node is going to split, it must have a parent to insert into
    if (m_startContainer->nodeType() == Node::TEXT_NODE && !m_startContainer->parentNode()) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return;
    }

    // In the case where the container is a text node, we check against the container's parent, because
    // text nodes get split up upon insertion.
    NodeImpl *checkAgainst;
    if (m_startContainer->nodeType() == Node::TEXT_NODE)
	checkAgainst = m_startContainer->parentNode();
    else
	checkAgainst = m_startContainer;

    if (newNode->nodeType() == Node::DOCUMENT_FRAGMENT_NODE) {
	// check each child node, not the DocumentFragment itself
    	NodeImpl *c;
    	for (c = newNode->firstChild(); c; c = c->nextSibling()) {
	    if (!checkAgainst->childTypeAllowed(c->nodeType())) {
		exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
		return;
	    }
    	}
    }
    else {
	if (!checkAgainst->childTypeAllowed(newNode->nodeType())) {
	    exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
	    return;
	}
    }

    for (NodeImpl *n = m_startContainer; n; n = n->parentNode()) {
        if (n == newNode) {
            exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
            return;
        }
    }

    // INVALID_NODE_TYPE_ERR: Raised if newNode is an Attr, Entity, Notation, or Document node.
    if( newNode->nodeType() == Node::ATTRIBUTE_NODE ||
        newNode->nodeType() == Node::ENTITY_NODE ||
        newNode->nodeType() == Node::NOTATION_NODE ||
        newNode->nodeType() == Node::DOCUMENT_NODE) {
        exceptioncode = RangeException::INVALID_NODE_TYPE_ERR + RangeException::_EXCEPTION_OFFSET;
        return;
    }

    if( m_startContainer->nodeType() == Node::TEXT_NODE ||
        m_startContainer->nodeType() == Node::CDATA_SECTION_NODE )
    {
        TextImpl *newText = static_cast<TextImpl*>(m_startContainer)->splitText(m_startOffset,exceptioncode);
        if (exceptioncode)
            return;
        m_startContainer->parentNode()->insertBefore( newNode, newText, exceptioncode );
    }
    else {
        m_startContainer->insertBefore( newNode, m_startContainer->childNode( m_startOffset ), exceptioncode );
    }
}

DOMString RangeImpl::toString( int &exceptioncode ) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return DOMString();
    }

    DOMString text = "";
    NodeImpl *pastEnd = pastEndNode();
    for (NodeImpl *n = startNode(); n != pastEnd; n = n->traverseNextNode()) {
        if (n->nodeType() == DOM::Node::TEXT_NODE || n->nodeType() == DOM::Node::CDATA_SECTION_NODE) {
            DOMString str = static_cast<TextImpl *>(n)->data().copy();
            if (n == m_endContainer)
                str.truncate(m_endOffset);
            if (n == m_startContainer)
                str.remove(0, m_startOffset);
            text += str;
        }
    }
    return text;
}

DOMString RangeImpl::toHTML() const
{
    return createMarkup(this);
}

DOMString RangeImpl::text() const
{
    if (m_detached)
        return DOMString();

    // We need to update layout, since plainText uses line boxes in the render tree.
    // FIXME: As with innerText, we'd like this to work even if there are no render objects.
    m_startContainer->getDocument()->updateLayout();

    // FIXME: Maybe DOMRange constructor should take const DOMRangeImpl*; if it did we would not need this const_cast.
    return plainText(const_cast<RangeImpl *>(this));
}

DocumentFragmentImpl *RangeImpl::createContextualFragment(const DOMString &html, int &exceptioncode) const
{
   if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return NULL;
    }

    if (! m_startContainer->isHTMLElement()) {
	exceptioncode = DOMException::NOT_SUPPORTED_ERR;
	return NULL;
    }

    HTMLElementImpl *e = static_cast<HTMLElementImpl *>(m_startContainer);
    DocumentFragmentImpl *fragment = e->createContextualFragment(html);
    if (!fragment) {
	exceptioncode = DOMException::NOT_SUPPORTED_ERR;
	return NULL;
    }

    return fragment;
}


void RangeImpl::detach( int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (m_startContainer)
        m_startContainer->deref();
    m_startContainer = 0;
    if (m_endContainer)
        m_endContainer->deref();
    m_endContainer = 0;
    m_detached = true;
}

bool RangeImpl::isDetached() const
{
    return m_detached;
}

void RangeImpl::checkNodeWOffset( NodeImpl *n, int offset, int &exceptioncode) const
{
    if( offset < 0 ) {
        exceptioncode = DOMException::INDEX_SIZE_ERR;
    }

    switch (n->nodeType()) {
	case Node::ENTITY_NODE:
	case Node::NOTATION_NODE:
	case Node::DOCUMENT_TYPE_NODE:
            exceptioncode = RangeException::INVALID_NODE_TYPE_ERR + RangeException::_EXCEPTION_OFFSET;
	    break;
        case Node::TEXT_NODE:
        case Node::COMMENT_NODE:
        case Node::CDATA_SECTION_NODE:
            if ( (unsigned long)offset > static_cast<CharacterDataImpl*>(n)->length() )
                exceptioncode = DOMException::INDEX_SIZE_ERR;
            break;
        case Node::PROCESSING_INSTRUCTION_NODE:
            // ### are we supposed to check with just data or the whole contents?
            if ( (unsigned long)offset > static_cast<ProcessingInstructionImpl*>(n)->data().length() )
                exceptioncode = DOMException::INDEX_SIZE_ERR;
            break;
        default:
            if ( (unsigned long)offset > n->childNodeCount() )
                exceptioncode = DOMException::INDEX_SIZE_ERR;
            break;
    }
}

void RangeImpl::checkNodeBA( NodeImpl *n, int &exceptioncode ) const
{
    // INVALID_NODE_TYPE_ERR: Raised if the root container of refNode is not an
    // Attr, Document or DocumentFragment node or if refNode is a Document,
    // DocumentFragment, Attr, Entity, or Notation node.
    NodeImpl *root = n;
    while (root->parentNode())
	root = root->parentNode();
    if (!(root->nodeType() == Node::ATTRIBUTE_NODE ||
          root->nodeType() == Node::DOCUMENT_NODE ||
          root->nodeType() == Node::DOCUMENT_FRAGMENT_NODE)) {
        exceptioncode = RangeException::INVALID_NODE_TYPE_ERR + RangeException::_EXCEPTION_OFFSET;
        return;
    }

    if( n->nodeType() == Node::DOCUMENT_NODE ||
        n->nodeType() == Node::DOCUMENT_FRAGMENT_NODE ||
        n->nodeType() == Node::ATTRIBUTE_NODE ||
        n->nodeType() == Node::ENTITY_NODE ||
        n->nodeType() == Node::NOTATION_NODE )
        exceptioncode = RangeException::INVALID_NODE_TYPE_ERR  + RangeException::_EXCEPTION_OFFSET;

}

RangeImpl *RangeImpl::cloneRange(int &exceptioncode) const
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return 0;
    }

    return new RangeImpl(m_ownerDocument,m_startContainer,m_startOffset,m_endContainer,m_endOffset);
}

void RangeImpl::setStartAfter( NodeImpl *refNode, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    if (refNode->getDocument() != m_ownerDocument->document()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }

    checkNodeBA( refNode, exceptioncode );
    if (exceptioncode)
        return;

    setStart( refNode->parentNode(), refNode->nodeIndex()+1, exceptioncode );
}

void RangeImpl::setEndBefore( NodeImpl *refNode, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    if (refNode->getDocument() != m_ownerDocument->document()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }

    checkNodeBA( refNode, exceptioncode );
    if (exceptioncode)
        return;

    setEnd( refNode->parentNode(), refNode->nodeIndex(), exceptioncode );
}

void RangeImpl::setEndAfter( NodeImpl *refNode, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    if (refNode->getDocument() != m_ownerDocument->document()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }

    checkNodeBA( refNode, exceptioncode );
    if (exceptioncode)
        return;

    setEnd( refNode->parentNode(), refNode->nodeIndex()+1, exceptioncode );

}

void RangeImpl::selectNode( NodeImpl *refNode, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    // INVALID_NODE_TYPE_ERR: Raised if an ancestor of refNode is an Entity, Notation or
    // DocumentType node or if refNode is a Document, DocumentFragment, Attr, Entity, or Notation
    // node.
    NodeImpl *anc;
    for (anc = refNode->parentNode(); anc; anc = anc->parentNode()) {
        if (anc->nodeType() == Node::ENTITY_NODE ||
            anc->nodeType() == Node::NOTATION_NODE ||
            anc->nodeType() == Node::DOCUMENT_TYPE_NODE) {

            exceptioncode = RangeException::INVALID_NODE_TYPE_ERR + RangeException::_EXCEPTION_OFFSET;
            return;
        }
    }

    if (refNode->nodeType() == Node::DOCUMENT_NODE ||
        refNode->nodeType() == Node::DOCUMENT_FRAGMENT_NODE ||
        refNode->nodeType() == Node::ATTRIBUTE_NODE ||
        refNode->nodeType() == Node::ENTITY_NODE ||
        refNode->nodeType() == Node::NOTATION_NODE) {

        exceptioncode = RangeException::INVALID_NODE_TYPE_ERR + RangeException::_EXCEPTION_OFFSET;
        return;
    }

    setStartBefore( refNode, exceptioncode );
    if (exceptioncode)
        return;
    setEndAfter( refNode, exceptioncode );
}

void RangeImpl::selectNodeContents( NodeImpl *refNode, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    // INVALID_NODE_TYPE_ERR: Raised if refNode or an ancestor of refNode is an Entity, Notation
    // or DocumentType node.
    NodeImpl *n;
    for (n = refNode; n; n = n->parentNode()) {
        if (n->nodeType() == Node::ENTITY_NODE ||
            n->nodeType() == Node::NOTATION_NODE ||
            n->nodeType() == Node::DOCUMENT_TYPE_NODE) {

            exceptioncode = RangeException::INVALID_NODE_TYPE_ERR + RangeException::_EXCEPTION_OFFSET;
            return;
        }
    }

    setStartContainer(refNode);
    m_startOffset = 0;
    setEndContainer(refNode);
    m_endOffset = refNode->childNodeCount();
}

void RangeImpl::surroundContents( NodeImpl *newParent, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if( !newParent ) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
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
        exceptioncode = RangeException::INVALID_NODE_TYPE_ERR + RangeException::_EXCEPTION_OFFSET;
        return;
    }

    // NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container of either boundary-point of
    // the Range is read-only.
    if (containedByReadOnly()) {
        exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
        return;
    }

    // WRONG_DOCUMENT_ERR: Raised if newParent and the container of the start of the Range were
    // not created from the same document.
    if (newParent->getDocument() != m_startContainer->getDocument()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }

    // HIERARCHY_REQUEST_ERR: Raised if the container of the start of the Range is of a type that
    // does not allow children of the type of newParent or if newParent is an ancestor of the container
    // or if node would end up with a child node of a type not allowed by the type of node.
    if (!m_startContainer->childTypeAllowed(newParent->nodeType())) {
        exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
        return;
    }

    for (NodeImpl *n = m_startContainer; n; n = n->parentNode()) {
        if (n == newParent) {
            exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
            return;
        }
    }

    // ### check if node would end up with a child node of a type not allowed by the type of node

    // BAD_BOUNDARYPOINTS_ERR: Raised if the Range partially selects a non-text node.
    if (!offsetInCharacters(m_startContainer->nodeType())) {
        if (m_startOffset > 0 && m_startOffset < m_startContainer->childNodeCount()) {
            exceptioncode = RangeException::BAD_BOUNDARYPOINTS_ERR + RangeException::_EXCEPTION_OFFSET;
            return;
        }
    }
    if (!offsetInCharacters(m_endContainer->nodeType())) {
        if (m_endOffset > 0 && m_endOffset < m_endContainer->childNodeCount()) {
            exceptioncode = RangeException::BAD_BOUNDARYPOINTS_ERR + RangeException::_EXCEPTION_OFFSET;
            return;
        }
    }

    while (newParent->firstChild()) {
    	newParent->removeChild(newParent->firstChild(),exceptioncode);
	if (exceptioncode)
	    return;
    }
    DocumentFragmentImpl *fragment = extractContents(exceptioncode);
    if (exceptioncode)
        return;
    insertNode( newParent, exceptioncode );
    if (exceptioncode)
        return;
    newParent->appendChild( fragment, exceptioncode );
    if (exceptioncode)
        return;
    selectNode( newParent, exceptioncode );
}

void RangeImpl::setStartBefore( NodeImpl *refNode, int &exceptioncode )
{
    if (m_detached) {
        exceptioncode = DOMException::INVALID_STATE_ERR;
        return;
    }

    if (!refNode) {
        exceptioncode = DOMException::NOT_FOUND_ERR;
        return;
    }

    if (refNode->getDocument() != m_ownerDocument->document()) {
        exceptioncode = DOMException::WRONG_DOCUMENT_ERR;
        return;
    }

    checkNodeBA( refNode, exceptioncode );
    if (exceptioncode)
        return;

    setStart( refNode->parentNode(), refNode->nodeIndex(), exceptioncode );
}

void RangeImpl::setStartContainer(NodeImpl *_startContainer)
{
    if (m_startContainer == _startContainer)
        return;

    if (m_startContainer)
        m_startContainer->deref();
    m_startContainer = _startContainer;
    if (m_startContainer)
        m_startContainer->ref();
}

void RangeImpl::setEndContainer(NodeImpl *_endContainer)
{
    if (m_endContainer == _endContainer)
        return;

    if (m_endContainer)
        m_endContainer->deref();
    m_endContainer = _endContainer;
    if (m_endContainer)
        m_endContainer->ref();
}

void RangeImpl::checkDeleteExtract(int &exceptioncode)
{
    NodeImpl *pastEnd = pastEndNode();
    for (NodeImpl *n = startNode(); n != pastEnd; n = n->traverseNextNode()) {
	if (n->isReadOnly()) {
	    exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
	    return;
	}
	if (n->nodeType() == Node::DOCUMENT_TYPE_NODE) { // ### is this for only directly under the DF, or anywhere?
	    exceptioncode = DOMException::HIERARCHY_REQUEST_ERR;
	    return;
	}
    }

    if (containedByReadOnly()) {
	exceptioncode = DOMException::NO_MODIFICATION_ALLOWED_ERR;
	return;
    }
}

bool RangeImpl::containedByReadOnly() const
{
    NodeImpl *n;
    for (n = m_startContainer; n; n = n->parentNode()) {
	if (n->isReadOnly())
	    return true;
    }
    for (n = m_endContainer; n; n = n->parentNode()) {
	if (n->isReadOnly())
	    return true;
    }
    return false;
}

Position RangeImpl::startPosition() const
{
    return Position(m_startContainer, m_startOffset);
}

Position RangeImpl::endPosition() const
{
    return Position(m_endContainer, m_endOffset);
}

NodeImpl *RangeImpl::startNode() const
{
    if (!m_startContainer)
        return 0;
    if (offsetInCharacters(m_startContainer->nodeType()))
        return m_startContainer;
    NodeImpl *child = m_startContainer->childNode(m_startOffset);
    if (child)
        return child;
    return m_startContainer->traverseNextSibling();
}

Position RangeImpl::editingStartPosition() const
{
    // This function is used by range style computations to avoid bugs like:
    // <rdar://problem/4017641> REGRESSION (Mail): you can only bold/unbold a selection starting from end of line once
    // It is important to skip certain irrelevant content at the start of the selection, so we do not wind up 
    // with a spurious "mixed" style.
    
    VisiblePosition pos(m_startContainer, m_startOffset, VP_DEFAULT_AFFINITY);
    if (pos.isNull())
        return Position();

    int exceptionCode = 0;
    // if the selection is a caret, just return the position, since the style
    // behind us is relevant
    if (collapsed(exceptionCode))
        return pos.position();

    // if the selection starts just before a paragraph break, skip over it
    if (isEndOfParagraph(pos))
        return pos.next().position().downstream();

    // otherwise, make sure to be at the start of the first selected node,
    // instead of possibly at the end of the last node before the selection
    return pos.position().downstream();
}

NodeImpl *RangeImpl::pastEndNode() const
{
    if (!m_endContainer)
        return 0;
    if (offsetInCharacters(m_endContainer->nodeType()))
        return m_endContainer->traverseNextSibling();
    NodeImpl *child = m_endContainer->childNode(m_endOffset);
    if (child)
        return child;
    return m_endContainer->traverseNextSibling();
}

#ifndef NDEBUG
#define FormatBufferSize 1024
void RangeImpl::formatForDebugger(char *buffer, unsigned length) const
{
    DOMString result;
    DOMString s;
    
    if (!m_startContainer || !m_endContainer) {
        result = "<empty>";
    }
    else {
        char s[FormatBufferSize];
        result += "from offset ";
        result += QString::number(m_startOffset);
        result += " of ";
        m_startContainer->formatForDebugger(s, FormatBufferSize);
        result += s;
        result += " to offset ";
        result += QString::number(m_endOffset);
        result += " of ";
        m_endContainer->formatForDebugger(s, FormatBufferSize);
        result += s;
    }
          
    strncpy(buffer, result.qstring().latin1(), length - 1);
}
#undef FormatBufferSize
#endif

bool operator==(const RangeImpl &a, const RangeImpl &b)
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

SharedPtr<RangeImpl> rangeOfContents(NodeImpl *node)
{
    RangeImpl *range = new RangeImpl(node->docPtr());
    int exception = 0;
    range->selectNodeContents(node, exception);
    return SharedPtr<RangeImpl>(range);
}

}
