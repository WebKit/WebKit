/*
 * Copyright (C) 2010 Patrick Gansterer <paroga@paroga.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ChromeClientWinCE_h
#define ChromeClientWinCE_h

#include "ChromeClient.h"

class WebView;

namespace WebKit {

class ChromeClientWinCE : public WebCore::ChromeClient {
public:
    ChromeClientWinCE(WebView* webView);

    virtual void chromeDestroyed() OVERRIDE;

    virtual void setWindowRect(const WebCore::FloatRect&) OVERRIDE;
    virtual WebCore::FloatRect windowRect() OVERRIDE;

    virtual WebCore::FloatRect pageRect() OVERRIDE;

    virtual void focus() OVERRIDE;
    virtual void unfocus() OVERRIDE;

    virtual bool canTakeFocus(WebCore::FocusDirection) OVERRIDE;
    virtual void takeFocus(WebCore::FocusDirection) OVERRIDE;

    virtual void focusedElementChanged(WebCore::Element*) OVERRIDE;
    virtual void focusedFrameChanged(WebCore::Frame*) OVERRIDE;

    // The Frame pointer provides the ChromeClient with context about which
    // Frame wants to create the new Page.  Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    virtual WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) OVERRIDE;
    virtual void show() OVERRIDE;

    virtual bool canRunModal() OVERRIDE;
    virtual void runModal() OVERRIDE;

    virtual void setToolbarsVisible(bool) OVERRIDE;
    virtual bool toolbarsVisible() OVERRIDE;

    virtual void setStatusbarVisible(bool) OVERRIDE;
    virtual bool statusbarVisible() OVERRIDE;

    virtual void setScrollbarsVisible(bool) OVERRIDE;
    virtual bool scrollbarsVisible() OVERRIDE;

    virtual void setMenubarVisible(bool) OVERRIDE;
    virtual bool menubarVisible() OVERRIDE;

    virtual void setResizable(bool) OVERRIDE;

    virtual void addMessageToConsole(WebCore::MessageSource, WebCore::MessageLevel, const WTF::String& message, unsigned lineNumber, unsigned columnNumber, const WTF::String& sourceID) OVERRIDE;

    virtual bool canRunBeforeUnloadConfirmPanel() OVERRIDE;
    virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame*) OVERRIDE;

    virtual void closeWindowSoon() OVERRIDE;

    virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&) OVERRIDE;
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&) OVERRIDE;
    virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) OVERRIDE;
    virtual void setStatusbarText(const WTF::String&) OVERRIDE;
    virtual bool shouldInterruptJavaScript() OVERRIDE;
    virtual WebCore::KeyboardUIMode keyboardUIMode() OVERRIDE;

    virtual WebCore::IntRect windowResizerRect() const OVERRIDE;

    // Methods used by HostWindow.
    virtual void invalidateRootView(const WebCore::IntRect&, bool) OVERRIDE;
    virtual void invalidateContentsAndRootView(const WebCore::IntRect&, bool) OVERRIDE;
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&, bool) OVERRIDE;
    virtual void scroll(const WebCore::IntSize&, const WebCore::IntRect&, const WebCore::IntRect&) OVERRIDE;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const OVERRIDE;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const OVERRIDE;
    virtual PlatformPageClient platformPageClient() const OVERRIDE;
    virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const OVERRIDE;
    virtual void scrollbarsModeDidChange() const OVERRIDE;
    virtual void setCursor(const WebCore::Cursor&) OVERRIDE;
    virtual void setCursorHiddenUntilMouseMoves(bool) OVERRIDE;
    // End methods used by HostWindow.

    virtual void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) OVERRIDE;

    virtual void setToolTip(const WTF::String&, WebCore::TextDirection) OVERRIDE;

    virtual void print(WebCore::Frame*) OVERRIDE;

#if ENABLE(SQL_DATABASE)
    virtual void exceededDatabaseQuota(WebCore::Frame*, const WTF::String& databaseName, WebCore::DatabaseDetails) OVERRIDE;
#endif

    // Callback invoked when the application cache fails to save a cache object
    // because storing it would grow the database file past its defined maximum
    // size or past the amount of free space on the device.
    // The chrome client would need to take some action such as evicting some
    // old caches.
    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) OVERRIDE;

    // Callback invoked when the application cache origin quota is reached. This
    // means that the resources attempting to be cached via the manifest are
    // more than allowed on this origin. This callback allows the chrome client
    // to take action, such as prompting the user to ask to increase the quota
    // for this origin.
    virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t totalSpaceNeeded) OVERRIDE;

    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) OVERRIDE;
    // Asynchronous request to load an icon for specified filenames.
    virtual void loadIconForFiles(const Vector<WTF::String>&, WebCore::FileIconLoader*) OVERRIDE;

    // Notification that the given form element has changed. This function
    // will be called frequently, so handling should be very fast.
    virtual void formStateDidChange(const WebCore::Node*) OVERRIDE;

#if USE(ACCELERATED_COMPOSITING)
    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) OVERRIDE;
    // Sets a flag to specify that the next time content is drawn to the window,
    // the changes appear on the screen in synchrony with updates to GraphicsLayers.
    virtual void setNeedsOneShotDrawingSynchronization() OVERRIDE;
    // Sets a flag to specify that the view needs to be updated, so we need
    // to do an eager layout before the drawing.
    virtual void scheduleCompositingLayerFlush() OVERRIDE;
#endif

    virtual void setLastSetCursorToCurrentCursor() OVERRIDE;
    virtual void AXStartFrameLoad() OVERRIDE;
    virtual void AXFinishFrameLoad() OVERRIDE;

#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) OVERRIDE;
#endif

    virtual bool selectItemWritingDirectionIsNatural() OVERRIDE;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() OVERRIDE;
    virtual bool hasOpenedPopup() const OVERRIDE;
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const OVERRIDE;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const OVERRIDE;

    virtual void numWheelEventHandlersChanged(unsigned) OVERRIDE { }

private:
    WebView* m_webView;
};

} // namespace WebKit

#endif // ChromeClientWinCE_h
