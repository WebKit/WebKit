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

#include "Position.h"

namespace WebCore {

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

class VisiblePosition {
public:
    // NOTE: UPSTREAM affinity will be used only if pos is at end of a wrapped line,
    // otherwise it will be converted to DOWNSTREAM
    VisiblePosition() : m_affinity(VP_DEFAULT_AFFINITY) { }
    VisiblePosition(Node*, int offset, EAffinity);
    VisiblePosition(const Position&, EAffinity = VP_DEFAULT_AFFINITY);

    void clear() { m_deepPosition.clear(); }

    bool isNull() const { return m_deepPosition.isNull(); }
    bool isNotNull() const { return m_deepPosition.isNotNull(); }

    Position deepEquivalent() const { return m_deepPosition; }
    EAffinity affinity() const { assert(m_affinity == UPSTREAM || m_affinity == DOWNSTREAM); return m_affinity; }
    void setAffinity(EAffinity affinity) { m_affinity = affinity; }

    // next() and previous() will increment/decrement by a character cluster.
    VisiblePosition next() const;
    VisiblePosition previous() const;

    bool isLastInBlock() const;

    QChar characterAfter() const;
    
    void debugPosition(const char* msg = "") const;
    
    static Position deepEquivalent(const Position&);
    
    Element* rootEditableElement() const { return m_deepPosition.isNotNull() ? m_deepPosition.node()->rootEditableElement() : 0; }

#ifndef NDEBUG
    void formatForDebugger(char* buffer, unsigned length) const;
    void showTreeForThis() const;
#endif
    
private:
    void init(const Position&, EAffinity);
    void initDeepPosition(const Position&, EAffinity affinity);

    static int maxOffset(const Node*);
    
    static Position previousVisiblePosition(const Position&);
    static Position nextVisiblePosition(const Position&);
        
    Position m_deepPosition;
    EAffinity m_affinity;
};

inline bool operator==(const VisiblePosition &a, const VisiblePosition &b)
{
    return a.deepEquivalent() == b.deepEquivalent() && a.affinity() == b.affinity();
}
 
inline bool operator!=(const VisiblePosition &a, const VisiblePosition &b)
{
    return !(a == b);
}

bool isEqualIgnoringAffinity(const VisiblePosition&, const VisiblePosition&);
bool isNotEqualIgnoringAffinity(const VisiblePosition&, const VisiblePosition&);

PassRefPtr<Range> makeRange(const VisiblePosition &, const VisiblePosition &);
bool setStart(Range*, const VisiblePosition&);
bool setEnd(Range*, const VisiblePosition&);
VisiblePosition startVisiblePosition(const Range*, EAffinity);
VisiblePosition endVisiblePosition(const Range*, EAffinity);

Node *enclosingBlockFlowElement(const VisiblePosition&);

bool isFirstVisiblePositionInNode(const VisiblePosition&, const Node*);
bool isLastVisiblePositionInNode(const VisiblePosition&, const Node*);

} // namespace WebCore

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
void showTree(const WebCore::VisiblePosition*);
void showTree(const WebCore::VisiblePosition&);
#endif

#endif // KHTML_EDITING_VISIBLE_POSITION_H
