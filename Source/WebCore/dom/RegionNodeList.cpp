/*
 * Copyright 2012 Adobe Systems Incorporated. All Rights Reserved.
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
#include "RegionNodeList.h"

#include "Document.h"
#include "RenderFlowThread.h"
#include "RenderRegion.h"
#include "WebKitNamedFlow.h"

namespace WebCore {

RegionNodeList::RegionNodeList(PassRefPtr<Node> node, const AtomicString& flowName)
: DynamicSubtreeNodeList(node->document())
, m_contentNode(node)
, m_flowName(flowName)
{
}

RegionNodeList::~RegionNodeList()
{
    m_contentNode->removeCachedRegionNodeList(this, m_flowName);
} 

bool RegionNodeList::nodeMatches(Element* testNode) const
{
    if (!m_contentNode->renderer())
        return false;

    if (!testNode->renderer() || !testNode->renderer()->isRenderRegion())
        return false;

    RenderRegion* region = toRenderRegion(testNode->renderer());
    if (!region->isValid() || (region->style()->regionThread() != m_flowName))
        return false;

    return region->flowThread()->objectInFlowRegion(m_contentNode->renderer(), region);
}

} // namespace WebCore

