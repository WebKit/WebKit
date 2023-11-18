/*
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ScrollingStatePluginHostingNode.h"

#if ENABLE(ASYNC_SCROLLING)

#include "ScrollingStateTree.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

Ref<ScrollingStatePluginHostingNode> ScrollingStatePluginHostingNode::create(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
{
    return adoptRef(*new ScrollingStatePluginHostingNode(stateTree, nodeID));
}

Ref<ScrollingStatePluginHostingNode> ScrollingStatePluginHostingNode::create(ScrollingNodeID nodeID, Vector<Ref<ScrollingStateNode>>&& children, OptionSet<ScrollingStateNodeProperty> changedProperties, std::optional<PlatformLayerIdentifier> layerID)
{
    return adoptRef(*new ScrollingStatePluginHostingNode(nodeID, WTFMove(children), changedProperties, layerID));
}

ScrollingStatePluginHostingNode::ScrollingStatePluginHostingNode(ScrollingNodeID nodeID, Vector<Ref<ScrollingStateNode>>&& children, OptionSet<ScrollingStateNodeProperty> changedProperties, std::optional<PlatformLayerIdentifier> layerID)
    : ScrollingStateNode(ScrollingNodeType::PluginHosting, nodeID, WTFMove(children), changedProperties, layerID)
{
    ASSERT(isPluginHostingNode());
}

ScrollingStatePluginHostingNode::ScrollingStatePluginHostingNode(ScrollingStateTree& stateTree, ScrollingNodeID nodeID)
    : ScrollingStateNode(ScrollingNodeType::PluginHosting, stateTree, nodeID)
{
    ASSERT(isPluginHostingNode());
}

ScrollingStatePluginHostingNode::ScrollingStatePluginHostingNode(const ScrollingStatePluginHostingNode& stateNode, ScrollingStateTree& adoptiveTree)
    : ScrollingStateNode(stateNode, adoptiveTree)
{
}

ScrollingStatePluginHostingNode::~ScrollingStatePluginHostingNode() = default;

Ref<ScrollingStateNode> ScrollingStatePluginHostingNode::clone(ScrollingStateTree& adoptiveTree)
{
    return adoptRef(*new ScrollingStatePluginHostingNode(*this, adoptiveTree));
}

void ScrollingStatePluginHostingNode::dumpProperties(TextStream& ts, OptionSet<ScrollingStateTreeAsTextBehavior> behavior) const
{
    ts << "Plugin hosting node";
    ScrollingStateNode::dumpProperties(ts, behavior);
}

} // namespace WebCore

#endif // ENABLE(ASYNC_SCROLLING)
