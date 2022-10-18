/*
 * Copyright (C) 2010-2016 Apple Inc. All rights reserved.
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

#include "DataReference.h"
#include "IdentifierTypes.h"
#include "LayerTreeContext.h"
#include "PDFPluginIdentifier.h"
#include "PasteboardAccessIntent.h"
#include "SameDocumentNavigationType.h"
#include "ShareableBitmap.h"
#include "WebColorPicker.h"
#include "WebDataListSuggestionsDropdown.h"
#include "WebDateTimePicker.h"
#include "WebPopupMenuProxy.h"
#include "WindowKind.h"
#include <WebCore/ActivityState.h>
#include <WebCore/AlternativeTextClient.h>
#include <WebCore/ContactInfo.h>
#include <WebCore/ContactsRequestData.h>
#include <WebCore/DataOwnerType.h>
#include <WebCore/DragActions.h>
#include <WebCore/EditorClient.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/InputMode.h>
#include <WebCore/MediaControlsContextMenuItem.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <WebCore/ValidationBubble.h>
#include <variant>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include "RemoteLayerTreeNode.h"
#include "WKFoundation.h"

#if PLATFORM(IOS_FAMILY)
#include <WebCore/InspectorOverlay.h>
#endif

#if ENABLE(IMAGE_ANALYSIS)
#include <WebCore/TextRecognitionResult.h>
#endif

OBJC_CLASS AVPlayerViewController;
OBJC_CLASS CALayer;
OBJC_CLASS NSFileWrapper;
OBJC_CLASS NSMenu;
OBJC_CLASS NSSet;
OBJC_CLASS NSTextAlternatives;
OBJC_CLASS UIGestureRecognizer;
OBJC_CLASS UIScrollEvent;
OBJC_CLASS UIScrollView;
OBJC_CLASS _WKRemoteObjectRegistry;

#if USE(APPKIT)
OBJC_CLASS WKView;
#endif
#endif

namespace API {
class Attachment;
class HitTestResult;
class Navigation;
class Object;
class OpenPanelParameters;
class SecurityOrigin;
}

namespace WebCore {
class Color;
class Cursor;
class DestinationColorSpace;
class FloatQuad;
class FloatRect;
class Region;
class TextIndicator;
class WebMediaSessionManager;

#if PLATFORM(GTK)
class SelectionData;
#endif

enum class MouseEventPolicy : uint8_t;
enum class RouteSharingPolicy : uint8_t;
enum class ScrollbarStyle : uint8_t;
enum class TextIndicatorLifetime : uint8_t;
enum class TextIndicatorDismissalAnimation : uint8_t;
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
enum class ScrollIsAnimated : uint8_t;

struct AppHighlight;
struct DataDetectorElementInfo;
struct DictionaryPopupInfo;
struct TextIndicatorData;
struct ViewportAttributes;
struct ShareDataWithParsedURL;

template <typename> class RectEdges;
using FloatBoxExtent = RectEdges<float>;

#if ENABLE(DRAG_SUPPORT)
struct DragItem;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
struct PromisedAttachmentInfo;
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
struct TranslationContextMenuInfo;
#endif
}

namespace WebKit {

enum class UndoOrRedo : bool;
enum class TapHandlingResult : uint8_t;

class ContextMenuContextData;
class DownloadProxy;
class DrawingAreaProxy;
class NativeWebGestureEvent;
class NativeWebKeyboardEvent;
class NativeWebMouseEvent;
class NativeWebWheelEvent;
class RemoteLayerTreeTransaction;
class SafeBrowsingWarning;
class UserData;
class ViewSnapshot;
class WebBackForwardListItem;
class WebContextMenuProxy;
class WebEditCommandProxy;
class WebFrameProxy;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
class WebPopupMenuProxy;
class WebProcessProxy;

enum class ContinueUnsafeLoad : bool { No, Yes };

struct FocusedElementInformation;
struct FrameInfoData;
struct InteractionInformationAtPosition;
struct WebAutocorrectionContext;
struct WebHitTestResultData;

#if ENABLE(TOUCH_EVENTS)
class NativeWebTouchEvent;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
class WebColorPicker;
#endif

#if ENABLE(DATALIST_ELEMENT)
class WebDataListSuggestionsDropdown;
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
class WebDateTimePicker;
#endif

#if ENABLE(FULLSCREEN_API)
class WebFullScreenManagerProxyClient;
#endif

#if USE(GSTREAMER)
class InstallMissingMediaPluginsPermissionRequest;
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
using LayerHostingContextID = uint32_t;
#endif

class PageClient : public CanMakeWeakPtr<PageClient> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~PageClient() { }

    // Create a new drawing area proxy for the given page.
    virtual std::unique_ptr<DrawingAreaProxy> createDrawingAreaProxy(WebProcessProxy&) = 0;

    // Tell the view to invalidate the given region. The region is in view coordinates.
    virtual void setViewNeedsDisplay(const WebCore::Region&) = 0;

    // Tell the view to scroll to the given position, and whether this was a programmatic scroll.
    virtual void requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, WebCore::ScrollIsAnimated) = 0;

    // Return the current scroll position (not necessarily the same as the WebCore scroll position, because of scaling, insets etc.)
    virtual WebCore::FloatPoint viewScrollPosition() = 0;

    // Return the size of the view the page is associated with.
    virtual WebCore::IntSize viewSize() = 0;

    // Return whether the view's containing window is active.
    virtual bool isViewWindowActive() = 0;

    // Return whether the view is focused.
    virtual bool isViewFocused() = 0;

    // Return whether the view is visible.
    virtual bool isViewVisible() = 0;

#if USE(RUNNINGBOARD)
    virtual bool canTakeForegroundAssertions() = 0;
#endif

    // Return whether the view is visible, or occluded by another window.
    virtual bool isViewVisibleOrOccluded() { return isViewVisible(); }

    // Return whether the view is in a window.
    virtual bool isViewInWindow() = 0;

    // Return whether the view is visually idle.
    virtual bool isVisuallyIdle() { return !isViewVisible(); }

    // Return the layer hosting mode for the view.
    virtual LayerHostingMode viewLayerHostingMode() { return LayerHostingMode::InProcess; }

    virtual WindowKind windowKind() { return isViewInWindow() ? WindowKind::Normal : WindowKind::Unparented; }

    virtual void processDidExit() = 0;
    virtual void processWillSwap() { processDidExit(); }
    virtual void didRelaunchProcess() = 0;
    virtual void pageClosed() = 0;

    virtual void preferencesDidChange() = 0;

    virtual void toolTipChanged(const String&, const String&) = 0;

#if PLATFORM(IOS_FAMILY)
    // FIXME: Adopt the WKUIDelegatePrivate callback on iOS and remove this.
    virtual void decidePolicyForGeolocationPermissionRequest(WebFrameProxy&, const FrameInfoData&, Function<void(bool)>&) = 0;
#endif

    virtual void didStartProvisionalLoadForMainFrame() { };
    virtual void didFailProvisionalLoadForMainFrame() { };
    virtual void didCommitLoadForMainFrame(const String& mimeType, bool useCustomContentProvider) = 0;

#if ENABLE(UI_PROCESS_PDF_HUD)
    virtual void createPDFHUD(PDFPluginIdentifier, const WebCore::IntRect&) = 0;
    virtual void updatePDFHUDLocation(PDFPluginIdentifier, const WebCore::IntRect&) = 0;
    virtual void removePDFHUD(PDFPluginIdentifier) = 0;
    virtual void removeAllPDFHUDs() = 0;
#endif
    
    virtual void handleDownloadRequest(DownloadProxy&) = 0;

    virtual bool handleRunOpenPanel(WebPageProxy*, WebFrameProxy*, const FrameInfoData&, API::OpenPanelParameters*, WebOpenPanelResultListenerProxy*) { return false; }
    virtual bool showShareSheet(const WebCore::ShareDataWithParsedURL&, WTF::CompletionHandler<void (bool)>&&) { return false; }
    virtual void showContactPicker(const WebCore::ContactsRequestData&, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& completionHandler) { completionHandler(std::nullopt); }

    virtual void didChangeContentSize(const WebCore::IntSize&) = 0;

    virtual void topContentInsetDidChange() { }

    virtual void showSafeBrowsingWarning(const SafeBrowsingWarning&, CompletionHandler<void(std::variant<ContinueUnsafeLoad, URL>&&)>&& completionHandler) { completionHandler(ContinueUnsafeLoad::Yes); }
    virtual void clearSafeBrowsingWarning() { }
    virtual void clearSafeBrowsingWarningIfForMainFrameNavigation() { }
    
#if ENABLE(DRAG_SUPPORT)
#if PLATFORM(GTK)
    virtual void startDrag(WebCore::SelectionData&&, OptionSet<WebCore::DragOperation>, RefPtr<ShareableBitmap>&& dragImage, WebCore::IntPoint&& dragImageHotspot) = 0;
#else
    virtual void startDrag(const WebCore::DragItem&, const ShareableBitmapHandle&) { }
#endif
    virtual void didPerformDragOperation(bool) { }
    virtual void didPerformDragControllerAction() { }
#endif // ENABLE(DRAG_SUPPORT)

    virtual void setCursor(const WebCore::Cursor&) = 0;
    virtual void setCursorHiddenUntilMouseMoves(bool) = 0;
    virtual void didChangeViewportProperties(const WebCore::ViewportAttributes&) = 0;

    virtual void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo) = 0;
    virtual void clearAllEditCommands() = 0;
    virtual bool canUndoRedo(UndoOrRedo) = 0;
    virtual void executeUndoRedo(UndoOrRedo) = 0;
    virtual void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) = 0;
#if PLATFORM(COCOA)
    virtual void accessibilityWebProcessTokenReceived(const IPC::DataReference&) = 0;
    virtual bool executeSavedCommandBySelector(const String& selector) = 0;
    virtual void updateSecureInputState() = 0;
    virtual void resetSecureInputState() = 0;
    virtual void notifyInputContextAboutDiscardedComposition() = 0;
    virtual void makeFirstResponder() = 0;
    virtual void assistiveTechnologyMakeFirstResponder() = 0;
    virtual void setRemoteLayerTreeRootNode(RemoteLayerTreeNode*) = 0;
    virtual CALayer *acceleratedCompositingRootLayer() const = 0;
#if ENABLE(MAC_GESTURE_EVENTS)
    virtual void gestureEventWasNotHandledByWebCore(const NativeWebGestureEvent&) = 0;
#endif
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    virtual void selectionDidChange() = 0;
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK)
    virtual RefPtr<ViewSnapshot> takeViewSnapshot(std::optional<WebCore::IntRect>&&) = 0;
#endif

#if USE(APPKIT)
    virtual void setPromisedDataForImage(const String& pasteboardName, Ref<WebCore::FragmentedSharedBuffer>&& imageBuffer, const String& filename, const String& extension, const String& title, const String& url, const String& visibleURL, RefPtr<WebCore::FragmentedSharedBuffer>&& archiveBuffer, const String& originIdentifier) = 0;
#endif

    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) = 0;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) = 0;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) = 0;
    virtual WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) = 0;
#if PLATFORM(MAC)
    virtual WebCore::IntRect rootViewToWindow(const WebCore::IntRect&) = 0;
#endif
#if PLATFORM(IOS_FAMILY)
    virtual void didNotHandleTapAsClick(const WebCore::IntPoint&) = 0;
    virtual void didCompleteSyntheticClick() = 0;
#endif

    virtual void runModalJavaScriptDialog(CompletionHandler<void()>&& callback) { callback(); }

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    virtual void didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID) { }
#if ENABLE(GPU_PROCESS)
    virtual void didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID) { }
#endif
#endif

#if ENABLE(GPU_PROCESS)
    virtual void gpuProcessDidFinishLaunching() { }
    virtual void gpuProcessDidExit() { }
#endif

    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) = 0;
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const NativeWebTouchEvent&, bool wasEventHandled) = 0;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    virtual void doneDeferringTouchStart(bool preventNativeGestures) = 0;
    virtual void doneDeferringTouchMove(bool preventNativeGestures) = 0;
    virtual void doneDeferringTouchEnd(bool preventNativeGestures) = 0;
#endif

    virtual RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) = 0;
#if ENABLE(CONTEXT_MENUS)
    virtual Ref<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, ContextMenuContextData&&, const UserData&) = 0;
    virtual void didShowContextMenu() { }
    virtual void didDismissContextMenu() { }
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual RefPtr<WebColorPicker> createColorPicker(WebPageProxy*, const WebCore::Color& initialColor, const WebCore::IntRect&, Vector<WebCore::Color>&&) = 0;
#endif

#if ENABLE(DATALIST_ELEMENT)
    virtual RefPtr<WebDataListSuggestionsDropdown> createDataListSuggestionsDropdown(WebPageProxy&) = 0;
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    virtual RefPtr<WebDateTimePicker> createDateTimePicker(WebPageProxy&) = 0;
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK)
    virtual Ref<WebCore::ValidationBubble> createValidationBubble(const String& message, const WebCore::ValidationBubble::Settings&) = 0;
#endif

#if PLATFORM(COCOA)
    virtual void setTextIndicator(Ref<WebCore::TextIndicator>, WebCore::TextIndicatorLifetime) = 0;
    virtual void clearTextIndicator(WebCore::TextIndicatorDismissalAnimation) = 0;
    virtual void setTextIndicatorAnimationProgress(float) = 0;
    
    virtual void didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&) = 0;
#endif

#if HAVE(APP_ACCENT_COLORS)
    virtual WebCore::Color accentColor() = 0;
#endif

    virtual bool effectiveAppearanceIsDark() const { return false; }
    virtual bool effectiveUserInterfaceLevelIsElevated() const { return false; }

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) = 0;
    virtual void exitAcceleratedCompositingMode() = 0;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) = 0;
    virtual void didFirstLayerFlush(const LayerTreeContext&) { }

    virtual void takeFocus(WebCore::FocusDirection) { }

#if USE(DICTATION_ALTERNATIVES)
    virtual WebCore::DictationContext addDictationAlternatives(NSTextAlternatives *) = 0;
    virtual void replaceDictationAlternatives(NSTextAlternatives *, WebCore::DictationContext) = 0;
    virtual void removeDictationAlternatives(WebCore::DictationContext) = 0;
    virtual void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext) = 0;
    virtual Vector<String> dictationAlternatives(WebCore::DictationContext) = 0;
    virtual NSTextAlternatives *platformDictationAlternatives(WebCore::DictationContext) = 0;
#endif

#if PLATFORM(MAC)
    virtual void showCorrectionPanel(WebCore::AlternativeTextType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings) = 0;
    virtual void dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeText) = 0;
    virtual String dismissCorrectionPanelSoon(WebCore::ReasonForDismissingAlternativeText) = 0;
    virtual void recordAutocorrectionResponse(WebCore::AutocorrectionResponse, const String& replacedString, const String& replacementString) = 0;
    virtual void recommendedScrollbarStyleDidChange(WebCore::ScrollbarStyle) = 0;
    virtual void handleControlledElementIDResponse(const String&) = 0;

    virtual CGRect boundsOfLayerInLayerBackedWindowCoordinates(CALayer *) const = 0;

    virtual WebCore::DestinationColorSpace colorSpace() = 0;
    
    virtual NSView *viewForPresentingRevealPopover() const = 0;

    virtual void showPlatformContextMenu(NSMenu *, WebCore::IntPoint) = 0;

    virtual void startWindowDrag() = 0;
    virtual NSWindow *platformWindow() = 0;
    virtual void setShouldSuppressFirstResponderChanges(bool) = 0;

    virtual NSView *inspectorAttachmentView() = 0;
    virtual _WKRemoteObjectRegistry *remoteObjectRegistry() = 0;

    virtual void intrinsicContentSizeDidChange(const WebCore::IntSize& intrinsicContentSize) = 0;

    virtual void registerInsertionUndoGrouping() = 0;

    virtual void setEditableElementIsFocused(bool) = 0;
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
    virtual void commitPotentialTapFailed() = 0;
    virtual void didGetTapHighlightGeometries(WebKit::TapIdentifier requestID, const WebCore::Color&, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling) = 0;

    virtual void didCommitLayerTree(const RemoteLayerTreeTransaction&) = 0;
    virtual void layerTreeCommitComplete() = 0;

    virtual void couldNotRestorePageState() = 0;
    virtual void restorePageState(std::optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale) = 0;
    virtual void restorePageCenterAndScale(std::optional<WebCore::FloatPoint> center, double scale) = 0;

    virtual void elementDidFocus(const FocusedElementInformation&, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState::Flag> activityStateChanges, API::Object* userData) = 0;
    virtual void updateInputContextAfterBlurringAndRefocusingElement() = 0;
    virtual void elementDidBlur() = 0;
    virtual void focusedElementDidChangeInputMode(WebCore::InputMode) = 0;
    virtual void didUpdateEditorState() = 0;
    virtual bool isFocusingElement() = 0;
    virtual bool interpretKeyEvent(const NativeWebKeyboardEvent&, bool isCharEvent) = 0;
    virtual void positionInformationDidChange(const InteractionInformationAtPosition&) = 0;
    virtual void saveImageToLibrary(Ref<WebCore::SharedBuffer>&&) = 0;
    virtual void showPlaybackTargetPicker(bool hasVideo, const WebCore::IntRect& elementRect, WebCore::RouteSharingPolicy, const String&) = 0;
    virtual void showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition&) = 0;
    virtual void disableDoubleTapGesturesDuringTapIfNecessary(WebKit::TapIdentifier) = 0;
    virtual void handleSmartMagnificationInformationForPotentialTap(WebKit::TapIdentifier, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel) = 0;
    virtual double minimumZoomScale() const = 0;
    virtual WebCore::FloatRect documentRect() const = 0;
    virtual void scrollingNodeScrollViewWillStartPanGesture(WebCore::ScrollingNodeID) = 0;
    virtual void scrollingNodeScrollViewDidScroll(WebCore::ScrollingNodeID) = 0;
    virtual void scrollingNodeScrollWillStartScroll(WebCore::ScrollingNodeID) = 0;
    virtual void scrollingNodeScrollDidEndScroll(WebCore::ScrollingNodeID) = 0;
    virtual Vector<String> mimeTypesWithCustomContentProviders() = 0;

    virtual void showInspectorHighlight(const WebCore::InspectorOverlay::Highlight&) = 0;
    virtual void hideInspectorHighlight() = 0;

    virtual void showInspectorIndication() = 0;
    virtual void hideInspectorIndication() = 0;

    virtual void enableInspectorNodeSearch() = 0;
    virtual void disableInspectorNodeSearch() = 0;

    virtual void handleAutocorrectionContext(const WebAutocorrectionContext&) = 0;

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
    virtual void handleAsynchronousCancelableScrollEvent(UIScrollView *, UIScrollEvent *, void (^completion)(BOOL handled)) = 0;
#endif

    virtual WebCore::Color contentViewBackgroundColor() = 0;
    virtual String sceneID() = 0;

    virtual void beginTextRecognitionForFullscreenVideo(const ShareableBitmapHandle&, AVPlayerViewController *) = 0;
    virtual void cancelTextRecognitionForFullscreenVideo(AVPlayerViewController *) = 0;
#endif
    virtual bool isTextRecognitionInFullscreenVideoEnabled() const { return false; }

    virtual void beginTextRecognitionForVideoInElementFullscreen(const ShareableBitmapHandle&, WebCore::FloatRect) { }
    virtual void cancelTextRecognitionForVideoInElementFullscreen() { }

    // Auxiliary Client Creation
#if ENABLE(FULLSCREEN_API)
    virtual WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() = 0;
#endif

    // Custom representations.
    virtual void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&) = 0;

    virtual void navigationGestureDidBegin() = 0;
    virtual void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) = 0;
    virtual void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) = 0;
    virtual void navigationGestureDidEnd() = 0;
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) = 0;
    virtual void didRemoveNavigationGestureSnapshot() = 0;

    virtual void didFirstVisuallyNonEmptyLayoutForMainFrame() = 0;
    virtual void didFinishNavigation(API::Navigation*) = 0;
    virtual void didFailNavigation(API::Navigation*) = 0;
    virtual void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) = 0;

    virtual void themeColorWillChange() { }
    virtual void themeColorDidChange() { }
    virtual void underPageBackgroundColorWillChange() { }
    virtual void underPageBackgroundColorDidChange() { }
    virtual void pageExtendedBackgroundColorWillChange() { }
    virtual void pageExtendedBackgroundColorDidChange() { }
    virtual void sampledPageTopColorWillChange() { }
    virtual void sampledPageTopColorDidChange() { }
    virtual void didChangeBackgroundColor() = 0;
    virtual void isPlayingAudioWillChange() = 0;
    virtual void isPlayingAudioDidChange() = 0;

    virtual void pinnedStateWillChange() { }
    virtual void pinnedStateDidChange() { }
    virtual void drawPageBorderForPrinting(WebCore::FloatSize&& size) { }
    virtual bool scrollingUpdatesDisabledForTesting() { return false; }

    virtual bool hasSafeBrowsingWarning() const { return false; }

    virtual void setMouseEventPolicy(WebCore::MouseEventPolicy) { }

    virtual void makeViewBlank(bool) { }

    virtual WebCore::DataOwnerType dataOwnerForPasteboard(PasteboardAccessIntent) const { return WebCore::DataOwnerType::Undefined; }

    virtual bool hasResizableWindows() const { return false; }

#if ENABLE(IMAGE_ANALYSIS)
    virtual void requestTextRecognition(const URL& imageURL, const ShareableBitmapHandle& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&& completion) { completion({ }); }
    virtual void computeHasVisualSearchResults(const URL&, ShareableBitmap&, CompletionHandler<void(bool)>&& completion) { completion(false); }
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    virtual void showMediaControlsContextMenu(WebCore::FloatRect&&, Vector<WebCore::MediaControlsContextMenuItem>&&, CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&& completionHandler) { completionHandler(WebCore::MediaControlsContextMenuItem::invalidID); }
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    
#if PLATFORM(MAC)
    virtual void didPerformImmediateActionHitTest(const WebHitTestResultData&, bool contentPreventsDefault, API::Object*) = 0;
    virtual NSObject *immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult>, uint64_t, RefPtr<API::Object>) = 0;
    virtual void didHandleAcceptedCandidate() = 0;
#endif

    virtual void microphoneCaptureWillChange() { }
    virtual void cameraCaptureWillChange() { }
    virtual void displayCaptureWillChange() { }
    virtual void displayCaptureSurfacesWillChange() { }
    virtual void systemAudioCaptureWillChange() { }
    virtual void microphoneCaptureChanged() { }
    virtual void cameraCaptureChanged() { }
    virtual void displayCaptureChanged() { }
    virtual void displayCaptureSurfacesChanged() { }
    virtual void systemAudioCaptureChanged() { }

    virtual void videoControlsManagerDidChange() { }

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    virtual WebCore::WebMediaSessionManager& mediaSessionManager() = 0;
#endif

    virtual void refView() = 0;
    virtual void derefView() = 0;

#if ENABLE(VIDEO) && USE(GSTREAMER)
    virtual bool decidePolicyForInstallMissingMediaPluginsPermissionRequest(InstallMissingMediaPluginsPermissionRequest&) = 0;
#endif

    virtual void pageDidScroll(const WebCore::IntPoint&) { }

    virtual void didRestoreScrollPosition() = 0;

    virtual bool windowIsFrontWindowUnderMouse(const NativeWebMouseEvent&) { return false; }

    virtual WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() = 0;

#if USE(QUICK_LOOK)
    virtual void requestPasswordForQuickLookDocument(const String& fileName, WTF::Function<void(const String&)>&&) = 0;
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    virtual void didHandleDragStartRequest(bool started) = 0;
    virtual void didHandleAdditionalDragItemsRequest(bool added) = 0;
    virtual void willReceiveEditDragSnapshot() = 0;
    virtual void didReceiveEditDragSnapshot(std::optional<WebCore::TextIndicatorData>) = 0;
    virtual void didChangeDragCaretRect(const WebCore::IntRect& previousCaretRect, const WebCore::IntRect& caretRect) = 0;
#endif

    virtual void requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, const WebCore::IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&) = 0;

#if ENABLE(ATTACHMENT_ELEMENT)
    virtual void didInsertAttachment(API::Attachment&, const String& source) { }
    virtual void didRemoveAttachment(API::Attachment&) { }
    virtual void didInvalidateDataForAttachment(API::Attachment&) { }
#if PLATFORM(IOS_FAMILY)
    virtual void writePromisedAttachmentToPasteboard(WebCore::PromisedAttachmentInfo&&) { }
#endif
#if PLATFORM(COCOA)
    virtual NSFileWrapper *allocFileWrapperInstance() const { return nullptr; }
    virtual NSSet *serializableFileWrapperClasses() const { return nullptr; }
#endif
#endif

#if ENABLE(APP_HIGHLIGHTS)
    virtual void storeAppHighlight(const WebCore::AppHighlight&) = 0;
#endif
    virtual void requestScrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin) { }

#if PLATFORM(COCOA)
    virtual void cancelPointersForGestureRecognizer(UIGestureRecognizer*) { }
    virtual std::optional<unsigned> activeTouchIdentifierForGestureRecognizer(UIGestureRecognizer*) { return std::nullopt; }
#endif

#if USE(WPE_RENDERER)
    virtual UnixFileDescriptor hostFileDescriptor() = 0;
#endif

    virtual void didChangeWebPageID() const { }

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
    virtual bool canHandleContextMenuTranslation() const = 0;
    virtual void handleContextMenuTranslation(const WebCore::TranslationContextMenuInfo&) = 0;
#endif

#if ENABLE(DATA_DETECTION)
    virtual void handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&) { }
#endif

#if USE(GRAPHICS_LAYER_WC)
    virtual bool usesOffscreenRendering() const = 0;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    virtual void didEnterFullscreen() = 0;
    virtual void didExitFullscreen() = 0;
#endif
};

} // namespace WebKit
