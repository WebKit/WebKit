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

#ifndef WebViewImpl_h
#define WebViewImpl_h

// FIXME: Remove these relative paths once consumers from glue are removed.
#include "../public/WebNavigationPolicy.h"
#include "../public/WebPoint.h"
#include "../public/WebSize.h"
#include "../public/WebString.h"
#include "../public/WebView.h"

#include "BackForwardListClientImpl.h"
#include "ChromeClientImpl.h"
#include "ContextMenuClientImpl.h"
#include "DragClientImpl.h"
#include "EditorClientImpl.h"
#include "InspectorClientImpl.h"
#include "NotificationPresenterImpl.h"

#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class ChromiumDataObject;
class Frame;
class HistoryItem;
class HitTestResult;
class KeyboardEvent;
class Page;
class PlatformKeyboardEvent;
class PopupContainer;
class Range;
class RenderTheme;
class Widget;
}

namespace WebKit {
class AutocompletePopupMenuClient;
class ContextMenuClientImpl;
class WebAccessibilityObject;
class WebDevToolsAgentPrivate;
class WebFrameImpl;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebSettingsImpl;

class WebViewImpl : public WebView, public RefCounted<WebViewImpl> {
public:
    // WebWidget methods:
    virtual void close();
    virtual WebSize size() { return m_size; }
    virtual void resize(const WebSize&);
    virtual void layout();
    virtual void paint(WebCanvas*, const WebRect&);
    virtual bool handleInputEvent(const WebInputEvent&);
    virtual void mouseCaptureLost();
    virtual void setFocus(bool enable);
    virtual bool handleCompositionEvent(WebCompositionCommand command,
                                        int cursorPosition,
                                        int targetStart,
                                        int targetEnd,
                                        const WebString& text);
    virtual bool queryCompositionStatus(bool* enabled,
                                        WebRect* caretRect);
    virtual void setTextDirection(WebTextDirection direction);

    // WebView methods:
    virtual void initializeMainFrame(WebFrameClient*);
    virtual WebSettings* settings();
    virtual WebString pageEncoding() const;
    virtual void setPageEncoding(const WebString& encoding);
    virtual bool isTransparent() const;
    virtual void setIsTransparent(bool value);
    virtual bool tabsToLinks() const;
    virtual void setTabsToLinks(bool value);
    virtual bool tabKeyCyclesThroughElements() const;
    virtual void setTabKeyCyclesThroughElements(bool value);
    virtual bool isActive() const;
    virtual void setIsActive(bool value);
    virtual bool dispatchBeforeUnloadEvent();
    virtual void dispatchUnloadEvent();
    virtual WebFrame* mainFrame();
    virtual WebFrame* findFrameByName(
        const WebString& name, WebFrame* relativeToFrame);
    virtual WebFrame* focusedFrame();
    virtual void setFocusedFrame(WebFrame* frame);
    virtual void setInitialFocus(bool reverse);
    virtual void clearFocusedNode();
    virtual void zoomIn(bool textOnly);
    virtual void zoomOut(bool textOnly);
    virtual void zoomDefault();
    virtual void performMediaPlayerAction(
        const WebMediaPlayerAction& action,
        const WebPoint& location);
    virtual void copyImageAt(const WebPoint& point);
    virtual void dragSourceEndedAt(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperation operation);
    virtual void dragSourceMovedTo(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint);
    virtual void dragSourceSystemDragEnded();
    virtual WebDragOperation dragTargetDragEnter(
        const WebDragData& dragData, int identity,
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed);
    virtual WebDragOperation dragTargetDragOver(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed);
    virtual void dragTargetDragLeave();
    virtual void dragTargetDrop(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint);
    virtual int dragIdentity();
    virtual bool setDropEffect(bool accept);
    virtual void inspectElementAt(const WebPoint& point);
    virtual WebString inspectorSettings() const;
    virtual void setInspectorSettings(const WebString& settings);
    virtual WebDevToolsAgent* devToolsAgent();
    virtual void setDevToolsAgent(WebDevToolsAgent*);
    virtual WebAccessibilityObject accessibilityObject();
    virtual void applyAutofillSuggestions(
        const WebNode&,
        const WebVector<WebString>& suggestions,
        int defaultSuggestionIndex);
    virtual void hideAutofillPopup();

    // WebViewImpl

    void setIgnoreInputEvents(bool newValue);
    WebDevToolsAgentPrivate* devToolsAgentPrivate() { return m_devToolsAgent.get(); }

    const WebPoint& lastMouseDownPoint() const
    {
        return m_lastMouseDownPoint;
    }

    WebCore::Frame* focusedWebCoreFrame();

    // Returns the currently focused Node or null if no node has focus.
    WebCore::Node* focusedWebCoreNode();

    static WebViewImpl* fromPage(WebCore::Page*);

    WebViewClient* client()
    {
        return m_client;
    }

    // Returns the page object associated with this view.  This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebCore::Page* page() const
    {
        return m_page.get();
    }

    WebCore::RenderTheme* theme() const;

    // Returns the main frame associated with this view.  This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebFrameImpl* mainFrameImpl();

    // History related methods:
    void setCurrentHistoryItem(WebCore::HistoryItem*);
    WebCore::HistoryItem* previousHistoryItem();
    void observeNewNavigation();

    // Event related methods:
    void mouseMove(const WebMouseEvent&);
    void mouseLeave(const WebMouseEvent&);
    void mouseDown(const WebMouseEvent&);
    void mouseUp(const WebMouseEvent&);
    void mouseContextMenu(const WebMouseEvent&);
    void mouseDoubleClick(const WebMouseEvent&);
    void mouseWheel(const WebMouseWheelEvent&);
    bool keyEvent(const WebKeyboardEvent&);
    bool charEvent(const WebKeyboardEvent&);

    // Handles context menu events orignated via the the keyboard. These
    // include the VK_APPS virtual key and the Shift+F10 combine.  Code is
    // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
    // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
    // significant change in this function is the code to convert from a
    // Keyboard event to the Right Mouse button down event.
    bool sendContextMenuEvent(const WebKeyboardEvent&);

    // Notifies the WebView that a load has been committed.  isNewNavigation
    // will be true if a new session history item should be created for that
    // load.
    void didCommitLoad(bool* isNewNavigation);

    bool contextMenuAllowed() const
    {
        return m_contextMenuAllowed;
    }

    // Set the disposition for how this webview is to be initially shown.
    void setInitialNavigationPolicy(WebNavigationPolicy policy)
    {
        m_initialNavigationPolicy = policy;
    }
    WebNavigationPolicy initialNavigationPolicy() const
    {
        return m_initialNavigationPolicy;
    }

    // Determines whether a page should e.g. be opened in a background tab.
    // Returns false if it has no opinion, in which case it doesn't set *policy.
    static bool navigationPolicyFromMouseEvent(
        unsigned short button,
        bool ctrl,
        bool shift,
        bool alt,
        bool meta,
        WebNavigationPolicy*);

    // Start a system drag and drop operation.
    void startDragging(
        const WebPoint& eventPos,
        const WebDragData& dragData,
        WebDragOperationsMask dragSourceOperationMask);

    // Hides the autocomplete popup if it is showing.
    void hideAutoCompletePopup();
    void autoCompletePopupDidHide();

#if ENABLE(NOTIFICATIONS)
    // Returns the provider of desktop notifications.
    NotificationPresenterImpl* notificationPresenterImpl();
#endif

    // Tries to scroll a frame or any parent of a frame. Returns true if the view
    // was scrolled.
    bool propagateScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity);

    // HACK: currentInputEvent() is for ChromeClientImpl::show(), until we can
    // fix WebKit to pass enough information up into ChromeClient::show() so we
    // can decide if the window.open event was caused by a middle-mouse click
    static const WebInputEvent* currentInputEvent()
    {
        return m_currentInputEvent;
    }

private:
    friend class WebView;  // So WebView::Create can call our constructor
    friend class WTF::RefCounted<WebViewImpl>;

    WebViewImpl(WebViewClient* client);
    ~WebViewImpl();

    // Returns true if the event was actually processed.
    bool keyEventDefault(const WebKeyboardEvent&);

    // Returns true if the autocomple has consumed the event.
    bool autocompleteHandleKeyEvent(const WebKeyboardEvent&);

    // Repaints the autofill popup.  Should be called when the suggestions have
    // changed.  Note that this should only be called when the autofill popup is
    // showing.
    void refreshAutofillPopup();

    // Returns true if the view was scrolled.
    bool scrollViewWithKeyboard(int keyCode, int modifiers);

    // Converts |pos| from window coordinates to contents coordinates and gets
    // the HitTestResult for it.
    WebCore::HitTestResult hitTestResultForWindowPos(const WebCore::IntPoint&);

    WebViewClient* m_client;

    BackForwardListClientImpl m_backForwardListClientImpl;
    ChromeClientImpl m_chromeClientImpl;
    ContextMenuClientImpl m_contextMenuClientImpl;
    DragClientImpl m_dragClientImpl;
    EditorClientImpl m_editorClientImpl;
    InspectorClientImpl m_inspectorClientImpl;

    WebSize m_size;

    WebPoint m_lastMousePosition;
    OwnPtr<WebCore::Page> m_page;

    // This flag is set when a new navigation is detected.  It is used to satisfy
    // the corresponding argument to WebFrameClient::didCommitProvisionalLoad.
    bool m_observedNewNavigation;
#ifndef NDEBUG
    // Used to assert that the new navigation we observed is the same navigation
    // when we make use of m_observedNewNavigation.
    const WebCore::DocumentLoader* m_newNavigationLoader;
#endif

    // An object that can be used to manipulate m_page->settings() without linking
    // against WebCore.  This is lazily allocated the first time GetWebSettings()
    // is called.
    OwnPtr<WebSettingsImpl> m_webSettings;

    // A copy of the web drop data object we received from the browser.
    RefPtr<WebCore::ChromiumDataObject> m_currentDragData;

    // The point relative to the client area where the mouse was last pressed
    // down. This is used by the drag client to determine what was under the
    // mouse when the drag was initiated. We need to track this here in
    // WebViewImpl since DragClient::startDrag does not pass the position the
    // mouse was at when the drag was initiated, only the current point, which
    // can be misleading as it is usually not over the element the user actually
    // dragged by the time a drag is initiated.
    WebPoint m_lastMouseDownPoint;

    // Keeps track of the current text zoom level.  0 means no zoom, positive
    // values mean larger text, negative numbers mean smaller.
    int m_zoomLevel;

    bool m_contextMenuAllowed;

    bool m_doingDragAndDrop;

    bool m_ignoreInputEvents;

    // Webkit expects keyPress events to be suppressed if the associated keyDown
    // event was handled. Safari implements this behavior by peeking out the
    // associated WM_CHAR event if the keydown was handled. We emulate
    // this behavior by setting this flag if the keyDown was handled.
    bool m_suppressNextKeypressEvent;

    // The policy for how this webview is to be initially shown.
    WebNavigationPolicy m_initialNavigationPolicy;

    // Represents whether or not this object should process incoming IME events.
    bool m_imeAcceptEvents;

    // True while dispatching system drag and drop events to drag/drop targets
    // within this WebView.
    bool m_dragTargetDispatch;

    // Valid when m_dragTargetDispatch is true; the identity of the drag data
    // copied from the WebDropData object sent from the browser process.
    int m_dragIdentity;

    // Valid when m_dragTargetDispatch is true.  Used to override the default
    // browser drop effect with the effects "none" or "copy".
    enum DragTargetDropEffect {
        DropEffectDefault = -1,
        DropEffectNone,
        DropEffectCopy
    } m_dropEffect;

    // The available drag operations (copy, move link...) allowed by the source.
    WebDragOperation m_operationsAllowed;

    // The current drag operation as negotiated by the source and destination.
    // When not equal to DragOperationNone, the drag data can be dropped onto the
    // current drop target in this WebView (the drop target can accept the drop).
    WebDragOperation m_dragOperation;

    // The autocomplete popup.  Kept around and reused every-time new suggestions
    // should be shown.
    RefPtr<WebCore::PopupContainer> m_autocompletePopup;

    // Whether the autocomplete popup is currently showing.
    bool m_autocompletePopupShowing;

    // The autocomplete client.
    OwnPtr<AutocompletePopupMenuClient> m_autocompletePopupClient;

    OwnPtr<WebDevToolsAgentPrivate> m_devToolsAgent;

    // Whether the webview is rendering transparently.
    bool m_isTransparent;

    // Whether the user can press tab to focus links.
    bool m_tabsToLinks;

    // Inspector settings.
    WebString m_inspectorSettings;

#if ENABLE(NOTIFICATIONS)
    // The provider of desktop notifications;
    NotificationPresenterImpl m_notificationPresenter;
#endif

    static const WebInputEvent* m_currentInputEvent;
};

} // namespace WebKit

#endif
