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

#ifndef KHTML_EDITING_VISIBLE_POSITION_H
#define KHTML_EDITING_VISIBLE_POSITION_H

#include "xml/dom_position.h"

namespace DOM {

class Range;
class RangeImpl;

class CaretPosition
{
public:
    CaretPosition() { }
    CaretPosition(NodeImpl *, long offset);
    CaretPosition(const Position &);

    void clear() { m_deepPosition.clear(); }

    bool isNull() const { return m_deepPosition.isNull(); }
    bool isNotNull() const { return m_deepPosition.isNotNull(); }

    Position position() const { return rangeCompliantEquivalent(m_deepPosition); }
    Position deepEquivalent() const { return m_deepPosition; }

    friend inline bool operator==(const CaretPosition &a, const CaretPosition &b);

    CaretPosition next() const;
    CaretPosition previous() const;

    bool isLastInBlock() const;

    void debugPosition(const char *msg = "") const;

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
#endif
    
private:
    void init(const Position &);

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
    
    Position m_deepPosition;
};

inline bool operator==(const CaretPosition &a, const CaretPosition &b)
{
    return a.m_deepPosition == b.m_deepPosition;
}

inline bool operator!=(const CaretPosition &a, const CaretPosition &b)
{
    return !(a == b);
}

Range makeRange(const CaretPosition &start, const CaretPosition &end);
bool setStart(Range &, const CaretPosition &start);
bool setStart(RangeImpl *, const CaretPosition &start);
bool setEnd(Range &, const CaretPosition &start);
bool setEnd(RangeImpl *, const CaretPosition &start);
CaretPosition start(const Range &);
CaretPosition start(const RangeImpl *);
CaretPosition end(const Range &);
CaretPosition end(const RangeImpl *);

} // namespace DOM

#endif // KHTML_EDITING_VISIBLE_POSITION_H
