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

#ifndef __dom_caretposition_h__
#define __dom_caretposition_h__

#include "dom_position.h"

namespace DOM {

class NodeImpl;
class Position;

class CaretPosition
{
public:
    CaretPosition() : m_node(0), m_offset(0) {}
    CaretPosition(NodeImpl *, long offset);
    explicit CaretPosition(const Position &);
    CaretPosition(const CaretPosition &);
    ~CaretPosition();

    CaretPosition &operator=(const CaretPosition &o);

    bool isLastInBlock() const;

    CaretPosition next() const;
    CaretPosition previous() const;

    bool isEmpty() const { return m_node == 0; }
    bool notEmpty() const { return m_node != 0; }

    Position position() const { return rangeCompliantEquivalent(*this); }
    Position deepEquivalent() const { return Position(node(), offset()); }

    friend inline bool operator==(const CaretPosition &a, const CaretPosition &b);
    friend inline bool operator!=(const CaretPosition &a, const CaretPosition &b);

    void debugPosition(const char *msg="") const;

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
#endif
    
private:
    operator Position() const { return Position(node(), offset()); }

    void init(const Position &);
    void setPosition(const Position &);

    NodeImpl *node() const { return m_node; }
    long offset() const { return m_offset; }

    static Position deepEquivalent(const Position &);
    static Position rangeCompliantEquivalent(const Position &);

    static long maxOffset(const NodeImpl *);
    static bool isAtomicNode(const NodeImpl *);
    
    static Position previousCaretPosition(const Position &);
    static Position nextCaretPosition(const Position &);

    static Position previousPosition(const Position &);
    static Position nextPosition(const Position &);

    static bool atStart(const Position &);
    static bool atEnd(const Position &);

    static bool isCandidate(const Position &);
    
    NodeImpl *m_node;
    long m_offset;
};

inline bool operator==(const CaretPosition &a, const CaretPosition &b)
{
    return a.node() == b.node() && a.offset() == b.offset();
}

inline bool operator!=(const CaretPosition &a, const CaretPosition &b)
{
    return !(a == b);
}

} // namespace DOM

#endif // __dom_caretposition_h__