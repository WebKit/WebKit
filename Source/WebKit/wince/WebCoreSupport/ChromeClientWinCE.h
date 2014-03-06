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

    virtual void chromeDestroyed() override;

    virtual void setWindowRect(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect windowRect() override;

    virtual WebCore::FloatRect pageRect() override;

    virtual void focus() override;
    virtual void unfocus() override;

    virtual bool canTakeFocus(WebCore::FocusDirection) override;
    virtual void takeFocus(WebCore::FocusDirection) override;

    virtual void focusedElementChanged(WebCore::Element*) override;
    virtual void focusedFrameChanged(WebCore::Frame*) override;

    // The Frame pointer provides the ChromeClient with context about which
    // Frame wants to create the new Page.  Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    virtual WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) override;
    virtual void show() override;

    virtual bool canRunModal() override;
    virtual void runModal() override;

    virtual void setToolbarsVisible(bool) override;
    virtual bool toolbarsVisible() override;

    virtual void setStatusbarVisible(bool) override;
    virtual bool statusbarVisible() override;

    virtual void setScrollbarsVisible(bool) override;
    virtual bool scrollbarsVisible() override;

    virtual void setMenubarVisible(bool) override;
    virtual bool menubarVisible() override;

    virtual void setResizable(bool) override;

    virtual void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const WTF::String& message, unsigned lineNumber, unsigned columnNumber, const WTF::String& sourceID) override;

    virtual bool canRunBeforeUnloadConfirmPanel() override;
    virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame*) override;

    virtual void closeWindowSoon() override;

    virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&) override;
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&) override;
    virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) override;
    virtual void setStatusbarText(const WTF::String&) override;
    virtual bool shouldInterruptJavaScript() override;
    virtual WebCore::KeyboardUIMode keyboardUIMode() override;

    virtual WebCore::IntRect windowResizerRect() const override;

    // Methods used by HostWindow.
    virtual void invalidateRootView(const WebCore::IntRect&) override;
    virtual void invalidateContentsAndRootView(const WebCore::IntRect&) override;
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&) override;
    virtual void scroll(const WebCore::IntSize&, const WebCore::IntRect&, const WebCore::IntRect&) override;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const override;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const override;
    virtual PlatformPageClient platformPageClient() const override;
    virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const override;
    virtual void scrollbarsModeDidChange() const override;
    virtual void setCursor(const WebCore::Cursor&) override;
    virtual void setCursorHiddenUntilMouseMoves(bool) override;
    // End methods used by HostWindow.

    virtual void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) override;

    virtual void setToolTip(const WTF::String&, WebCore::TextDirection) override;

    virtual void print(WebCore::Frame*) override;

#if ENABLE(SQL_DATABASE)
    virtual void exceededDatabaseQuota(WebCore::Frame*, const WTF::String& databaseName, WebCore::DatabaseDetails) override;
#endif

    // Callback invoked when the application cache fails to save a cache object
    // because storing it would grow the database file past its defined maximum
    // size or past the amount of free space on the device.
    // The chrome client would need to take some action such as evicting some
    // old caches.
    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) override;

    // Callback invoked when the application cache origin quota is reached. This
    // means that the resources attempting to be cached via the manifest are
    // more than allowed on this origin. This callback allows the chrome client
    // to take action, such as prompting the user to ask to increase the quota
    // for this origin.
    virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t totalSpaceNeeded) override;

    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override;
    // Asynchronous request to load an icon for specified filenames.
    virtual void loadIconForFiles(const Vector<WTF::String>&, WebCore::FileIconLoader*) override;

    // Pass 0 as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
    // Sets a flag to specify that the next time content is drawn to the window,
    // the changes appear on the screen in synchrony with updates to GraphicsLayers.
    virtual void setNeedsOneShotDrawingSynchronization() override;
    // Sets a flag to specify that the view needs to be updated, so we need
    // to do an eager layout before the drawing.
    virtual void scheduleCompositingLayerFlush() override;

    virtual void setLastSetCursorToCurrentCursor() override;
    virtual void AXStartFrameLoad() override;
    virtual void AXFinishFrameLoad() override;

#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) override;
#endif

    virtual bool selectItemWritingDirectionIsNatural() override;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() override;
    virtual bool hasOpenedPopup() const override;
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const override;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const override;

    virtual void numWheelEventHandlersChanged(unsigned) override { }

private:
    WebView* m_webView;
};

} // namespace WebKit

#endif // ChromeClientWinCE_h
