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

#ifndef KHTML_EDITING_VISIBLE_RANGE_H
#define KHTML_EDITING_VISIBLE_RANGE_H

#include "visible_position.h"

namespace khtml {

class VisibleRange
{
public:
    typedef DOM::NodeImpl NodeImpl;
    typedef DOM::Position Position;
    typedef DOM::RangeImpl RangeImpl;

    VisibleRange() { }
    VisibleRange(NodeImpl *startContainer, int startOffset, NodeImpl *endContainer, int endOffset);
    VisibleRange(const VisiblePosition &);
    VisibleRange(const VisiblePosition &, const VisiblePosition &);
    VisibleRange(const RangeImpl *);
    VisibleRange(const Position &);
    VisibleRange(const Position &, const Position &);

    VisibleRange &operator=(const VisiblePosition &);
    VisibleRange &operator=(const RangeImpl *);
    VisibleRange &operator=(const Position &);

    VisiblePosition start() const { return m_start; }
    VisiblePosition end() const { return m_end; }

    SharedPtr<RangeImpl> range() const;

    void clear() { m_start.clear(); m_end.clear(); }

    bool isNull() const { return m_start.isNull(); }
    bool isCollapsed() const { return m_start == m_end; }

    friend inline bool operator==(const VisibleRange &a, const VisibleRange &b);

private:
    VisiblePosition m_start;
    VisiblePosition m_end;
};

inline bool operator==(const VisibleRange &a, const VisibleRange &b)
{
    return a.start() == b.start() && a.end() == b.end();
}

inline bool operator!=(const VisibleRange &a, const VisibleRange &b)
{
    return !(a == b);
}

} // namespace khtml

#endif
