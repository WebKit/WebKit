/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef WebDevToolsAgentImpl_h
#define WebDevToolsAgentImpl_h

#include "InspectorClient.h"
#include "InspectorFrontendChannel.h"

#include "WebDevToolsAgentPrivate.h"
#include "WebPageOverlay.h"
#include <public/WebSize.h>
#include <public/WebThread.h>
#include <wtf/Forward.h>
#include <wtf/OwnPtr.h>

namespace WebCore {
class Document;
class Frame;
class FrameView;
class GraphicsContext;
class InspectorClient;
class InspectorController;
class Node;
}

namespace WebKit {

class DeviceMetricsSupport;
class WebDevToolsAgentClient;
class WebFrame;
class WebFrameImpl;
class WebString;
class WebURLRequest;
class WebURLResponse;
class WebViewImpl;
struct WebMemoryUsageInfo;
struct WebURLError;
struct WebDevToolsMessageData;

class WebDevToolsAgentImpl : public WebDevToolsAgentPrivate,
                             public WebCore::InspectorClient,
                             public WebCore::InspectorFrontendChannel,
                             public WebPageOverlay,
                             private WebThread::TaskObserver {
public:
    WebDevToolsAgentImpl(WebViewImpl* webViewImpl, WebDevToolsAgentClient* client);
    virtual ~WebDevToolsAgentImpl();

    // WebDevToolsAgentPrivate implementation.
    virtual void didCreateScriptContext(WebFrameImpl*, int worldId);
    virtual void mainFrameViewCreated(WebFrameImpl*);
    virtual bool metricsOverridden();
    virtual void webViewResized(const WebSize&);

    // WebDevToolsAgent implementation.
    virtual void attach();
    virtual void reattach(const WebString& savedState);
    virtual void detach();
    virtual void didNavigate();
    virtual void didBeginFrame();
    virtual void didCancelFrame();
    virtual void willComposite();
    virtual void didComposite();
    virtual void dispatchOnInspectorBackend(const WebString& message);
    virtual void inspectElementAt(const WebPoint& point);
    virtual void evaluateInWebInspector(long callId, const WebString& script);
    virtual void setProcessId(long);

    // InspectorClient implementation.
    virtual void inspectorDestroyed();
    virtual WebCore::InspectorFrontendChannel* openInspectorFrontend(WebCore::InspectorController*);
    virtual void closeInspectorFrontend();

    virtual void bringFrontendToFront();
    virtual void highlight();
    virtual void hideHighlight();
    virtual bool supportsInspectorStateUpdates() const { return true; }
    virtual void updateInspectorStateCookie(const WTF::String&);
    virtual bool sendMessageToFrontend(const WTF::String&);

    virtual void clearBrowserCache();
    virtual void clearBrowserCookies();

    virtual void overrideDeviceMetrics(int width, int height, float fontScaleFactor, bool fitWindow);
    virtual void autoZoomPageToFitWidth();

    virtual void getAllocatedObjects(HashSet<const void*>&);
    virtual void dumpUncountedAllocatedObjects(const HashMap<const void*, size_t>&);
    virtual void setTraceEventCallback(TraceEventCallback);

    virtual bool captureScreenshot(WTF::String* data);

    virtual bool handleJavaScriptDialog(bool accept, const WTF::String* promptText);

    int hostId() { return m_hostId; }

    // WebPageOverlay
    virtual void paintPageOverlay(WebCanvas*);

    virtual WebVector<WebMemoryUsageInfo> processMemoryDistribution() const;

private:
    // WebThread::TaskObserver
    virtual void willProcessTask();
    virtual void didProcessTask();

    WebCore::InspectorController* inspectorController();
    WebCore::Frame* mainFrame();

    int m_hostId;
    WebDevToolsAgentClient* m_client;
    WebViewImpl* m_webViewImpl;
    bool m_attached;
    OwnPtr<DeviceMetricsSupport> m_metricsSupport;
    BrowserDataHint m_sendWithBrowserDataHint;
};

} // namespace WebKit

#endif
