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

#ifndef WebViewImpl_h
#define WebViewImpl_h

#include "WebNavigationPolicy.h"
#include "platform/WebPoint.h"
#include "platform/WebRect.h"
#include "platform/WebSize.h"
#include "platform/WebString.h"
#include "WebView.h"

#include "ChromeClientImpl.h"
#include "ContextMenuClientImpl.h"
#include "DragClientImpl.h"
#include "EditorClientImpl.h"
#include "GraphicsContext3D.h"
#include "GraphicsLayer.h"
#include "InspectorClientImpl.h"
#include "IntRect.h"
#include "NotificationPresenterImpl.h"
#include "PageOverlayList.h"
#include "UserMediaClientImpl.h"
#include "cc/CCLayerTreeHost.h"
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class ChromiumDataObject;
class DocumentLoader;
class Frame;
class GraphicsContext3D;
class HistoryItem;
class HitTestResult;
class KeyboardEvent;
class Page;
class PlatformKeyboardEvent;
class PopupContainer;
class PopupMenuClient;
class Range;
class RenderTheme;
class Widget;
#if ENABLE(GESTURE_RECOGNIZER)
class PlatformGestureRecognizer;
#endif
}

namespace WebKit {
class AutocompletePopupMenuClient;
class AutofillPopupMenuClient;
class ContextMenuClientImpl;
class DeviceOrientationClientProxy;
class DragScrollTimer;
class GeolocationClientProxy;
class NonCompositedContentHost;
class SpeechInputClientImpl;
class UserMediaClientImpl;
class WebAccessibilityObject;
class WebCompositorImpl;
class WebDevToolsAgentClient;
class WebDevToolsAgentPrivate;
class WebFrameImpl;
class WebGestureEvent;
class WebImage;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebSettingsImpl;
class WebTouchEvent;

class WebViewImpl : public WebView, public WebCore::CCLayerTreeHostClient, public RefCounted<WebViewImpl> {
public:
    // WebWidget methods:
    virtual void close();
    virtual WebSize size() { return m_size; }
    virtual void willStartLiveResize();
    virtual void resize(const WebSize&);
    virtual void willEndLiveResize();
    virtual void willEnterFullScreen();
    virtual void didEnterFullScreen();
    virtual void willExitFullScreen();
    virtual void didExitFullScreen();
    virtual void animate(double frameBeginTime);
    virtual void layout(); // Also implements CCLayerTreeHostClient::layout()
    virtual void paint(WebCanvas*, const WebRect&);
    virtual void themeChanged();
    virtual void composite(bool finish);
    virtual void setNeedsRedraw();
    virtual bool handleInputEvent(const WebInputEvent&);
    virtual void mouseCaptureLost();
    virtual void setFocus(bool enable);
    virtual bool setComposition(
        const WebString& text,
        const WebVector<WebCompositionUnderline>& underlines,
        int selectionStart,
        int selectionEnd);
    virtual bool confirmComposition();
    virtual bool confirmComposition(const WebString& text);
    virtual bool compositionRange(size_t* location, size_t* length);
    virtual WebTextInputType textInputType();
    virtual bool selectionBounds(WebRect& start, WebRect& end) const;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length);
    virtual void setTextDirection(WebTextDirection direction);
    virtual bool isAcceleratedCompositingActive() const;
    virtual void didAcquirePointerLock();
    virtual void didNotAcquirePointerLock();
    virtual void didLosePointerLock();
    virtual void didChangeWindowResizerRect();

    // WebView methods:
    virtual void initializeMainFrame(WebFrameClient*);
    virtual void setAutofillClient(WebAutofillClient*);
    virtual void setDevToolsAgentClient(WebDevToolsAgentClient*);
    virtual void setPermissionClient(WebPermissionClient*);
    virtual void setSpellCheckClient(WebSpellCheckClient*);
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
    virtual void setDomainRelaxationForbidden(bool, const WebString& scheme);
    virtual bool dispatchBeforeUnloadEvent();
    virtual void dispatchUnloadEvent();
    virtual WebFrame* mainFrame();
    virtual WebFrame* findFrameByName(
        const WebString& name, WebFrame* relativeToFrame);
    virtual WebFrame* focusedFrame();
    virtual void setFocusedFrame(WebFrame* frame);
    virtual void setInitialFocus(bool reverse);
    virtual void clearFocusedNode();
    virtual void scrollFocusedNodeIntoView();
    virtual void scrollFocusedNodeIntoRect(const WebRect&);
    virtual double zoomLevel();
    virtual double setZoomLevel(bool textOnly, double zoomLevel);
    virtual void zoomLimitsChanged(double minimumZoomLevel,
                                   double maximumZoomLevel);
    virtual float pageScaleFactor() const;
    virtual bool isPageScaleFactorSet() const;
    virtual void setPageScaleFactorPreservingScrollOffset(float);
    virtual void setPageScaleFactor(float scaleFactor, const WebPoint& origin);
    virtual void setPageScaleFactorLimits(float minPageScale, float maxPageScale);
    virtual float minimumPageScaleFactor() const;
    virtual float maximumPageScaleFactor() const;

    virtual float deviceScaleFactor() const;
    virtual void setDeviceScaleFactor(float);
    virtual bool isFixedLayoutModeEnabled() const;
    virtual void enableFixedLayoutMode(bool enable);
    virtual WebSize fixedLayoutSize() const;
    virtual void setFixedLayoutSize(const WebSize&);
    virtual void enableAutoResizeMode(
        bool enable,
        const WebSize& minSize,
        const WebSize& maxSize);
    virtual void performMediaPlayerAction(
        const WebMediaPlayerAction& action,
        const WebPoint& location);
    virtual void performPluginAction(
        const WebPluginAction&,
        const WebPoint&);
    virtual void copyImageAt(const WebPoint& point);
    virtual void dragSourceEndedAt(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperation operation);
    virtual void dragSourceMovedTo(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperation operation);
    virtual void dragSourceSystemDragEnded();
    virtual WebDragOperation dragTargetDragEnter(
        const WebDragData&,
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
    virtual unsigned long createUniqueIdentifierForRequest();
    virtual void inspectElementAt(const WebPoint& point);
    virtual WebString inspectorSettings() const;
    virtual void setInspectorSettings(const WebString& settings);
    virtual bool inspectorSetting(const WebString& key, WebString* value) const;
    virtual void setInspectorSetting(const WebString& key,
                                     const WebString& value);
    virtual WebDevToolsAgent* devToolsAgent();
    virtual WebAccessibilityObject accessibilityObject();
    virtual void applyAutofillSuggestions(
        const WebNode&,
        const WebVector<WebString>& names,
        const WebVector<WebString>& labels,
        const WebVector<WebString>& icons,
        const WebVector<int>& uniqueIDs,
        int separatorIndex);
    virtual void hidePopups();
    virtual void setScrollbarColors(unsigned inactiveColor,
                                    unsigned activeColor,
                                    unsigned trackColor);
    virtual void setSelectionColors(unsigned activeBackgroundColor,
                                    unsigned activeForegroundColor,
                                    unsigned inactiveBackgroundColor,
                                    unsigned inactiveForegroundColor);
    virtual void performCustomContextMenuAction(unsigned action);
    virtual void addPageOverlay(WebPageOverlay*, int /* zOrder */);
    virtual void removePageOverlay(WebPageOverlay*);

    // CCLayerTreeHostClient
    virtual void updateAnimations(double frameBeginTime);
    virtual void applyScrollAndScale(const WebCore::IntSize&, float);
    virtual PassRefPtr<WebCore::GraphicsContext3D> createLayerTreeHostContext3D();
    virtual void didCommitAndDrawFrame();
    virtual void didCompleteSwapBuffers();
    virtual void didRecreateGraphicsContext(bool success);
    virtual void scheduleComposite();

    // WebViewImpl

    void setIgnoreInputEvents(bool newValue);
    WebDevToolsAgentPrivate* devToolsAgentPrivate() { return m_devToolsAgent.get(); }

    PageOverlayList* pageOverlays() const { return m_pageOverlays.get(); }

    void setOverlayLayer(WebCore::GraphicsLayer*);

    const WebPoint& lastMouseDownPoint() const
    {
        return m_lastMouseDownPoint;
    }

    WebCore::Frame* focusedWebCoreFrame() const;

    // Returns the currently focused Node or null if no node has focus.
    WebCore::Node* focusedWebCoreNode();

    static WebViewImpl* fromPage(WebCore::Page*);

    WebViewClient* client()
    {
        return m_client;
    }

    WebAutofillClient* autofillClient()
    {
        return m_autofillClient;
    }

    WebPermissionClient* permissionClient()
    {
        return m_permissionClient;
    }

    WebSpellCheckClient* spellCheckClient()
    {
        return m_spellCheckClient;
    }

    // Returns the page object associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebCore::Page* page() const
    {
        return m_page.get();
    }

    WebCore::RenderTheme* theme() const;

    // Returns the main frame associated with this view. This may be null when
    // the page is shutting down, but will be valid at all other times.
    WebFrameImpl* mainFrameImpl();

    // History related methods:
    void observeNewNavigation();

    // Event related methods:
    void mouseMove(const WebMouseEvent&);
    void mouseLeave(const WebMouseEvent&);
    void mouseDown(const WebMouseEvent&);
    void mouseUp(const WebMouseEvent&);
    void mouseContextMenu(const WebMouseEvent&);
    void mouseDoubleClick(const WebMouseEvent&);
    bool mouseWheel(const WebMouseWheelEvent&);
    bool gestureEvent(const WebGestureEvent&);
    bool keyEvent(const WebKeyboardEvent&);
    bool charEvent(const WebKeyboardEvent&);
    bool touchEvent(const WebTouchEvent&);

    void numberOfWheelEventHandlersChanged(unsigned);

    // Handles context menu events orignated via the the keyboard. These
    // include the VK_APPS virtual key and the Shift+F10 combine. Code is
    // based on the Webkit function bool WebView::handleContextMenuEvent(WPARAM
    // wParam, LPARAM lParam) in webkit\webkit\win\WebView.cpp. The only
    // significant change in this function is the code to convert from a
    // Keyboard event to the Right Mouse button down event.
    bool sendContextMenuEvent(const WebKeyboardEvent&);

    // Notifies the WebView that a load has been committed. isNewNavigation
    // will be true if a new session history item should be created for that
    // load. isNavigationWithinPage will be true if the navigation does
    // not take the user away from the current page.
    void didCommitLoad(bool* isNewNavigation, bool isNavigationWithinPage);

    // Indicates two things:
    //   1) This view may have a new layout now.
    //   2) Calling layout() is a no-op.
    // After calling WebWidget::layout(), expect to get this notification
    // unless the view did not need a layout.
    void layoutUpdated(WebFrameImpl*);

    void didChangeContentsSize();

    // Returns true if popup menus should be rendered by the browser, false if
    // they should be rendered by WebKit (which is the default).
    static bool useExternalPopupMenus();

    bool contextMenuAllowed() const
    {
        return m_contextMenuAllowed;
    }

    bool shouldAutoResize() const
    {
        return m_shouldAutoResize;
    }

    WebCore::IntSize minAutoSize() const
    {
        return m_minAutoSize;
    }

    WebCore::IntSize maxAutoSize() const
    {
        return m_maxAutoSize;
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
        const WebDragData& dragData,
        WebDragOperationsMask mask,
        const WebImage& dragImage,
        const WebPoint& dragImageOffset);

    void autofillPopupDidHide()
    {
        m_autofillPopupShowing = false;
    }

#if ENABLE(NOTIFICATIONS)
    // Returns the provider of desktop notifications.
    NotificationPresenterImpl* notificationPresenterImpl();
#endif

    // Tries to scroll a frame or any parent of a frame. Returns true if the view
    // was scrolled.
    bool propagateScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity);

    // Notification that a popup was opened/closed.
    void popupOpened(WebCore::PopupContainer* popupContainer);
    void popupClosed(WebCore::PopupContainer* popupContainer);

    void hideAutofillPopup();

    // Returns the input event we're currently processing. This is used in some
    // cases where the WebCore DOM event doesn't have the information we need.
    static const WebInputEvent* currentInputEvent()
    {
        return m_currentInputEvent;
    }

#if USE(ACCELERATED_COMPOSITING)
    bool allowsAcceleratedCompositing();
    bool pageHasRTLStyle() const;
    void setRootGraphicsLayer(WebCore::GraphicsLayer*);
    void setRootLayerNeedsDisplay();
    void scrollRootLayerRect(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& clipRect);
    void invalidateRootLayerRect(const WebCore::IntRect&);
    NonCompositedContentHost* nonCompositedContentHost();
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void scheduleAnimation();
#endif

    // Returns the onscreen 3D context used by the compositor. This is
    // used by the renderer's code to set up resource sharing between
    // the compositor's context and subordinate contexts for APIs like
    // WebGL. Returns 0 if compositing support is not compiled in.
    virtual WebGraphicsContext3D* graphicsContext3D();

    virtual void setVisibilityState(WebPageVisibilityState, bool);

    WebCore::PopupContainer* selectPopup() const { return m_selectPopup.get(); }

    // Returns true if the event leads to scrolling.
    static bool mapKeyCodeForScroll(int keyCode,
                                   WebCore::ScrollDirection* scrollDirection,
                                   WebCore::ScrollGranularity* scrollGranularity);

    // Called by a full frame plugin inside this view to inform it that its
    // zoom level has been updated.  The plugin should only call this function
    // if the zoom change was triggered by the browser, it's only needed in case
    // a plugin can update its own zoom, say because of its own UI.
    void fullFramePluginZoomLevelChanged(double zoomLevel);

#if ENABLE(GESTURE_RECOGNIZER)
    void resetGestureRecognizer();
#endif

    void loseCompositorContext(int numTimes);

    void enterFullScreenForElement(WebCore::Element*);
    void exitFullScreenForElement(WebCore::Element*);

    // Exposed for testing purposes.
    bool hasHorizontalScrollbar();
    bool hasVerticalScrollbar();

    // Pointer Lock calls allow a page to capture all mouse events and
    // disable the system cursor.
#if ENABLE(POINTER_LOCK)
    virtual bool requestPointerLock();
    virtual void requestPointerUnlock();
    virtual bool isPointerLocked();
#endif

private:
    bool computePageScaleFactorLimits();
    float clampPageScaleFactorToLimits(float scale);
    WebPoint clampOffsetAtScale(const WebPoint& offset, float scale);

    friend class WebView;  // So WebView::Create can call our constructor
    friend class WTF::RefCounted<WebViewImpl>;

    enum DragAction {
      DragEnter,
      DragOver
    };

    WebViewImpl(WebViewClient*);
    virtual ~WebViewImpl();

    // Returns true if the event was actually processed.
    bool keyEventDefault(const WebKeyboardEvent&);

    // Returns true if the autocomple has consumed the event.
    bool autocompleteHandleKeyEvent(const WebKeyboardEvent&);

    // Repaints the Autofill popup. Should be called when the suggestions
    // have changed. Note that this should only be called when the Autofill
    // popup is showing.
    void refreshAutofillPopup();

    // Returns true if the view was scrolled.
    bool scrollViewWithKeyboard(int keyCode, int modifiers);

    void hideSelectPopup();

    // Converts |pos| from window coordinates to contents coordinates and gets
    // the HitTestResult for it.
    WebCore::HitTestResult hitTestResultForWindowPos(const WebCore::IntPoint&);

    // Consolidate some common code between starting a drag over a target and
    // updating a drag over a target. If we're starting a drag, |isEntering|
    // should be true.
    WebDragOperation dragTargetDragEnterOrOver(const WebPoint& clientPoint,
                                               const WebPoint& screenPoint,
                                               DragAction);

    void sendResizeEventAndRepaint();

#if USE(ACCELERATED_COMPOSITING)
    void setIsAcceleratedCompositingActive(bool);
    void doComposite();
    void doPixelReadbackToCanvas(WebCanvas*, const WebCore::IntRect&);
    void reallocateRenderer();
    void updateLayerTreeViewport();
#endif

#if ENABLE(POINTER_LOCK)
    void pointerLockMouseEvent(const WebInputEvent&);
#endif

    WebViewClient* m_client;
    WebAutofillClient* m_autofillClient;
    WebPermissionClient* m_permissionClient;
    WebSpellCheckClient* m_spellCheckClient;

    ChromeClientImpl m_chromeClientImpl;
    ContextMenuClientImpl m_contextMenuClientImpl;
    DragClientImpl m_dragClientImpl;
    EditorClientImpl m_editorClientImpl;
    InspectorClientImpl m_inspectorClientImpl;

    WebSize m_size;
    // If true, automatically resize the render view around its content.
    bool m_shouldAutoResize;
    // The lower bound on the size when auto-resizing.
    WebCore::IntSize m_minAutoSize;
    // The upper bound on the size when auto-resizing.
    WebCore::IntSize m_maxAutoSize;

    WebPoint m_lastMousePosition;
    OwnPtr<WebCore::Page> m_page;

    // This flag is set when a new navigation is detected. It is used to satisfy
    // the corresponding argument to WebFrameClient::didCommitProvisionalLoad.
    bool m_observedNewNavigation;
#ifndef NDEBUG
    // Used to assert that the new navigation we observed is the same navigation
    // when we make use of m_observedNewNavigation.
    const WebCore::DocumentLoader* m_newNavigationLoader;
#endif

    // An object that can be used to manipulate m_page->settings() without linking
    // against WebCore. This is lazily allocated the first time GetWebSettings()
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

    // Keeps track of the current zoom level. 0 means no zoom, positive numbers
    // mean zoom in, negative numbers mean zoom out.
    double m_zoomLevel;

    double m_minimumZoomLevel;

    double m_maximumZoomLevel;

    float m_pageDefinedMinimumPageScaleFactor;
    float m_pageDefinedMaximumPageScaleFactor;
    float m_minimumPageScaleFactor;
    float m_maximumPageScaleFactor;

    bool m_pageScaleFactorIsSet;

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

    // The available drag operations (copy, move link...) allowed by the source.
    WebDragOperation m_operationsAllowed;

    // The current drag operation as negotiated by the source and destination.
    // When not equal to DragOperationNone, the drag data can be dropped onto the
    // current drop target in this WebView (the drop target can accept the drop).
    WebDragOperation m_dragOperation;

    // Whether an Autofill popup is currently showing.
    bool m_autofillPopupShowing;

    // The Autofill popup client.
    OwnPtr<AutofillPopupMenuClient> m_autofillPopupClient;

    // The Autofill popup.
    RefPtr<WebCore::PopupContainer> m_autofillPopup;

    // The popup associated with a select element.
    RefPtr<WebCore::PopupContainer> m_selectPopup;

    OwnPtr<WebDevToolsAgentPrivate> m_devToolsAgent;
    OwnPtr<PageOverlayList> m_pageOverlays;

    // Whether the webview is rendering transparently.
    bool m_isTransparent;

    // Whether the user can press tab to focus links.
    bool m_tabsToLinks;

    // Inspector settings.
    WebString m_inspectorSettings;

    typedef HashMap<WTF::String, WTF::String> SettingsMap;
    OwnPtr<SettingsMap> m_inspectorSettingsMap;
    OwnPtr<DragScrollTimer> m_dragScrollTimer;

#if ENABLE(NOTIFICATIONS)
    // The provider of desktop notifications;
    NotificationPresenterImpl m_notificationPresenter;
#endif

    // If set, the (plugin) node which has mouse capture.
    RefPtr<WebCore::Node> m_mouseCaptureNode;

    // If set, the WebView is transitioning to fullscreen for this element.
    RefPtr<WebCore::Element> m_provisionalFullScreenElement;

    // If set, the WebView is in fullscreen mode for an element in this frame.
    RefPtr<WebCore::Frame> m_fullScreenFrame;

#if USE(ACCELERATED_COMPOSITING)
    WebCore::IntRect m_rootLayerScrollDamage;
    OwnPtr<NonCompositedContentHost> m_nonCompositedContentHost;
    RefPtr<WebCore::CCLayerTreeHost> m_layerTreeHost;
    WebCore::GraphicsLayer* m_rootGraphicsLayer;
    bool m_isAcceleratedCompositingActive;
    bool m_compositorCreationFailed;
    // If true, the graphics context is being restored.
    bool m_recreatingGraphicsContext;
#endif
    bool m_haveWheelEventHandlers;
    static const WebInputEvent* m_currentInputEvent;

#if ENABLE(INPUT_SPEECH)
    OwnPtr<SpeechInputClientImpl> m_speechInputClient;
#endif
    // If we attempt to fetch the on-screen GraphicsContext3D before
    // the compositor has been turned on, we need to instantiate it
    // early. This member holds on to the GC3D in this case.
    RefPtr<WebCore::GraphicsContext3D> m_temporaryOnscreenGraphicsContext3D;
    OwnPtr<DeviceOrientationClientProxy> m_deviceOrientationClientProxy;
    OwnPtr<GeolocationClientProxy> m_geolocationClientProxy;

#if ENABLE(GESTURE_RECOGNIZER)
    OwnPtr<WebCore::PlatformGestureRecognizer> m_gestureRecognizer;
#endif

#if ENABLE(MEDIA_STREAM)
    UserMediaClientImpl m_userMediaClientImpl;
#endif
};

} // namespace WebKit

#endif
