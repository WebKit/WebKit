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

#ifndef _PositionIterator_h_
#define _PositionIterator_h_

#include "dom_position.h"

namespace DOM {

class NodeImpl;
class Position;

class PositionIterator
{
public:
    PositionIterator() : m_current() {}
    PositionIterator(NodeImpl *node, long offset) : m_current(node, offset) {}
    PositionIterator(const Position &o) : m_current(o) {}

    Position current() const { return m_current; }
    Position previous() { return m_current = peekPrevious(); }
    Position next() { return m_current = peekNext(); }
    Position peekPrevious() const;
    Position peekNext() const;

    void setPosition(const Position &pos) { m_current = pos; }

    bool atStart() const;
    bool atEnd() const;
    bool isEmpty() const { return m_current.isNull(); }

private:
    Position m_current;
};

} // namespace DOM

#endif // _PositionIterator_h_
