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

#include "ChromeClientImpl.h"
#include "ContextMenuClientImpl.h"
#include "DragClientImpl.h"
#include "EditorClientImpl.h"
#include "GraphicsContext3D.h"
#include "GraphicsLayer.h"
#include "InspectorClientImpl.h"
#include "IntPoint.h"
#include "IntRect.h"
#include "NotificationPresenterImpl.h"
#include "PageOverlayList.h"
#include "PagePopupDriver.h"
#include "PageWidgetDelegate.h"
#include "PlatformGestureCurveTarget.h"
#include "UserMediaClientImpl.h"
#include "WebInputEvent.h"
#include "WebNavigationPolicy.h"
#include "WebView.h"
#include "WebViewBenchmarkSupportImpl.h"
#include <public/WebFloatQuad.h>
#include <public/WebLayer.h>
#include <public/WebLayerTreeView.h>
#include <public/WebLayerTreeViewClient.h>
#include <public/WebPoint.h>
#include <public/WebRect.h>
#include <public/WebSize.h>
#include <public/WebString.h>
#include <wtf/OwnPtr.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class ActivePlatformGestureAnimation;
class ChromiumDataObject;
class Color;
class DocumentLoader;
class Frame;
class GraphicsContext3D;
class HistoryItem;
class HitTestResult;
class KeyboardEvent;
class Page;
class PageGroup;
class PagePopup;
class PagePopupClient;
class PlatformGestureCurveTarget;
class PlatformKeyboardEvent;
class PopupContainer;
class PopupMenuClient;
class Range;
class RenderTheme;
class TextFieldDecorator;
class Widget;
}

namespace WebKit {
class AutocompletePopupMenuClient;
class AutofillPopupMenuClient;
class BatteryClientImpl;
class ContextFeaturesClientImpl;
class ContextMenuClientImpl;
class DeviceOrientationClientProxy;
class DragScrollTimer;
class GeolocationClientProxy;
class LinkHighlight;
class WebHelperPluginImpl;
class NonCompositedContentHost;
class PrerendererClientImpl;
class SpeechInputClientImpl;
class SpeechRecognitionClientProxy;
class UserMediaClientImpl;
class WebAccessibilityObject;
class WebCompositorImpl;
class WebDevToolsAgentClient;
class WebDevToolsAgentPrivate;
class WebFrameImpl;
class WebGestureEvent;
class WebPagePopupImpl;
class WebPrerendererClient;
class WebViewBenchmarkSupport;
class WebImage;
class WebKeyboardEvent;
class WebMouseEvent;
class WebMouseWheelEvent;
class WebSettingsImpl;
class WebTouchEvent;

class WebViewImpl : public WebView
                  , public WebLayerTreeViewClient
                  , public RefCounted<WebViewImpl>
                  , public WebCore::PlatformGestureCurveTarget
#if ENABLE(PAGE_POPUP)
                  , public WebCore::PagePopupDriver
#endif
                  , public PageWidgetEventHandler {
public:
    enum AutoZoomType {
        DoubleTap,
        FindInPage,
    };

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
    virtual void setCompositorSurfaceReady();
    virtual void animate(double);
    virtual void layout(); // Also implements WebLayerTreeViewClient::layout()
    virtual void paint(WebCanvas*, const WebRect&, PaintOptions = ReadbackFromCompositorIfAvailable);
    virtual void themeChanged();
    virtual void composite(bool finish);
    virtual void setNeedsRedraw();
    virtual bool isInputThrottled() const;
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
    virtual WebTextInputInfo textInputInfo();
    virtual WebTextInputType textInputType();
    virtual bool setEditableSelectionOffsets(int start, int end);
    virtual bool setCompositionFromExistingText(int compositionStart, int compositionEnd, const WebVector<WebCompositionUnderline>& underlines);
    virtual void extendSelectionAndDelete(int before, int after);
    virtual bool isSelectionEditable() const;
    virtual WebColor backgroundColor() const;
    virtual bool selectionBounds(WebRect& start, WebRect& end) const;
    virtual bool selectionTextDirection(WebTextDirection& start, WebTextDirection& end) const;
    virtual bool caretOrSelectionRange(size_t* location, size_t* length);
    virtual void setTextDirection(WebTextDirection direction);
    virtual bool isAcceleratedCompositingActive() const;
    virtual void didAcquirePointerLock();
    virtual void didNotAcquirePointerLock();
    virtual void didLosePointerLock();
    virtual void didChangeWindowResizerRect();
    virtual void instrumentBeginFrame();
    virtual void instrumentCancelFrame();
    virtual void renderingStats(WebRenderingStats&) const;

    // WebView methods:
    virtual void initializeMainFrame(WebFrameClient*);
    virtual void initializeHelperPluginFrame(WebFrameClient*);
    virtual void setAutofillClient(WebAutofillClient*);
    virtual void setDevToolsAgentClient(WebDevToolsAgentClient*);
    virtual void setPermissionClient(WebPermissionClient*);
    virtual void setPrerendererClient(WebPrerendererClient*) OVERRIDE;
    virtual void setSpellCheckClient(WebSpellCheckClient*);
    virtual void addTextFieldDecoratorClient(WebTextFieldDecoratorClient*) OVERRIDE;
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
    virtual void zoomToFindInPageRect(const WebRect&);
    virtual void advanceFocus(bool reverse);
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
    virtual void saveScrollAndScaleState();
    virtual void restoreScrollAndScaleState();
    virtual void setIgnoreViewportTagMaximumScale(bool);

    virtual float deviceScaleFactor() const;
    virtual void setDeviceScaleFactor(float);
    virtual bool isFixedLayoutModeEnabled() const;
    virtual void enableFixedLayoutMode(bool enable);
    virtual WebSize fixedLayoutSize() const;
    virtual void setFixedLayoutSize(const WebSize&);
    virtual void enableAutoResizeMode(
        const WebSize& minSize,
        const WebSize& maxSize);
    virtual void disableAutoResizeMode();
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
        WebDragOperationsMask operationsAllowed,
        int keyModifiers);
    virtual WebDragOperation dragTargetDragOver(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        WebDragOperationsMask operationsAllowed,
        int keyModifiers);
    virtual void dragTargetDragLeave();
    virtual void dragTargetDrop(
        const WebPoint& clientPoint,
        const WebPoint& screenPoint,
        int keyModifiers);
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
        const WebVector<int>& itemIDs,
        int separatorIndex);
    virtual void hidePopups();
    virtual void selectAutofillSuggestionAtIndex(unsigned listIndex);
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
#if ENABLE(BATTERY_STATUS)
    virtual void updateBatteryStatus(const WebBatteryStatus&);
#endif
    virtual void transferActiveWheelFlingAnimation(const WebActiveWheelFlingParameters&);
    virtual WebViewBenchmarkSupport* benchmarkSupport();

    virtual WebVector<WebFloatQuad> getTouchHighlightQuads(const WebPoint&,
                                                           int padding,
                                                           WebTouchCandidatesInfo& outTouchInfo,
                                                           WebColor& outTapHighlightColor);

    // WebLayerTreeViewClient
    virtual void willBeginFrame();
    virtual void didBeginFrame();
    virtual void updateAnimations(double monotonicFrameBeginTime);
    virtual void applyScrollAndScale(const WebSize&, float);
    virtual WebGraphicsContext3D* createContext3D() OVERRIDE;
    virtual void didRebindGraphicsContext(bool success) OVERRIDE;
    virtual WebCompositorOutputSurface* createOutputSurface() OVERRIDE;
    virtual void didRecreateOutputSurface(bool success) OVERRIDE;
    virtual void willCommit();
    virtual void didCommit();
    virtual void didCommitAndDrawFrame();
    virtual void didCompleteSwapBuffers();
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

    // A pageGroup identifies a namespace of pages. Page groups are used on PLATFORM(MAC)
    // for some programs that use HTML views to display things that don't seem like
    // web pages to the user (so shouldn't have visited link coloring). We only use
    // one page group.
    static WebCore::PageGroup* defaultPageGroup();

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

    const Vector<OwnPtr<WebCore::TextFieldDecorator> >& textFieldDecorators() const { return m_textFieldDecorators; }

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
    void mouseContextMenu(const WebMouseEvent&);
    void mouseDoubleClick(const WebMouseEvent&);

    bool detectContentOnTouch(const WebPoint&, WebInputEvent::Type);
    void startPageScaleAnimation(const WebCore::IntPoint& targetPosition, bool useAnchor, float newScale, double durationInSeconds);

    void numberOfWheelEventHandlersChanged(unsigned);
    void hasTouchEventHandlers(bool);

    // PlatformGestureCurveTarget implementation for wheel fling.
    virtual void scrollBy(const WebCore::IntPoint&);

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

    // Sets the emulated text zoom factor
    // (may not be 1 in the device metrics emulation mode).
    void setEmulatedTextZoomFactor(float);

    // Returns the emulated text zoom factor
    // (which may not be 1 in the device metrics emulation mode).
    float emulatedTextZoomFactor() const
    {
        return m_emulatedTextZoomFactor;
    }

    bool ignoreViewportTagMaximumScale() const { return m_ignoreViewportTagMaximumScale; }

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
        WebCore::Frame*,
        const WebDragData& dragData,
        WebDragOperationsMask mask,
        const WebImage& dragImage,
        const WebPoint& dragImageOffset);

    void autofillPopupDidHide()
    {
        m_autofillPopupShowing = false;
    }

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    // Returns the provider of desktop notifications.
    NotificationPresenterImpl* notificationPresenterImpl();
#endif

    // Tries to scroll a frame or any parent of a frame. Returns true if the view
    // was scrolled.
    bool propagateScroll(WebCore::ScrollDirection, WebCore::ScrollGranularity);

    // Notification that a popup was opened/closed.
    void popupOpened(WebCore::PopupContainer* popupContainer);
    void popupClosed(WebCore::PopupContainer* popupContainer);
#if ENABLE(PAGE_POPUP)
    // PagePopupDriver functions.
    virtual WebCore::PagePopup* openPagePopup(WebCore::PagePopupClient*, const WebCore::IntRect& originBoundsInRootView) OVERRIDE;
    virtual void closePagePopup(WebCore::PagePopup*) OVERRIDE;
#endif

    void hideAutofillPopup();

    WebHelperPluginImpl* createHelperPlugin(const String& pluginType);

    // Returns the input event we're currently processing. This is used in some
    // cases where the WebCore DOM event doesn't have the information we need.
    static const WebInputEvent* currentInputEvent()
    {
        return m_currentInputEvent;
    }

    WebCore::GraphicsLayer* rootGraphicsLayer();
#if USE(ACCELERATED_COMPOSITING)
    bool allowsAcceleratedCompositing();
    void setRootGraphicsLayer(WebCore::GraphicsLayer*);
    void scheduleCompositingLayerSync();
    void scrollRootLayerRect(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& clipRect);
    void invalidateRootLayerRect(const WebCore::IntRect&);
    void paintRootLayer(WebCore::GraphicsContext&, const WebCore::IntRect& contentRect);
    NonCompositedContentHost* nonCompositedContentHost();
    void setBackgroundColor(const WebCore::Color&);
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME)
    void scheduleAnimation();
#endif

    virtual WebGraphicsContext3D* sharedGraphicsContext3D();

    virtual void setVisibilityState(WebPageVisibilityState, bool);

    WebCore::PopupContainer* selectPopup() const { return m_selectPopup.get(); }
#if ENABLE(PAGE_POPUP)
    bool hasOpenedPopup() const { return m_selectPopup || m_pagePopup; }
#else
    bool hasOpenedPopup() const { return m_selectPopup; }
#endif

    // Returns true if the event leads to scrolling.
    static bool mapKeyCodeForScroll(int keyCode,
                                   WebCore::ScrollDirection* scrollDirection,
                                   WebCore::ScrollGranularity* scrollGranularity);

    // Called by a full frame plugin inside this view to inform it that its
    // zoom level has been updated.  The plugin should only call this function
    // if the zoom change was triggered by the browser, it's only needed in case
    // a plugin can update its own zoom, say because of its own UI.
    void fullFramePluginZoomLevelChanged(double zoomLevel);

#if ENABLE(GESTURE_EVENTS)
    void computeScaleAndScrollForHitRect(const WebRect& hitRect, AutoZoomType, float& scale, WebPoint& scroll);
    WebCore::Node* bestTouchLinkNode(WebCore::IntPoint touchEventLocation);
    void enableTouchHighlight(WebCore::IntPoint touchEventLocation);
#endif
    void animateZoomAroundPoint(const WebCore::IntPoint&, AutoZoomType);

    void loseCompositorContext(int numTimes);

    void enterFullScreenForElement(WebCore::Element*);
    void exitFullScreenForElement(WebCore::Element*);

    // Exposed for the purpose of overriding device metrics.
    void sendResizeEventAndRepaint();

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

#if ENABLE(GESTURE_EVENTS)
    // Exposed for tests.
    LinkHighlight* linkHighlight() { return m_linkHighlight.get(); }
#endif


private:
    bool computePageScaleFactorLimits();
    float clampPageScaleFactorToLimits(float scale);
    WebPoint clampOffsetAtScale(const WebPoint& offset, float scale);

    void resetSavedScrollAndScaleState();

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
                                               DragAction,
                                               int keyModifiers);

    void configureAutoResizeMode();

#if USE(ACCELERATED_COMPOSITING)
    void setIsAcceleratedCompositingActive(bool);
    void doComposite();
    void doPixelReadbackToCanvas(WebCanvas*, const WebCore::IntRect&);
    void reallocateRenderer();
    void updateLayerTreeViewport();
#endif

#if ENABLE(GESTURE_EVENTS)
    // Returns the bounding box of the block type node touched by the WebRect.
    WebRect computeBlockBounds(const WebRect&, AutoZoomType);

    // Helper function: Widens the width of |source| by the specified margins
    // while keeping it smaller than page width.
    WebRect widenRectWithinPageBounds(const WebRect& source, int targetMargin, int minimumMargin);
#endif

#if ENABLE(POINTER_LOCK)
    void pointerLockMouseEvent(const WebInputEvent&);
#endif

    // PageWidgetEventHandler functions
    virtual void handleMouseLeave(WebCore::Frame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseDown(WebCore::Frame&, const WebMouseEvent&) OVERRIDE;
    virtual void handleMouseUp(WebCore::Frame&, const WebMouseEvent&) OVERRIDE;
    virtual bool handleGestureEvent(const WebGestureEvent&) OVERRIDE;
    virtual bool handleKeyEvent(const WebKeyboardEvent&) OVERRIDE;
    virtual bool handleCharEvent(const WebKeyboardEvent&) OVERRIDE;

    WebSettingsImpl* settingsImpl();

    WebViewClient* m_client;
    WebAutofillClient* m_autofillClient;
    WebPermissionClient* m_permissionClient;
    WebSpellCheckClient* m_spellCheckClient;
    Vector<OwnPtr<WebCore::TextFieldDecorator> > m_textFieldDecorators;

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

    // State related to the page scale
    float m_pageDefinedMinimumPageScaleFactor;
    float m_pageDefinedMaximumPageScaleFactor;
    float m_minimumPageScaleFactor;
    float m_maximumPageScaleFactor;
    bool m_ignoreViewportTagMaximumScale;
    bool m_pageScaleFactorIsSet;

    // Saved page scale state.
    float m_savedPageScaleFactor; // 0 means that no page scale factor is saved.
    WebCore::IntSize m_savedScrollOffset;

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

    // Context-based feature switches.
    OwnPtr<ContextFeaturesClientImpl> m_featureSwitchClient;

    // Whether an Autofill popup is currently showing.
    bool m_autofillPopupShowing;

    // The Autofill popup client.
    OwnPtr<AutofillPopupMenuClient> m_autofillPopupClient;

    // The Autofill popup.
    RefPtr<WebCore::PopupContainer> m_autofillPopup;

    // The popup associated with a select element.
    RefPtr<WebCore::PopupContainer> m_selectPopup;

#if ENABLE(PAGE_POPUP)
    // The popup associated with an input element.
    RefPtr<WebPagePopupImpl> m_pagePopup;
#endif

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

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
    // The provider of desktop notifications;
    NotificationPresenterImpl m_notificationPresenter;
#endif

    // If set, the (plugin) node which has mouse capture.
    RefPtr<WebCore::Node> m_mouseCaptureNode;

    // If set, the WebView is transitioning to fullscreen for this element.
    RefPtr<WebCore::Element> m_provisionalFullScreenElement;

    // If set, the WebView is in fullscreen mode for an element in this frame.
    RefPtr<WebCore::Frame> m_fullScreenFrame;
    bool m_isCancelingFullScreen;

    WebViewBenchmarkSupportImpl m_benchmarkSupport;

#if USE(ACCELERATED_COMPOSITING)
    WebCore::IntRect m_rootLayerScrollDamage;
    OwnPtr<NonCompositedContentHost> m_nonCompositedContentHost;
    WebLayerTreeView m_layerTreeView;
    WebLayer* m_rootLayer;
    WebCore::GraphicsLayer* m_rootGraphicsLayer;
    bool m_isAcceleratedCompositingActive;
    bool m_compositorCreationFailed;
    // If true, the graphics context is being restored.
    bool m_recreatingGraphicsContext;
    bool m_compositorSurfaceReady;
    float m_deviceScaleInCompositor;
#endif
    static const WebInputEvent* m_currentInputEvent;

#if ENABLE(INPUT_SPEECH)
    OwnPtr<SpeechInputClientImpl> m_speechInputClient;
#endif
#if ENABLE(SCRIPTED_SPEECH)
    OwnPtr<SpeechRecognitionClientProxy> m_speechRecognitionClient;
#endif

    OwnPtr<DeviceOrientationClientProxy> m_deviceOrientationClientProxy;
    OwnPtr<GeolocationClientProxy> m_geolocationClientProxy;
#if ENABLE(BATTERY_STATUS)
    OwnPtr<BatteryClientImpl> m_batteryClient;
#endif

    float m_emulatedTextZoomFactor;

#if ENABLE(MEDIA_STREAM)
    UserMediaClientImpl m_userMediaClientImpl;
#endif
#if ENABLE(REGISTER_PROTOCOL_HANDLER)
    OwnPtr<RegisterProtocolHandlerClientImpl> m_registerProtocolHandlerClient;
#endif
    OwnPtr<WebCore::ActivePlatformGestureAnimation> m_gestureAnimation;
    WebPoint m_lastWheelPosition;
    WebPoint m_lastWheelGlobalPosition;
    int m_flingModifier;
#if ENABLE(GESTURE_EVENTS)
    OwnPtr<LinkHighlight> m_linkHighlight;
#endif
};

} // namespace WebKit

#endif
