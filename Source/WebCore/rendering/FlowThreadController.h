/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef FlowThreadController_h
#define FlowThreadController_h

#include "RenderView.h"
#include <wtf/ListHashSet.h>
#include <wtf/OwnPtr.h>

namespace WebCore {

class RenderFlowThread;
class RenderLayer;
class RenderNamedFlowThread;

typedef ListHashSet<RenderNamedFlowThread*> RenderNamedFlowThreadList;

class FlowThreadController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<FlowThreadController> create(RenderView*);
    ~FlowThreadController();

    RenderFlowThread* currentRenderFlowThread() const { return m_currentRenderFlowThread; }
    void setCurrentRenderFlowThread(RenderFlowThread* flowThread) { m_currentRenderFlowThread = flowThread; }

    bool isRenderNamedFlowThreadOrderDirty() const { return m_isRenderNamedFlowThreadOrderDirty; }
    void setIsRenderNamedFlowThreadOrderDirty(bool dirty)
    {
        m_isRenderNamedFlowThreadOrderDirty = dirty;
        if (dirty)
            m_view->setNeedsLayout();
    }

    RenderNamedFlowThread& ensureRenderFlowThreadWithName(const AtomicString&);
    const RenderNamedFlowThreadList* renderNamedFlowThreadList() const { return m_renderNamedFlowThreadList.get(); }
    bool hasRenderNamedFlowThreads() const { return m_renderNamedFlowThreadList && !m_renderNamedFlowThreadList->isEmpty(); }
    void layoutRenderNamedFlowThreads();
    void styleDidChange();

    void registerNamedFlowContentElement(Element&, RenderNamedFlowThread&);
    void unregisterNamedFlowContentElement(Element&);
    bool isContentElementRegisteredWithAnyNamedFlow(const Element&) const;

    bool hasFlowThreadsWithAutoLogicalHeightRegions() const { return m_flowThreadsWithAutoLogicalHeightRegions; }
    void incrementFlowThreadsWithAutoLogicalHeightRegions() { ++m_flowThreadsWithAutoLogicalHeightRegions; }
    void decrementFlowThreadsWithAutoLogicalHeightRegions() { ASSERT(m_flowThreadsWithAutoLogicalHeightRegions > 0); --m_flowThreadsWithAutoLogicalHeightRegions; }

    bool updateFlowThreadsNeedingLayout();
    bool updateFlowThreadsNeedingTwoStepLayout();
    void updateFlowThreadsIntoConstrainedPhase();
    void updateFlowThreadsIntoOverflowPhase();
    void updateFlowThreadsIntoMeasureContentPhase();
    void updateFlowThreadsIntoFinalPhase();

    void updateNamedFlowsLayerListsIfNeeded();
    // Collect the fixed positioned layers that have the named flows as containing block
    // These layers are painted and hit-tested by RenderView
    void collectFixedPositionedLayers(Vector<RenderLayer*>& fixedPosLayers) const;

#if USE(ACCELERATED_COMPOSITING)
    void updateRenderFlowThreadLayersIfNeeded();
#endif

#ifndef NDEBUG
    bool isAutoLogicalHeightRegionsCountConsistent() const;
#endif

protected:
    explicit FlowThreadController(RenderView*);
    void updateFlowThreadsChainIfNecessary();
    void resetFlowThreadsWithAutoHeightRegions();

private:
    RenderView* m_view;
    RenderFlowThread* m_currentRenderFlowThread;
    bool m_isRenderNamedFlowThreadOrderDirty;
    unsigned m_flowThreadsWithAutoLogicalHeightRegions;
    OwnPtr<RenderNamedFlowThreadList> m_renderNamedFlowThreadList;
    HashMap<const Element*, RenderNamedFlowThread*> m_mapNamedFlowContentElement;
};

}

#endif
