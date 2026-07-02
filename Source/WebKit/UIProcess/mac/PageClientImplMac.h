/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include <WebCore/DOMPasteAccess.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/WeakObjCPtr.h>

@class WKEditorUndoTarget;
@class WKView;

namespace WebCore {
class AlternativeTextUIController;
struct DragItem;
struct PromisedAttachmentInfo;
}

namespace WebKit {

enum class ColorControlSupportsAlpha : bool;

class RemoteLayerTreeNode;
class WebViewImpl;

class PageClientImpl final : public PageClientImplCocoa
#if ENABLE(FULLSCREEN_API)
    , public WebFullScreenManagerProxyClient
#endif
    {
    WTF_MAKE_TZONE_ALLOCATED(PageClientImpl);
#if ENABLE(FULLSCREEN_API)
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(PageClientImpl);
#endif
public:
    PageClientImpl(NSView *, WKWebView *);
    virtual ~PageClientImpl();

    // FIXME: Eventually WebViewImpl should become the PageClient.
    void NODELETE setImpl(WebViewImpl&);

    void NODELETE viewWillMoveToAnotherWindow();

private:
    // PageClient
    Ref<DrawingAreaProxy> createDrawingAreaProxy(WebProcessProxy&) override;
    void NODELETE setViewNeedsDisplay(const WebCore::Region&) override;
    void NODELETE requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, WebCore::ScrollIsAnimated, WebCore::InterruptScrollAnimation) override;
    WebCore::FloatPoint NODELETE viewScrollPosition() override;

    WebCore::IntSize NODELETE viewSize() override;
    bool NODELETE isViewWindowActive() override;
    bool NODELETE isViewFocused() override;
    bool NODELETE isActiveViewVisible() override;
    bool NODELETE isMainViewVisible() override;
    bool canTakeForegroundAssertions() override { return true; };
    void NODELETE scrollingCoordinatorWasCreated() override;
    bool NODELETE isViewVisibleOrOccluded() override;
    bool NODELETE isViewInWindow() override;
    bool NODELETE isVisuallyIdle() override;
    WebCore::DestinationColorSpace NODELETE colorSpace() override;
    void NODELETE setRemoteLayerTreeRootNode(RemoteLayerTreeNode*) override;
    CALayer *NODELETE acceleratedCompositingRootLayer() const override;
    CALayer *NODELETE headerBannerLayer() const override;
    CALayer *NODELETE footerBannerLayer() const override;

    void NODELETE processDidExit() override;
    void NODELETE processWillSwap() override;
    void NODELETE pageClosed() override;
    void NODELETE didRelaunchProcess() override;
    void NODELETE preferencesDidChange() override;
    void NODELETE toolTipChanged(const String& oldToolTip, const String& newToolTip) override;
    void NODELETE didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) override;
    void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, std::span<const uint8_t>) override;
    void NODELETE didChangeContentSize(const WebCore::IntSize&) override;
    void NODELETE setCursor(const WebCore::Cursor&) override;
    void NODELETE setCursorHiddenUntilMouseMoves(bool) override;

    void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo) override;
    void NODELETE clearAllEditCommands() override;
    bool NODELETE canUndoRedo(UndoOrRedo) override;
    void NODELETE executeUndoRedo(UndoOrRedo) override;
    bool NODELETE executeSavedCommandBySelector(const String& selector) override;
    void NODELETE startDrag(const WebCore::DragItem&, WebCore::ShareableBitmap::Handle&& image, const std::optional<WebCore::NodeIdentifier>&, const std::optional<WebCore::FrameIdentifier>& = std::nullopt) override;
    void setPromisedDataForImage(const String& pasteboardName, Ref<WebCore::FragmentedSharedBuffer>&& imageBuffer, const String& filename, const String& extension, const String& title,
        const String& url, const String& visibleURL, RefPtr<WebCore::FragmentedSharedBuffer>&& archiveBuffer, const String& originIdentifier) override;
    void NODELETE updateSecureInputState() override;
    void NODELETE resetSecureInputState() override;
    void NODELETE notifyInputContextAboutDiscardedComposition() override;
    void NODELETE selectionDidChange() override;
    void NODELETE showBrowsingWarning(const BrowsingWarning&, CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&) override;
    void NODELETE clearBrowsingWarning() override;
    void NODELETE clearBrowsingWarningIfForMainFrameNavigation() override;
    bool NODELETE hasBrowsingWarning() const override;

    void NODELETE didChangeLocalInspectorAttachment() final;

    bool NODELETE showShareSheet(WebCore::ShareDataWithParsedURL&&, WTF::CompletionHandler<void(bool)>&&) override;

#if ENABLE(WEB_AUTHN)
    void showDigitalCredentialsChooser(const WebCore::DigitalCredentialsRequestData&, WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&) override;
    void NODELETE dismissDigitalCredentialsChooser(WTF::CompletionHandler<void(bool)>&&) override;
#endif

    WebCore::FloatRect NODELETE convertToDeviceSpace(const WebCore::FloatRect&) override;
    WebCore::FloatRect NODELETE convertToUserSpace(const WebCore::FloatRect&) override;
    WebCore::IntPoint NODELETE screenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntPoint NODELETE rootViewToScreen(const WebCore::IntPoint&) override;
    WebCore::IntRect NODELETE rootViewToScreen(const WebCore::IntRect&) override;
#if PLATFORM(MAC)
    WebCore::IntRect NODELETE rootViewToWindow(const WebCore::IntRect&) override;
#endif
    WebCore::IntPoint NODELETE accessibilityScreenToRootView(const WebCore::IntPoint&) override;
    WebCore::IntRect NODELETE rootViewToAccessibilityScreen(const WebCore::IntRect&) override;

    void NODELETE pinnedStateWillChange() final;
    void NODELETE pinnedStateDidChange() final;
        
    void NODELETE drawPageBorderForPrinting(WebCore::FloatSize&&) final;

    CGRect NODELETE boundsOfLayerInLayerBackedWindowCoordinates(CALayer *) const override;

    void NODELETE doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) override;

#if ENABLE(IMAGE_ANALYSIS)
    void NODELETE requestTextRecognition(const URL& imageURL, WebCore::ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&) override;
    void NODELETE computeHasVisualSearchResults(const URL&, WebCore::ShareableBitmap&, CompletionHandler<void(bool)>&&) override;
#endif

    RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) override;
#if ENABLE(CONTEXT_MENUS)
    Ref<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, FrameInfoData&&, ContextMenuContextData&&, const UserData&) override;
    void NODELETE didShowContextMenu() override;
    void NODELETE didDismissContextMenu() override;
#endif

    RefPtr<WebColorPicker> createColorPicker(WebPageProxy&, const WebCore::Color& initialColor, const WebCore::IntRect&, ColorControlSupportsAlpha, Vector<WebCore::Color>&&, std::optional<WebCore::FrameIdentifier>) override;

    RefPtr<WebDataListSuggestionsDropdown> createDataListSuggestionsDropdown(WebPageProxy&) override;

    RefPtr<WebDateTimePicker> createDateTimePicker(WebPageProxy&) override;

    Ref<WebCore::ValidationBubble> createValidationBubble(String&& message, const WebCore::ValidationBubble::Settings&) final;

    CALayer *NODELETE textIndicatorInstallationLayer() override;

    void NODELETE enterAcceleratedCompositingMode(const LayerTreeContext&) override;
    void NODELETE exitAcceleratedCompositingMode() override;
    void NODELETE updateAcceleratedCompositingMode(const LayerTreeContext&) override;
    void NODELETE didFirstLayerFlush(const LayerTreeContext&) override;

    RefPtr<ViewSnapshot> takeViewSnapshot(std::optional<WebCore::IntRect>&&) override;
    RefPtr<ViewSnapshot> takeViewSnapshot(std::optional<WebCore::IntRect>&&, ForceSoftwareCapturingViewportSnapshot) override;
    void NODELETE wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) override;
#if ENABLE(MAC_GESTURE_EVENTS)
    void gestureEventWasNotHandledByWebCore(const NativeWebGestureEvent&) override;
#endif

    void accessibilityWebProcessTokenReceived(std::span<const uint8_t>, pid_t) override;

    void NODELETE makeFirstResponder() override;
    void NODELETE assistiveTechnologyMakeFirstResponder() override;
    void setShouldSuppressFirstResponderChanges(bool shouldSuppress) override { m_shouldSuppressFirstResponderChanges = shouldSuppress; }

    void NODELETE didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&) override;

    void showCorrectionPanel(WebCore::AlternativeTextType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings) override;
    void NODELETE dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeText) override;
    String NODELETE dismissCorrectionPanelSoon(WebCore::ReasonForDismissingAlternativeText) override;
    void NODELETE recordAutocorrectionResponse(WebCore::AutocorrectionResponse, const String& replacedString, const String& replacementString) override;

    void NODELETE recommendedScrollbarStyleDidChange(WebCore::ScrollbarStyle) override;

    void NODELETE intrinsicContentSizeDidChange(const WebCore::IntSize& intrinsicContentSize) override;

    void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext) final;

    void NODELETE setFocusedElementInputType(InputType) override;

    void NODELETE scrollingNodeScrollViewDidScroll(WebCore::ScrollingNodeID) override;

#if HAVE(NSREFRESHCONTROLLER)
    void topScrollStretchDidChange(CGFloat) override;
#endif

    void NODELETE registerInsertionUndoGrouping() override;

    void createPDFHUD(PDFPluginIdentifier, WebCore::FrameIdentifier, const WebCore::IntRect&) override;
    void updatePDFHUDLocation(PDFPluginIdentifier, const WebCore::IntRect&) override;
    void removePDFHUD(PDFPluginIdentifier) override;
    void NODELETE removeAllPDFHUDs() override;
    void showPDFHUD(PDFPluginIdentifier) final;

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxyClient& NODELETE fullScreenManagerProxyClient() final;
#endif

#if ENABLE(FULLSCREEN_API)
    // WebFullScreenManagerProxyClient
    void NODELETE closeFullScreenManager() override;
    bool NODELETE isFullScreen() override;
    void NODELETE enterFullScreen(WebCore::FloatSize, CompletionHandler<void(bool)>&&) override;
    void NODELETE exitFullScreen(CompletionHandler<void()>&&) override;
    void NODELETE beganEnterFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void(bool)>&&) override;
    void NODELETE beganExitFullScreen(const WebCore::IntRect& initialFrame, const WebCore::IntRect& finalFrame, CompletionHandler<void()>&&) override;
#endif

    void NODELETE navigationGestureDidBegin() override;
    void NODELETE navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) override;
    void NODELETE navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) override;
    void NODELETE navigationGestureDidEnd() override;
    void NODELETE willRecordNavigationSnapshot(WebBackForwardListItem&) override;
    void NODELETE didRemoveNavigationGestureSnapshot() override;

    void NODELETE willBeginViewGesture() final;
    void NODELETE didEndViewGesture() final;

    void NODELETE requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::DOMPasteRequiresInteraction, WebCore::FrameIdentifier, const WebCore::IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&) final;

    void NODELETE makeViewBlank(bool) final;

    NSView *NODELETE activeView() const;
    NSWindow *NODELETE activeWindow() const;
    NSView *viewForPresentingRevealPopover() const override { return activeView(); }

    void NODELETE didStartProvisionalLoadForMainFrame() override;
    void NODELETE didFirstVisuallyNonEmptyLayoutForMainFrame() override;
    void NODELETE didFinishNavigation(API::Navigation*) override;
    void NODELETE didFailNavigation(API::Navigation*) override;
    void NODELETE didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) override;
    void NODELETE handleControlledElementIDResponse(const String&) override;

    void NODELETE didPerformImmediateActionHitTest(const WebHitTestResultData&, bool contentPreventsDefault, API::Object*) override;
    NSObject *immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult>, uint64_t, RefPtr<API::Object>) override;

    void NODELETE videoControlsManagerDidChange() override;

    void NODELETE showPlatformContextMenu(NSMenu *, WebCore::IntPoint) override;

    void NODELETE didChangeBackgroundColor() override;

    void NODELETE startWindowDrag() override;

    WebCore::UserInterfaceLayoutDirection NODELETE userInterfaceLayoutDirection() override;
    bool NODELETE effectiveAppearanceIsDark() const override;
    bool NODELETE effectiveUserInterfaceLevelIsElevated() const override;

    bool NODELETE useFormSemanticContext() const override;

    bool isTextRecognitionInFullscreenVideoEnabled() const final { return true; }
    void NODELETE beginTextRecognitionForVideoInElementFullscreen(WebCore::ShareableBitmap::Handle&&, WebCore::FloatRect) final;
    void NODELETE cancelTextRecognitionForVideoInElementFullscreen() final;

#if ENABLE(DRAG_SUPPORT)
    void NODELETE didPerformDragOperation(bool handled) final;
#endif

    RetainPtr<NSView> NODELETE inspectorAttachmentView() override;
    _WKRemoteObjectRegistry *NODELETE remoteObjectRegistry() override;

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    WebCore::WebMediaSessionManager& NODELETE mediaSessionManager() final;
#endif

    void NODELETE refView() override;
    void NODELETE derefView() override;

    void NODELETE pageDidScroll(const WebCore::IntPoint&) override;
    void NODELETE didEndSyntheticMomentumScrolling() override;
    void NODELETE didRestoreScrollPosition() override;
    bool NODELETE windowIsFrontWindowUnderMouse(const NativeWebMouseEvent&) override;

#if ENABLE(HORIZONTAL_BANNER_VIEW_OVERLAYS)
    void NODELETE didUpdateTransientZoomStateForScrollPocket(std::optional<TransientZoomState>) override;
#endif
    std::optional<float> NODELETE computeAutomaticTopObscuredInset() override;

    void NODELETE takeFocus(WebCore::FocusDirection) override;

    void NODELETE performSwitchHapticFeedback() final;

#if HAVE(APP_ACCENT_COLORS)
    WebCore::Color NODELETE accentColor() override;
#if PLATFORM(MAC)
    bool NODELETE appUsesCustomAccentColor() override;
#endif
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
    bool NODELETE canHandleContextMenuTranslation() const override;
    void NODELETE handleContextMenuTranslation(const WebCore::TranslationContextMenuInfo&) override;
#endif

#if ENABLE(WRITING_TOOLS) && ENABLE(CONTEXT_MENUS)
    bool NODELETE canHandleContextMenuWritingTools() const override;
    void NODELETE handleContextMenuWritingTools(WebCore::WritingTools::RequestedTool, WebCore::IntRect) override;
#endif

#if ENABLE(DATA_DETECTION)
    void NODELETE handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&) final;
#endif
        
    void NODELETE requestScrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin) override;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void didEnterFullscreen() final { }
    void didExitFullscreen() final { }
    void didCleanupFullscreen() final { }
#endif

#if ENABLE(VIDEO)
    void showCaptionDisplaySettings(WebCore::HTMLMediaElementIdentifier, const WebCore::ResolvedCaptionDisplaySettingsOptions&, CompletionHandler<void(Expected<void, WebCore::ExceptionData>&&)>&&) final;
#endif

    void NODELETE positionInformationDidChange(const InteractionInformationAtPosition&) override;

    bool NODELETE isViewVisible(NSView *, NSWindow *) const final;

    WeakObjCPtr<NSView> m_view;
    WeakPtr<WebViewImpl> m_impl;
#if USE(AUTOCORRECTION_PANEL)
    CorrectionPanel m_correctionPanel;
#endif

    bool m_shouldSuppressFirstResponderChanges { false };
};

} // namespace WebKit

#endif // PLATFORM(MAC)
