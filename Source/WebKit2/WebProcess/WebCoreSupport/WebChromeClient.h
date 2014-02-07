/*
 * Copyright (C) 2010, 2011, 2012 Apple Inc. All rights reserved.
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

#ifndef WebChromeClient_h
#define WebChromeClient_h

#include <WebCore/ChromeClient.h>
#include <WebCore/ViewportArguments.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

class WebFrame;
class WebPage;

class WebChromeClient : public WebCore::ChromeClient {
public:
    WebChromeClient(WebPage* page)
        : m_cachedMainFrameHasHorizontalScrollbar(false)
        , m_cachedMainFrameHasVerticalScrollbar(false)
        , m_page(page)
    {
    }
    
    WebPage* page() const { return m_page; }

    virtual void* webView() const { return 0; }

private:
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
    
    virtual void addMessageToConsole(Inspector::MessageSource, Inspector::MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) override;
    
    virtual bool canRunBeforeUnloadConfirmPanel() override;
    virtual bool runBeforeUnloadConfirmPanel(const String& message, WebCore::Frame*) override;
    
    virtual void closeWindowSoon() override;
    
    virtual void runJavaScriptAlert(WebCore::Frame*, const String&) override;
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const String&) override;
    virtual bool runJavaScriptPrompt(WebCore::Frame*, const String& message, const String& defaultValue, String& result) override;
    virtual void setStatusbarText(const String&) override;
    virtual bool shouldInterruptJavaScript() override;

    virtual WebCore::KeyboardUIMode keyboardUIMode() override;

    virtual WebCore::IntRect windowResizerRect() const override;
    
    // HostWindow member function overrides.
    virtual void invalidateRootView(const WebCore::IntRect&, bool) override;
    virtual void invalidateContentsAndRootView(const WebCore::IntRect&, bool) override;
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&, bool) override;
    virtual void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& scrollRect, const WebCore::IntRect& clipRect) override;
#if USE(TILED_BACKING_STORE)
    virtual void delegatedScrollRequested(const WebCore::IntPoint& scrollOffset) override;
#endif
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const override;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const override;
    virtual PlatformPageClient platformPageClient() const override;
    virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const override;
    virtual void scrollRectIntoView(const WebCore::IntRect&) const override; // Currently only Mac has a non empty implementation.

    virtual bool shouldUnavailablePluginMessageBeButton(WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;
    virtual void unavailablePluginButtonClicked(WebCore::Element*, WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;

    virtual void scrollbarsModeDidChange() const override;
    virtual void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) override;
    
    virtual void setToolTip(const String&, WebCore::TextDirection) override;
    
    virtual void print(WebCore::Frame*) override;
    
#if ENABLE(SQL_DATABASE)
    virtual void exceededDatabaseQuota(WebCore::Frame*, const String& databaseName, WebCore::DatabaseDetails) override;
#endif

    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) override;
    virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t spaceNeeded) override;

#if ENABLE(DASHBOARD_SUPPORT)
    virtual void annotatedRegionsChanged() override;
#endif

    virtual void populateVisitedLinks() override;
    
    virtual WebCore::FloatRect customHighlightRect(WebCore::Node*, const WTF::AtomicString& type, const WebCore::FloatRect& lineRect) override;
    virtual void paintCustomHighlight(WebCore::Node*, const AtomicString& type, const WebCore::FloatRect& boxRect, const WebCore::FloatRect& lineRect,
        bool behindText, bool entireLine) override;
    
    virtual bool shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename) override;
    virtual String generateReplacementFile(const String& path) override;
    
#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassOwnPtr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient*, const WebCore::Color&) override;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    virtual void didPreventDefaultForEvent() override;
#endif

#if PLATFORM(IOS)
    virtual void didReceiveMobileDocType() override;
    virtual void setNeedsScrollNotifications(WebCore::Frame*, bool) override;
    virtual void observedContentChange(WebCore::Frame*) override;
    virtual void clearContentChangeObservers(WebCore::Frame*) override;
    virtual void notifyRevealedSelectionByScrollingFrame(WebCore::Frame*) override;
    virtual bool isStopping() override;

    virtual void didLayout(LayoutType = NormalLayout) override;
    virtual void didStartOverflowScroll() override;
    virtual void didEndOverflowScroll() override;

    // FIXME: See <rdar://problem/5975559>
    virtual void suppressFormNotifications() override;
    virtual void restoreFormNotifications() override;

    virtual void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer, const WebCore::IntSize& scrollSize, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) override;
    virtual void removeScrollingLayer(WebCore::Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer) override;

    virtual void webAppOrientationsUpdated() override;
#endif

    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override;
    virtual void loadIconForFiles(const Vector<String>&, WebCore::FileIconLoader*) override;

#if !PLATFORM(IOS)
    virtual void setCursor(const WebCore::Cursor&) override;
    virtual void setCursorHiddenUntilMouseMoves(bool) override;
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
    virtual void scheduleAnimation() override;
#endif

    // Notification that the given form element has changed. This function
    // will be called frequently, so handling should be very fast.
    virtual void formStateDidChange(const WebCore::Node*) override;

    virtual void didAssociateFormControls(const Vector<RefPtr<WebCore::Element>>&) override;
    virtual bool shouldNotifyOnFormChanges() override;

    virtual bool selectItemWritingDirectionIsNatural() override;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() override;
    virtual bool hasOpenedPopup() const override;
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const override;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const override;

    virtual WebCore::GraphicsLayerFactory* graphicsLayerFactory() const override;
    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
    virtual void setNeedsOneShotDrawingSynchronization() override;
    virtual void scheduleCompositingLayerFlush() override;

    virtual CompositingTriggerFlags allowedCompositingTriggers() const
    {
        return static_cast<CompositingTriggerFlags>(
            ThreeDTransformTrigger |
            VideoTrigger |
            PluginTrigger| 
            CanvasTrigger |
#if PLATFORM(IOS)
            AnimatedOpacityTrigger | // Allow opacity animations to trigger compositing mode for iPhone: <rdar://problem/7830677>
#endif
            AnimationTrigger);
    }

    virtual bool layerTreeStateIsFrozen() const override;

#if ENABLE(ASYNC_SCROLLING)
    virtual PassRefPtr<WebCore::ScrollingCoordinator> createScrollingCoordinator(WebCore::Page*) const override;
#endif

#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) override { }
#endif

#if PLATFORM(IOS)
    virtual void elementDidFocus(const WebCore::Node*) override;
    virtual void elementDidBlur(const WebCore::Node*) override;
#endif

#if PLATFORM(IOS)
    virtual bool supportsFullscreenForNode(const WebCore::Node*);
    virtual void enterFullscreenForNode(WebCore::Node*);
    virtual void exitFullscreenForNode(WebCore::Node*);
#endif

#if ENABLE(FULLSCREEN_API)
    virtual bool supportsFullScreenForElement(const WebCore::Element*, bool withKeyboard) override;
    virtual void enterFullScreenForElement(WebCore::Element*) override;
    virtual void exitFullScreenForElement(WebCore::Element*) override;
#endif

#if PLATFORM(MAC)
    virtual void makeFirstResponder() override;
#endif

    virtual void enableSuddenTermination() override;
    virtual void disableSuddenTermination() override;
    
    virtual void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const override;

    virtual void notifyScrollerThumbIsVisibleInRect(const WebCore::IntRect&) override;
    virtual void recommendedScrollbarStyleDidChange(int32_t newStyle) override;

    virtual WebCore::Color underlayColor() const override;
    
    virtual void numWheelEventHandlersChanged(unsigned) override;

    virtual void logDiagnosticMessage(const String& message, const String& description, const String& success) override;

    virtual String plugInStartLabelTitle(const String& mimeType) const override;
    virtual String plugInStartLabelSubtitle(const String& mimeType) const override;
    virtual String plugInExtraStyleSheet() const override;
    virtual String plugInExtraScript() const override;

    virtual void didAddHeaderLayer(WebCore::GraphicsLayer*) override;
    virtual void didAddFooterLayer(WebCore::GraphicsLayer*) override;

    virtual bool shouldUseTiledBackingForFrameView(const WebCore::FrameView*) const override;

    String m_cachedToolTip;
    mutable RefPtr<WebFrame> m_cachedFrameSetLargestFrame;
    mutable bool m_cachedMainFrameHasHorizontalScrollbar;
    mutable bool m_cachedMainFrameHasVerticalScrollbar;

    WebPage* m_page;
};

} // namespace WebKit

#endif // WebChromeClient_h
