/*
 * Copyright (C) 2012-2016 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS_FAMILY)

#import "PageClientImplCocoa.h"
#import "WebFullScreenManagerProxy.h"
#import <wtf/RetainPtr.h>

OBJC_CLASS WKContentView;
OBJC_CLASS WKEditorUndoTarget;

namespace WebCore {
enum class DOMPasteAccessResponse : uint8_t;
struct PromisedAttachmentInfo;
}

namespace WebKit {

enum class UndoOrRedo : bool;

class PageClientImpl : public PageClientImplCocoa
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
    {
public:
    PageClientImpl(WKContentView *, WKWebView *);
    virtual ~PageClientImpl();
    
private:
    // PageClient
    std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy(WebProcessProxy&) override;
    void setViewNeedsDisplay(const WebCore::Region&) override;
    void requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, bool isProgrammaticScroll) override;
    WebCore::FloatPoint viewScrollPosition() override;
    WebCore::IntSize viewSize() override;
    bool isViewWindowActive() override;
    bool isViewFocused() override;
    bool isViewVisible() override;
    bool isViewInWindow() override;
    bool isViewVisibleOrOccluded() override;
    bool isVisuallyIdle() override;
    void processDidExit() override;
    void processWillSwap() override;
    void didRelaunchProcess() override;
    void pageClosed() override;
    void preferencesDidChange() override;
    void toolTipChanged(const String&, const String&) override;
    void decidePolicyForGeolocationPermissionRequest(WebFrameProxy&, API::SecurityOrigin&, Function<void(bool)>&) override;
    void didStartProvisionalLoadForMainFrame() override;
    void didFailProvisionalLoadForMainFrame() override;
    void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;
    void handleDownloadRequest(DownloadProxy*) override;
    void didChangeContentSize(const WebCore::IntSize&) override;
    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;
    void didChangeViewportProperties(const WebCore::ViewportAttributes&) override;
    void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo) override;
    void clearAllEditCommands() override;
    bool canUndoRedo(UndoOrRedo) override;
    void executeUndoRedo(UndoOrRedo) override;
    void accessibilityWebProcessTokenReceived(const IPC::DataReference&) override;
    bool executeSavedCommandBySelector(const String& selector) override;
    void updateSecureInputState() override;
    void resetSecureInputState() override;
    void notifyInputContextAboutDiscardedComposition() override;
    void makeFirstResponder() override;
    void assistiveTechnologyMakeFirstResponder() override;
    WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) override;
    void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) override;
#if ENABLE(TOUCH_EVENTS)
    void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) override;
#endif
    RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
    Ref<WebCore::ValidationBubble> createValidationBubble(const String& message, const WebCore::ValidationBubble::Settings&) final;

#if ENABLE(INPUT_TYPE_COLOR)
    RefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&, Vector<WebCore::Color>&&) final;
#endif

#if ENABLE(DATALIST_ELEMENT)
    RefPtr<WebDataListSuggestionsDropdown> createDataListSuggestionsDropdown(WebPageProxy&) final;
#endif

    void setTextIndicator(Ref<WebCore::TextIndicator>, WebCore::TextIndicatorWindowLifetime) override;
    void clearTextIndicator(WebCore::TextIndicatorWindowDismissalAnimation) override;
    void setTextIndicatorAnimationProgress(float) override;

    void showSafeBrowsingWarning(const SafeBrowsingWarning&, CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&) override;
    void clearSafeBrowsingWarning() override;
    void clearSafeBrowsingWarningIfForMainFrameNavigation() override;

    void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode() override;
    void updateAcceleratedCompositingMode(const LayerTreeContext&) override;
    void setRemoteLayerTreeRootNode(RemoteLayerTreeNode*) override;
    CALayer* acceleratedCompositingRootLayer() const override;
    LayerHostingMode viewLayerHostingMode() override { return LayerHostingMode::OutOfProcess; }

    RefPtr<ViewSnapshot> takeViewSnapshot() override;
    void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) override;

    void commitPotentialTapFailed() override;
    void didGetTapHighlightGeometries(uint64_t requestID, const WebCore::Color&, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling) override;

    void didCommitLayerTree(const RemoteLayerTreeTransaction&) override;
    void layerTreeCommitComplete() override;
        
    void didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&) override;

#if USE(APPLE_INTERNAL_SDK)
#include <WebKitAdditions/PageClientImplIOSAdditions.h>
#endif

    void couldNotRestorePageState() override;
    void restorePageState(Optional<WebCore::FloatPoint>, const WebCore::FloatPoint&, const WebCore::FloatBoxExtent&, double) override;
    void restorePageCenterAndScale(Optional<WebCore::FloatPoint>, double) override;

    void elementDidFocus(const FocusedElementInformation&, bool userIsInteracting, bool blurPreviousNode, bool changingActivityState, API::Object* userData) override;
    void elementDidBlur() override;
    void focusedElementDidChangeInputMode(WebCore::InputMode) override;
    void didReceiveEditorStateUpdateAfterFocus() override;
    bool isFocusingElement() override;
    void selectionDidChange() override;
    bool interpretKeyEvent(const NativeWebKeyboardEvent&, bool isCharEvent) override;
    void positionInformationDidChange(const InteractionInformationAtPosition&) override;
    void saveImageToLibrary(Ref<WebCore::SharedBuffer>&&) override;
    void showPlaybackTargetPicker(bool hasVideo, const WebCore::IntRect& elementRect, WebCore::RouteSharingPolicy, const String&) override;

    bool handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, API::OpenPanelParameters*, WebOpenPanelResultListenerProxy*) override;
    bool showShareSheet(const WebCore::ShareDataWithParsedURL&, WTF::CompletionHandler<void(bool)>&&) override;
    
    void disableDoubleTapGesturesDuringTapIfNecessary(uint64_t requestID) override;
    double minimumZoomScale() const override;
    WebCore::FloatRect documentRect() const override;

    void showInspectorHighlight(const WebCore::Highlight&) override;
    void hideInspectorHighlight() override;

    void showInspectorIndication() override;
    void hideInspectorIndication() override;

    void enableInspectorNodeSearch() override;
    void disableInspectorNodeSearch() override;

    void scrollingNodeScrollViewWillStartPanGesture() override;
    void scrollingNodeScrollViewDidScroll() override;
    void scrollingNodeScrollWillStartScroll() override;
    void scrollingNodeScrollDidEndScroll() override;

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

    void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) override;

    Vector<String> mimeTypesWithCustomContentProviders() override;

    void navigationGestureDidBegin() override;
    void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) override;
    void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) override;
    void navigationGestureDidEnd() override;
    void willRecordNavigationSnapshot(WebBackForwardListItem&) override;
    void didRemoveNavigationGestureSnapshot() override;

    void didFirstVisuallyNonEmptyLayoutForMainFrame() override;
    void didFinishLoadForMainFrame() override;
    void didFailLoadForMainFrame() override;
    void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override;
    void didNotHandleTapAsClick(const WebCore::IntPoint&) override;
    void didCompleteSyntheticClick() override;

    void didChangeBackgroundColor() override;
    void videoControlsManagerDidChange() override;

    void refView() override;
    void derefView() override;

    void didRestoreScrollPosition() override;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() override;

#if USE(QUICK_LOOK)
    void requestPasswordForQuickLookDocument(const String& fileName, WTF::Function<void(const String&)>&&) override;
#endif

    void requestDOMPasteAccess(const WebCore::IntRect& elementRect, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&) final;

#if ENABLE(DATA_INTERACTION)
    void didPerformDragOperation(bool handled) override;
    void didHandleDragStartRequest(bool started) override;
    void didHandleAdditionalDragItemsRequest(bool added) override;
    void startDrag(const WebCore::DragItem&, const ShareableBitmap::Handle& image) override;
    void didConcludeEditDrag(Optional<WebCore::TextIndicatorData>) override;
    void didChangeDragCaretRect(const WebCore::IntRect& previousCaretRect, const WebCore::IntRect& caretRect) override;
#endif

    void handleAutocorrectionContext(const WebAutocorrectionContext&) final;

    void didFinishProcessingAllPendingMouseEvents() final { }

#if HAVE(PENCILKIT)
    RetainPtr<WKDrawingView> createDrawingView(WebCore::GraphicsLayer::EmbeddedViewID) override;
#endif

#if ENABLE(POINTER_EVENTS)
    void cancelPointersForGestureRecognizer(UIGestureRecognizer*) override;
#endif

    WKContentView *m_contentView;
    RetainPtr<WKEditorUndoTarget> m_undoTarget;
};
} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
