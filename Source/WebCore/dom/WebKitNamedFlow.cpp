/*
 * Copyright (C) 2011 Adobe Systems Incorporated. All rights reserved.
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
#include "WebKitNamedFlow.h"

#include "RenderNamedFlowThread.h"
#include "RenderRegion.h"
#include "StaticNodeList.h"

namespace WebCore {

WebKitNamedFlow::WebKitNamedFlow(RenderNamedFlowThread* parentFlowThread)
: m_parentFlowThread(parentFlowThread)
{
}

WebKitNamedFlow::~WebKitNamedFlow()
{
}

bool WebKitNamedFlow::overflow() const
{
    m_parentFlowThread->document()->updateLayoutIgnorePendingStylesheets();
    return m_parentFlowThread->overflow();
}

PassRefPtr<NodeList> WebKitNamedFlow::getRegionsByContentNode(Node* contentNode)
{
    if (!contentNode)
        return 0;

    m_parentFlowThread->document()->updateLayoutIgnorePendingStylesheets();

    Vector<RefPtr<Node> > regionNodes;
    if (contentNode->renderer()
        && contentNode->renderer()->inRenderFlowThread()
        && m_parentFlowThread == contentNode->renderer()->enclosingRenderFlowThread()) {
        const RenderRegionList& regionList = m_parentFlowThread->renderRegionList();
        for (RenderRegionList::const_iterator iter = regionList.begin(); iter != regionList.end(); ++iter) {
            const RenderRegion* renderRegion = *iter;
            if (!renderRegion->isValid())
                continue;
            if (m_parentFlowThread->objectInFlowRegion(contentNode->renderer(), renderRegion))
                regionNodes.append(renderRegion->node());
        }
    }
    return StaticNodeList::adopt(regionNodes);
}

PassRefPtr<NodeList> WebKitNamedFlow::contentNodes() const
{
    m_parentFlowThread->document()->updateLayoutIgnorePendingStylesheets();

    Vector<RefPtr<Node> > contentNodes;
    for (NamedFlowContentNodes::const_iterator it = m_parentFlowThread->contentNodes().begin(); it != m_parentFlowThread->contentNodes().end(); ++it) {
        Node* node = const_cast<Node*>(*it);
        ASSERT(node->computedStyle()->flowThread() == m_parentFlowThread->flowThreadName());
        contentNodes.append(node);
    }

    return StaticNodeList::adopt(contentNodes);
}

} // namespace WebCore

