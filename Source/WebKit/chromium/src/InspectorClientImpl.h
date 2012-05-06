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

#ifndef InspectorClientImpl_h
#define InspectorClientImpl_h

#include "InspectorClient.h"
#include "InspectorController.h"
#include <wtf/OwnPtr.h>

namespace WebKit {

class WebDevToolsAgentClient;
class WebDevToolsAgentImpl;
class WebViewImpl;

class InspectorClientImpl : public WebCore::InspectorClient {
public:
    InspectorClientImpl(WebViewImpl*);
    ~InspectorClientImpl();

    // InspectorClient methods:
    virtual void inspectorDestroyed();
    virtual void openInspectorFrontend(WebCore::InspectorController*);
    virtual void closeInspectorFrontend();
    virtual void bringFrontendToFront();

    virtual void highlight();
    virtual void hideHighlight();

    virtual bool sendMessageToFrontend(const WTF::String&);

    virtual void updateInspectorStateCookie(const WTF::String&);

    virtual bool canClearBrowserCache();
    virtual void clearBrowserCache();
    virtual bool canClearBrowserCookies();
    virtual void clearBrowserCookies();

    virtual bool canOverrideDeviceMetrics();
    virtual void overrideDeviceMetrics(int, int, float, bool);
    virtual void autoZoomPageToFitWidth();

    virtual bool supportsFrameInstrumentation();

private:
    WebDevToolsAgentImpl* devToolsAgent();

    // The WebViewImpl of the page being inspected; gets passed to the constructor
    WebViewImpl* m_inspectedWebView;
};

} // namespace WebKit

#endif
