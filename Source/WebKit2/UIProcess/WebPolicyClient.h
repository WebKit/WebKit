/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#ifndef WebPolicyClient_h
#define WebPolicyClient_h

#include "APIClient.h"
#include "APIPolicyClient.h"
#include "WKPage.h"
#include "WKPagePolicyClientInternal.h"
#include "WebEvent.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/Forward.h>

namespace API {
class Object;

template<> struct ClientTraits<WKPagePolicyClientBase> {
    typedef std::tuple<WKPagePolicyClientV0, WKPagePolicyClientV1, WKPagePolicyClientInternal> Versions;
};
}

namespace WebKit {

class WebPolicyClient final : public API::Client<WKPagePolicyClientBase>, public API::PolicyClient {
public:
    explicit WebPolicyClient(const WKPagePolicyClientBase*);

private:
    virtual void decidePolicyForNavigationAction(WebPageProxy*, WebFrameProxy*, WebCore::NavigationType, WebEvent::Modifiers, WebMouseEvent::Button, WebFrameProxy* originatingFrame, const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceRequest&, WebFramePolicyListenerProxy*, API::Object* userData) override;
    virtual void decidePolicyForNewWindowAction(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebCore::NavigationType, WebKit::WebEvent::Modifiers, WebKit::WebMouseEvent::Button, const WebCore::ResourceRequest&, const String& frameName, WebKit::WebFramePolicyListenerProxy*, API::Object* userData) override;
    virtual void decidePolicyForResponse(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, WebFramePolicyListenerProxy*, API::Object* userData) override;
    virtual void unableToImplementPolicy(WebPageProxy*, WebFrameProxy*, const WebCore::ResourceError&, API::Object* userData) override;
};

} // namespace WebKit

#endif // WebPolicyClient_h
