/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "FrameInfoData.h"
#include "WebPageProxy.h"
#include <WebCore/ResourceRequest.h>

namespace WebCore {
struct SecurityOriginData;
}

namespace WebKit {
class WebFrameProxy;
struct FrameInfoData;
}

namespace API {

class FrameHandle;
class SecurityOrigin;

class FrameInfo final : public ObjectImpl<Object::Type::FrameInfo> {
public:
    static Ref<FrameInfo> create(WebKit::FrameInfoData&&, WebKit::WebPageProxy*);
    virtual ~FrameInfo();

    bool isMainFrame() const { return m_data.isMainFrame; }
    const WebCore::ResourceRequest& request() const { return m_data.request; }
    WebCore::SecurityOriginData& securityOrigin() { return m_data.securityOrigin; }
    Ref<FrameHandle> handle() const;
    WebKit::WebPageProxy* page() { return m_page.get(); }
    RefPtr<FrameHandle> parentFrameHandle() const;

private:
    FrameInfo(WebKit::FrameInfoData&&, WebKit::WebPageProxy*);

    WebKit::FrameInfoData m_data;
    RefPtr<WebKit::WebPageProxy> m_page;
};

} // namespace API
