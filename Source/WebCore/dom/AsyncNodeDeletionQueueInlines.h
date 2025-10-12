/*
 * Copyright (C) 2024-2025 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AsyncNodeDeletionQueue.h>
#include <WebCore/ContainerNode.h>
#include <WebCore/Element.h>
#include <WebCore/HTMLElement.h>
#include <WebCore/HTMLNames.h>
#include <WebCore/NodeName.h>

namespace WebCore {

ALWAYS_INLINE AsyncNodeDeletionQueue::AsyncNodeDeletionQueue() = default;
ALWAYS_INLINE AsyncNodeDeletionQueue::~AsyncNodeDeletionQueue() = default;

ALWAYS_INLINE void AsyncNodeDeletionQueue::addIfSubtreeSizeIsUnderLimit(NodeVector&& children, unsigned subTreeSize)
{
    if (m_nodeCount + subTreeSize > s_maxSizeAsyncNodeDeletionQueue)
        return;
    m_nodeCount += subTreeSize;
    m_queue.appendVector(WTFMove(children));
}

ALWAYS_INLINE void AsyncNodeDeletionQueue::deleteNodesNow()
{
    m_queue.clear();
    m_nodeCount = 0;
}

ALWAYS_INLINE ContainerNode::CanDelayNodeDeletion AsyncNodeDeletionQueue::canNodeBeDeletedAsync(const Node& node)
{
    if (!dynamicDowncast<HTMLElement>(node))
        return ContainerNode::CanDelayNodeDeletion::Yes;
    if (isNodeLikelyLarge(node))
        return ContainerNode::CanDelayNodeDeletion::No;
    return ContainerNode::CanDelayNodeDeletion::Yes;
}

ALWAYS_INLINE bool AsyncNodeDeletionQueue::isNodeLikelyLarge(const Node& node)
{
    ASSERT(node.isElementNode());

    switch (downcast<Element>(node).elementName()) {
    case NodeName::HTML_audio:
    case NodeName::HTML_body:
    case NodeName::HTML_canvas:
    case NodeName::HTML_iframe:
    case NodeName::HTML_img:
    case NodeName::HTML_object:
    case NodeName::HTML_source:
    case NodeName::HTML_track:
    case NodeName::HTML_video:
    case NodeName::SVG_svg:
        return true;
    default:
        return false;
    }
}

} // namespace WebCore
