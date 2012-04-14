/*
 * Copyright (C) 2012 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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


#ifndef RenderNamedFlowThread_h
#define RenderNamedFlowThread_h

#include "RenderFlowThread.h"
#include <wtf/HashCountedSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class Node;
class RenderNamedFlowThread;
class WebKitNamedFlow;

typedef ListHashSet<RenderNamedFlowThread*> RenderNamedFlowThreadList;
typedef HashCountedSet<RenderNamedFlowThread*> RenderNamedFlowThreadCountedSet;
typedef ListHashSet<Node*> NamedFlowContentNodes;

class RenderNamedFlowThread : public RenderFlowThread {
public:
    RenderNamedFlowThread(Node*, const AtomicString&);

    AtomicString flowThreadName() const { return m_flowThreadName; }

    RenderObject* nextRendererForNode(Node*) const;
    RenderObject* previousRendererForNode(Node*) const;

    void addFlowChild(RenderObject* newChild, RenderObject* beforeChild = 0);
    void removeFlowChild(RenderObject*);
    bool hasChildren() const { return !m_flowThreadChildList.isEmpty(); }
#ifndef NDEBUG
    bool hasChild(RenderObject* child) const { return m_flowThreadChildList.contains(child); }
#endif

    void pushDependencies(RenderNamedFlowThreadList&);

    virtual void addRegionToThread(RenderRegion*) OVERRIDE;
    virtual void removeRegionFromThread(RenderRegion*) OVERRIDE;

    WebKitNamedFlow* ensureNamedFlow();
    void registerNamedFlowContentNode(Node*);
    void unregisterNamedFlowContentNode(Node*);
    const NamedFlowContentNodes& contentNodes() const { return m_contentNodes; }
    bool hasContentNode(Node* contentNode) const { ASSERT(contentNode); return m_contentNodes.contains(contentNode); }

private:
    virtual const char* renderName() const OVERRIDE;
    virtual bool isRenderNamedFlowThread() const OVERRIDE { return true; }

    bool dependsOn(RenderNamedFlowThread* otherRenderFlowThread) const;
    void addDependencyOnFlowThread(RenderNamedFlowThread*);
    void removeDependencyOnFlowThread(RenderNamedFlowThread*);
    void checkInvalidRegions();

private:
    // The name of the flow thread as specified in CSS.
    AtomicString m_flowThreadName;

    // Observer flow threads have invalid regions that depend on the state of this thread
    // to re-validate their regions. Keeping a set of observer threads make it easy
    // to notify them when a region was removed from this flow.
    RenderNamedFlowThreadCountedSet m_observerThreadsSet;

    // Some threads need to have a complete layout before we layout this flow.
    // That's because they contain a RenderRegion that should display this thread. The set makes it
    // easy to sort the order of threads layout.
    RenderNamedFlowThreadCountedSet m_layoutBeforeThreadsSet;

    // Holds the sorted children of a named flow. This is the only way we can get the ordering right.
    typedef ListHashSet<RenderObject*> FlowThreadChildList;
    FlowThreadChildList m_flowThreadChildList;

    NamedFlowContentNodes m_contentNodes;

    // The DOM Object that represents a named flow.
    RefPtr<WebKitNamedFlow> m_namedFlow;
};

inline RenderNamedFlowThread* toRenderNamedFlowThread(RenderObject* object)
{
    ASSERT(!object || object->isRenderNamedFlowThread());
    return static_cast<RenderNamedFlowThread*>(object);
}

inline const RenderNamedFlowThread* toRenderNamedFlowThread(const RenderObject* object)
{
    ASSERT(!object || object->isRenderNamedFlowThread());
    return static_cast<const RenderNamedFlowThread*>(object);
}

// This will catch anyone doing an unnecessary cast.
void toRenderNamedFlowThread(const RenderNamedFlowThread*);

} // namespace WebCore

#endif // RenderNamedFlowThread_h

