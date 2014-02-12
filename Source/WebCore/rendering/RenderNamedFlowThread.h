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
#include "Timer.h"
#include <wtf/HashCountedSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class Node;
class RenderNamedFlowThread;
class WebKitNamedFlow;

typedef ListHashSet<RenderNamedFlowThread*> RenderNamedFlowThreadList;
typedef HashCountedSet<RenderNamedFlowThread*> RenderNamedFlowThreadCountedSet;
typedef ListHashSet<Element*> NamedFlowContentElements;

class RenderNamedFlowThread final : public RenderFlowThread {
public:
    RenderNamedFlowThread(Document&, PassRef<RenderStyle>, PassRef<WebKitNamedFlow>);
    virtual ~RenderNamedFlowThread();

    const AtomicString& flowThreadName() const;

    const RenderRegionList& invalidRenderRegionList() const { return m_invalidRegionList; }

    RenderObject* nextRendererForNode(Node*) const;

    void addFlowChild(RenderObject* newChild);
    void removeFlowChild(RenderObject*);
    bool hasChildren() const { return !m_flowThreadChildList.isEmpty(); }
#ifndef NDEBUG
    bool hasChild(RenderObject* child) const { return m_flowThreadChildList.contains(child); }
#endif

    void pushDependencies(RenderNamedFlowThreadList&);

    virtual void addRegionToThread(RenderRegion*) override;
    virtual void removeRegionFromThread(RenderRegion*) override;

    virtual void regionChangedWritingMode(RenderRegion*) override;

    bool overset() const { return m_overset; }
    void computeOversetStateForRegions(LayoutUnit oldClientAfterEdge);

    void registerNamedFlowContentElement(Element&);
    void unregisterNamedFlowContentElement(Element&);
    const NamedFlowContentElements& contentElements() const { return m_contentElements; }
    bool hasContentElement(Element&) const;

    bool isMarkedForDestruction() const;
    void getRanges(Vector<RefPtr<Range>>&, const RenderRegion*) const;

    virtual bool collectsGraphicsLayersUnderRegions() const override;

    // Check if the content is flown into at least a region with region styling rules.
    bool hasRegionsWithStyling() const { return m_hasRegionsWithStyling; }
    void checkRegionsWithStyling();

    void clearRenderObjectCustomStyle(const RenderObject*);

    virtual void removeFlowChildInfo(RenderObject*) override final;

protected:
    void setMarkForDestruction();
    void resetMarkForDestruction();

private:
    virtual const char* renderName() const override;
    virtual bool isRenderNamedFlowThread() const override { return true; }
    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;

    virtual void dispatchRegionLayoutUpdateEvent() override;
    virtual void dispatchRegionOversetChangeEvent() override;

    bool dependsOn(RenderNamedFlowThread* otherRenderFlowThread) const;
    void addDependencyOnFlowThread(RenderNamedFlowThread*);
    void removeDependencyOnFlowThread(RenderNamedFlowThread*);

    void addRegionToNamedFlowThread(RenderRegion*);

    void checkInvalidRegions();

    bool canBeDestroyed() const { return m_invalidRegionList.isEmpty() && m_regionList.isEmpty() && m_contentElements.isEmpty(); }
    void regionLayoutUpdateEventTimerFired(Timer<RenderNamedFlowThread>&);
    void regionOversetChangeEventTimerFired(Timer<RenderNamedFlowThread>&);
    void clearContentElements();
    void updateWritingMode();

    WebKitNamedFlow& namedFlow() { return m_namedFlow.get(); }
    const WebKitNamedFlow& namedFlow() const { return m_namedFlow.get(); }

    // Observer flow threads have invalid regions that depend on the state of this thread
    // to re-validate their regions. Keeping a set of observer threads make it easy
    // to notify them when a region was removed from this flow.
    RenderNamedFlowThreadCountedSet m_observerThreadsSet;

    // Some threads need to have a complete layout before we layout this flow.
    // That's because they contain a RenderRegion that should display this thread. The set makes it
    // easy to sort the order of threads layout.
    RenderNamedFlowThreadCountedSet m_layoutBeforeThreadsSet;

    // Holds the sorted children of a named flow. This is the only way we can get the ordering right.
    ListHashSet<RenderObject*> m_flowThreadChildList;

    NamedFlowContentElements m_contentElements;

    RenderRegionList m_invalidRegionList;

    bool m_overset : 1;
    bool m_hasRegionsWithStyling : 1;

    // The DOM Object that represents a named flow.
    Ref<WebKitNamedFlow> m_namedFlow;

    Timer<RenderNamedFlowThread> m_regionLayoutUpdateEventTimer;
    Timer<RenderNamedFlowThread> m_regionOversetChangeEventTimer;
};

RENDER_OBJECT_TYPE_CASTS(RenderNamedFlowThread, isRenderNamedFlowThread())

} // namespace WebCore

#endif // RenderNamedFlowThread_h
