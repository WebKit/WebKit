/*
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2010 Nokia Corporation and/or its subsidiary(-ies)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#import <WebCore/ChromeClient.h>
#import <WebCore/FocusDirection.h>
#import <wtf/Forward.h>

#if PLATFORM(IOS)
#import <wtf/Platform.h>
#import <wtf/text/WTFString.h>
#import <WebCore/Chrome.h>
#endif

@class WebView;

class WebChromeClient : public WebCore::ChromeClient {
public:
    WebChromeClient(WebView*);
    
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
    
    void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const WTF::String& message, unsigned lineNumber, unsigned columnNumber, const WTF::String& sourceURL) override;

    bool canRunBeforeUnloadConfirmPanel() override;
    bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame*) override;

    void closeWindowSoon() override;

    void runJavaScriptAlert(WebCore::Frame*, const WTF::String&) override;
    bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&) override;
    bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) override;

    bool supportsImmediateInvalidation() override;
    void invalidateRootView(const WebCore::IntRect&) override;
    void invalidateContentsAndRootView(const WebCore::IntRect&) override;
    void invalidateContentsForSlowScroll(const WebCore::IntRect&) override;
    void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect& clipRect) override;

    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const override;
#if PLATFORM(IOS)
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) const override;
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) const override;
#endif
    PlatformPageClient platformPageClient() const override;
    void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const override;
    void scrollRectIntoView(const WebCore::IntRect&) const override;
    
    void setStatusbarText(const WTF::String&) override;

    void scrollbarsModeDidChange() const override { }
    bool shouldUnavailablePluginMessageBeButton(WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;
    void unavailablePluginButtonClicked(WebCore::Element*, WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;
    void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) override;

    void setToolTip(const WTF::String&, WebCore::TextDirection) override;

    void print(WebCore::Frame*) override;
    void exceededDatabaseQuota(WebCore::Frame*, const WTF::String& databaseName, WebCore::DatabaseDetails) override;
    void reachedMaxAppCacheSize(int64_t spaceNeeded) override;
    void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t totalSpaceNeeded) override;

#if ENABLE(DASHBOARD_SUPPORT)
    void annotatedRegionsChanged() override;
#endif

    void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override;
    void loadIconForFiles(const Vector<WTF::String>&, WebCore::FileIconLoader*) override;

#if !PLATFORM(IOS)
    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    std::unique_ptr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient*, const WebCore::Color&) override;
#endif

    WebCore::KeyboardUIMode keyboardUIMode() override;

    NSResponder *firstResponder() override;
    void makeFirstResponder(NSResponder *) override;

    void enableSuddenTermination() override;
    void disableSuddenTermination() override;
    
#if ENABLE(TOUCH_EVENTS)
    void needTouchEvents(bool) override { }
#endif

    bool shouldReplaceWithGeneratedFileForUpload(const WTF::String& path, WTF::String &generatedFilename) override;
    WTF::String generateReplacementFile(const WTF::String& path) override;

    void elementDidFocus(const WebCore::Node*) override;
    void elementDidBlur(const WebCore::Node*) override;

    bool shouldPaintEntireContents() const override;

    void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
    void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
    void setNeedsOneShotDrawingSynchronization() override;
    void scheduleCompositingLayerFlush() override;

    CompositingTriggerFlags allowedCompositingTriggers() const override
    {
        return static_cast<CompositingTriggerFlags>(
            ThreeDTransformTrigger |
            VideoTrigger |
            PluginTrigger| 
            CanvasTrigger |
#if PLATFORM(IOS)
            AnimatedOpacityTrigger | // Allow opacity animations to trigger compositing mode for iOS: <rdar://problem/7830677>
#endif
            AnimationTrigger);
    }

#if ENABLE(VIDEO)
    bool supportsVideoFullscreen(WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;
    void enterVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&, WebCore::HTMLMediaElementEnums::VideoFullscreenMode) override;
    void exitVideoFullscreenForVideoElement(WebCore::HTMLVideoElement&) override;
#endif
    
#if ENABLE(FULLSCREEN_API)
    bool supportsFullScreenForElement(const WebCore::Element*, bool withKeyboard) override;
    void enterFullScreenForElement(WebCore::Element*) override;
    void exitFullScreenForElement(WebCore::Element*) override;
#endif

    bool selectItemWritingDirectionIsNatural() override;
    bool selectItemAlignmentFollowsMenuWritingDirection() override;
    bool hasOpenedPopup() const override;
    RefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const override;
    RefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const override;

    void wheelEventHandlersChanged(bool) override { }

#if ENABLE(SUBTLE_CRYPTO)
    bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const override;
    bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const override;
#endif

#if ENABLE(SERVICE_CONTROLS)
    void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&) override;
    bool hasRelevantSelectionServices(bool isTextOnly) const override;
#endif

#if PLATFORM(IOS)
    WebView* webView() const { return m_webView; }
#else
    WebView* webView() { return m_webView; }
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
    void addPlaybackTargetPickerClient(uint64_t /*contextId*/) override;
    void removePlaybackTargetPickerClient(uint64_t /*contextId*/) override;
    void showPlaybackTargetPicker(uint64_t /*contextId*/, const WebCore::IntPoint&, bool /* hasVideo */, const String&) override;
    void playbackTargetPickerClientStateDidChange(uint64_t /*contextId*/, WebCore::MediaProducer::MediaStateFlags) override;
    void setMockMediaPlaybackTargetPickerEnabled(bool) override;
    void setMockMediaPlaybackTargetPickerState(const String&, WebCore::MediaPlaybackTargetContext::State) override;
#endif
    
    bool mediaShouldUsePersistentCache() const override;

private:
    WebView *m_webView;
};
