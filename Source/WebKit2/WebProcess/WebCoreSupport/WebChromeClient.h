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

#include "WebFrame.h"
#include <WebCore/ChromeClient.h>
#include <WebCore/ViewportArguments.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

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
    void chromeDestroyed() override;
    
    void setWindowRect(const WebCore::FloatRect&) override;
    WebCore::FloatRect windowRect() override;
    
    WebCore::FloatRect pageRect() override;
    
    void focus() override;
    void unfocus() override;
    
    bool canTakeFocus(WebCore::FocusDirection) override;
    void takeFocus(WebCore::FocusDirection) override;

    void focusedElementChanged(WebCore::Element*) override;
    void focusedFrameChanged(WebCore::Frame*) override;

    // The Frame pointer provides the ChromeClient with context about which
    // Frame wants to create the new Page.  Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    WebCore::Page* createWindow(WebCore::Frame*, const WebCore::FrameLoadRequest&, const WebCore::WindowFeatures&, const WebCore::NavigationAction&) override;
    void show() override;
    
    bool canRunModal() override;
    void runModal() override;
    
    void setToolbarsVisible(bool) override;
    bool toolbarsVisible() override;
    
    void setStatusbarVisible(bool) override;
    bool statusbarVisible() override;
    
    void setScrollbarsVisible(bool) override;
    bool scrollbarsVisible() override;
    
    void setMenubarVisible(bool) override;
    bool menubarVisible() override;
    
    void setResizable(bool) override;
    
    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) override;
    
    bool canRunBeforeUnloadConfirmPanel() override;
    bool runBeforeUnloadConfirmPanel(const String& message, WebCore::Frame*) override;
    
    void closeWindowSoon() override;
    
    void runJavaScriptAlert(WebCore::Frame*, const String&) override;
    bool runJavaScriptConfirm(WebCore::Frame*, const String&) override;
    bool runJavaScriptPrompt(WebCore::Frame*, const String& message, const String& defaultValue, String& result) override;
    void setStatusbarText(const String&) override;

    WebCore::KeyboardUIMode keyboardUIMode() override;

    // HostWindow member function overrides.
    void invalidateRootView(const WebCore::IntRect&) override;
    void invalidateContentsAndRootView(const WebCore::IntRect&) override;
    void invalidateContentsForSlowScroll(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& scrollRect, const WebCore::IntRect& clipRect) override;
#if USE(COORDINATED_GRAPHICS)
    void delegatedScrollRequested(const WebCore::IntPoint& scrollOffset) override;
#endif
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const override;
#if PLATFORM(IOS)
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) const override;
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) const override;
#endif
    PlatformPageClient platformPageClient() const override;
    void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const override;
    void scrollRectIntoView(const WebCore::IntRect&) const override; // Currently only Mac has a non empty implementation.

    bool shouldUnavailablePluginMessageBeButton(WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;
    void unavailablePluginButtonClicked(WebCore::Element*, WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;

    void scrollbarsModeDidChange() const override;
    void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) override;

    void setToolTip(const String&, WebCore::TextDirection) override;
    
    void print(WebCore::Frame*) override;

    void exceededDatabaseQuota(WebCore::Frame*, const String& databaseName, WebCore::DatabaseDetails) override;

    void reachedMaxAppCacheSize(int64_t spaceNeeded) override;
    void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t spaceNeeded) override;

#if ENABLE(DASHBOARD_SUPPORT)
    void annotatedRegionsChanged() override;
#endif

    bool shouldReplaceWithGeneratedFileForUpload(const String& path, String& generatedFilename) override;
    String generateReplacementFile(const String& path) override;
    
#if ENABLE(INPUT_TYPE_COLOR)
    std::unique_ptr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient*, const WebCore::Color&) override;
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    void didPreventDefaultForEvent() override;
#endif

#if PLATFORM(IOS)
    void didReceiveMobileDocType(bool) override;
    void setNeedsScrollNotifications(WebCore::Frame*, bool) override;
    void observedContentChange(WebCore::Frame*) override;
    void clearContentChangeObservers(WebCore::Frame*) override;
    void notifyRevealedSelectionByScrollingFrame(WebCore::Frame*) override;
    bool isStopping() override;

    void didLayout(LayoutType = NormalLayout) override;
    void didStartOverflowScroll() override;
    void didEndOverflowScroll() override;
    bool hasStablePageScaleFactor() const override;

    // FIXME: See <rdar://problem/5975559>
    void suppressFormNotifications() override;
    void restoreFormNotifications() override;

    void addOrUpdateScrollingLayer(WebCore::Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer, const WebCore::IntSize& scrollSize, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) override;
    void removeScrollingLayer(WebCore::Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer) override;

    void webAppOrientationsUpdated() override;
    void showPlaybackTargetPicker(bool hasVideo) override;

    std::chrono::milliseconds eventThrottlingDelay() override;
#endif

#if ENABLE(ORIENTATION_EVENTS)
    int deviceOrientation() const override;
#endif

    void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override;
    void loadIconForFiles(const Vector<String>&, WebCore::FileIconLoader*) override;

#if !PLATFORM(IOS)
    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;
#endif
#if ENABLE(REQUEST_ANIMATION_FRAME) && !USE(REQUEST_ANIMATION_FRAME_TIMER)
    void scheduleAnimation() override;
#endif

    void didAssociateFormControls(const Vector<RefPtr<WebCore::Element>>&) override;
    bool shouldNotifyOnFormChanges() override;

    bool selectItemWritingDirectionIsNatural() override;
    bool selectItemAlignmentFollowsMenuWritingDirection() override;
    bool hasOpenedPopup() const override;
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const override;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const override;

    WebCore::GraphicsLayerFactory* graphicsLayerFactory() const override;
    void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
    void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
    void setNeedsOneShotDrawingSynchronization() override;
    void scheduleCompositingLayerFlush() override;
    bool adjustLayerFlushThrottling(WebCore::LayerFlushThrottleState::Flags) override;

#if USE(REQUEST_ANIMATION_FRAME_DISPLAY_MONITOR)
    RefPtr<WebCore::DisplayRefreshMonitor> createDisplayRefreshMonitor(PlatformDisplayID) const override;
#endif

    CompositingTriggerFlags allowedCompositingTriggers() const override
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

    bool layerTreeStateIsFrozen() const override;

#if ENABLE(ASYNC_SCROLLING)
    PassRefPtr<WebCore::ScrollingCoordinator> createScrollingCoordinator(WebCore::Page*) const override;
#endif

#if ENABLE(TOUCH_EVENTS)
    void needTouchEvents(bool) override { }
#endif

#if PLATFORM(IOS)
    void elementDidFocus(const WebCore::Node*) override;
    void elementDidBlur(const WebCore::Node*) override;
    void elementDidRefocus(const WebCore::Node*) override;
#endif

#if PLATFORM(IOS) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    bool supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;
    void setUpVideoControlsManager(WebCore::HTMLVideoElement&) override;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&) override;
#if PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE)
    void exitVideoFullscreenToModeWithoutAnimation(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;
#endif
#endif

#if ENABLE(FULLSCREEN_API)
    bool supportsFullScreenForElement(const WebCore::Element*, bool withKeyboard) override;
    void enterFullScreenForElement(WebCore::Element*) override;
    void exitFullScreenForElement(WebCore::Element*) override;
#endif

#if PLATFORM(COCOA)
    void makeFirstResponder() override;
#endif

    void enableSuddenTermination() override;
    void disableSuddenTermination() override;

#if PLATFORM(IOS)
    WebCore::FloatSize screenSize() const override;
    WebCore::FloatSize availableScreenSize() const override;
#endif
    void dispatchViewportPropertiesDidChange(const WebCore::ViewportArguments&) const override;

    void notifyScrollerThumbIsVisibleInRect(const WebCore::IntRect&) override;
    void recommendedScrollbarStyleDidChange(WebCore::ScrollbarStyle newStyle) override;

    WTF::Optional<WebCore::ScrollbarOverlayStyle> preferredScrollbarOverlayStyle() override;

    WebCore::Color underlayColor() const override;

    void pageExtendedBackgroundColorDidChange(WebCore::Color) const override;
    
    void wheelEventHandlersChanged(bool) override;

    String plugInStartLabelTitle(const String& mimeType) const override;
    String plugInStartLabelSubtitle(const String& mimeType) const override;
    String plugInExtraStyleSheet() const override;
    String plugInExtraScript() const override;

    void didAddHeaderLayer(WebCore::GraphicsLayer*) override;
    void didAddFooterLayer(WebCore::GraphicsLayer*) override;

    bool shouldUseTiledBackingForFrameView(const WebCore::FrameView*) const override;

    void isPlayingMediaDidChange(WebCore::MediaProducer::MediaStateFlags, uint64_t) override;
    void setPageActivityState(WebCore::PageActivityState::Flags) override;

#if ENABLE(MEDIA_SESSION)
    void hasMediaSessionWithActiveMediaElementsDidChange(bool) override;
    void mediaSessionMetadataDidChange(const WebCore::MediaSessionMetadata&) override;
    void focusedContentMediaElementDidChange(uint64_t) override;
#endif

#if ENABLE(SUBTLE_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const override;
    bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const override;
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
    void handleTelephoneNumberClick(const String& number, const WebCore::IntPoint&) override;
#endif
#if ENABLE(SERVICE_CONTROLS)
    void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&) override;
    bool hasRelevantSelectionServices(bool isTextOnly) const override;
#endif

    bool shouldDispatchFakeMouseMoveEvents() const override;

    void handleAutoFillButtonClick(WebCore::HTMLInputElement&) override;

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
    void addPlaybackTargetPickerClient(uint64_t /*contextId*/) override;
    void removePlaybackTargetPickerClient(uint64_t /*contextId*/) override;
    void showPlaybackTargetPicker(uint64_t /*contextId*/, const WebCore::IntPoint&, bool, const String&) override;
    void playbackTargetPickerClientStateDidChange(uint64_t, WebCore::MediaProducer::MediaStateFlags) override;
    void setMockMediaPlaybackTargetPickerEnabled(bool) override;
    void setMockMediaPlaybackTargetPickerState(const String&, WebCore::MediaPlaybackTargetContext::State) override;
#endif

    void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&) override;
#if ENABLE(VIDEO)
#if USE(GSTREAMER)
    void requestInstallMissingMediaPlugins(const String& /*details*/, const String& /*description*/, WebCore::MediaPlayerRequestInstallMissingPluginsCallback&) override;
#endif
#endif

    void didInvalidateDocumentMarkerRects() override;
    bool mediaShouldUsePersistentCache() const override;

    String m_cachedToolTip;
    mutable RefPtr<WebFrame> m_cachedFrameSetLargestFrame;
    mutable bool m_cachedMainFrameHasHorizontalScrollbar;
    mutable bool m_cachedMainFrameHasVerticalScrollbar;

    WebPage* m_page;
};

} // namespace WebKit

#endif // WebChromeClient_h
