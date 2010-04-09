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

#ifndef WebPageProxy_h
#define WebPageProxy_h

#include "DrawingAreaProxy.h"
#include "ScriptReturnValueCallback.h"
#include "WebEvent.h"
#include "WebFrameProxy.h"
#include "WebLoaderClient.h"
#include "WebPolicyClient.h"
#include "WebUIClient.h"
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/KURL.h>
#include <WebCore/PlatformString.h>
#include <wtf/HashMap.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class IntSize;
    class KURL;
}

namespace WebKit {

class DrawingAreaProxy;
class PageClient;
class WebKeyboardEvent;
class WebMouseEvent;
class WebPageNamespace;
class WebProcessProxy;
class WebWheelEvent;

class WebPageProxy : public RefCounted<WebPageProxy> {
public:
    static PassRefPtr<WebPageProxy> create(WebPageNamespace*, uint64_t pageID);
    ~WebPageProxy();

    uint64_t pageID() const { return m_pageID; }

    WebFrameProxy* webFrame(uint64_t) const;
    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }

    DrawingAreaProxy* drawingArea() { return m_drawingArea.get(); }

    void setPageClient(PageClient*);
    void initializeLoaderClient(WKPageLoaderClient*);
    void initializePolicyClient(WKPagePolicyClient*);
    void initializeUIClient(WKPageUIClient*);

    void revive();

    void initializeWebPage(const WebCore::IntSize&, PassOwnPtr<DrawingAreaProxy>);
    void reinitializeWebPage(const WebCore::IntSize&);

    void close();
    bool tryClose();
    bool isClosed() const { return m_closed; }

    void loadURL(const WebCore::KURL&);
    void stopLoading();
    void reload();

    void goForward();
    bool canGoForward() const { return m_canGoForward; }
    void goBack();
    bool canGoBack() const { return m_canGoBack; }

    void setFocused(bool isFocused);
    void setActive(bool active);

    void mouseEvent(const WebMouseEvent&);
    void wheelEvent(const WebWheelEvent&);
    void keyEvent(const WebKeyboardEvent&);

    const WebCore::String& pageTitle() const { return m_pageTitle; }
    const WebCore::String& toolTip() const { return m_toolTip; }

    void terminateProcess();

    void runJavaScriptInMainFrame(const WebCore::String&, PassRefPtr<ScriptReturnValueCallback>);

    void receivedPolicyDecision(WebCore::PolicyAction, WebFrameProxy*, uint64_t listenerID);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder&);
    void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder&, CoreIPC::ArgumentEncoder&);

    void processDidBecomeUnresponsive();
    void processDidBecomeResponsive();
    void processDidExit();
    void processDidRevive();

    WebProcessProxy* process() const;
    WebPageNamespace* pageNamespace() const { return m_pageNamespace.get(); }

    bool isValid();

    // REMOVE: For demo purposes only.
    const WebCore::KURL& urlAtProcessExit() const { return m_urlAtProcessExit; }

    void preferencesDidChange();

private:
    WebPageProxy(WebPageNamespace*, uint64_t pageID);

    void didCreateMainFrame(uint64_t frameID);
    void didCreateSubFrame(uint64_t frameID);

    void didStartProvisionalLoadForFrame(WebFrameProxy*, const WebCore::KURL&);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebFrameProxy*);
    void didFailProvisionalLoadForFrame(WebFrameProxy*);
    void didCommitLoadForFrame(WebFrameProxy*);
    void didFinishLoadForFrame(WebFrameProxy*);
    void didFailLoadForFrame(WebFrameProxy*);
    void didReceiveTitleForFrame(WebFrameProxy*, const WebCore::String&);
    void didFirstLayoutForFrame(WebFrameProxy*);
    void didFirstVisuallyNonEmptyLayoutForFrame(WebFrameProxy*);
    void didStartProgress();
    void didChangeProgress(double);
    void didFinishProgress();
    
    void decidePolicyForNavigationAction(WebFrameProxy*, uint32_t navigationType, const WebCore::KURL& url, uint64_t listenerID);
    void decidePolicyForNewWindowAction(WebFrameProxy*, uint32_t navigationType, const WebCore::KURL& url, uint64_t listenerID);
    void decidePolicyForMIMEType(WebFrameProxy*, const WebCore::String& MIMEType, const WebCore::KURL& url, uint64_t listenerID);

    WebPageProxy* createNewPage();
    void showPage();
    void closePage();

    void runJavaScriptAlert(WebFrameProxy*, const WebCore::String&);

    void takeFocus(bool direction);
    void setToolTip(const WebCore::String&);

    void didReceiveEvent(WebEvent::Type);
    void didRunJavaScriptInMainFrame(const WebCore::String&, uint64_t);

    OwnPtr<PageClient> m_pageClient;
    WebLoaderClient m_loaderClient;
    WebPolicyClient m_policyClient;
    WebUIClient m_uiClient;

    OwnPtr<DrawingAreaProxy> m_drawingArea;

    RefPtr<WebPageNamespace> m_pageNamespace;
    RefPtr<WebFrameProxy> m_mainFrame;
    HashMap<uint64_t, RefPtr<WebFrameProxy> > m_frameMap;
    WebCore::String m_pageTitle;

    HashMap<uint64_t, RefPtr<ScriptReturnValueCallback> > m_scriptReturnValueCallbacks;
    
    bool m_canGoBack;
    bool m_canGoForward;

    WebCore::String m_toolTip;

    // REMOVE: For demo purposes only.
    WebCore::KURL m_urlAtProcessExit;
    
    bool m_valid;
    bool m_closed;

    uint64_t m_pageID;
};

} // namespace WebKit

#endif // WebPageProxy_h
