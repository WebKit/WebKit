/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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

#ifndef ChromeClientImpl_h
#define ChromeClientImpl_h

#include "ChromeClientChromium.h"
#include "PopupMenu.h"
#include "SearchPopupMenu.h"

namespace WebCore {
class AccessibilityObject;
class Element;
class FileChooser;
class PopupContainer;
class PopupMenuClient;
class RenderBox;
class SecurityOrigin;
struct WindowFeatures;
}

namespace WebKit {
class WebViewImpl;
struct WebCursorInfo;
struct WebPopupMenuInfo;

// Handles window-level notifications from WebCore on behalf of a WebView.
class ChromeClientImpl : public WebCore::ChromeClientChromium {
public:
    explicit ChromeClientImpl(WebViewImpl* webView);
    virtual ~ChromeClientImpl();

    virtual void* webView() const;

    // ChromeClient methods:
    virtual void chromeDestroyed();
    virtual void setWindowRect(const WebCore::FloatRect&);
    virtual WebCore::FloatRect windowRect();
    virtual WebCore::FloatRect pageRect();
    virtual float scaleFactor();
    virtual void focus();
    virtual void unfocus();
    virtual bool canTakeFocus(WebCore::FocusDirection);
    virtual void takeFocus(WebCore::FocusDirection);
    virtual void focusedNodeChanged(WebCore::Node*);
    virtual void focusedFrameChanged(WebCore::Frame*);
    virtual WebCore::Page* createWindow(
        WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&);
    virtual void show();
    virtual bool canRunModal();
    virtual void runModal();
    virtual void setToolbarsVisible(bool);
    virtual bool toolbarsVisible();
    virtual void setStatusbarVisible(bool);
    virtual bool statusbarVisible();
    virtual void setScrollbarsVisible(bool);
    virtual bool scrollbarsVisible();
    virtual void setMenubarVisible(bool);
    virtual bool menubarVisible();
    virtual void setResizable(bool);
    virtual void addMessageToConsole(
        WebCore::MessageSource, WebCore::MessageType, WebCore::MessageLevel,
        const WTF::String& message, unsigned lineNumber,
        const WTF::String& sourceID);
    virtual bool canRunBeforeUnloadConfirmPanel();
    virtual bool runBeforeUnloadConfirmPanel(
        const WTF::String& message, WebCore::Frame*);
    virtual void closeWindowSoon();
    virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&);
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&);
    virtual bool runJavaScriptPrompt(
        WebCore::Frame*, const WTF::String& message,
        const WTF::String& defaultValue, WTF::String& result);
    virtual void setStatusbarText(const WTF::String& message);
    virtual bool shouldInterruptJavaScript();
    virtual WebCore::KeyboardUIMode keyboardUIMode();
    virtual WebCore::IntRect windowResizerRect() const;
#if ENABLE(REGISTER_PROTOCOL_HANDLER)
    virtual void registerProtocolHandler(const String& scheme, const String& baseURL, const String& url, const String& title);
#endif
    virtual void invalidateWindow(const WebCore::IntRect&, bool);
    virtual void invalidateContentsAndWindow(const WebCore::IntRect&, bool);
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&, bool);
#if ENABLE(REQUEST_ANIMATION_FRAME)
    virtual void scheduleAnimation();
#endif
    virtual void scroll(
        const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll,
        const WebCore::IntRect& clipRect);
    virtual WebCore::IntPoint screenToWindow(const WebCore::IntPoint&) const;
    virtual WebCore::IntRect windowToScreen(const WebCore::IntRect&) const;
    virtual PlatformPageClient platformPageClient() const { return 0; }
    virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const;
    virtual void scrollRectIntoView(
        const WebCore::IntRect&, const WebCore::ScrollView*) const { }
    virtual void scrollbarsModeDidChange() const;
    virtual void mouseDidMoveOverElement(
        const WebCore::HitTestResult& result, unsigned modifierFlags);
    virtual void setToolTip(const WTF::String& tooltipText, WebCore::TextDirection);
    virtual void print(WebCore::Frame*);
    virtual void exceededDatabaseQuota(
        WebCore::Frame*, const WTF::String& databaseName);
#if ENABLE(OFFLINE_WEB_APPLICATIONS)
    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded);
    virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*);
#endif
#if ENABLE(NOTIFICATIONS)
    virtual WebCore::NotificationPresenter* notificationPresenter() const;
#endif
    virtual void requestGeolocationPermissionForFrame(WebCore::Frame*, WebCore::Geolocation*);
    virtual void cancelGeolocationPermissionRequestForFrame(WebCore::Frame*, WebCore::Geolocation*);
    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>);
    virtual void chooseIconForFiles(const Vector<WTF::String>&, WebCore::FileChooser*);
#if ENABLE(DIRECTORY_UPLOAD)
    virtual void enumerateChosenDirectory(const WTF::String&, WebCore::FileChooser*);
#endif
    virtual void setCursor(const WebCore::Cursor&);
    virtual void formStateDidChange(const WebCore::Node*);
#if ENABLE(TOUCH_EVENTS)
    // FIXME: All touch events are forwarded regardless of whether or not they are needed.
    virtual void needTouchEvents(bool needTouchEvents) { }
#endif

#if USE(ACCELERATED_COMPOSITING)
    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*);

    // Sets a flag to specify that the next time content is drawn to the window,
    // the changes appear on the screen in synchrony with updates to GraphicsLayers.
    virtual void setNeedsOneShotDrawingSynchronization() { }

    // Sets a flag to specify that the view needs to be updated, so we need
    // to do an eager layout before the drawing.
    virtual void scheduleCompositingLayerSync();

    virtual CompositingTriggerFlags allowedCompositingTriggers() const;
#endif

    virtual bool supportsFullscreenForNode(const WebCore::Node*);
    virtual void enterFullscreenForNode(WebCore::Node*);
    virtual void exitFullscreenForNode(WebCore::Node*);

#if ENABLE(FULLSCREEN_API)
    virtual bool supportsFullScreenForElement(const WebCore::Element*, bool withKeyboard);
    virtual void enterFullScreenForElement(WebCore::Element*);
    virtual void exitFullScreenForElement(WebCore::Element*);
    virtual void fullScreenRendererChanged(WebCore::RenderBox*);
#endif

    // ChromeClientChromium methods:
    virtual void popupOpened(WebCore::PopupContainer* popupContainer,
                             const WebCore::IntRect& bounds,
                             bool handleExternally);
    virtual void popupClosed(WebCore::PopupContainer* popupContainer);
    virtual void postAccessibilityNotification(WebCore::AccessibilityObject*, WebCore::AXObjectCache::AXNotification);

    // ChromeClientImpl:
    void setCursorForPlugin(const WebCursorInfo&);

    virtual bool selectItemWritingDirectionIsNatural();
    virtual bool selectItemAlignmentFollowsMenuWritingDirection();
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const;

#if ENABLE(CONTEXT_MENUS)
    virtual void showContextMenu() { }
#endif

    virtual void willRunModalDialogDuringPageDismissal(const DialogType&) const;

private:
    void getPopupMenuInfo(WebCore::PopupContainer*, WebPopupMenuInfo*);
    void setCursor(const WebCursorInfo&);

    WebViewImpl* m_webView;  // weak pointer
    bool m_toolbarsVisible;
    bool m_statusbarVisible;
    bool m_scrollbarsVisible;
    bool m_menubarVisible;
    bool m_resizable;
};

} // namespace WebKit

#endif
