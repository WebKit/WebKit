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
    
    virtual void addMessageToConsole(JSC::MessageSource, JSC::MessageLevel, const WTF::String& message, unsigned lineNumber, unsigned columnNumber, const WTF::String& sourceURL) override;

    virtual bool canRunBeforeUnloadConfirmPanel() override;
    virtual bool runBeforeUnloadConfirmPanel(const WTF::String& message, WebCore::Frame*) override;

    virtual void closeWindowSoon() override;

    virtual void runJavaScriptAlert(WebCore::Frame*, const WTF::String&) override;
    virtual bool runJavaScriptConfirm(WebCore::Frame*, const WTF::String&) override;
    virtual bool runJavaScriptPrompt(WebCore::Frame*, const WTF::String& message, const WTF::String& defaultValue, WTF::String& result) override;
    virtual bool shouldInterruptJavaScript() override;

    virtual WebCore::IntRect windowResizerRect() const override;

    virtual bool supportsImmediateInvalidation() override;
    virtual void invalidateRootView(const WebCore::IntRect&) override;
    virtual void invalidateContentsAndRootView(const WebCore::IntRect&) override;
    virtual void invalidateContentsForSlowScroll(const WebCore::IntRect&) override;
    virtual void scroll(const WebCore::IntSize& scrollDelta, const WebCore::IntRect& rectToScroll, const WebCore::IntRect& clipRect) override;

    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) const override;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) const override;
#if PLATFORM(IOS)
    virtual WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) const override;
    virtual WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) const override;
#endif
    virtual PlatformPageClient platformPageClient() const override;
    virtual void contentsSizeChanged(WebCore::Frame*, const WebCore::IntSize&) const override;
    virtual void scrollRectIntoView(const WebCore::IntRect&) const override;
    
    virtual void setStatusbarText(const WTF::String&) override;

    virtual void scrollbarsModeDidChange() const override { }
    virtual bool shouldUnavailablePluginMessageBeButton(WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;
    virtual void unavailablePluginButtonClicked(WebCore::Element*, WebCore::RenderEmbeddedObject::PluginUnavailabilityReason) const override;
    virtual void mouseDidMoveOverElement(const WebCore::HitTestResult&, unsigned modifierFlags) override;

    virtual void setToolTip(const WTF::String&, WebCore::TextDirection) override;

    virtual void print(WebCore::Frame*) override;
#if ENABLE(SQL_DATABASE)
    virtual void exceededDatabaseQuota(WebCore::Frame*, const WTF::String& databaseName, WebCore::DatabaseDetails) override;
#endif
    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) override;
    virtual void reachedApplicationCacheOriginQuota(WebCore::SecurityOrigin*, int64_t totalSpaceNeeded) override;
    virtual void populateVisitedLinks() override;

#if ENABLE(DASHBOARD_SUPPORT)
    virtual void annotatedRegionsChanged() override;
#endif

    virtual void runOpenPanel(WebCore::Frame*, PassRefPtr<WebCore::FileChooser>) override;
    virtual void loadIconForFiles(const Vector<WTF::String>&, WebCore::FileIconLoader*) override;

#if !PLATFORM(IOS)
    virtual void setCursor(const WebCore::Cursor&) override;
    virtual void setCursorHiddenUntilMouseMoves(bool) override;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual PassOwnPtr<WebCore::ColorChooser> createColorChooser(WebCore::ColorChooserClient*, const WebCore::Color&) override;
#endif

    virtual WebCore::KeyboardUIMode keyboardUIMode() override;

    virtual NSResponder *firstResponder() override;
    virtual void makeFirstResponder(NSResponder *) override;

    virtual void enableSuddenTermination() override;
    virtual void disableSuddenTermination() override;
    
#if ENABLE(TOUCH_EVENTS)
    virtual void needTouchEvents(bool) override { }
#endif

    virtual bool shouldReplaceWithGeneratedFileForUpload(const WTF::String& path, WTF::String &generatedFilename) override;
    virtual WTF::String generateReplacementFile(const WTF::String& path) override;

    virtual void elementDidFocus(const WebCore::Node*) override;
    virtual void elementDidBlur(const WebCore::Node*) override;

    virtual bool shouldPaintEntireContents() const override;

    virtual void attachRootGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
    virtual void attachViewOverlayGraphicsLayer(WebCore::Frame*, WebCore::GraphicsLayer*) override;
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
            AnimatedOpacityTrigger | // Allow opacity animations to trigger compositing mode for iOS: <rdar://problem/7830677>
#endif
            AnimationTrigger);
    }

#if ENABLE(VIDEO)
    virtual bool supportsFullscreenForNode(const WebCore::Node*) override;
    virtual void enterFullscreenForNode(WebCore::Node*) override;
    virtual void exitFullscreenForNode(WebCore::Node*) override;
#endif
    
#if ENABLE(FULLSCREEN_API)
    virtual bool supportsFullScreenForElement(const WebCore::Element*, bool withKeyboard) override;
    virtual void enterFullScreenForElement(WebCore::Element*) override;
    virtual void exitFullScreenForElement(WebCore::Element*) override;
#endif

    virtual bool selectItemWritingDirectionIsNatural() override;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() override;
    virtual bool hasOpenedPopup() const override;
    virtual PassRefPtr<WebCore::PopupMenu> createPopupMenu(WebCore::PopupMenuClient*) const override;
    virtual PassRefPtr<WebCore::SearchPopupMenu> createSearchPopupMenu(WebCore::PopupMenuClient*) const override;

    virtual void numWheelEventHandlersChanged(unsigned) override { }

#if ENABLE(SUBTLE_CRYPTO)
    virtual bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const override;
    virtual bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const override;
#endif

#if ENABLE(SERVICE_CONTROLS)
    virtual void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&) override;
    virtual bool hasRelevantSelectionServices(bool isTextOnly) const override;
#endif

#if PLATFORM(IOS)
    WebView* webView() const { return m_webView; }
#else
    WebView* webView() { return m_webView; }
#endif

private:
    WebView *m_webView;
};
