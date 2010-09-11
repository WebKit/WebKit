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

#include "APIObject.h"
#include "DrawingAreaProxy.h"
#include "GenericCallback.h"
#include "WKBase.h"
#include "WebEvent.h"
#include "WebFormClient.h"
#include "WebFrameProxy.h"
#include "WebHistoryClient.h"
#include "WebLoaderClient.h"
#include "WebPolicyClient.h"
#include "WebUIClient.h"
#include <WebCore/EditAction.h>
#include <WebCore/FrameLoaderTypes.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace CoreIPC {
    class ArgumentDecoder;
    class Connection;
    class MessageID;
}

namespace WebCore {
    class IntSize;
    class Cursor;
}

struct WKContextStatistics;

namespace WebKit {

class DrawingAreaProxy;
class PageClient;
class WebBackForwardList;
class WebBackForwardListItem;
class WebData;
class WebEditCommandProxy;
class WebKeyboardEvent;
class WebMouseEvent;
class WebPageNamespace;
class WebProcessProxy;
class WebURLRequest;
class WebWheelEvent;
struct WebNavigationDataStore;

typedef GenericCallback<WKStringRef, WTF::StringImpl*> RenderTreeExternalRepresentationCallback;
typedef GenericCallback<WKStringRef, WTF::StringImpl*> ScriptReturnValueCallback;

class WebPageProxy : public APIObject {
public:
    static const Type APIType = TypePage;

    static PassRefPtr<WebPageProxy> create(WebPageNamespace*, uint64_t pageID);
    ~WebPageProxy();

    uint64_t pageID() const { return m_pageID; }

    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }

    DrawingAreaProxy* drawingArea() { return m_drawingArea.get(); }
    void setDrawingArea(PassOwnPtr<DrawingAreaProxy>);

    WebBackForwardList* backForwardList() { return m_backForwardList.get(); }

    void setPageClient(PageClient*);
    void initializeLoaderClient(const WKPageLoaderClient*);
    void initializePolicyClient(const WKPagePolicyClient*);
    void initializeFormClient(const WKPageFormClient*);
    void initializeUIClient(const WKPageUIClient*);

    void revive();

    void initializeWebPage(const WebCore::IntSize&, PassOwnPtr<DrawingAreaProxy>);
    void reinitializeWebPage(const WebCore::IntSize&);

    void close();
    bool tryClose();
    bool isClosed() const { return m_closed; }

    void loadURL(const WTF::String&);
    void loadURLRequest(WebURLRequest*);

    void stopLoading();
    void reload(bool reloadFromOrigin);

    void goForward();
    bool canGoForward() const;
    void goBack();
    bool canGoBack() const;

    void goToBackForwardItem(WebBackForwardListItem*);
    void didChangeBackForwardList();

    void setFocused(bool isFocused);
    void setActive(bool active);
    void setIsInWindow(bool isInWindow);

    void mouseEvent(const WebMouseEvent&);
    void wheelEvent(const WebWheelEvent&);
    void keyEvent(const WebKeyboardEvent&);
#if ENABLE(TOUCH_EVENTS)
    void touchEvent(const WebTouchEvent&);
#endif

    const WTF::String& pageTitle() const { return m_pageTitle; }
    const WTF::String& toolTip() const { return m_toolTip; }

    double estimatedProgress() const { return m_estimatedProgress; }

    void setCustomUserAgent(const String&);

    void terminateProcess();
    
    PassRefPtr<WebData> sessionState() const;
    void restoreFromSessionState(WebData*);

    double textZoomFactor() const { return m_textZoomFactor; }
    void setTextZoomFactor(double);
    double pageZoomFactor() const { return m_pageZoomFactor; }
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);

    void runJavaScriptInMainFrame(const WTF::String&, PassRefPtr<ScriptReturnValueCallback>);
    void getRenderTreeExternalRepresentation(PassRefPtr<RenderTreeExternalRepresentationCallback>);

    void receivedPolicyDecision(WebCore::PolicyAction, WebFrameProxy*, uint64_t listenerID);

    void didReceiveMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    void didReceiveSyncMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    void processDidBecomeUnresponsive();
    void processDidBecomeResponsive();
    void processDidExit();
    void processDidRevive();

#if USE(ACCELERATED_COMPOSITING)
    void didEnterAcceleratedCompositing();
    void didLeaveAcceleratedCompositing();
#endif

    void addEditCommand(WebEditCommandProxy*);
    void removeEditCommand(WebEditCommandProxy*);
    void registerEditCommandForUndo(PassRefPtr<WebEditCommandProxy>);
    void registerEditCommandForRedo(PassRefPtr<WebEditCommandProxy>);

    WebProcessProxy* process() const;
    WebPageNamespace* pageNamespace() const { return m_pageNamespace.get(); }

    bool isValid();

    // REMOVE: For demo purposes only.
    const WTF::String& urlAtProcessExit() const { return m_urlAtProcessExit; }

    void preferencesDidChange();

    void getStatistics(WKContextStatistics*);

private:
    WebPageProxy(WebPageNamespace*, uint64_t pageID);

    virtual Type type() const { return APIType; }

    void didCreateMainFrame(uint64_t frameID);
    void didCreateSubFrame(uint64_t frameID);

    void didStartProvisionalLoadForFrame(WebFrameProxy*, const WTF::String&);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebFrameProxy*);
    void didFailProvisionalLoadForFrame(WebFrameProxy*);
    void didCommitLoadForFrame(WebFrameProxy*);
    void didFinishDocumentLoadForFrame(WebFrameProxy*);
    void didFinishLoadForFrame(WebFrameProxy*);
    void didFailLoadForFrame(WebFrameProxy*);
    void didReceiveTitleForFrame(WebFrameProxy*, const WTF::String&);
    void didFirstLayoutForFrame(WebFrameProxy*);
    void didFirstVisuallyNonEmptyLayoutForFrame(WebFrameProxy*);
    void didStartProgress();
    void didChangeProgress(double);
    void didFinishProgress();
    
    void decidePolicyForNavigationAction(WebFrameProxy*, WebCore::NavigationType navigationType, WebEvent::Modifiers modifiers, const WTF::String& url, uint64_t listenerID);
    void decidePolicyForNewWindowAction(WebFrameProxy*, WebCore::NavigationType navigationType, WebEvent::Modifiers modifiers, const WTF::String& url, uint64_t listenerID);
    void decidePolicyForMIMEType(WebFrameProxy*, const WTF::String& MIMEType, const WTF::String& url, uint64_t listenerID);

    void willSubmitForm(WebFrameProxy* frame, WebFrameProxy* frameSource, Vector<std::pair<WTF::String, WTF::String> >& textFieldValues, APIObject* userData, uint64_t listenerID);

    PassRefPtr<WebPageProxy> createNewPage();
    void showPage();
    void closePage();
    void runJavaScriptAlert(WebFrameProxy*, const WTF::String&);
    bool runJavaScriptConfirm(WebFrameProxy* frame, const WTF::String&);
    WTF::String runJavaScriptPrompt(WebFrameProxy* frame, const WTF::String&, const WTF::String&);
    void contentsSizeChanged(WebFrameProxy*, const WebCore::IntSize&);

    // Back/Forward list management
    void addItemToBackForwardList(WebBackForwardListItem*);
    void goToItemInBackForwardList(WebBackForwardListItem*);

    // Undo management
    void registerEditCommandForUndo(uint64_t commandID, WebCore::EditAction);
    void clearAllEditCommands();

    void takeFocus(bool direction);
    void setToolTip(const WTF::String&);
    void setCursor(const WebCore::Cursor&);

    void didReceiveEvent(WebEvent::Type);
    void didRunJavaScriptInMainFrame(const WTF::String&, uint64_t);
    void didGetRenderTreeExternalRepresentation(const WTF::String&, uint64_t);

#if USE(ACCELERATED_COMPOSITING)
    void didChangeAcceleratedCompositing(bool compositing);
#endif    

    PageClient* m_pageClient;
    WebLoaderClient m_loaderClient;
    WebPolicyClient m_policyClient;
    WebFormClient m_formClient;
    WebUIClient m_uiClient;

    OwnPtr<DrawingAreaProxy> m_drawingArea;
    RefPtr<WebPageNamespace> m_pageNamespace;
    RefPtr<WebFrameProxy> m_mainFrame;
    WTF::String m_pageTitle;

    HashMap<uint64_t, RefPtr<ScriptReturnValueCallback> > m_scriptReturnValueCallbacks;
    HashMap<uint64_t, RefPtr<RenderTreeExternalRepresentationCallback> > m_renderTreeExternalRepresentationCallbacks;

    HashSet<WebEditCommandProxy*> m_editCommandSet;

    double m_estimatedProgress;

    // Whether the web page is contained in a top-level window.
    bool m_isInWindow;

    bool m_canGoBack;
    bool m_canGoForward;
    RefPtr<WebBackForwardList> m_backForwardList;

    WTF::String m_toolTip;

    // REMOVE: For demo purposes only.
    WTF::String m_urlAtProcessExit;

    double m_textZoomFactor;
    double m_pageZoomFactor;
    
    bool m_valid;
    bool m_closed;

    uint64_t m_pageID;
};

} // namespace WebKit

#endif // WebPageProxy_h
