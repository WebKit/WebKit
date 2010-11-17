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

#ifndef WebViewHost_h
#define WebViewHost_h

#include "MockSpellCheck.h"
#include "TestNavigationController.h"
#include "WebAccessibilityNotification.h"
#include "WebCursorInfo.h"
#include "WebFrameClient.h"
#include "WebViewClient.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

class LayoutTestController;
class TestShell;
namespace WebKit {
class WebFrame;
class WebDeviceOrientationClient;
class WebDeviceOrientationClientMock;
class WebGeolocationServiceMock;
class WebSpeechInputController;
class WebSpeechInputControllerMock;
class WebSpeechInputListener;
class WebURL;
struct WebRect;
struct WebURLError;
struct WebWindowFeatures;
}
namespace skia {
class PlatformCanvas;
}

class WebViewHost : public WebKit::WebViewClient, public WebKit::WebFrameClient, public NavigationHost {
 public:
    WebViewHost(TestShell* shell);
    ~WebViewHost();
    void setWebWidget(WebKit::WebWidget* widget) { m_webWidget = widget; }
    WebKit::WebView* webView() const;
    WebKit::WebWidget* webWidget() const;
    void reset();
    void setSelectTrailingWhitespaceEnabled(bool);
    void setSmartInsertDeleteEnabled(bool);
    void waitForPolicyDelegate();
    void setCustomPolicyDelegate(bool, bool);
    WebKit::WebFrame* topLoadingFrame() { return m_topLoadingFrame; }
    void setBlockRedirects(bool block) { m_blocksRedirects = block; }
    void setRequestReturnNull(bool returnNull) { m_requestReturnNull = returnNull; }
    void setEditCommand(const std::string& name, const std::string& value);
    void clearEditCommand();
    void setPendingExtraData(TestShellExtraData*);

    void paintRect(const WebKit::WebRect&);
    void updatePaintRect(const WebKit::WebRect&);
    void paintInvalidatedRegion();
    skia::PlatformCanvas* canvas();
    void displayRepaintMask();

    void loadURLForFrame(const WebKit::WebURL&, const WebKit::WebString& frameName);
    TestNavigationController* navigationController() { return m_navigationController.get(); }

    void addClearHeader(const WTF::String& header) { m_clearHeaders.add(header); }
    const HashSet<WTF::String>& clearHeaders() const { return m_clearHeaders; }
    void closeWidget();

    WebKit::WebContextMenuData* lastContextMenuData() const;
    void clearContextMenuData();

    WebKit::WebSpeechInputControllerMock* speechInputControllerMock() { return m_speechInputControllerMock.get(); }

    // NavigationHost
    virtual bool navigate(const TestNavigationEntry&, bool reload);

    // WebKit::WebViewClient
    virtual WebKit::WebView* createView(WebKit::WebFrame*, const WebKit::WebWindowFeatures&, const WebKit::WebString&);
    virtual WebKit::WebWidget* createPopupMenu(WebKit::WebPopupType);
    virtual WebKit::WebWidget* createPopupMenu(const WebKit::WebPopupMenuInfo&);
    virtual WebKit::WebStorageNamespace* createSessionStorageNamespace(unsigned quota);
    virtual void didAddMessageToConsole(const WebKit::WebConsoleMessage&, const WebKit::WebString& sourceName, unsigned sourceLine);
    virtual void didStartLoading();
    virtual void didStopLoading();
    virtual bool shouldBeginEditing(const WebKit::WebRange&);
    virtual bool shouldEndEditing(const WebKit::WebRange&);
    virtual bool shouldInsertNode(const WebKit::WebNode&, const WebKit::WebRange&, WebKit::WebEditingAction);
    virtual bool shouldInsertText(const WebKit::WebString&, const WebKit::WebRange&, WebKit::WebEditingAction);
    virtual bool shouldChangeSelectedRange(const WebKit::WebRange& from, const WebKit::WebRange& to, WebKit::WebTextAffinity, bool stillSelecting);
    virtual bool shouldDeleteRange(const WebKit::WebRange&);
    virtual bool shouldApplyStyle(const WebKit::WebString& style, const WebKit::WebRange&);
    virtual bool isSmartInsertDeleteEnabled();
    virtual bool isSelectTrailingWhitespaceEnabled();
    virtual void didBeginEditing();
    virtual void didChangeSelection(bool isSelectionEmpty);
    virtual void didChangeContents();
    virtual void didEndEditing();
    virtual bool handleCurrentKeyboardEvent();
    virtual void spellCheck(const WebKit::WebString&, int& offset, int& length);
    virtual WebKit::WebString autoCorrectWord(const WebKit::WebString&);
    virtual void runModalAlertDialog(WebKit::WebFrame*, const WebKit::WebString&);
    virtual bool runModalConfirmDialog(WebKit::WebFrame*, const WebKit::WebString&);
    virtual bool runModalPromptDialog(WebKit::WebFrame*, const WebKit::WebString& message, const WebKit::WebString& defaultValue, WebKit::WebString* actualValue);
    virtual bool runModalBeforeUnloadDialog(WebKit::WebFrame*, const WebKit::WebString&);
    virtual void showContextMenu(WebKit::WebFrame*, const WebKit::WebContextMenuData&);
    virtual void setStatusText(const WebKit::WebString&);
    virtual void startDragging(const WebKit::WebDragData&, WebKit::WebDragOperationsMask, const WebKit::WebImage&, const WebKit::WebPoint&);
    virtual void navigateBackForwardSoon(int offset);
    virtual int historyBackListCount();
    virtual int historyForwardListCount();
    virtual void postAccessibilityNotification(const WebKit::WebAccessibilityObject&, WebKit::WebAccessibilityNotification);
    virtual WebKit::WebNotificationPresenter* notificationPresenter();
#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    virtual WebKit::WebGeolocationService* geolocationService();
#endif
    virtual WebKit::WebSpeechInputController* speechInputController(WebKit::WebSpeechInputListener*);
    virtual WebKit::WebDeviceOrientationClient* deviceOrientationClient();

    // WebKit::WebWidgetClient
    virtual void didInvalidateRect(const WebKit::WebRect&);
    virtual void didScrollRect(int dx, int dy, const WebKit::WebRect&);
    virtual void scheduleComposite();
    virtual void didFocus();
    virtual void didBlur();
    virtual void didChangeCursor(const WebKit::WebCursorInfo&);
    virtual void closeWidgetSoon();
    virtual void show(WebKit::WebNavigationPolicy);
    virtual void runModal();
    virtual WebKit::WebRect windowRect();
    virtual void setWindowRect(const WebKit::WebRect&);
    virtual WebKit::WebRect rootWindowRect();
    virtual WebKit::WebRect windowResizerRect();
    virtual WebKit::WebScreenInfo screenInfo();

    // WebKit::WebFrameClient
    virtual WebKit::WebPlugin* createPlugin(WebKit::WebFrame*, const WebKit::WebPluginParams&);
    virtual WebKit::WebWorker* createWorker(WebKit::WebFrame*, WebKit::WebWorkerClient*);
    virtual WebKit::WebMediaPlayer* createMediaPlayer(WebKit::WebFrame*, WebKit::WebMediaPlayerClient*);
    virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(WebKit::WebFrame*, WebKit::WebApplicationCacheHostClient*);
     virtual bool allowPlugins(WebKit::WebFrame*, bool enabledPerSettings);
    virtual bool allowImages(WebKit::WebFrame*, bool enabledPerSettings);
    virtual void loadURLExternally(WebKit::WebFrame*, const WebKit::WebURLRequest&, WebKit::WebNavigationPolicy);
    virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
        WebKit::WebFrame*, const WebKit::WebURLRequest&,
        WebKit::WebNavigationType, const WebKit::WebNode&,
        WebKit::WebNavigationPolicy, bool isRedirect);
    virtual bool canHandleRequest(WebKit::WebFrame*, const WebKit::WebURLRequest&);
    virtual WebKit::WebURLError cannotHandleRequestError(WebKit::WebFrame*, const WebKit::WebURLRequest&);
    virtual WebKit::WebURLError cancelledError(WebKit::WebFrame*, const WebKit::WebURLRequest&);
    virtual void unableToImplementPolicyWithError(WebKit::WebFrame*, const WebKit::WebURLError&);
    virtual void willPerformClientRedirect(
        WebKit::WebFrame*, const WebKit::WebURL& from, const WebKit::WebURL& to,
        double interval, double fireTime);
    virtual void didCancelClientRedirect(WebKit::WebFrame*);
    virtual void didCreateDataSource(WebKit::WebFrame*, WebKit::WebDataSource*);
    virtual void didStartProvisionalLoad(WebKit::WebFrame*);
    virtual void didReceiveServerRedirectForProvisionalLoad(WebKit::WebFrame*);
    virtual void didFailProvisionalLoad(WebKit::WebFrame*, const WebKit::WebURLError&);
    virtual void didCommitProvisionalLoad(WebKit::WebFrame*, bool isNewNavigation);
    virtual void didClearWindowObject(WebKit::WebFrame*);
    virtual void didReceiveTitle(WebKit::WebFrame*, const WebKit::WebString&);
    virtual void didFinishDocumentLoad(WebKit::WebFrame*);
    virtual void didHandleOnloadEvents(WebKit::WebFrame*);
    virtual void didFailLoad(WebKit::WebFrame*, const WebKit::WebURLError&);
    virtual void didFinishLoad(WebKit::WebFrame*);
    virtual void didNavigateWithinPage(WebKit::WebFrame*, bool isNewNavigation);
    virtual void didChangeLocationWithinPage(WebKit::WebFrame*);
    virtual void assignIdentifierToRequest(WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLRequest&);
    virtual void removeIdentifierForRequest(unsigned identifier);
    virtual void willSendRequest(WebKit::WebFrame*, unsigned identifier, WebKit::WebURLRequest&, const WebKit::WebURLResponse&);
    virtual void didReceiveResponse(WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLResponse&);
    virtual void didFinishResourceLoad(WebKit::WebFrame*, unsigned identifier);
    virtual void didFailResourceLoad(WebKit::WebFrame*, unsigned identifier, const WebKit::WebURLError&);
    virtual void didDisplayInsecureContent(WebKit::WebFrame*);
    virtual void didRunInsecureContent(WebKit::WebFrame*, const WebKit::WebSecurityOrigin&);
    virtual bool allowScript(WebKit::WebFrame*, bool enabledPerSettings);
    virtual void openFileSystem(WebKit::WebFrame*, WebKit::WebFileSystem::Type, long long size, bool create, WebKit::WebFileSystemCallbacks*);

    WebKit::WebDeviceOrientationClientMock* deviceOrientationClientMock();
    MockSpellCheck* mockSpellCheck();

private:
    LayoutTestController* layoutTestController() const;

    // Called the title of the page changes.
    // Can be used to update the title of the window.
    void setPageTitle(const WebKit::WebString&);

    // Called when the URL of the page changes.
    // Extracts the URL and forwards on to SetAddressBarURL().
    void updateAddressBar(WebKit::WebView*);

    // Called when the URL of the page changes.
    // Should be used to update the text of the URL bar.
    void setAddressBarURL(const WebKit::WebURL&);

    // In the Mac code, this is called to trigger the end of a test after the
    // page has finished loading.  From here, we can generate the dump for the
    // test.
    void locationChangeDone(WebKit::WebFrame*);

    void updateForCommittedLoad(WebKit::WebFrame*, bool isNewNavigation);
    void updateURL(WebKit::WebFrame*);
    void updateSessionHistory(WebKit::WebFrame*);

    // Dumping a frame to the console.
    void printFrameDescription(WebKit::WebFrame*);

    // Dumping the user gesture status to the console.
    void printFrameUserGestureStatus(WebKit::WebFrame*, const char*);

    bool hasWindow() const { return m_hasWindow; }
    void resetScrollRect();
    void discardBackingStore();

    // Causes navigation actions just printout the intended navigation instead
    // of taking you to the page. This is used for cases like mailto, where you
    // don't actually want to open the mail program.
    bool m_policyDelegateEnabled;

    // Toggles the behavior of the policy delegate.  If true, then navigations
    // will be allowed.  Otherwise, they will be ignored (dropped).
    bool m_policyDelegateIsPermissive;

    // If true, the policy delegate will signal layout test completion.
    bool m_policyDelegateShouldNotifyDone;

    // Non-owning pointer. The WebViewHost instance is owned by this TestShell instance.
    TestShell* m_shell;

    // This delegate works for the following widget.
    WebKit::WebWidget* m_webWidget;

    // This is non-0 IFF a load is in progress.
    WebKit::WebFrame* m_topLoadingFrame;

    // For tracking session history.  See RenderView.
    int m_pageId;
    int m_lastPageIdUpdated;

    OwnPtr<TestShellExtraData> m_pendingExtraData;

    // Maps resource identifiers to a descriptive string.
    typedef HashMap<unsigned, std::string> ResourceMap;
    ResourceMap m_resourceIdentifierMap;
    void printResourceDescription(unsigned identifier);

    WebKit::WebCursorInfo m_currentCursor;

    bool m_hasWindow;
    bool m_inModalLoop;
    WebKit::WebRect m_windowRect;

    // true if we want to enable smart insert/delete.
    bool m_smartInsertDeleteEnabled;

    // true if we want to enable selection of trailing whitespaces
    bool m_selectTrailingWhitespaceEnabled;

    // Set of headers to clear in willSendRequest.
    HashSet<WTF::String> m_clearHeaders;

    // true if we should block any redirects
    bool m_blocksRedirects;

    // true if we should block (set an empty request for) any requests
    bool m_requestReturnNull;

    // Edit command associated to the current keyboard event.
    std::string m_editCommandName;
    std::string m_editCommandValue;

    // The mock spellchecker used in spellCheck().
    MockSpellCheck m_spellcheck;

    // Painting.
    OwnPtr<skia::PlatformCanvas> m_canvas;
    WebKit::WebRect m_paintRect;
    bool m_isPainting;

    OwnPtr<WebKit::WebContextMenuData> m_lastContextMenuData;

#if !ENABLE(CLIENT_BASED_GEOLOCATION)
    // Geolocation
    OwnPtr<WebKit::WebGeolocationServiceMock> m_geolocationServiceMock;
#endif

    OwnPtr<WebKit::WebDeviceOrientationClientMock> m_deviceOrientationClientMock;
    OwnPtr<WebKit::WebSpeechInputControllerMock> m_speechInputControllerMock;

    OwnPtr<TestNavigationController*> m_navigationController;
};

#endif // WebViewHost_h
