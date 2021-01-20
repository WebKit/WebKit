/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#pragma once

#include "APIObject.h"
#include "FrameTreeNodeData.h"

namespace WebKit {
class WebPageProxy;
}

namespace API {

class FrameHandle;

class FrameTreeNode final : public ObjectImpl<Object::Type::FrameTreeNode> {
public:
    static Ref<FrameTreeNode> create(WebKit::FrameTreeNodeData&& data, WebKit::WebPageProxy& page) { return adoptRef(*new FrameTreeNode(WTFMove(data), page)); }
    virtual ~FrameTreeNode();

    WebKit::WebPageProxy& page() { return m_page.get(); }
    bool isMainFrame() const { return m_data.info.isMainFrame; }
    const WebCore::ResourceRequest& request() const { return m_data.info.request; }
    const WebCore::SecurityOriginData& securityOrigin() const { return m_data.info.securityOrigin; }
    const Vector<WebKit::FrameTreeNodeData>& childFrames() const { return m_data.children; }
    Ref<FrameHandle> handle() const;
    RefPtr<FrameHandle> parentFrameHandle() const;

private:
    FrameTreeNode(WebKit::FrameTreeNodeData&& data, WebKit::WebPageProxy& page)
        : m_data(WTFMove(data))
        , m_page(page) { }

    WebKit::FrameTreeNodeData m_data;
    Ref<WebKit::WebPageProxy> m_page;
};

} // namespace API
