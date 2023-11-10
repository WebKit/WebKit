/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#include "StaticRange.h"

#include "ContainerNode.h"
#include "JSNode.h"
#include "Text.h"
#include "WebCoreOpaqueRootInlines.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(StaticRange);

StaticRange::StaticRange(SimpleRange&& range)
    : SimpleRange(WTFMove(range))
{
}

Ref<StaticRange> StaticRange::create(SimpleRange&& range)
{
    return adoptRef(*new StaticRange(WTFMove(range)));
}

Ref<StaticRange> StaticRange::create(const SimpleRange& range)
{
    return create(SimpleRange { range });
}

static bool isDocumentTypeOrAttr(Node& node)
{
    // Before calling nodeType, do two fast non-virtual checks that cover almost all normal nodes, but are false for DocumentType and Attr.
    if (is<ContainerNode>(node) || is<Text>(node))
        return false;

    // Call nodeType explicitly and use a switch so we don't have to call it twice.
    switch (node.nodeType()) {
    case Node::ATTRIBUTE_NODE:
    case Node::DOCUMENT_TYPE_NODE:
        return true;
    default:
        return false;
    }
}

ExceptionOr<Ref<StaticRange>> StaticRange::create(Init&& init)
{
    ASSERT(init.startContainer);
    ASSERT(init.endContainer);
    if (isDocumentTypeOrAttr(*init.startContainer) || isDocumentTypeOrAttr(*init.endContainer))
        return Exception { ExceptionCode::InvalidNodeTypeError };
    return create({ { init.startContainer.releaseNonNull(), init.startOffset }, { init.endContainer.releaseNonNull(), init.endOffset } });
}

void StaticRange::visitNodesConcurrently(JSC::AbstractSlotVisitor& visitor) const
{
    addWebCoreOpaqueRoot(visitor, start.container.get());
    addWebCoreOpaqueRoot(visitor, end.container.get());
}

bool StaticRange::computeValidity() const
{
    Node& startContainer = this->startContainer();
    Node& endContainer = this->endContainer();

    if (!connectedInSameTreeScope(&startContainer.rootNode(), &endContainer.rootNode()))
        return false;
    if (startOffset() > startContainer.length())
        return false;
    if (endOffset() > endContainer.length())
        return false;
    if (&startContainer == &endContainer)
        return endOffset() > startOffset();
    return !is_gt(treeOrder<ComposedTree>(startContainer, endContainer));
}
} // namespace WebCore
