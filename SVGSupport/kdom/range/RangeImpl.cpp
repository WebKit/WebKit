/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 1999-2003 Lars Knoll (knoll@kde.org)
 * (C) 2001-2003 Dirk Mueller (mueller@kde.org)
 * (C) 2000 Gunnstein Lye (gunnstein@netcom.no)
 * (C) 2000 Frederik Holljen (frederik.holljen@hig.no)
 * (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2003 Apple Computer, Inc.
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

#include "config.h"
#include "kdom.h"
#include "TextImpl.h"
#include "NodeImpl.h"
#include "kdomrange.h"
#include "RangeImpl.h"
#include "DocumentImpl.h"
#include "NodeListImpl.h"
#include "XMLElementImpl.h"
#include "DOMExceptionImpl.h"
#include "RangeExceptionImpl.h"
#include "DocumentFragmentImpl.h"
#include "ProcessingInstructionImpl.h"

using namespace KDOM;

RangeImpl::RangeImpl(DocumentPtr *_ownerDocument)  : Shared()
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
                     NodeImpl *_endContainer, long _endOffset) : Shared()
{
    m_ownerDocument = _ownerDocument;
    m_ownerDocument->ref();

    m_startContainer = _startContainer;
    m_startContainer->ref();

    m_endContainer = _endContainer;
    m_endContainer->ref();

    m_startOffset = _startOffset;
    m_endOffset = _endOffset;
    m_detached = false;
}

RangeImpl::~RangeImpl()
{
    m_ownerDocument->deref();
    if(!m_detached)
        detach();
}

NodeImpl *RangeImpl::startContainer() const
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);
        
    return m_startContainer;
}

long RangeImpl::startOffset() const
{
    if(m_detached) 
        throw new DOMExceptionImpl(INVALID_STATE_ERR);
        
    return m_startOffset;
}

NodeImpl *RangeImpl::endContainer() const
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    return m_endContainer;
}

long RangeImpl::endOffset() const
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    return m_endOffset;
}

NodeImpl *RangeImpl::commonAncestorContainer()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    NodeImpl *com = commonAncestorContainer(m_startContainer, m_endContainer);
    if(!com) //  should never happen
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);
        
    return com;
}

NodeImpl *RangeImpl::commonAncestorContainer(NodeImpl *containerA, NodeImpl *containerB)
{
    NodeImpl *parentStart;
    for(parentStart = containerA; parentStart != 0; parentStart = parentStart->parentNode())
    {
        NodeImpl *parentEnd = containerB;
        while(parentEnd && (parentStart != parentEnd))
            parentEnd = parentEnd->parentNode();
    
        if(parentStart == parentEnd)
            break;
    }
    
    return parentStart;
}

bool RangeImpl::isCollapsed() const
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);
        
    return (m_startContainer == m_endContainer && m_startOffset == m_endOffset);
}

void RangeImpl::setStart(NodeImpl *refNode, long offset)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    if(refNode->ownerDocument() != m_ownerDocument->document())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    checkNodeWOffset(refNode, offset);
    setStartContainer(refNode);
    m_startOffset = offset;

    // check if different root container
    NodeImpl *endRootContainer = m_endContainer;
    while(endRootContainer->parentNode())
        endRootContainer = endRootContainer->parentNode();
        
    NodeImpl *startRootContainer = m_startContainer;
    while(startRootContainer->parentNode())
        startRootContainer = startRootContainer->parentNode();
        
    if(startRootContainer != endRootContainer)
        collapse(true);
    // check if new start after end
    else if(compareBoundaryPoints(m_startContainer, m_startOffset, m_endContainer, m_endOffset) > 0)
        collapse(true);
}

void RangeImpl::setEnd(NodeImpl *refNode, long offset)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    if(refNode->ownerDocument() != m_ownerDocument->document())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    checkNodeWOffset( refNode, offset);

    setEndContainer(refNode);
    m_endOffset = offset;

    // check if different root container
    NodeImpl *endRootContainer = m_endContainer;
    while(endRootContainer->parentNode())
        endRootContainer = endRootContainer->parentNode();
        
    NodeImpl *startRootContainer = m_startContainer;
    while(startRootContainer->parentNode())
        startRootContainer = startRootContainer->parentNode();
        
    if(startRootContainer != endRootContainer)
        collapse(false);
    // check if new end before start
    else if(compareBoundaryPoints(m_startContainer, m_startOffset, m_endContainer, m_endOffset) > 0)
        collapse(false);
}

void RangeImpl::collapse(bool toStart)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(toStart) // collapse to start
    {
        setEndContainer(m_startContainer);
        m_endOffset = m_startOffset;
    }
    else // collapse to end
    {
        setStartContainer(m_endContainer);
        m_startOffset = m_endOffset;
    }
}

short RangeImpl::compareBoundaryPoints(unsigned short how, RangeImpl *sourceRange)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);
        
    if(!sourceRange)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    NodeImpl *thisCont = commonAncestorContainer();
    NodeImpl *sourceCont = sourceRange->commonAncestorContainer();

    if(thisCont->ownerDocument() != sourceCont->ownerDocument())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    NodeImpl *thisTop = thisCont;
    while(thisTop->parentNode())
        thisTop = thisTop->parentNode();
    
    NodeImpl *sourceTop = sourceCont;
    while(sourceTop->parentNode())
        sourceTop = sourceTop->parentNode();
        
    if(thisTop != sourceTop) // in different DocumentFragments
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    switch(how)
    {
        case START_TO_START:
            return compareBoundaryPoints(m_startContainer, m_startOffset,
                                         sourceRange->startContainer(), sourceRange->startOffset());
        case START_TO_END:
            return compareBoundaryPoints(m_startContainer, m_startOffset, sourceRange->endContainer(),
                                         sourceRange->endOffset());
        case END_TO_END:
            return compareBoundaryPoints(m_endContainer, m_endOffset, sourceRange->endContainer(),
                                         sourceRange->endOffset());
        case END_TO_START:
            return compareBoundaryPoints(m_endContainer, m_endOffset, sourceRange->startContainer(),
                                         sourceRange->startOffset());
        default:
            throw new DOMExceptionImpl(SYNTAX_ERR);
    }
}

short RangeImpl::compareBoundaryPoints(NodeImpl *containerA, long offsetA, NodeImpl *containerB, long offsetB)
{
    // see DOM2 traversal & range section 2.5

    // case 1: both points have the same container
    if( containerA == containerB )
    {
        if(offsetA == offsetB) // A is equal to B
            return 0;
            
        if(offsetA < offsetB) // A is before B
            return -1;
        
        return 1; // A is after B
    }

    // case 2: node C (container B or an ancestor) is a child node of A
    NodeImpl *c = containerB;
    while(c && c->parentNode() != containerA)
        c = c->parentNode();
        
    if(c)
    {
        int offsetC = 0;
        NodeImpl *n = containerA->firstChild();
        while(n != c)
        {
            offsetC++;
            n = n->nextSibling();
        }

        if(offsetA <= offsetC) // A is before B
            return -1;

        return 1; // A is after B
    }

    // case 3: node C (container A or an ancestor) is a child node of B
    c = containerA;
    while(c && c->parentNode() != containerB)
        c = c->parentNode();
        
    if(c)
    {
        int offsetC = 0;
        NodeImpl *n = containerB->firstChild();
        while(n != c)
        {
            offsetC++;
            n = n->nextSibling();
        }

        if(offsetC < offsetB) // A is before B
            return -1;

        return 1; // A is after B
    }

    // case 4: containers A & B are siblings, or children of siblings
    // ### we need to do a traversal here instead
    NodeImpl *cmnRoot = commonAncestorContainer(containerA,containerB);
    if(!cmnRoot) // Whatever...
        return -1;
        
    NodeImpl *childA = containerA;
    while(childA->parentNode() != cmnRoot)
        childA = childA->parentNode();
        
    NodeImpl *childB = containerB;
    while(childB->parentNode() != cmnRoot)
        childB = childB->parentNode();
        
    NodeImpl *n = cmnRoot->firstChild();
    
    int i = 0;
    int childAOffset = -1;
    int childBOffset = -1;
    while(childAOffset < 0 || childBOffset < 0)
    {
        if(n == childA)
            childAOffset = i;
            
        if(n == childB)
            childBOffset = i;
            
        n = n->nextSibling();
        i++;
    }

    if(childAOffset == childBOffset) // A is equal to B
        return 0;
        
    if(childAOffset < childBOffset) // A is before B
        return -1;

    return 1; // A is after B
}

bool RangeImpl::boundaryPointsValid()
{
    short valid = compareBoundaryPoints(m_startContainer, m_startOffset, m_endContainer, m_endOffset);
    if(valid == 1)
        return false;

    return true;
}

void RangeImpl::deleteContents()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);
        
    checkDeleteExtract();
    processContents(DELETE_CONTENTS);
}

DocumentFragmentImpl *RangeImpl::processContents(ActionType action)
{
    // ### when mutation events are implemented, we will have to take into account
    // situations where the tree is being transformed while we delete - ugh!

    // ### perhaps disable node deletion notification for this range while we do this?

    if(isCollapsed())
        return 0;

    NodeImpl *cmnRoot = commonAncestorContainer();

    // what is the highest node that partially selects the start of the range?
    NodeImpl *partialStart = 0;
    if(m_startContainer != cmnRoot)
    {
        partialStart = m_startContainer;
        while(partialStart->parentNode() != cmnRoot)
            partialStart = partialStart->parentNode();
    }

    // what is the highest node that partially selects the end of the range?
    NodeImpl *partialEnd = 0;
    if(m_endContainer != cmnRoot)
    {
        partialEnd = m_endContainer;
        while(partialEnd->parentNode() != cmnRoot)
            partialEnd = partialEnd->parentNode();
    }

    DocumentFragmentImpl *fragment = 0;
    if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
        fragment = new DocumentFragmentImpl(m_ownerDocument);
    
    // Simple case: the start and end containers are the same. We just grab
    // everything >= start offset and < end offset
    if(m_startContainer == m_endContainer)
    {
        if(m_startContainer->nodeType() == TEXT_NODE ||
           m_startContainer->nodeType() == CDATA_SECTION_NODE ||
           m_startContainer->nodeType() == COMMENT_NODE)
        {
            if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
            {
                CharacterDataImpl *c = static_cast<CharacterDataImpl*>(m_startContainer->cloneNode(true));
                c->deleteData(m_endOffset,static_cast<CharacterDataImpl*>(m_startContainer)->length()-m_endOffset);
                c->deleteData(0,m_startOffset);
                fragment->appendChild(c);
            }
            
            if(action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
                static_cast<CharacterDataImpl*>(m_startContainer)->deleteData(m_startOffset,m_endOffset-m_startOffset);
        }
        else if(m_startContainer->nodeType() == PROCESSING_INSTRUCTION_NODE)
        {
            // ### operate just on data ?
        }
        else
        {
            NodeImpl *n = m_startContainer->firstChild();
            unsigned long i;
            for(i = 0; i < m_startOffset; i++) // skip until m_startOffset
                n = n->nextSibling();
            
            while(n && i < m_endOffset) // delete until m_endOffset
            {
                NodeImpl *next = n->nextSibling();
                if(action == EXTRACT_CONTENTS)
                    fragment->appendChild(n); // will remove n from its parent
                else if(action == CLONE_CONTENTS)
                    fragment->appendChild(n->cloneNode(true));
                else
                    m_startContainer->removeChild(n);
                
                n = next;
                i++;
            }
        }
        
        collapse(true);
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
    if(m_startContainer != cmnRoot)
    {
        // process the left-hand side of the range, up until the last ancestor of
        // m_startContainer before cmnRoot
        if(m_startContainer->nodeType() == TEXT_NODE ||
           m_startContainer->nodeType() == CDATA_SECTION_NODE ||
           m_startContainer->nodeType() == COMMENT_NODE)
        {
            if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
            {
                CharacterDataImpl *c = static_cast<CharacterDataImpl *>(m_startContainer->cloneNode(true));
                c->deleteData(0,m_startOffset);
                leftContents = c;
            }
            
            if(action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
            {
                static_cast<CharacterDataImpl *>(m_startContainer)->deleteData(m_startOffset,
                static_cast<CharacterDataImpl *>(m_startContainer)->length() - m_startOffset);
            }
            else if(m_startContainer->nodeType() == PROCESSING_INSTRUCTION_NODE)
            {
                // TODO: operate just on data ?
                // leftContents = ...
            }
            else
            {
                if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
                    leftContents = m_startContainer->cloneNode(false);
    
                NodeImpl *n = m_startContainer->firstChild();
                unsigned long i;
                for(i = 0; i < m_startOffset; i++) // skip until m_startOffset
                    n = n->nextSibling();
                
                while(n) // process until end
                {
                    NodeImpl *next = n->nextSibling();
                    if(action == EXTRACT_CONTENTS)
                        leftContents->appendChild(n); // will remove n from m_startContainer
                    else if(action == CLONE_CONTENTS)
                        leftContents->appendChild(n->cloneNode(true));
                    else
                        m_startContainer->removeChild(n);
                    
                    n = next;
                }
            }

            NodeImpl *leftParent = m_startContainer->parentNode();
            NodeImpl *n = m_startContainer->nextSibling();
            for(; leftParent != cmnRoot; leftParent = leftParent->parentNode())
            {
                if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
                {
                    NodeImpl *leftContentsParent = leftParent->cloneNode(false);
                    leftContentsParent->appendChild(leftContents);
                    leftContents = leftContentsParent;
                }
                
                NodeImpl *next;
                for(; n != 0; n = next)
                {
                    next = n->nextSibling();
                    if(action == EXTRACT_CONTENTS)
                        leftContents->appendChild(n); // will remove n from leftParent
                    else if(action == CLONE_CONTENTS)
                        leftContents->appendChild(n->cloneNode(true));
                    else
                        leftParent->removeChild(n);
                }
        
                n = leftParent->nextSibling();
            }
        }
    }

    NodeImpl *rightContents = 0;
    if(m_endContainer != cmnRoot)
    {
        // delete the right-hand side of the range, up until the last ancestor of
        // m_endContainer before cmnRoot
        if(m_endContainer->nodeType() == TEXT_NODE ||
           m_endContainer->nodeType() == CDATA_SECTION_NODE ||
           m_endContainer->nodeType() == COMMENT_NODE)
        {
            if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
            {
                CharacterDataImpl *c = static_cast<CharacterDataImpl *>(m_endContainer->cloneNode(true));
                c->deleteData(m_endOffset,static_cast<CharacterDataImpl *>(m_endContainer)->length() - m_endOffset);
                rightContents = c;
            }

            if(action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
                static_cast<CharacterDataImpl *>(m_endContainer)->deleteData(0, m_endOffset);
        }
        else if(m_startContainer->nodeType() == PROCESSING_INSTRUCTION_NODE)
        {
            // TODO: operate just on data ?
            // rightContents = ...
        }
        else
        {
            if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
                rightContents = m_endContainer->cloneNode(false);
            
            NodeImpl *n = m_endContainer->firstChild();
            unsigned long i;
            for(i = 0; i + 1 < m_endOffset; i++) // skip to m_endOffset
                n = n->nextSibling();
    
            NodeImpl *prev;
            for(; n != 0; n = prev)
            {
                prev = n->previousSibling();
                
                if(action == EXTRACT_CONTENTS)
                    rightContents->insertBefore(n, rightContents->firstChild()); // will remove n from its parent
                else if(action == CLONE_CONTENTS)
                    rightContents->insertBefore(n->cloneNode(true),rightContents->firstChild());
                else
                    m_endContainer->removeChild(n);
            }
        }

        NodeImpl *rightParent = m_endContainer->parentNode();
        NodeImpl *n = m_endContainer->previousSibling();
        for(; rightParent != cmnRoot; rightParent = rightParent->parentNode())
        {
            if(action == EXTRACT_CONTENTS || action == CLONE_CONTENTS)
            {
                NodeImpl *rightContentsParent = rightParent->cloneNode(false);
                rightContentsParent->appendChild(rightContents);
                rightContents = rightContentsParent;
            }

            NodeImpl *prev;
            for(; n != 0; n = prev)
            {
                prev = n->previousSibling();
                if(action == EXTRACT_CONTENTS)
                    rightContents->insertBefore(n, rightContents->firstChild()); // will remove n from its parent
                else if(action == CLONE_CONTENTS)
                    rightContents->insertBefore(n->cloneNode(true), rightContents->firstChild());
                else
                    rightParent->removeChild(n);
            }
    
            n = rightParent->previousSibling();
        }
    }

    // delete all children of cmnRoot between the start and end container
    NodeImpl *processStart; // child of cmnRooot
    if(m_startContainer == cmnRoot)
    {
        unsigned long i;
        processStart = m_startContainer->firstChild();
        for(i = 0; i < m_startOffset; i++)
            processStart = processStart->nextSibling();
    }
    else
    {
        processStart = m_startContainer;
        while(processStart->parentNode() != cmnRoot)
            processStart = processStart->parentNode();
        
        processStart = processStart->nextSibling();
    }
    
    NodeImpl *processEnd; // child of cmnRooot
    if(m_endContainer == cmnRoot)
    {
        unsigned long i;
        processEnd = m_endContainer->firstChild();
        for(i = 0; i < m_endOffset; i++)
            processEnd = processEnd->nextSibling();
    }
    else
    {
        processEnd = m_endContainer;
        while(processEnd->parentNode() != cmnRoot)
            processEnd = processEnd->parentNode();
    }

    // Now add leftContents, stuff in between, and rightContents to the fragment
    // (or just delete the stuff in between)
    if((action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) && leftContents)
        fragment->appendChild(leftContents);

    NodeImpl *next;
    NodeImpl *n;
    if(processStart)
    {
        for(n = processStart; n && n != processEnd; n = next)
        {
            next = n->nextSibling();
            if(action == EXTRACT_CONTENTS)
                fragment->appendChild(n); // will remove from cmnRoot
            else if(action == CLONE_CONTENTS)
                fragment->appendChild(n->cloneNode(true));
            else
                cmnRoot->removeChild(n);
        }
    }

    if((action == EXTRACT_CONTENTS || action == CLONE_CONTENTS) && rightContents)
        fragment->appendChild(rightContents);

    // collapse to the proper position - see spec section 2.6
    if(action == EXTRACT_CONTENTS || action == DELETE_CONTENTS)
    {
        if(!partialStart && !partialEnd)
            collapse(true);
        else if(partialStart)
        {
            setStartContainer(partialStart->parentNode());
            setEndContainer(partialStart->parentNode());
            m_startOffset = m_endOffset = partialStart->nodeIndex() + 1;
        }
        else if(partialEnd)
        {
            setStartContainer(partialEnd->parentNode());
            setEndContainer(partialEnd->parentNode());
            m_startOffset = m_endOffset = partialEnd->nodeIndex();
        }
    }
    
    return fragment;
}

DocumentFragmentImpl *RangeImpl::extractContents()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    checkDeleteExtract();
    return processContents(EXTRACT_CONTENTS);
}

DocumentFragmentImpl *RangeImpl::cloneContents()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    return processContents(CLONE_CONTENTS);
}

void RangeImpl::insertNode(NodeImpl *newNode)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    // NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container
    // of either boundary-point of the Range is read-only.
    NodeImpl *n = m_startContainer;
    while(n && !n->isReadOnly())
        n = n->parentNode();

    if(n)
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    n = m_endContainer;
    while(n && !n->isReadOnly())
        n = n->parentNode();
    
    if(n)
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    // WRONG_DOCUMENT_ERR: Raised if newParent and the container of the start of the Range were
    // not created from the same document.
    if(newNode->ownerDocument() != m_startContainer->ownerDocument())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    // HIERARCHY_REQUEST_ERR: Raised if the container of the start of the Range is of a type that
    // does not allow children of the type of newNode or if newNode is an ancestor of the container.

    // an extra one here - if a text node is going to split, it must have a parent to insert into
    if(m_startContainer->nodeType() == TEXT_NODE && !m_startContainer->parentNode())
        throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);

    // In the case where the container is a text node, we check against the container's parent, because
    // text nodes get split up upon insertion.
    NodeImpl *checkAgainst;
    if(m_startContainer->nodeType() == TEXT_NODE)
        checkAgainst = m_startContainer->parentNode();
    else
        checkAgainst = m_startContainer;

    if(newNode->nodeType() == DOCUMENT_FRAGMENT_NODE)
    {
        // check each child node, not the DocumentFragment itself
        NodeImpl *c;
        for(c = newNode->firstChild(); c != 0; c = c->nextSibling())
        {
            if(!checkAgainst->childTypeAllowed(c->nodeType()))
                throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
        }
    }
    else
    {
        if(!checkAgainst->childTypeAllowed(newNode->nodeType()))
            throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
    }

    for(n = m_startContainer; n; n = n->parentNode())
    {
        if(n == newNode)
            throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
    }

    // INVALID_NODE_TYPE_ERR: Raised if newNode is an Attr, Entity, Notation, or Document node.
    if(newNode->nodeType() == ATTRIBUTE_NODE ||
       newNode->nodeType() == ENTITY_NODE ||
       newNode->nodeType() == NOTATION_NODE ||
       newNode->nodeType() == DOCUMENT_NODE)
        throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);

    if(m_startContainer->nodeType() == TEXT_NODE ||
       m_startContainer->nodeType() == CDATA_SECTION_NODE)
    {
        TextImpl *newText = static_cast<TextImpl *>(m_startContainer)->splitText(m_startOffset);
        m_startContainer->parentNode()->insertBefore(newNode, newText);
    }
    else
        m_startContainer->insertBefore(newNode, m_startContainer->childNodes()->item(m_startOffset));
}

void RangeImpl::detach()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(m_startContainer)
        m_startContainer->deref();

    m_startContainer = 0;
    if(m_endContainer)
        m_endContainer->deref();
    
    m_endContainer = 0;
    m_detached = true;
}

bool RangeImpl::isDetached() const
{
    return m_detached;
}

void RangeImpl::checkNodeWOffset( NodeImpl *n, int offset ) const
{
    if(offset < 0)
        throw new DOMExceptionImpl(INDEX_SIZE_ERR);

    switch(n->nodeType())
    {
        case ENTITY_NODE:
        case NOTATION_NODE:
        case DOCUMENT_TYPE_NODE:
        {
            throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);
            break;
        }
        case TEXT_NODE:
        case COMMENT_NODE:
        case CDATA_SECTION_NODE:
        {
            if((unsigned long) offset > static_cast<CharacterDataImpl*>(n)->length())
                throw new DOMExceptionImpl(INDEX_SIZE_ERR);
            
            break;
        }
        case PROCESSING_INSTRUCTION_NODE:
        {
            // TODO: are we supposed to check with just data or the whole contents?
            if((unsigned long) offset > DOMString(static_cast<ProcessingInstructionImpl*>(n)->data()).length())
                throw new DOMExceptionImpl(INDEX_SIZE_ERR);
                
            break;
        }
        default:
        {
            if((unsigned long) offset > n->childNodes()->length())
                throw new DOMExceptionImpl(INDEX_SIZE_ERR);

            break;
        }
    }
}

void RangeImpl::checkNodeBA(NodeImpl *n) const
{
    // INVALID_NODE_TYPE_ERR: Raised if the root container of refNode is not an
    // Attr, Document or DocumentFragment node or if refNode is a Document,
    // DocumentFragment, Attr, Entity, or Notation node.
    NodeImpl *root = n;
    while(root->parentNode())
        root = root->parentNode();
    
    if(!(root->nodeType() == ATTRIBUTE_NODE ||
         root->nodeType() == DOCUMENT_NODE ||
         root->nodeType() == DOCUMENT_FRAGMENT_NODE))
        throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);

    if(n->nodeType() == DOCUMENT_NODE ||
       n->nodeType() == DOCUMENT_FRAGMENT_NODE ||
       n->nodeType() == ATTRIBUTE_NODE ||
       n->nodeType() == ENTITY_NODE ||
       n->nodeType() == NOTATION_NODE)
        throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);
}

RangeImpl *RangeImpl::cloneRange()
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    return new RangeImpl(m_ownerDocument, m_startContainer, m_startOffset, m_endContainer, m_endOffset);
}

DOMStringImpl *RangeImpl::toString()
{
    /* This function converts a dom range to the plain text string that the user would see in this
     * portion of rendered html.
     *
     * There are several ways ranges can be used.
     *
     * The simplest is the start and endContainer is a text node.  The start and end offset is the
     * number of characters into the text to remove/truncate.
     *
     * The next case is the start and endContainer is, well, a container, such a P tag or DIV tag.
     * In this case the start and end offset is the number of children into the container to start
     * from and end at.
     *
     * The other cases are different arrangements of the first two.
     *
     * psuedo code:
     *
     * if start container is not text:
     *     count through the children to find where we start (m_startOffset children)
     *
     * loop from the start position:
     *     if the current node is text, add the text to our variable 'text', truncating/removing if at the end/start.
     *     
     *     if the node has children, step to the first child.
     *     if the node has no children but does have siblings, step to the next sibling
     *     until we find a sibling, go to next the parent but:
     *         make sure this sibling isn't past the end of where we are supposed to go. (position > endOffset and the parent is the endContainer)
     *         
     */
    
    if( m_startContainer == m_endContainer && m_startOffset >= m_endOffset)
        return 0;

    DOMStringImpl *text = new DOMStringImpl();;

    bool skipNode = false;
    bool endAfter = false;
    
    NodeImpl *n = m_startContainer;
    if(n->firstChild()) {
        if(m_startOffset >= n->childNodes()->length()) {
            n = n->childNodes()->item(n->childNodes()->length() - 1);
            skipNode = true;
        }
        else
            n = n->childNodes()->item(m_startOffset);
    }
    
    // n is now our starting point.  If skipNode is true then we don't print this node
    
    NodeImpl *endNode = m_endContainer;
    if(endNode->firstChild()) {
        if( m_endOffset >= endNode->childNodes()->length()) {
            endNode = endNode->childNodes()->item(endNode->childNodes()->length() - 1);
            endAfter = true;
        }
        else
            endNode = endNode->childNodes()->item(m_endOffset);
    } else
        endAfter = true;
    
    // endNode is now our end point.  If endAfter is true then we print this node

    while(n) {
        if(n == endNode && !endAfter) break;

        NodeImpl *next = NULL;
        if(!skipNode) {
            if(n->nodeType() == TEXT_NODE ||
               n->nodeType() == CDATA_SECTION_NODE) {
    
                DOMStringImpl *str = static_cast<TextImpl *>(n)->toString();
                if(n == m_endContainer || n == m_startContainer)
                    str = str->copy();  //copy if we are going to modify.
    
                if (n == m_endContainer)
                    str->truncate(m_endOffset);
                if (n == m_startContainer)
                    str->remove(0,m_startOffset);
                text->append(str);
                if (n == endNode)
                    break;
            } else 
                next = n->firstChild();
        } 
        skipNode = false;
        
        if(!next) {
            if(n == endNode) //No children, and since we are at an end node, our work here is done
                break;
            next = n->nextSibling();
        }
        
        while( !next && n->parentNode() ) {
            n = n->parentNode();
            if(n == endNode) return text;
            next = n->nextSibling();
            //If this sibling is the end it will be caught when we go round the loop
        }

        n = next;
    }
    return text;
}


void RangeImpl::setStartAfter(NodeImpl *refNode)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    if(refNode->ownerDocument() != m_ownerDocument->document())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    checkNodeBA(refNode);
    setStart(refNode->parentNode(), refNode->nodeIndex() + 1);
}

void RangeImpl::setEndBefore(NodeImpl *refNode)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    if(refNode->ownerDocument() != m_ownerDocument->document())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    checkNodeBA(refNode);
    setEnd(refNode->parentNode(), refNode->nodeIndex());
}

void RangeImpl::setEndAfter(NodeImpl *refNode)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode) 
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    if(refNode->ownerDocument() != m_ownerDocument->document())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    checkNodeBA(refNode);
    setEnd(refNode->parentNode(), refNode->nodeIndex() + 1);
}

void RangeImpl::selectNode(NodeImpl *refNode)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode) 
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    // INVALID_NODE_TYPE_ERR: Raised if an ancestor of refNode is an Entity,
    // Notation or DocumentType node or if refNode is a Document,
    // DocumentFragment, Attr, Entity, or Notation node.
    NodeImpl *anc;
    for(anc = refNode->parentNode(); anc != 0; anc = anc->parentNode())
    {
        if(anc->nodeType() == ENTITY_NODE ||
           anc->nodeType() == NOTATION_NODE ||
           anc->nodeType() == DOCUMENT_TYPE_NODE)
            throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);
    }

    if(refNode->nodeType() == DOCUMENT_NODE ||
       refNode->nodeType() == DOCUMENT_FRAGMENT_NODE ||
       refNode->nodeType() == ATTRIBUTE_NODE ||
       refNode->nodeType() == ENTITY_NODE ||
       refNode->nodeType() == NOTATION_NODE)
        throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);

    setStartBefore(refNode);
    setEndAfter(refNode);
}

void RangeImpl::selectNodeContents(NodeImpl *refNode)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    // INVALID_NODE_TYPE_ERR: Raised if refNode or an ancestor of refNode
    // is an Entity, Notation or DocumentType node.
    NodeImpl *n;
    for(n = refNode; n != 0; n = n->parentNode())
    {
        if(n->nodeType() == ENTITY_NODE ||
           n->nodeType() == NOTATION_NODE ||
           n->nodeType() == DOCUMENT_TYPE_NODE)
            throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);
    }

    setStartContainer(refNode);
    m_startOffset = 0;
    
    setEndContainer(refNode);    
    m_endOffset = refNode->childNodes()->length();
}

void RangeImpl::surroundContents(NodeImpl *newParent)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!newParent)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    // INVALID_NODE_TYPE_ERR: Raised if node is an Attr, Entity,
    // DocumentType, Notation, Document, or DocumentFragment node.
    if(newParent->nodeType() == ATTRIBUTE_NODE ||
       newParent->nodeType() == ENTITY_NODE ||
       newParent->nodeType() == NOTATION_NODE ||
       newParent->nodeType() == DOCUMENT_TYPE_NODE ||
       newParent->nodeType() == DOCUMENT_NODE ||
       newParent->nodeType() == DOCUMENT_FRAGMENT_NODE)
        throw new RangeExceptionImpl(INVALID_NODE_TYPE_ERR);

    // NO_MODIFICATION_ALLOWED_ERR: Raised if an ancestor container of
    // either boundary-point of the Range is read-only.
    if(readOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    NodeImpl *n = m_startContainer;
    while(n && !n->isReadOnly())
        n = n->parentNode();
    
    if(n)
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    n = m_endContainer;
    while(n && !n->isReadOnly())
        n = n->parentNode();
    
    if(n)
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);

    // WRONG_DOCUMENT_ERR: Raised if newParent and the container of
    // the start of the Range were not created from the same document.
    if(newParent->ownerDocument() != m_startContainer->ownerDocument())
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    // HIERARCHY_REQUEST_ERR: Raised if the container of the start of the Range is of a type that
    // does not allow children of the type of newParent or if newParent is an ancestor of the container
    /// or if node would end up with a child node of a type not allowed by the type of node.
    if(!m_startContainer->childTypeAllowed(newParent->nodeType()))
        throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);

    for(n = m_startContainer; n != 0; n = n->parentNode())
    {
        if(n == newParent)
            throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
    }

    // ### check if node would end up with a child node of a type not allowed by the type of node

    // BAD_BOUNDARYPOINTS_ERR: Raised if the Range partially selects a non-text node.
    if(m_startContainer->nodeType() != TEXT_NODE &&
       m_startContainer->nodeType() != COMMENT_NODE &&
       m_startContainer->nodeType() != CDATA_SECTION_NODE &&
       m_startContainer->nodeType() != PROCESSING_INSTRUCTION_NODE)
    {
        if(m_startOffset > 0 && m_startOffset < m_startContainer->childNodes()->length())
            throw new RangeExceptionImpl(BAD_BOUNDARYPOINTS_ERR);
    }
    
    if(m_endContainer->nodeType() != TEXT_NODE &&
       m_endContainer->nodeType() != COMMENT_NODE &&
       m_endContainer->nodeType() != CDATA_SECTION_NODE &&
       m_endContainer->nodeType() != PROCESSING_INSTRUCTION_NODE)
    {
        if(m_endOffset > 0 && m_endOffset < m_endContainer->childNodes()->length())
            throw new RangeExceptionImpl(BAD_BOUNDARYPOINTS_ERR);
    }

    while(newParent->firstChild())
        newParent->removeChild(newParent->firstChild());

    DocumentFragmentImpl *fragment = extractContents();
    insertNode(newParent);
    
    newParent->appendChild(fragment);
    selectNode(newParent);
}

void RangeImpl::setStartBefore(NodeImpl *refNode)
{
    if(m_detached)
        throw new DOMExceptionImpl(INVALID_STATE_ERR);

    if(!refNode)
        throw new DOMExceptionImpl(NOT_FOUND_ERR);

    if(refNode->ownerDocument() != m_ownerDocument->document()) 
        throw new DOMExceptionImpl(WRONG_DOCUMENT_ERR);

    checkNodeBA(refNode);

    setStart(refNode->parentNode(), refNode->nodeIndex());
}

void RangeImpl::setStartContainer(NodeImpl *_startContainer)
{
    KDOM_SAFE_SET(m_startContainer, _startContainer);
}

void RangeImpl::setEndContainer(NodeImpl *_endContainer)
{
    KDOM_SAFE_SET(m_endContainer, _endContainer);
}

void RangeImpl::checkDeleteExtract()
{
    NodeImpl *start;
    if(m_startContainer->nodeType() != TEXT_NODE &&
       m_startContainer->nodeType() != CDATA_SECTION_NODE &&
       m_startContainer->nodeType() != COMMENT_NODE &&
       m_startContainer->nodeType() != PROCESSING_INSTRUCTION_NODE)
    {
        start = m_startContainer->childNodes()->item(m_startOffset);
        if(!start)
        {
            if(m_startContainer->lastChild())
                start = m_startContainer->lastChild()->traverseNextNode();
            else
                start = m_startContainer->traverseNextNode();
        }
    }
    else
        start = m_startContainer;

    NodeImpl *end;
    if(m_endContainer->nodeType() != TEXT_NODE &&
       m_endContainer->nodeType() != CDATA_SECTION_NODE &&
       m_endContainer->nodeType() != COMMENT_NODE &&
       m_endContainer->nodeType() != PROCESSING_INSTRUCTION_NODE)
    {
        end = m_endContainer->childNodes()->item(m_endOffset);
        if(!end)
        {
            if(m_endContainer->lastChild())
                end = m_endContainer->lastChild()->traverseNextNode();
            else
                end = m_endContainer->traverseNextNode();
        }
    }
    else
        end = m_endContainer;

    NodeImpl *n;
    for(n = start; n != end; n = n->traverseNextNode())
    {
        if(n->isReadOnly())
            throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
        
        if(n->nodeType() == DOCUMENT_TYPE_NODE) // ### is this for only directly under the DF, or anywhere?
            throw new DOMExceptionImpl(HIERARCHY_REQUEST_ERR);
    }    
    
    if(containedByReadOnly())
        throw new DOMExceptionImpl(NO_MODIFICATION_ALLOWED_ERR);
}

bool RangeImpl::containedByReadOnly()
{
    NodeImpl *n;
    for(n = m_startContainer; n != 0; n = n->parentNode())
    {
        if(n->isReadOnly())
            return true;
    }
    
    for(n = m_endContainer; n != 0; n = n->parentNode())
    {
        if(n->isReadOnly())
            return true;
    }

    return false;
}

// vim:ts=4:noet

