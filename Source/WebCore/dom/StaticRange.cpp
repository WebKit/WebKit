/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "DOMException.h"
#include "Node.h"
#include "Range.h"

namespace WebCore {

StaticRange::StaticRange(Ref<Node>&& startContainer, unsigned startOffset, Ref<Node>&& endContainer, unsigned endOffset)
    : m_startContainer(WTFMove(startContainer))
    , m_startOffset(startOffset)
    , m_endContainer(WTFMove(endContainer))
    , m_endOffset(endOffset)
{
}

StaticRange::~StaticRange() = default;

Ref<StaticRange> StaticRange::create(Ref<Node>&& startContainer, unsigned startOffset, Ref<Node>&& endContainer, unsigned endOffset)
{
    return adoptRef(*new StaticRange(WTFMove(startContainer), startOffset, WTFMove(endContainer), endOffset));
}

Ref<StaticRange> StaticRange::createFromRange(const Range& range)
{
    return StaticRange::create(range.startContainer(), range.startOffset(), range.endContainer(), range.endOffset());
}

static inline bool isDocumentTypeOrAttr(Node& node)
{
    return node.isDocumentTypeNode() || node.isAttributeNode();
}

ExceptionOr<Ref<StaticRange>> StaticRange::create(Init&& init)
{
    ASSERT(init.startContainer);
    ASSERT(init.endContainer);
    if (isDocumentTypeOrAttr(*init.startContainer) || isDocumentTypeOrAttr(*init.endContainer))
        return Exception { InvalidNodeTypeError };
    return StaticRange::create(init.startContainer.releaseNonNull(), init.startOffset, init.endContainer.releaseNonNull(), init.endOffset);
}

Node* StaticRange::startContainer() const
{
    return (Node*)m_startContainer.ptr();
}

Node* StaticRange::endContainer() const
{
    return (Node*)m_endContainer.ptr();
}

bool StaticRange::collapsed() const
{
    return m_startOffset == m_endOffset && m_startContainer.ptr() == m_endContainer.ptr();
}

}
