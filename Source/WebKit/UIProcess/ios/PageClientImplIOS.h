/*
 * Copyright (C) 2012-2025 Apple Inc. All rights reserved.
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

#import "PDFPluginIdentifier.h"
#import "PageClientImplCocoa.h"
#import "WKBrowserEngineDefinitions.h"
#import "WebFullScreenManagerProxy.h"
#import <WebCore/InspectorOverlay.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

OBJC_CLASS PlatformTextAlternatives;
OBJC_CLASS WKContentView;
OBJC_CLASS WKEditorUndoTarget;

namespace WebCore {
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
struct PromisedAttachmentInfo;
}

namespace WebKit {

class RemoteLayerTreeNode;

enum class ColorControlSupportsAlpha : bool;
enum class UndoOrRedo : bool;

class PageClientImpl final : public PageClientImplCocoa
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
    {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED(PageClientImpl);
#if ENABLE(FULLSCREEN_API)
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageClientImpl);
#endif
public:
    PageClientImpl(WKContentView *, WKWebView *);
    virtual ~PageClientImpl();
    
private:
    // PageClient
    Ref<DrawingAreaProxy> createDrawingAreaProxy(WebProcessProxy&) override;
    void setViewNeedsDisplay(const WebCore::Region&) override;
    void requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, WebCore::ScrollIsAnimated) override;
    WebCore::FloatPoint viewScrollPosition() override;
    WebCore::IntSize viewSize() override;
    bool isViewWindowActive() override;
    bool isViewFocused() override;
    bool isActiveViewVisible() override;
    void viewIsBecomingVisible() override;
    bool canTakeForegroundAssertions() override;
    bool isViewInWindow() override;
    bool isViewVisibleOrOccluded() override;
    bool isVisuallyIdle() override;
    void processDidExit() override;
    void processWillSwap() override;
    void didRelaunchProcess() override;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID) override;
#if ENABLE(GPU_PROCESS)
    void didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID) override;
#endif // ENABLE(GPU_PROCESS)
#if ENABLE(MODEL_PROCESS)
    void didCreateContextInModelProcessForVisibilityPropagation(LayerHostingContextID) override;
    void didReceiveInteractiveModelElement(std::optional<WebCore::NodeIdentifier>) override;
#endif // ENABLE(MODEL_PROCESS)
#if USE(EXTENSIONKIT)
    UIView *createVisibilityPropagationView() override;
    void removeVisibilityPropagationView(UIView *) override;
#endif
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(GPU_PROCESS)
    void gpuProcessDidExit() override;
#endif
#if ENABLE(MODEL_PROCESS)
    void modelProcessDidExit() override;
#endif
    void preferencesDidChange() override;
    void toolTipChanged(const String&, const String&) override;
    void decidePolicyForGeolocationPermissionRequest(WebFrameProxy&, const FrameInfoData&, Function<void(bool)>&) override;
    void didStartProvisionalLoadForMainFrame() override;
    void didFailProvisionalLoadForMainFrame() override;
    void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;
    void didChangeContentSize(const WebCore::IntSize&) override;
    void setCursor(const WebCore::Cursor&) override;
    void setCursorHiddenUntilMouseMoves(bool) override;
    void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo) override;
    void clearAllEditCommands() override;
    bool canUndoRedo(UndoOrRedo) override;
    void executeUndoRedo(UndoOrRedo) override;
    void accessibilityWebProcessTokenReceived(std::span<const uint8_t>, pid_t) override;
    bool executeSavedCommandBySelector(const String& selector) override;
    void updateSecureInputState() override;
    void resetSecureInputState() override;
    void notifyInputContextAboutDiscardedComposition() override;
    void makeFirstResponder() override;
    void assistiveTechnologyMakeFirstResponder() override;
    WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) override;
    WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) override;
    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntPoint rootViewToScreen(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) override;
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) override;
    void relayAccessibilityNotification(String&&, RetainPtr<NSData>&&) override;
    void relayAriaNotifyNotification(const WebCore::AriaNotifyData&) override;
    void relayLiveRegionNotification(const WebCore::LiveRegionAnnouncementData&) override;
    void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) override;
#if ENABLE(TOUCH_EVENTS)
    void doneWithTouchEvent(const WebTouchEvent&, bool wasEventHandled) override;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    void doneDeferringTouchStart(bool preventNativeGestures) override;
    void doneDeferringTouchMove(bool preventNativeGestures) override;
    void doneDeferringTouchEnd(bool preventNativeGestures) override;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    void requestTextRecognition(const URL& imageURL, WebCore::ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&) final;
#endif

    RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
    Ref<WebCore::ValidationBubble> createValidationBubble(String&& message, const WebCore::ValidationBubble::Settings&) final;

    RefPtr<WebColorPicker> createColorPicker(WebPageProxy&, const WebCore::Color& initialColor, const WebCore::IntRect&, ColorControlSupportsAlpha, Vector<WebCore::Color>&&) final { return nullptr; }

    RefPtr<WebDataListSuggestionsDropdown> createDataListSuggestionsDropdown(WebPageProxy&) final;

    WebCore::DataOwnerType dataOwnerForPasteboard(PasteboardAccessIntent) const final;

    RefPtr<WebDateTimePicker> createDateTimePicker(WebPageProxy&) final { return nullptr; }

    CALayer *textIndicatorInstallationLayer() override;

    void showBrowsingWarning(const BrowsingWarning&, CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&) override;
    void clearBrowsingWarning() override;
    void clearBrowsingWarningIfForMainFrameNavigation() override;

    void enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    void exitAcceleratedCompositingMode() override;
    void updateAcceleratedCompositingMode(const LayerTreeContext&) override;
    void setRemoteLayerTreeRootNode(RemoteLayerTreeNode*) override;
    CALayer* acceleratedCompositingRootLayer() const override;

    void makeViewBlank(bool) final;

    RefPtr<ViewSnapshot> takeViewSnapshot(std::optional<WebCore::IntRect>&&) override;
    void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) override;

    void commitPotentialTapFailed() override;
    void didGetTapHighlightGeometries(WebKit::TapIdentifier requestID, const WebCore::Color&, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling) override;

    void didCommitLayerTree(const RemoteLayerTreeTransaction&, const std::optional<MainFrameData>&, const PageData&, const TransactionID&) final;
    void didCommitMainFrameData(const MainFrameData&) final;
    void layerTreeCommitComplete() override;
        
    void didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&) override;

    bool effectiveAppearanceIsDark() const override;
    bool effectiveUserInterfaceLevelIsElevated() const override;

    void couldNotRestorePageState() override;
    void restorePageState(std::optional<WebCore::FloatPoint>, const WebCore::FloatPoint&, const WebCore::FloatBoxExtent&, double) override;
    void restorePageCenterAndScale(std::optional<WebCore::FloatPoint>, double) override;

    void elementDidFocus(const FocusedElementInformation&, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState> activityStateChanges, API::Object* userData) override;
    void updateInputContextAfterBlurringAndRefocusingElement() final;
    void didProgrammaticallyClearFocusedElement(WebCore::ElementContext&&) final;
    void updateFocusedElementInformation(const FocusedElementInformation&) final;
    void elementDidBlur() override;
    void focusedElementDidChangeInputMode(WebCore::InputMode) override;
    void didUpdateEditorState() override;
    void reconcileEnclosingScrollViewContentOffset(EditorState&) final;
    bool isFocusingElement() override;
    void selectionDidChange() override;
    bool interpretKeyEvent(const NativeWebKeyboardEvent&, KeyEventInterpretationContext&&) override;
    void positionInformationDidChange(const InteractionInformationAtPosition&) override;
    void saveImageToLibrary(Ref<WebCore::SharedBuffer>&&) override;
    void showPlaybackTargetPicker(bool hasVideo, const WebCore::IntRect& elementRect, WebCore::RouteSharingPolicy, const String&) override;
    void showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition&) override;

    bool canStartNavigationSwipeAtLastInteractionLocation() const final;

    void hardwareKeyboardAvailabilityChanged() override;

    bool handleRunOpenPanel(const WebPageProxy&, const WebFrameProxy&, const FrameInfoData&, API::OpenPanelParameters&, WebOpenPanelResultListenerProxy&) override;
    bool showShareSheet(WebCore::ShareDataWithParsedURL&&, WTF::CompletionHandler<void(bool)>&&) override;
    void showContactPicker(WebCore::ContactsRequestData&&, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&) override;

#if HAVE(DIGITAL_CREDENTIALS_UI)
    void showDigitalCredentialsPicker(const WebCore::DigitalCredentialsRequestData&, WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&) override;
    void dismissDigitalCredentialsPicker(WTF::CompletionHandler<void(bool)>&&) override;
#endif

    void dismissAnyOpenPicker() override;

    void disableDoubleTapGesturesDuringTapIfNecessary(WebKit::TapIdentifier) override;
    void handleSmartMagnificationInformationForPotentialTap(WebKit::TapIdentifier, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel, bool nodeIsPluginElement) override;

    double minimumZoomScale() const override;
    WebCore::FloatRect documentRect() const override;

    void showInspectorHighlight(const WebCore::InspectorOverlay::Highlight&) override;
    void hideInspectorHighlight() override;

    void showInspectorIndication() override;
    void hideInspectorIndication() override;

    void enableInspectorNodeSearch() override;
    void disableInspectorNodeSearch() override;

    void scrollingNodeScrollViewWillStartPanGesture(WebCore::ScrollingNodeID) override;
    void scrollingNodeScrollViewDidScroll(WebCore::ScrollingNodeID) override;
    void scrollingNodeScrollWillStartScroll(std::optional<WebCore::ScrollingNodeID>) override;
    void scrollingNodeScrollDidEndScroll(std::optional<WebCore::ScrollingNodeID>) override;
        
    void requestScrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin) override;

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() final;

    // WebFullScreenManagerProxyClient
    void closeFullScreenManager() override;
    bool isFullScreen() override;
    void enterFullScreen(WebCore::FloatSize mediaDimensions, CompletionHandler<void(bool)>&&) override;
#if ENABLE(QUICKLOOK_FULLSCREEN)
    void updateImageSource() override;
#endif
    void exitFullScreen(CompletionHandler<void()>&&) override;
    void beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void(bool)>&&) override;
    void beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void()>&&) override;
    bool lockFullscreenOrientation(WebCore::ScreenOrientationType) override;
    void unlockFullscreenOrientation() override;
#endif

    void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, std::span<const uint8_t>) override;

    Vector<String> mimeTypesWithCustomContentProviders() override;

    void navigationGestureDidBegin() override;
    void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) override;
    void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) override;
    void navigationGestureDidEnd() override;
    void willRecordNavigationSnapshot(WebBackForwardListItem&) override;
    void didRemoveNavigationGestureSnapshot() override;

    void didFirstVisuallyNonEmptyLayoutForMainFrame() override;
    void didFinishNavigation(API::Navigation*) override;
    void didFailNavigation(API::Navigation*) override;
    void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override;
    void didNotHandleTapAsClick(const WebCore::IntPoint&) override;
    void didHandleTapAsHover() override;
    void didCompleteSyntheticClick() override;

    void runModalJavaScriptDialog(CompletionHandler<void()>&& callback) final;

    void didChangeBackgroundColor() override;
    void videoControlsManagerDidChange() override;
    void videosInElementFullscreenChanged() override;

    void refView() override;
    void derefView() override;

    void didRestoreScrollPosition() override;

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() override;

#if USE(QUICK_LOOK)
    void requestPasswordForQuickLookDocument(const String& fileName, WTF::Function<void(const String&)>&&) override;
#endif

    void requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::DOMPasteRequiresInteraction, const WebCore::IntRect& elementRect, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&) final;

#if ENABLE(DRAG_SUPPORT)
    void didPerformDragOperation(bool handled) override;
    void startDrag(const WebCore::DragItem&, WebCore::ShareableBitmap::Handle&& image, const std::optional<WebCore::NodeIdentifier>&) override;
    void willReceiveEditDragSnapshot() override;
    void didReceiveEditDragSnapshot(RefPtr<WebCore::TextIndicator>&&) override;
    void didChangeDragCaretRect(const WebCore::IntRect& previousCaretRect, const WebCore::IntRect& caretRect) override;
#endif

    void performSwitchHapticFeedback() final;

    void handleAutocorrectionContext(const WebAutocorrectionContext&) final;

    void setMouseEventPolicy(WebCore::MouseEventPolicy) final;

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    void showMediaControlsContextMenu(WebCore::FloatRect&&, Vector<WebCore::MediaControlsContextMenuItem>&&, const FrameInfoData&, WebCore::HTMLMediaElementIdentifier,  CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&) final;
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(ATTACHMENT_ELEMENT)
    void writePromisedAttachmentToPasteboard(WebCore::PromisedAttachmentInfo&&) final;
#endif

    void cancelPointersForGestureRecognizer(UIGestureRecognizer*) override;
    std::optional<unsigned> activeTouchIdentifierForGestureRecognizer(UIGestureRecognizer*) override;

    void showDictationAlternativeUI(const WebCore::FloatRect&, WebCore::DictationContext) final;

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
    void handleAsynchronousCancelableScrollEvent(WKBaseScrollView *, WKBEScrollViewScrollUpdate *, void (^completion)(BOOL handled)) final;
#endif

    bool isSimulatingCompatibilityPointerTouches() const final;

    WebCore::FloatBoxExtent computedObscuredInset() const final;
    WebCore::Color contentViewBackgroundColor() final;
    WebCore::Color insertionPointColor() final;
    bool isScreenBeingCaptured() final;

    String sceneID() final;

    void beginTextRecognitionForFullscreenVideo(WebCore::ShareableBitmap::Handle&&, AVPlayerViewController *) final;
    void cancelTextRecognitionForFullscreenVideo(AVPlayerViewController *) final;
    bool isTextRecognitionInFullscreenVideoEnabled() const final;

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)
    void beginTextRecognitionForVideoInElementFullscreen(WebCore::ShareableBitmap::Handle&&, WebCore::FloatRect) final;
    void cancelTextRecognitionForVideoInElementFullscreen() final;
#endif

    bool hasResizableWindows() const final;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void didEnterFullscreen() final;
    void didExitFullscreen() final;
    void didCleanupFullscreen() final;
#endif

    UIViewController *presentingViewController() const final;

    bool isPotentialTapInProgress() const final;

    WebCore::FloatPoint webViewToRootView(const WebCore::FloatPoint&) const final;
    WebCore::FloatRect rootViewToWebView(const WebCore::FloatRect&) const final;

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    void createPDFPageNumberIndicator(PDFPluginIdentifier, const WebCore::IntRect&, size_t pageCount) override;
    void updatePDFPageNumberIndicatorLocation(PDFPluginIdentifier, const WebCore::IntRect&) override;
    void updatePDFPageNumberIndicatorCurrentPage(PDFPluginIdentifier, size_t pageIndex) override;
    void removePDFPageNumberIndicator(PDFPluginIdentifier) override;
    void removeAnyPDFPageNumberIndicator() override;
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    String spatialTrackingLabel() const final;
#endif

    void scheduleVisibleContentRectUpdate() final;

#if ENABLE(POINTER_LOCK)
    void beginPointerLockMouseTracking() final;
    void endPointerLockMouseTracking() final;
#endif

#if ENABLE(VIDEO)
    void showCaptionDisplaySettings(WebCore::HTMLMediaElementIdentifier, const WebCore::ResolvedCaptionDisplaySettingsOptions&, CompletionHandler<void(Expected<void, WebCore::ExceptionData>&&)>&&) final;
#endif

    RetainPtr<WKContentView> contentView() const { return m_contentView.get(); }

    WeakObjCPtr<WKContentView> m_contentView;
    RetainPtr<WKEditorUndoTarget> m_undoTarget;
};
} // namespace WebKit

#endif // PLATFORM(IOS_FAMILY)
