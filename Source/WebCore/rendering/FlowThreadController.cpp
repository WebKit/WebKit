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

#include "config.h"

#include "FlowThreadController.h"

#include "RenderFlowThread.h"
#include "RenderNamedFlowThread.h"
#include <wtf/text/AtomicString.h>

namespace WebCore {

PassOwnPtr<FlowThreadController> FlowThreadController::create(RenderView* view)
{
    return adoptPtr(new FlowThreadController(view));
}

FlowThreadController::FlowThreadController(RenderView* view)
    : m_view(view)
    , m_currentRenderFlowThread(0)
    , m_isRenderNamedFlowThreadOrderDirty(false)
{
}

FlowThreadController::~FlowThreadController()
{
}

RenderNamedFlowThread* FlowThreadController::ensureRenderFlowThreadWithName(const AtomicString& name)
{
    if (!m_renderNamedFlowThreadList)
        m_renderNamedFlowThreadList = adoptPtr(new RenderNamedFlowThreadList());
    else {
        for (RenderNamedFlowThreadList::iterator iter = m_renderNamedFlowThreadList->begin(); iter != m_renderNamedFlowThreadList->end(); ++iter) {
            RenderNamedFlowThread* flowRenderer = *iter;
            if (flowRenderer->flowThreadName() == name)
                return flowRenderer;
        }
    }

    RenderNamedFlowThread* flowRenderer = new (m_view->renderArena()) RenderNamedFlowThread(m_view->document(), name);
    flowRenderer->setStyle(RenderFlowThread::createFlowThreadStyle(m_view->style()));
    m_renderNamedFlowThreadList->add(flowRenderer);

    // Keep the flow renderer as a child of RenderView.
    m_view->addChild(flowRenderer);

    setIsRenderNamedFlowThreadOrderDirty(true);

    return flowRenderer;
}

void FlowThreadController::layoutRenderNamedFlowThreads()
{
    ASSERT(m_renderNamedFlowThreadList);

    if (isRenderNamedFlowThreadOrderDirty()) {
        // Arrange the thread list according to dependencies.
        RenderNamedFlowThreadList sortedList;
        for (RenderNamedFlowThreadList::iterator iter = m_renderNamedFlowThreadList->begin(); iter != m_renderNamedFlowThreadList->end(); ++iter) {
            RenderNamedFlowThread* flowRenderer = *iter;
            if (sortedList.contains(flowRenderer))
                continue;
            flowRenderer->pushDependencies(sortedList);
            sortedList.add(flowRenderer);
        }
        m_renderNamedFlowThreadList->swap(sortedList);
        setIsRenderNamedFlowThreadOrderDirty(false);
    }

    for (RenderNamedFlowThreadList::iterator iter = m_renderNamedFlowThreadList->begin(); iter != m_renderNamedFlowThreadList->end(); ++iter) {
        RenderNamedFlowThread* flowRenderer = *iter;
        flowRenderer->layoutIfNeeded();
    }
}

void FlowThreadController::registerNamedFlowContentNode(Node* contentNode, RenderNamedFlowThread* namedFlow)
{
    ASSERT(contentNode && contentNode->isElementNode());
    ASSERT(namedFlow);
    ASSERT(!m_mapNamedFlowContentNodes.contains(contentNode));
    ASSERT(!namedFlow->hasContentNode(contentNode));
    m_mapNamedFlowContentNodes.add(contentNode, namedFlow);
    namedFlow->registerNamedFlowContentNode(contentNode);
}

void FlowThreadController::unregisterNamedFlowContentNode(Node* contentNode)
{
    ASSERT(contentNode && contentNode->isElementNode());
    HashMap<Node*, RenderNamedFlowThread*>::iterator it = m_mapNamedFlowContentNodes.find(contentNode);
    ASSERT(it != m_mapNamedFlowContentNodes.end());
    ASSERT(it->second);
    ASSERT(it->second->hasContentNode(contentNode));
    it->second->unregisterNamedFlowContentNode(contentNode);
    m_mapNamedFlowContentNodes.remove(contentNode);
}

} // namespace WebCore
