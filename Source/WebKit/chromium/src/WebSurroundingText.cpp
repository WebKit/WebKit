/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebSurroundingText.h"

#include "Element.h"
#include "Node.h"
#include "Range.h"
#include "RenderObject.h"
#include "SurroundingText.h"
#include "Text.h"
#include "VisiblePosition.h"
#include "WebHitTestResult.h"

#include "platform/WebPoint.h"

using namespace WebCore;

namespace WebKit {

void WebSurroundingText::initialize(const WebHitTestResult& hitTestInfo, size_t maxLength)
{
    Node* node = hitTestInfo.node().unwrap<Node>();
    if (!node || !node->renderer())
        return;

    VisiblePosition visiblePosition(node->renderer()->positionForPoint(static_cast<IntPoint>(hitTestInfo.localPoint())));
    if (visiblePosition.isNull())
        return;

    m_private.reset(new SurroundingText(visiblePosition, maxLength));
}

void WebSurroundingText::initialize(WebNode textNode, size_t offset, size_t maxLength)
{
    Node* node = textNode.unwrap<Node>();
    if (!node || !node->isTextNode() || offset >= node->nodeValue().length())
        return;

    m_private.reset(new SurroundingText(VisiblePosition(Position(toText(node), offset).parentAnchoredEquivalent(), DOWNSTREAM), maxLength));
}

WebString WebSurroundingText::textContent() const
{
    return m_private->content();
}

size_t WebSurroundingText::hitOffsetInTextContent() const
{
    return m_private->positionOffsetInContent();
}

WebRange WebSurroundingText::rangeFromContentOffsets(size_t startOffsetInContent, size_t endOffsetInContent)
{
    return m_private->rangeFromContentOffsets(startOffsetInContent, endOffsetInContent);
}

bool WebSurroundingText::isNull() const
{
    return !m_private.get();
}

void WebSurroundingText::reset()
{
    m_private.reset(0);
}

} // namespace WebKit
