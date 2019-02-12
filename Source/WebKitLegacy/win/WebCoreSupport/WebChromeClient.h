/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include <WebCore/ChromeClient.h>
#include <WebCore/COMPtr.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/ScrollTypes.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

class WebView;
class WebDesktopNotificationsDelegate;

interface IWebUIDelegate;

class WebChromeClient final : public WebCore::ChromeClient {
public:
    WebChromeClient(WebView*);

    void chromeDestroyed() final;

    void setWindowRect(const WebCore::FloatRect&) final;
    WebCore::FloatRect windowRect() final;
    
    WebCore::FloatRect pageRect() final;

    void focus() final;
    void unfocus() final;

    bool canTakeFocus(WebCore::FocusDirection) final;
    void takeFocus(WebCore::FocusDirection) final;

    void focusedElementChanged(WebCore::Element*) final;
    void focusedFrameChanged(WebCore::Frame*) final;

    WebCore::Page* createWindow(WebCore::Frame&, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) final;
    void show() final;

    bool canRunModal() final;
    void runModal() final;

    void setToolbarsVisible(bool) final;
    bool toolbarsVisible() final;
    
    void setStatusbarVisible(bool) final;
    bool statusbarVisible() final;
    
    void setScrollbarsVisible(bool) final;
    bool scrollbarsVisible() final;
    
    void setMenubarVisible(bool) final;
    bool menubarVisible() final;

    void setResizable(bool) final;

    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const WTF::String& message, unsigned lineNumber, unsigned columnNumber, const WTF::String& url) final;

    bool canRunBeforeUnloadConfirmPanel() final;
    bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame&) final;

    void closeWindowSoon() final;

    void runJavaScriptAlert(WebCore::Frame&, const WTF::String&) final;
    bool runJavaScriptConfirm(WebCore::Frame&, const WTF::String&) final;
    bool runJavaScriptPrompt(WebCore::Frame&, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) final;
    void setStatusbarText(const WTF::String&) final;

    WebCore::KeyboardUIMode keyboardUIMode() final;

    void invalidateRootView(const WebCore::IntRect&) final;
    void invalidateContentsAndRootView(const WebCore::IntRect&) final;
    void invalidateContentsForSlowScroll(const WebCore::IntRect&) final;
    void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect& clipRect) final;

    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const final;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const final;
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) const final;
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) const final;
    PlatformPageClient platformPageClient() const final;
    void contentsSizeChanged(WebCore::Frame&, const WebCore::IntSize&) const final;

    void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) final;
    bool shouldUnavailablePluginMessageBeButton(WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const final;
    void unavailablePluginButtonClicked(WebCore::Element&, WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const final;

    void setToolTip(const WTF::String&, WebCore::TextDirection) final;

    void print(WebCore::Frame&) final;

    void exceededDatabaseQuota(WebCore::Frame&, const WTF::String&, WebCore::DatabaseDetails) final;

    void reachedMaxAppCacheSize(int64_t spaceNeeded) final;
    void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin&, int64_t totalSpaceNeeded) final;

    void runOpenPanel(WebCore::Frame&, WebCore::FileChooser&) final;
    void loadIconForFiles(const Vector<WTF::String>&, WebCore::FileIconLoader&) final;

    void setCursor(const WebCore::Cursor&) final;
    void setCursorHiddenUntilMouseMoves(bool) final;
    void setLastSetCursorToCurrentCursor() final;

    // Pass 0 as the GraphicsLayer to detatch the root layer.
    void attachRootGraphicsLayer(WebCore::Frame&, WebCore::GraphicsLayer*) final;
    void attachViewOverlayGraphicsLayer(WebCore::GraphicsLayer*) final;
    // Sets a flag to specify that the next time content is drawn to the window,
    // the changes appear on the screen in synchrony with updates to GraphicsLayers.
    void setNeedsOneShotDrawingSynchronization() final { }
    // Sets a flag to specify that the view needs to be updated, so we need
    // to do an eager layout before the drawing.
    void scheduleCompositingLayerFlush() final;

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    WebCore::GraphicsDeviceAdapter* graphicsDeviceAdapter() const final;
#endif

    void scrollRectIntoView(const WebCore::IntRect&) const final { }

#if ENABLE(VIDEO)
    bool supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) final;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode, bool) final;
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&) final;
#endif

#if ENABLE(NOTIFICATIONS)
    WebCore::NotificationClient* notificationPresenter() const final { return reinterpret_cast<WebCore::NotificationClient*>(m_notificationsDelegate.get()); }
#endif

    bool selectItemWritingDirectionIsNatural() final;
    bool selectItemAlignmentFollowsMenuWritingDirection() final;
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient&) const final;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient&) const final;

#if ENABLE(FULLSCREEN_API)
    bool supportsFullScreenForElement(const WebCore::Element&, bool withKeyboard) final;
    void enterFullScreenForElement(WebCore::Element&) final;
    void exitFullScreenForElement(WebCore::Element*) final;
#endif

    void wheelEventHandlersChanged(bool) final { }

    WebView* webView() { return m_webView; }

    void AXStartFrameLoad() final;
    void AXFinishFrameLoad() final;

    bool shouldUseTiledBackingForFrameView(const WebCore::FrameView&) const final;

    RefPtr<WebCore::Icon> createIconForFiles(const Vector<String>&) final;

private:
    COMPtr<IWebUIDelegate> uiDelegate();

    WebView* m_webView;

#if ENABLE(NOTIFICATIONS)
    std::unique_ptr<WebDesktopNotificationsDelegate> m_notificationsDelegate;
#endif
};
