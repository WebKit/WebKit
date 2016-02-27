/*
 * Copyright (C) 2010, 2011, 2016 Apple Inc. All rights reserved.
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

#ifndef PageClientImpl_h
#define PageClientImpl_h

#if PLATFORM(MAC)

#include "CorrectionPanel.h"
#include "PageClient.h"
#include "WebFullScreenManagerProxy.h"
#include <wtf/RetainPtr.h>

@class WKEditorUndoTargetObjC;
@class WKView;
@class WKWebView;

namespace WebCore {
class AlternativeTextUIController;
}

namespace WebKit {

class WebViewImpl;

class PageClientImpl final : public PageClient
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
    {
public:
    PageClientImpl(NSView *, WKWebView *);
    virtual ~PageClientImpl();

    // FIXME: Eventually WebViewImpl should become the PageClient.
    void setImpl(WebViewImpl& impl) { m_impl = &impl; }

    void viewWillMoveToAnotherWindow();

private:
    // PageClient
    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() override;
    virtual void setViewNeedsDisplay(const WebCore::IntRect&) override;
    virtual void displayView() override;
    virtual bool canScrollView() override;
    virtual void scrollView(const WebCore::IntRect& scrollRect, const WebCore::IntSize& scrollOffset) override;
    virtual void requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, bool isProgrammaticScroll) override;

    virtual WebCore::IntSize viewSize() override;
    virtual bool isViewWindowActive() override;
    virtual bool isViewFocused() override;
    virtual bool isViewVisible() override;
    virtual bool isViewVisibleOrOccluded() override;
    virtual bool isViewInWindow() override;
    virtual bool isVisuallyIdle() override;
    virtual LayerHostingMode viewLayerHostingMode() override;
    virtual ColorSpaceData colorSpace() override;
    virtual void setAcceleratedCompositingRootLayer(LayerOrView *) override;
    virtual LayerOrView *acceleratedCompositingRootLayer() const override;

    virtual void processDidExit() override;
    virtual void pageClosed() override;
    virtual void didRelaunchProcess() override;
    virtual void preferencesDidChange() override;
    virtual void toolTipChanged(const String& oldToolTip, const String& newToolTip) override;
    virtual void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;
    virtual void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) override;
    virtual void handleDownloadRequest(DownloadProxy*) override;
    virtual void didChangeContentSize(const WebCore::IntSize&) override;
    virtual void setCursor(const WebCore::Cursor&) override;
    virtual void setCursorHiddenUntilMouseMoves(bool) override;
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;

    virtual void registerEditCommand(PassRefPtr<WebEditCommandProxy>, WebPageProxy::UndoOrRedo) override;
    virtual void clearAllEditCommands() override;
    virtual bool canUndoRedo(WebPageProxy::UndoOrRedo) override;
    virtual void executeUndoRedo(WebPageProxy::UndoOrRedo) override;
    virtual bool executeSavedCommandBySelector(const String& selector) override;
    virtual void setDragImage(const WebCore::IntPoint& clientPosition, PassRefPtr<ShareableBitmap> dragImage, bool isLinkDrag) override;
    virtual void setPromisedDataForImage(const String& pasteboardName, PassRefPtr<WebCore::SharedBuffer> imageBuffer, const String& filename, const String& extension, const String& title,
        const String& url, const String& visibleUrl, PassRefPtr<WebCore::SharedBuffer> archiveBuffer) override;
#if ENABLE(ATTACHMENT_ELEMENT)
    virtual void setPromisedDataForAttachment(const String& pasteboardName, const String& filename, const String& extension, const String& title, const String& url, const String& visibleUrl) override;
#endif
    virtual void updateSecureInputState() override;
    virtual void resetSecureInputState() override;
    virtual void notifyInputContextAboutDiscardedComposition() override;
    virtual void selectionDidChange() override;

    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;
#if PLATFORM(MAC)
    virtual WebCore::IntRect rootViewToWindow(const WebCore::IntRect&) override;
#endif
#if PLATFORM(IOS)
    virtual WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) = 0;
#endif

    CGRect boundsOfLayerInLayerBackedWindowCoordinates(CALayer *layer) const override;

    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) override;

    virtual RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
#if ENABLE(CONTEXT_MENUS)
    virtual std::unique_ptr<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, const ContextMenuContextData&, const UserData&) override;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual RefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&) override;
#endif

    virtual void setTextIndicator(Ref<WebCore::TextIndicator>, WebCore::TextIndicatorWindowLifetime) override;
    virtual void clearTextIndicator(WebCore::TextIndicatorWindowDismissalAnimation) override;
    virtual void setTextIndicatorAnimationProgress(float) override;

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    virtual void exitAcceleratedCompositingMode() override;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) override;
    virtual void willEnterAcceleratedCompositingMode() override;

    virtual PassRefPtr<ViewSnapshot> takeViewSnapshot() override;
    virtual void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) override;
#if ENABLE(MAC_GESTURE_EVENTS)
    virtual void gestureEventWasNotHandledByWebCore(const NativeWebGestureEvent&) override;
#endif

    virtual void accessibilityWebProcessTokenReceived(const IPC::DataReference&) override;

    virtual void pluginFocusOrWindowFocusChanged(uint64_t pluginComplexTextInputIdentifier, bool pluginHasFocusAndWindowHasFocus) override;
    virtual void setPluginComplexTextInputState(uint64_t pluginComplexTextInputIdentifier, PluginComplexTextInputState) override;

    virtual void makeFirstResponder() override;
    
    virtual void didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&) override;
    virtual void dismissContentRelativeChildWindows(bool withAnimation = true) override;

    virtual void showCorrectionPanel(WebCore::AlternativeTextType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings) override;
    virtual void dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeText) override;
    virtual String dismissCorrectionPanelSoon(WebCore::ReasonForDismissingAlternativeText) override;
    virtual void recordAutocorrectionResponse(WebCore::AutocorrectionResponseType, const String& replacedString, const String& replacementString) override;

    virtual void recommendedScrollbarStyleDidChange(WebCore::ScrollbarStyle) override;

    virtual void intrinsicContentSizeDidChange(const WebCore::IntSize& intrinsicContentSize) override;

#if USE(DICTATION_ALTERNATIVES)
    virtual uint64_t addDictationAlternatives(const RetainPtr<NSTextAlternatives>&) override;
    virtual void removeDictationAlternatives(uint64_t dictationContext) override;
    virtual void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, uint64_t dictationContext) override;
    virtual Vector<String> dictationAlternatives(uint64_t dictationContext) override;
#endif
#if USE(INSERTION_UNDO_GROUPING)
    virtual void registerInsertionUndoGrouping() override;
#endif

    // Auxiliary Client Creation
#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() override;
#endif

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    virtual void closeFullScreenManager() override;
    virtual bool isFullScreen() override;
    virtual void enterFullScreen() override;
    virtual void exitFullScreen() override;
    virtual void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) override;
    virtual void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) override;
#endif

    virtual void navigationGestureDidBegin() override;
    virtual void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) override;
    virtual void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) override;
    virtual void navigationGestureDidEnd() override;
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) override;
    virtual void didRemoveNavigationGestureSnapshot() override;

    NSView *activeView() const;
    NSWindow *activeWindow() const;

    virtual void didFirstVisuallyNonEmptyLayoutForMainFrame() override;
    virtual void didFinishLoadForMainFrame() override;
    virtual void didFailLoadForMainFrame() override;
    virtual void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override;
    virtual void removeNavigationGestureSnapshot() override;

    virtual void didPerformImmediateActionHitTest(const WebHitTestResultData&, bool contentPreventsDefault, API::Object*) override;
    virtual void* immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult>, uint64_t, RefPtr<API::Object>) override;

    virtual void didHandleAcceptedCandidate() override;

    virtual void showPlatformContextMenu(NSMenu *, WebCore::IntPoint) override;

    virtual void didChangeBackgroundColor() override;

    virtual void startWindowDrag() override;
    virtual NSWindow *platformWindow() override;

#if WK_API_ENABLED
    virtual NSView *inspectorAttachmentView() override;
    virtual _WKRemoteObjectRegistry *remoteObjectRegistry() override;
#endif

    NSView *m_view;
    WKWebView *m_webView;
    WebViewImpl* m_impl { nullptr };
#if USE(AUTOCORRECTION_PANEL)
    CorrectionPanel m_correctionPanel;
#endif
#if USE(DICTATION_ALTERNATIVES)
    std::unique_ptr<WebCore::AlternativeTextUIController> m_alternativeTextUIController;
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    virtual WebCore::WebMediaSessionManager& mediaSessionManager() override;
#endif

    virtual void refView() override;
    virtual void derefView() override;

    virtual void didRestoreScrollPosition() override;
    virtual bool windowIsFrontWindowUnderMouse(const NativeWebMouseEvent&) override;
};

} // namespace WebKit

#endif // PLATFORM(MAC)

#endif // PageClientImpl_h
