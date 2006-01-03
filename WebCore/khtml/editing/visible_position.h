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

#include <qstring.h> // for QChar
#include <kxmlcore/PassRefPtr.h>

#include "xml/dom_position.h"
#include "text_affinity.h"
#include "misc/shared.h"

namespace DOM {
    class NodeImpl;
    class RangeImpl;
}

namespace khtml {

// VisiblePosition default affinity is downstream because
// the callers do not really care (they just want the
// deep position without regard to line position), and this
// is cheaper than UPSTREAM
#define VP_DEFAULT_AFFINITY DOWNSTREAM

// Callers who do not know where on the line the position is,
// but would like UPSTREAM if at a line break or DOWNSTREAM
// otherwise, need a clear way to specify that.  The
// constructors auto-correct UPSTREAM to DOWNSTREAM if the
// position is not at a line break.
#define VP_UPSTREAM_IF_POSSIBLE UPSTREAM

class VisiblePosition
{
public:
    typedef DOM::NodeImpl NodeImpl;
    typedef DOM::Position Position;

    // NOTE: UPSTREAM affinity will be used only if pos is at end of a wrapped line,
    // otherwise it will be converted to DOWNSTREAM
    VisiblePosition() { m_affinity = VP_DEFAULT_AFFINITY; };
    VisiblePosition(NodeImpl *, int offset, EAffinity);
    VisiblePosition(const Position &pos, EAffinity affinity = VP_DEFAULT_AFFINITY);
    VisiblePosition(const VisiblePosition &);

    void clear() { m_deepPosition.clear(); }

    bool isNull() const { return m_deepPosition.isNull(); }
    bool isNotNull() const { return m_deepPosition.isNotNull(); }

    Position position() const { return rangeCompliantEquivalent(m_deepPosition); }
    Position deepEquivalent() const { return m_deepPosition; }
    EAffinity affinity() const { assert(m_affinity == UPSTREAM || m_affinity == DOWNSTREAM); return m_affinity; }
    void setAffinity(EAffinity affinity) { m_affinity = affinity; }
    
    Position downstreamDeepEquivalent() const;

    friend bool operator==(const VisiblePosition &a, const VisiblePosition &b);
    friend bool operator!=(const VisiblePosition &a, const VisiblePosition &b);

    friend bool isEqualIgnoringAffinity(const VisiblePosition &a, const VisiblePosition &b);
    friend bool isNotEqualIgnoringAffinity(const VisiblePosition &a, const VisiblePosition &b);

    // next() and previous() will increment/decrement by a character cluster.
    VisiblePosition next() const;
    VisiblePosition previous() const;

    bool isLastInBlock() const;

    QChar character() const;
    
    void debugPosition(const char *msg = "") const;

#ifndef NDEBUG
    void formatForDebugger(char *buffer, unsigned length) const;
    void showTree() const;
#endif
    
private:
    void init(const Position &, EAffinity);
    void initUpstream(const Position &);
    void initDownstream(const Position &);

    static Position deepEquivalent(const Position &);
    static Position rangeCompliantEquivalent(const Position &);

    static int maxOffset(const NodeImpl *);
    static bool isAtomicNode(const NodeImpl *);
    
    static Position previousVisiblePosition(const Position &);
    static Position nextVisiblePosition(const Position &);

    static bool isCandidate(const Position &);
        
    Position m_deepPosition;
    EAffinity m_affinity;
};

inline bool operator==(const VisiblePosition &a, const VisiblePosition &b)
{
    return a.m_deepPosition == b.m_deepPosition && a.m_affinity == b.m_affinity;
}
 
inline bool operator!=(const VisiblePosition &a, const VisiblePosition &b)
{
    return !(a == b);
}

PassRefPtr<DOM::RangeImpl> makeRange(const VisiblePosition &start, const VisiblePosition &end);
bool setStart(DOM::RangeImpl *, const VisiblePosition &start);
bool setEnd(DOM::RangeImpl *, const VisiblePosition &start);
VisiblePosition startVisiblePosition(const DOM::RangeImpl *, EAffinity);
VisiblePosition endVisiblePosition(const DOM::RangeImpl *, EAffinity);

DOM::NodeImpl *enclosingBlockFlowElement(const VisiblePosition &);

bool isFirstVisiblePositionInNode(const VisiblePosition &, const DOM::NodeImpl *);
bool isLastVisiblePositionInNode(const VisiblePosition &, const DOM::NodeImpl *);

} // namespace khtml

#endif // KHTML_EDITING_VISIBLE_POSITION_H
