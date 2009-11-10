/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebNode.h"

#include "Document.h"
#include "Frame.h"
#include "FrameLoaderClientImpl.h"
#include "Node.h"

#include "WebFrameImpl.h"
#include "WebString.h"

#include <wtf/PassRefPtr.h>

using namespace WebCore;

namespace WebKit {

class WebNodePrivate : public Node {
};

void WebNode::reset()
{
    assign(0);
}

void WebNode::assign(const WebNode& other)
{
    WebNodePrivate* p = const_cast<WebNodePrivate*>(other.m_private);
    if (p)
        p->ref();
    assign(p);
}

WebNode WebNode::parentNode() const
{
    return PassRefPtr<Node>(const_cast<Node*>(m_private->parentNode()));
}

WebString WebNode::nodeName() const
{
    return m_private->nodeName();
}

WebNode::WebNode(const WTF::PassRefPtr<WebCore::Node>& node)
    : m_private(static_cast<WebNodePrivate*>(node.releaseRef()))
{
}

WebNode& WebNode::operator=(const WTF::PassRefPtr<WebCore::Node>& node)
{
    assign(static_cast<WebNodePrivate*>(node.releaseRef()));
    return *this;
}

WebNode::operator WTF::PassRefPtr<WebCore::Node>() const
{
    return PassRefPtr<Node>(const_cast<WebNodePrivate*>(m_private));
}

void WebNode::assign(WebNodePrivate* p)
{
    // p is already ref'd for us by the caller
    if (m_private)
        m_private->deref();
    m_private = p;
}

WebFrame* WebNode::frame()
{
    FrameLoaderClientImpl* frame_loader_client =
        static_cast<FrameLoaderClientImpl*>(m_private->document()->
                                            frame()->loader()->client());
    return static_cast<WebFrame*>(frame_loader_client->webFrame());
}

} // namespace WebKit
