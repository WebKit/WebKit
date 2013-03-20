/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
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

#include "TestNavigationController.h"
#include "WebCursorInfo.h"
#include "WebFrameClient.h"
#include "WebPrerendererClient.h"
#include "WebTask.h"
#include "WebTestDelegate.h"
#include "WebTestProxy.h"
#include "WebViewClient.h"
#include <public/WebFileSystemType.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

class SkCanvas;
class TestShell;

namespace WebKit {
class WebFrame;
class WebSerializedScriptValue;
class WebSharedWorkerClient;
class WebURL;
struct WebRect;
struct WebURLError;
struct WebWindowFeatures;
}

namespace webkit_support {
class DRTLayerTreeViewClient;
class MediaStreamUtil;
class TestMediaStreamClient;
}

namespace WebTestRunner {
class WebTestRunner;
}

class WebViewHost : public WebKit::WebViewClient, public WebKit::WebFrameClient, public NavigationHost, public WebKit::WebPrerendererClient, public WebTestRunner::WebTestDelegate {
 public:
    WebViewHost(TestShell*);
    virtual ~WebViewHost();
    void shutdown();
    void setWebWidget(WebKit::WebWidget*);
    WebKit::WebView* webView() const;
    WebKit::WebWidget* webWidget() const;
    WebTestRunner::WebTestProxyBase* proxy() const;
    void setProxy(WebTestRunner::WebTestProxyBase*);
    void reset();
    void setPendingExtraData(PassOwnPtr<TestShellExtraData>);

    TestNavigationController* navigationController() { return m_navigationController.get(); }

    void closeWidget();

    // WebTestDelegate.
    virtual void setEditCommand(const std::string& name, const std::string& value) OVERRIDE;
    virtual void clearEditCommand() OVERRIDE;
    virtual void setGamepadData(const WebKit::WebGamepads&) OVERRIDE;
    virtual void printMessage(const std::string& message) OVERRIDE;
    virtual void postTask(WebTestRunner::WebTask*) OVERRIDE;
    virtual void postDelayedTask(WebTestRunner::WebTask*, long long ms) OVERRIDE;
    virtual WebKit::WebString registerIsolatedFileSystem(const WebKit::WebVector<WebKit::WebString>& absoluteFilenames) OVERRIDE;
    virtual long long getCurrentTimeInMillisecond() OVERRIDE;
    virtual WebKit::WebString getAbsoluteWebStringFromUTF8Path(const std::string& path) OVERRIDE;
    virtual WebKit::WebURL localFileToDataURL(const WebKit::WebURL&) OVERRIDE;
    virtual WebKit::WebURL rewriteLayoutTestsURL(const std::string&) OVERRIDE;
    virtual WebTestRunner::WebPreferences* preferences() OVERRIDE;
    virtual void applyPreferences() OVERRIDE;
    virtual std::string makeURLErrorDescription(const WebKit::WebURLError&) OVERRIDE;
    virtual void setClientWindowRect(const WebKit::WebRect&) OVERRIDE;
    virtual void showDevTools() OVERRIDE;
    virtual void closeDevTools() OVERRIDE;
    virtual void evaluateInWebInspector(long, const std::string&) OVERRIDE;
    virtual void clearAllDatabases() OVERRIDE;
    virtual void setDatabaseQuota(int) OVERRIDE;
    virtual void setDeviceScaleFactor(float) OVERRIDE;
    virtual void setFocus(WebTestRunner::WebTestProxyBase*, bool) OVERRIDE;
    virtual void setAcceptAllCookies(bool) OVERRIDE;
    virtual std::string pathToLocalResource(const std::string& url) OVERRIDE;
    virtual void setLocale(const std::string&) OVERRIDE;
    virtual void testFinished() OVERRIDE;
    virtual void testTimedOut() OVERRIDE;
    virtual bool isBeingDebugged() OVERRIDE;
    virtual int layoutTestTimeout() OVERRIDE;
    virtual void closeRemainingWindows() OVERRIDE;
    virtual int navigationEntryCount() OVERRIDE;
    virtual void goToOffset(int) OVERRIDE;
    virtual void reload() OVERRIDE;
    virtual void loadURLForFrame(const WebKit::WebURL&, const std::string& frameName) OVERRIDE;
    virtual bool allowExternalPages() OVERRIDE;
    virtual void captureHistoryForWindow(WebTestRunner::WebTestProxyBase*, WebKit::WebVector<WebKit::WebHistoryItem>*, size_t* currentEntryIndex) OVERRIDE;

    // NavigationHost
    virtual bool navigate(const TestNavigationEntry&, bool reload);

    // WebKit::WebPrerendererClient
    virtual void willAddPrerender(WebKit::WebPrerender*) OVERRIDE;

    // WebKit::WebViewClient
    virtual WebKit::WebView* createView(WebKit::WebFrame*, const WebKit::WebURLRequest&, const WebKit::WebWindowFeatures&, const WebKit::WebString&, WebKit::WebNavigationPolicy);
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
    virtual bool handleCurrentKeyboardEvent();
    virtual void runModalAlertDialog(WebKit::WebFrame*, const WebKit::WebString&);
    virtual bool runModalConfirmDialog(WebKit::WebFrame*, const WebKit::WebString&);
    virtual bool runModalPromptDialog(WebKit::WebFrame*, const WebKit::WebString& message, const WebKit::WebString& defaultValue, WebKit::WebString* actualValue);
    virtual void showContextMenu(WebKit::WebFrame*, const WebKit::WebContextMenuData&);
    virtual void didUpdateLayout();
    virtual void navigateBackForwardSoon(int offset);
    virtual int historyBackListCount();
    virtual int historyForwardListCount();

    // WebKit::WebWidgetClient
    virtual void didAutoResize(const WebKit::WebSize& newSize);
    virtual void initializeLayerTreeView();
    virtual WebKit::WebLayerTreeView* layerTreeView();
    virtual void scheduleAnimation();
    virtual void didFocus();
    virtual void didBlur();
    virtual void didChangeCursor(const WebKit::WebCursorInfo&);
    virtual void closeWidgetSoon();
    virtual void show(WebKit::WebNavigationPolicy);
    virtual void runModal();
    virtual bool enterFullScreen();
    virtual void exitFullScreen();
    virtual WebKit::WebRect windowRect();
    virtual void setWindowRect(const WebKit::WebRect&);
    virtual WebKit::WebRect rootWindowRect();
    virtual WebKit::WebRect windowResizerRect();
    virtual WebKit::WebScreenInfo screenInfo();

    // WebKit::WebFrameClient
    virtual WebKit::WebPlugin* createPlugin(WebKit::WebFrame*, const WebKit::WebPluginParams&);
    virtual WebKit::WebMediaPlayer* createMediaPlayer(WebKit::WebFrame*, const WebKit::WebURL&, WebKit::WebMediaPlayerClient*);
    virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(WebKit::WebFrame*, WebKit::WebApplicationCacheHostClient*);
    virtual void loadURLExternally(WebKit::WebFrame*, const WebKit::WebURLRequest&, WebKit::WebNavigationPolicy);
    virtual void loadURLExternally(WebKit::WebFrame*, const WebKit::WebURLRequest&, WebKit::WebNavigationPolicy, const WebKit::WebString& downloadName);
    virtual WebKit::WebNavigationPolicy decidePolicyForNavigation(
        WebKit::WebFrame*, const WebKit::WebURLRequest&,
        WebKit::WebNavigationType, const WebKit::WebNode&,
        WebKit::WebNavigationPolicy, bool isRedirect);
    virtual bool canHandleRequest(WebKit::WebFrame*, const WebKit::WebURLRequest&);
    virtual WebKit::WebURLError cancelledError(WebKit::WebFrame*, const WebKit::WebURLRequest&);
    virtual void unableToImplementPolicyWithError(WebKit::WebFrame*, const WebKit::WebURLError&);
    virtual void didCreateDataSource(WebKit::WebFrame*, WebKit::WebDataSource*);
    virtual void didCommitProvisionalLoad(WebKit::WebFrame*, bool isNewNavigation);
    virtual void didClearWindowObject(WebKit::WebFrame*);
    virtual void didReceiveTitle(WebKit::WebFrame*, const WebKit::WebString&, WebKit::WebTextDirection);
    virtual void didChangeIcon(WebKit::WebFrame* , WebKit::WebIconURL::Type);
    virtual void didNavigateWithinPage(WebKit::WebFrame*, bool isNewNavigation);
    virtual void willSendRequest(WebKit::WebFrame*, unsigned identifier, WebKit::WebURLRequest&, const WebKit::WebURLResponse&);
    virtual void openFileSystem(WebKit::WebFrame*, WebKit::WebFileSystemType, long long size, bool create, WebKit::WebFileSystemCallbacks*);
    virtual void deleteFileSystem(WebKit::WebFrame*, WebKit::WebFileSystemType, WebKit::WebFileSystemCallbacks*);
    virtual bool willCheckAndDispatchMessageEvent(
        WebKit::WebFrame* sourceFrame, WebKit::WebFrame* targetFrame, 
        WebKit::WebSecurityOrigin target, WebKit::WebDOMMessageEvent);

    // Pending task list, Note taht the method is referred from WebMethodTask class.
    WebTestRunner::WebTaskList* taskList() { return &m_taskList; }

    // Exposed for WebTestProxy.
    void scheduleComposite() { }

private:

    class HostMethodTask : public WebTestRunner::WebMethodTask<WebViewHost> {
    public:
        typedef void (WebViewHost::*CallbackMethodType)();
        HostMethodTask(WebViewHost* object, CallbackMethodType callback)
            : WebTestRunner::WebMethodTask<WebViewHost>(object)
            , m_callback(callback)
        { }

        virtual void runIfValid() { (m_object->*m_callback)(); }

    private:
        CallbackMethodType m_callback;
    };

    // Called the title of the page changes.
    // Can be used to update the title of the window.
    void setPageTitle(const WebKit::WebString&);

    void enterFullScreenNow();
    void exitFullScreenNow();

    void updateForCommittedLoad(WebKit::WebFrame*, bool isNewNavigation);
    void updateURL(WebKit::WebFrame*);
    void updateSessionHistory(WebKit::WebFrame*);

    // Dumping a frame to the console.
    void printFrameDescription(WebKit::WebFrame*);

    bool hasWindow() const { return m_hasWindow; }

    void updateViewportSize();

#if ENABLE(MEDIA_STREAM)
    webkit_support::TestMediaStreamClient* testMediaStreamClient();
#endif

    // Non-owning pointer. The WebViewHost instance is owned by this TestShell instance.
    TestShell* m_shell;

    // Non-owning pointer. This class needs to be wrapped in a WebTestProxy. This is the pointer to the WebTestProxyBase.
    WebTestRunner::WebTestProxyBase* m_proxy;

    // This delegate works for the following widget.
    WebKit::WebWidget* m_webWidget;

    // For tracking session history. See RenderView.
    int m_pageId;
    int m_lastPageIdUpdated;

    OwnPtr<TestShellExtraData> m_pendingExtraData;

    WebKit::WebCursorInfo m_currentCursor;

    bool m_hasWindow;
    bool m_inModalLoop;

    bool m_shutdownWasInvoked;

    WebKit::WebRect m_windowRect;

    // Edit command associated to the current keyboard event.
    std::string m_editCommandName;
    std::string m_editCommandValue;


    OwnPtr<WebKit::WebContextMenuData> m_lastContextMenuData;


#if ENABLE(MEDIA_STREAM)
    OwnPtr<webkit_support::TestMediaStreamClient> m_testMediaStreamClient;
#endif

    OwnPtr<TestNavigationController> m_navigationController;

    WebTestRunner::WebTaskList m_taskList;
    Vector<WebKit::WebWidget*> m_popupmenus;

    OwnPtr<webkit_support::DRTLayerTreeViewClient> m_layerTreeViewClient;
    OwnPtr<WebKit::WebLayerTreeView> m_layerTreeView;
};

#endif // WebViewHost_h
