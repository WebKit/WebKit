/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef APIPolicyClient_h
#define APIPolicyClient_h

#include "WebEvent.h"
#include "WebFramePolicyListenerProxy.h"
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/Forward.h>

namespace WebCore {
class ResourceError;
class ResourceRequest;
class ResourceResponse;
}

namespace WebKit {
struct NavigationActionData;
class WebPageProxy;
class WebFrameProxy;
class WebFramePolicyListenerProxy;
}

namespace API {
class Object;

class PolicyClient {
public:
    virtual ~PolicyClient() { }

    virtual void decidePolicyForNavigationAction(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, const WebKit::NavigationActionData&, WebKit::WebFrameProxy* originatingFrame, const WebCore::ResourceRequest& originalRequest, const WebCore::ResourceRequest&, WebKit::WebFramePolicyListenerProxy* listener, API::Object* userData) { listener->use(); }
    virtual void decidePolicyForNewWindowAction(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, WebCore::NavigationType, WebKit::WebEvent::Modifiers, WebKit::WebMouseEvent::Button, const WebCore::ResourceRequest&, const WTF::String& frameName, WebKit::WebFramePolicyListenerProxy* listener, API::Object* userData) { listener->use(); }
    virtual void decidePolicyForResponse(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, WebKit::WebFramePolicyListenerProxy* listener, API::Object* userData) { listener->use(); }
    virtual void unableToImplementPolicy(WebKit::WebPageProxy*, WebKit::WebFrameProxy*, const WebCore::ResourceError&, API::Object* userData) { }
};

} // namespace API

#endif // APIPolicyClient_h
