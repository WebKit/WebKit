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

#include "dom_positioniterator.h"

#include "dom_nodeimpl.h"

namespace DOM {

Position PositionIterator::peekPrevious() const
{
    Position pos = m_current;
    
    if (pos.isNull())
        return Position();
    
    if (pos.offset() <= 0) {
        NodeImpl *prevNode = pos.node()->traversePreviousNode();
        if (prevNode)
            pos = Position(prevNode, prevNode->maxOffset());
    }
    else {
        pos = Position(pos.node(), pos.offset() - 1);
    }
    
    return pos;
}

Position PositionIterator::peekNext() const
{
    Position pos = m_current;
    
    if (pos.isNull())
        return Position();
    
    if (pos.offset() >= pos.node()->maxOffset()) {
        NodeImpl *nextNode = pos.node()->traverseNextNode();
        if (nextNode)
            pos = Position(nextNode, 0);
    }
    else {
        pos = Position(pos.node(), pos.offset() + 1);
    }
    
    return pos;
}

bool PositionIterator::atStart() const
{
    if (m_current.isNull())
        return true;

    return m_current.offset() == 0 && 
        m_current.node()->traversePreviousNode() == 0;
}

bool PositionIterator::atEnd() const
{
    if (m_current.isNull())
        return true;

    return m_current.offset() >= m_current.node()->maxOffset() && 
        m_current.node()->traverseNextNode() == 0;
}

} // namespace DOM
