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

#pragma once

#if PLATFORM(MAC)

#include "CorrectionPanel.h"
#include "PageClientImplCocoa.h"
#include "WebFullScreenManagerProxy.h"
#include <wtf/RetainPtr.h>

@class WKEditorUndoTargetObjC;
@class WKView;

namespace WebCore {
class AlternativeTextUIController;
struct DragItem;
struct PromisedAttachmentInfo;
}

namespace WebKit {

class WebViewImpl;

class PageClientImpl final : public PageClientImplCocoa
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
    std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy() override;
    void setViewNeedsDisplay(const WebCore::Region&) override;
    void requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, bool isProgrammaticScroll) override;
    WebCore::FloatPoint viewScrollPosition() override;

    WebCore::IntSize viewSize() override;
    bool isViewWindowActive() override;
    bool isViewFocused() override;
    bool isViewVisible() override;
    bool isViewVisibleOrOccluded() override;
    bool isViewInWindow() override;
    bool isVisuallyIdle() override;
    LayerHostingMode viewLayerHostingMode() override;
    ColorSpaceData colorSpace() override;
    void setAcceleratedCompositingRootLayer(LayerOrView *) override;
    LayerOrView *acceleratedCompositingRootLayer() const override;

    void processDidExit() override;
    void processWillSwap() override;
    void pageClosed() override;
    void didRelaunchProcess() override;
    void preferencesDidChange() override;
    void toolTipChanged(const String& oldToolTip, const String& newToolTip) override;
    void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;
    void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) override;
    void handleDownloadRequest(DownloadProxy*) override;
    void didChangeContentSize(const WebCore::IntSize&) override;
    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;
    void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;

    void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo) override;
    void clearAllEditCommands() override;
    bool canUndoRedo(UndoOrRedo) override;
    void executeUndoRedo(UndoOrRedo) override;
    bool executeSavedCommandBySelector(const String& selector) override;
    void startDrag(const WebCore::DragItem&, const ShareableBitmap::Handle& image) override;
    void setPromisedDataForImage(const String& pasteboardName, Ref<WebCore::SharedBuffer>&& imageBuffer, const String& filename, const String& extension, const String& title,
        const String& url, const String& visibleUrl, RefPtr<WebCore::SharedBuffer>&& archiveBuffer) override;
    void updateSecureInputState() override;
    void resetSecureInputState() override;
    void notifyInputContextAboutDiscardedComposition() override;
    void selectionDidChange() override;
    
#if WK_API_ENABLED
    bool showShareSheet(const WebCore::ShareDataWithParsedURL&, WTF::CompletionHandler<void(bool)>&&) override;
#endif
        
    WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;
#if PLATFORM(MAC)
    WebCore::IntRect rootViewToWindow(const WebCore::IntRect&) override;
#endif
#if PLATFORM(IOS_FAMILY)
    virtual WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) = 0;
#endif

    void pinnedStateWillChange() final;
    void pinnedStateDidChange() final;

    CGRect boundsOfLayerInLayerBackedWindowCoordinates(CALayer *) const override;

    void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) override;

    RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
#if ENABLE(CONTEXT_MENUS)
    Ref<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, ContextMenuContextData&&, const UserData&) override;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    RefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&, Vector<WebCore::Color>&&) override;
#endif

#if ENABLE(DATALIST_ELEMENT)
    RefPtr<WebDataListSuggestionsDropdown> createDataListSuggestionsDropdown(WebPageProxy&) override;
#endif

    Ref<WebCore::ValidationBubble> createValidationBubble(const String& message, const WebCore::ValidationBubble::Settings&) final;

    void setTextIndicator(Ref<WebCore::TextIndicator>, WebCore::TextIndicatorWindowLifetime) override;
    void clearTextIndicator(WebCore::TextIndicatorWindowDismissalAnimation) override;
    void setTextIndicatorAnimationProgress(float) override;

    void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode() override;
    void updateAcceleratedCompositingMode(const LayerTreeContext&) override;

    RefPtr<ViewSnapshot> takeViewSnapshot() override;
    void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) override;
#if ENABLE(MAC_GESTURE_EVENTS)
    void gestureEventWasNotHandledByWebCore(const NativeWebGestureEvent&) override;
#endif

    void accessibilityWebProcessTokenReceived(const IPC::DataReference&) override;

    void pluginFocusOrWindowFocusChanged(uint64_t pluginComplexTextInputIdentifier, bool pluginHasFocusAndWindowHasFocus) override;
    void setPluginComplexTextInputState(uint64_t pluginComplexTextInputIdentifier, PluginComplexTextInputState) override;

    void makeFirstResponder() override;
    void assistiveTechnologyMakeFirstResponder() override;
    void setShouldSuppressFirstResponderChanges(bool shouldSuppress) override { m_shouldSuppressFirstResponderChanges = shouldSuppress; }

    void didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&) override;

    void showCorrectionPanel(WebCore::AlternativeTextType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings) override;
    void dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeText) override;
    String dismissCorrectionPanelSoon(WebCore::ReasonForDismissingAlternativeText) override;
    void recordAutocorrectionResponse(WebCore::AutocorrectionResponse, const String& replacedString, const String& replacementString) override;

    void recommendedScrollbarStyleDidChange(WebCore::ScrollbarStyle) override;

    void intrinsicContentSizeDidChange(const WebCore::IntSize& intrinsicContentSize) override;

#if USE(DICTATION_ALTERNATIVES)
    uint64_t addDictationAlternatives(const RetainPtr<NSTextAlternatives>&) override;
    void removeDictationAlternatives(uint64_t dictationContext) override;
    void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, uint64_t dictationContext) override;
    Vector<String> dictationAlternatives(uint64_t dictationContext) override;
#endif
    void setEditableElementIsFocused(bool) override;

#if USE(INSERTION_UNDO_GROUPING)
    void registerInsertionUndoGrouping() override;
#endif

    // Auxiliary Client Creation
#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() override;
#endif

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    void closeFullScreenManager() override;
    bool isFullScreen() override;
    void enterFullScreen() override;
    void exitFullScreen() override;
    void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) override;
    void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame) override;
#endif

    void navigationGestureDidBegin() override;
    void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) override;
    void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) override;
    void navigationGestureDidEnd() override;
    void willRecordNavigationSnapshot(WebBackForwardListItem&) override;
    void didRemoveNavigationGestureSnapshot() override;

    NSView *activeView() const;
    NSWindow *activeWindow() const;

    void didStartProvisionalLoadForMainFrame() override;
    void didFirstVisuallyNonEmptyLayoutForMainFrame() override;
    void didFinishLoadForMainFrame() override;
    void didFailLoadForMainFrame() override;
    void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override;
    void handleControlledElementIDResponse(const String&) override;

    void didPerformImmediateActionHitTest(const WebHitTestResultData&, bool contentPreventsDefault, API::Object*) override;
    NSObject *immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult>, uint64_t, RefPtr<API::Object>) override;

    void didHandleAcceptedCandidate() override;

    void videoControlsManagerDidChange() override;

    void showPlatformContextMenu(NSMenu *, WebCore::IntPoint) override;

    void didChangeBackgroundColor() override;

    void startWindowDrag() override;
    NSWindow *platformWindow() override;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() override;
    bool effectiveAppearanceIsDark() const override;

#if ENABLE(DRAG_SUPPORT)
    void didPerformDragOperation(bool handled) final;
#endif

#if WK_API_ENABLED
    NSView *inspectorAttachmentView() override;
    _WKRemoteObjectRegistry *remoteObjectRegistry() override;
#endif

    void didFinishProcessingAllPendingMouseEvents() final;

    NSView *m_view;
    WebViewImpl* m_impl { nullptr };
#if USE(AUTOCORRECTION_PANEL)
    CorrectionPanel m_correctionPanel;
#endif
#if USE(DICTATION_ALTERNATIVES)
    std::unique_ptr<WebCore::AlternativeTextUIController> m_alternativeTextUIController;
#endif

    bool m_shouldSuppressFirstResponderChanges { false };

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    WebCore::WebMediaSessionManager& mediaSessionManager() override;
#endif

    void refView() override;
    void derefView() override;

    void didRestoreScrollPosition() override;
    bool windowIsFrontWindowUnderMouse(const NativeWebMouseEvent&) override;
};

} // namespace WebKit

#endif // PLATFORM(MAC)
