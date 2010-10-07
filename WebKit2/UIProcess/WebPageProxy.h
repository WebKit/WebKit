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
#include "FindOptions.h"
#include "GenericCallback.h"
#include "WKBase.h"
#include "WebEvent.h"
#include "WebFindClient.h"
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
class NativeWebKeyboardEvent;
class PageClient;
class PlatformCertificateInfo;
class StringPairVector;
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
struct WebPageCreationParameters;

typedef GenericCallback<WKStringRef, StringImpl*> FrameSourceCallback;
typedef GenericCallback<WKStringRef, StringImpl*> RenderTreeExternalRepresentationCallback;
typedef GenericCallback<WKStringRef, StringImpl*> ScriptReturnValueCallback;

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
    void initializeFindClient(const WKPageFindClient*);
    void revive();

    void initializeWebPage(const WebCore::IntSize&);
    void reinitializeWebPage(const WebCore::IntSize&);

    void close();
    bool tryClose();
    bool isClosed() const { return m_closed; }

    void loadURL(const String&);
    void loadURLRequest(WebURLRequest*);
    void loadHTMLString(const String& htmlString, const String& baseURL);
    void loadPlainTextString(const String& string);

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
    void setWindowResizerSize(const WebCore::IntSize&);

    void executeEditCommand(const String& commandName);
    void validateMenuItem(const String& commandName);

// These are only used on Mac currently.
#if PLATFORM(MAC)
    void setWindowIsVisible(bool windowIsVisible);
    void setWindowFrame(const WebCore::IntRect&);
#endif

    void handleMouseEvent(const WebMouseEvent&);
    void handleWheelEvent(const WebWheelEvent&);
    void handleKeyboardEvent(const NativeWebKeyboardEvent&);
#if ENABLE(TOUCH_EVENTS)
    void handleTouchEvent(const WebTouchEvent&);
#endif

    const String& pageTitle() const { return m_pageTitle; }
    const String& toolTip() const { return m_toolTip; }

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

    // Find.
    void findString(const String&, FindDirection, FindOptions, unsigned maxNumMatches);
    void hideFindUI();
    void countStringMatches(const String&, bool caseInsensitive, unsigned maxNumMatches);

    void runJavaScriptInMainFrame(const String&, PassRefPtr<ScriptReturnValueCallback>);
    void getRenderTreeExternalRepresentation(PassRefPtr<RenderTreeExternalRepresentationCallback>);
    void getSourceForFrame(WebFrameProxy*, PassRefPtr<FrameSourceCallback>);

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

    enum UndoOrRedo { Undo, Redo };
    void addEditCommand(WebEditCommandProxy*);
    void removeEditCommand(WebEditCommandProxy*);
    void registerEditCommand(PassRefPtr<WebEditCommandProxy>, UndoOrRedo);

    WebProcessProxy* process() const;
    WebPageNamespace* pageNamespace() const { return m_pageNamespace.get(); }

    bool isValid();

    // REMOVE: For demo purposes only.
    const String& urlAtProcessExit() const { return m_urlAtProcessExit; }

    void preferencesDidChange();

    void getStatistics(WKContextStatistics*);

private:
    WebPageProxy(WebPageNamespace*, uint64_t pageID);

    virtual Type type() const { return APIType; }

    // Implemented in generated WebPageProxyMessageReceiver.cpp
    void didReceiveWebPageProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*);
    CoreIPC::SyncReplyMode didReceiveSyncWebPageProxyMessage(CoreIPC::Connection*, CoreIPC::MessageID, CoreIPC::ArgumentDecoder*, CoreIPC::ArgumentEncoder*);

    void didCreateMainFrame(uint64_t frameID);
    void didCreateSubFrame(uint64_t frameID);

    void didStartProvisionalLoadForFrame(uint64_t frameID, const String&, CoreIPC::ArgumentDecoder*);
    void didReceiveServerRedirectForProvisionalLoadForFrame(uint64_t frameID, const String&, CoreIPC::ArgumentDecoder*);
    void didFailProvisionalLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didCommitLoadForFrame(uint64_t frameID, const String& mimeType, const PlatformCertificateInfo&, CoreIPC::ArgumentDecoder*);
    void didFinishDocumentLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFinishLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFailLoadForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didReceiveTitleForFrame(uint64_t frameID, const String&, CoreIPC::ArgumentDecoder*);
    void didFirstLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didFirstVisuallyNonEmptyLayoutForFrame(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didRemoveFrameFromHierarchy(uint64_t frameID, CoreIPC::ArgumentDecoder*);
    void didStartProgress();
    void didChangeProgress(double);
    void didFinishProgress();
    
    void decidePolicyForNavigationAction(uint64_t frameID, uint32_t navigationType, uint32_t modifiers, int32_t mouseButton, const String& url, uint64_t listenerID);
    void decidePolicyForNewWindowAction(uint64_t frameID, uint32_t navigationType, uint32_t modifiers, int32_t mouseButton, const String& url, uint64_t listenerID);
    void decidePolicyForMIMEType(uint64_t frameID, const String& MIMEType, const String& url, uint64_t listenerID);

    void willSubmitForm(uint64_t frameID, uint64_t sourceFrameID, const StringPairVector& textFieldValues, uint64_t listenerID, CoreIPC::ArgumentDecoder*);

    void createNewPage(uint64_t& newPageID, WebPageCreationParameters&);
    void showPage();
    void closePage();
    void runJavaScriptAlert(uint64_t frameID, const String&);
    void runJavaScriptConfirm(uint64_t frameID, const String&, bool& result);
    void runJavaScriptPrompt(uint64_t frameID, const String&, const String&, String& result);
    void setStatusText(const String&);
    void mouseDidMoveOverElement(uint32_t modifiers, CoreIPC::ArgumentDecoder*);
    void contentsSizeChanged(uint64_t frameID, const WebCore::IntSize&);

    // Back/Forward list management
    void backForwardAddItem(uint64_t itemID);
    void backForwardGoToItem(uint64_t itemID);
    void backForwardBackItem(uint64_t& itemID);
    void backForwardCurrentItem(uint64_t& itemID);
    void backForwardForwardItem(uint64_t& itemID);
    void backForwardItemAtIndex(int32_t index, uint64_t& itemID);
    void backForwardBackListCount(int32_t& count);
    void backForwardForwardListCount(int32_t& count);

    // Undo management
    void registerEditCommandForUndo(uint64_t commandID, uint32_t editAction);
    void clearAllEditCommands();

    // Find.
    void didCountStringMatches(const String&, uint32_t numMatches);

    void takeFocus(bool direction);
    void setToolTip(const String&);
    void setCursor(const WebCore::Cursor&);
    void didValidateMenuItem(const String& commandName, bool isEnabled, int32_t state);

    void didReceiveEvent(uint32_t opaqueType, bool handled);

    void didRunJavaScriptInMainFrame(const String&, uint64_t);
    void didGetRenderTreeExternalRepresentation(const String&, uint64_t);
    void didGetSourceForFrame(const String&, uint64_t);

    WebPageCreationParameters creationParameters(const WebCore::IntSize&) const;

#if USE(ACCELERATED_COMPOSITING)
    void didChangeAcceleratedCompositing(bool compositing, DrawingAreaBase::DrawingAreaInfo&);
#endif    

    PageClient* m_pageClient;
    WebLoaderClient m_loaderClient;
    WebPolicyClient m_policyClient;
    WebFormClient m_formClient;
    WebUIClient m_uiClient;
    WebFindClient m_findClient;

    OwnPtr<DrawingAreaProxy> m_drawingArea;
    RefPtr<WebPageNamespace> m_pageNamespace;
    RefPtr<WebFrameProxy> m_mainFrame;
    String m_pageTitle;

    HashMap<uint64_t, RefPtr<ScriptReturnValueCallback> > m_scriptReturnValueCallbacks;
    HashMap<uint64_t, RefPtr<RenderTreeExternalRepresentationCallback> > m_renderTreeExternalRepresentationCallbacks;
    HashMap<uint64_t, RefPtr<FrameSourceCallback> > m_frameSourceCallbacks;

    HashSet<WebEditCommandProxy*> m_editCommandSet;

    double m_estimatedProgress;

    // Whether the web page is contained in a top-level window.
    bool m_isInWindow;

    bool m_canGoBack;
    bool m_canGoForward;
    RefPtr<WebBackForwardList> m_backForwardList;

    String m_toolTip;

    // REMOVE: For demo purposes only.
    String m_urlAtProcessExit;

    double m_textZoomFactor;
    double m_pageZoomFactor;
    
    bool m_valid;
    bool m_closed;

    uint64_t m_pageID;

    Deque<NativeWebKeyboardEvent> m_keyEventQueue;
};

} // namespace WebKit

#endif // WebPageProxy_h
