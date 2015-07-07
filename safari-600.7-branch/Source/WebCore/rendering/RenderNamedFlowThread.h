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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
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
#include "SelectionSubtreeRoot.h"
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

class RenderNamedFlowThread final : public RenderFlowThread, public SelectionSubtreeRoot {
public:
    RenderNamedFlowThread(Document&, PassRef<RenderStyle>, PassRef<WebKitNamedFlow>);
    virtual ~RenderNamedFlowThread();

    const AtomicString& flowThreadName() const;

    const RenderRegionList& invalidRenderRegionList() const { return m_invalidRegionList; }

    RenderElement* nextRendererForElement(Element&) const;

    void addFlowChild(RenderElement&);
    void removeFlowChild(RenderElement&);
    bool hasChildren() const { return !m_flowThreadChildList.isEmpty(); }
#ifndef NDEBUG
    bool hasChild(RenderElement& child) const { return m_flowThreadChildList.contains(&child); }
#endif

    void pushDependencies(RenderNamedFlowThreadList&);

    virtual void addRegionToThread(RenderRegion*) override;
    virtual void removeRegionFromThread(RenderRegion*) override;

    virtual void regionChangedWritingMode(RenderRegion*) override;

    LayoutRect decorationsClipRectForBoxInNamedFlowFragment(const RenderBox&, RenderNamedFlowFragment&) const;

    RenderNamedFlowFragment* fragmentFromAbsolutePointAndBox(const IntPoint&, const RenderBox& flowedBox);

    void registerNamedFlowContentElement(Element&);
    void unregisterNamedFlowContentElement(Element&);
    const NamedFlowContentElements& contentElements() const { return m_contentElements; }
    bool hasContentElement(Element&) const;

    bool isMarkedForDestruction() const;
    void getRanges(Vector<RefPtr<Range>>&, const RenderNamedFlowFragment*) const;

    virtual void applyBreakAfterContent(LayoutUnit) override final;

    virtual bool collectsGraphicsLayersUnderRegions() const override;

    // Check if the content is flown into at least a region with region styling rules.
    bool hasRegionsWithStyling() const { return m_hasRegionsWithStyling; }
    void checkRegionsWithStyling();

    void clearRenderObjectCustomStyle(const RenderObject*);

    virtual void removeFlowChildInfo(RenderObject*) override final;

    LayoutUnit flowContentBottom() const { return m_flowContentBottom; }
    void dispatchNamedFlowEvents();

    void setDispatchRegionOversetChangeEvent(bool value) { m_dispatchRegionOversetChangeEvent = value; }

    virtual bool absoluteQuadsForBox(Vector<FloatQuad>&, bool*, const RenderBox*, float, float) const override;

protected:
    void setMarkForDestruction();
    void resetMarkForDestruction();

private:
    virtual const char* renderName() const override;
    virtual bool isRenderNamedFlowThread() const override { return true; }
    virtual bool isChildAllowed(const RenderObject&, const RenderStyle&) const override;
    virtual void computeOverflow(LayoutUnit, bool = false) override;
    virtual void layout() override final;

    void dispatchRegionOversetChangeEventIfNeeded();

    bool dependsOn(RenderNamedFlowThread* otherRenderFlowThread) const;
    void addDependencyOnFlowThread(RenderNamedFlowThread*);
    void removeDependencyOnFlowThread(RenderNamedFlowThread*);

    void addFragmentToNamedFlowThread(RenderNamedFlowFragment*);

    void checkInvalidRegions();

    bool canBeDestroyed() const { return m_invalidRegionList.isEmpty() && m_regionList.isEmpty() && m_contentElements.isEmpty(); }
    void regionOversetChangeEventTimerFired(Timer&);
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
    ListHashSet<RenderElement*> m_flowThreadChildList;

    NamedFlowContentElements m_contentElements;

    RenderRegionList m_invalidRegionList;

    bool m_hasRegionsWithStyling : 1;
    bool m_dispatchRegionOversetChangeEvent : 1;

    // The DOM Object that represents a named flow.
    Ref<WebKitNamedFlow> m_namedFlow;

    Timer m_regionOversetChangeEventTimer;

    LayoutUnit m_flowContentBottom;
};

RENDER_OBJECT_TYPE_CASTS(RenderNamedFlowThread, isRenderNamedFlowThread())

} // namespace WebCore

#endif // RenderNamedFlowThread_h
