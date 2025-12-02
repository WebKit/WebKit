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

#include "IdentifierTypes.h"
#include "LayerTreeContext.h"
#include "PDFPluginIdentifier.h"
#include "PasteboardAccessIntent.h"
#include "SameDocumentNavigationType.h"
#include "TransactionID.h"
#include "WebPopupMenuProxy.h"
#include "WindowKind.h"
#include <WebCore/ActivityState.h>
#include <WebCore/AlternativeTextClient.h>
#include <WebCore/ContactInfo.h>
#include <WebCore/ContactsRequestData.h>
#include <WebCore/DataOwnerType.h>
#include <WebCore/DigitalCredentialsRequestData.h>
#include <WebCore/DigitalCredentialsResponseData.h>
#include <WebCore/DragActions.h>
#include <WebCore/EditorClient.h>
#include <WebCore/ExceptionData.h>
#include <WebCore/FocusDirection.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/HTMLMediaElementIdentifier.h>
#include <WebCore/InputMode.h>
#include <WebCore/MediaControlsContextMenuItem.h>
#include <WebCore/ScrollTypes.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/TextAnimationTypes.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <WebCore/ValidationBubble.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/URL.h>
#include <wtf/WeakPtr.h>

#if PLATFORM(COCOA)
#include "CocoaWindow.h"
#include "WKBrowserEngineDefinitions.h"
#include "WKFoundation.h"

#if PLATFORM(IOS_FAMILY)
#include <WebCore/InspectorOverlay.h>
#endif

#if ENABLE(IMAGE_ANALYSIS)
#include <WebCore/TextRecognitionResult.h>
#endif

#if USE(DICTATION_ALTERNATIVES)
#include <WebCore/PlatformTextAlternatives.h>
#endif

OBJC_CLASS AVPlayerViewController;
OBJC_CLASS CALayer;
OBJC_CLASS NSFileWrapper;
OBJC_CLASS NSMenu;
OBJC_CLASS NSObject;
OBJC_CLASS NSSet;
OBJC_CLASS NSTextAlternatives;
OBJC_CLASS UIGestureRecognizer;
OBJC_CLASS UIScrollView;
OBJC_CLASS UIView;
OBJC_CLASS UIViewController;
OBJC_CLASS WKBaseScrollView;
OBJC_CLASS WKBEScrollViewScrollUpdate;
OBJC_CLASS WKFullScreenWindowController;
OBJC_CLASS _WKRemoteObjectRegistry;

#if USE(APPKIT)
OBJC_CLASS NSWindow;
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

struct NodeIdentifierType;
enum class MouseEventPolicy : uint8_t;
enum class RouteSharingPolicy : uint8_t;
enum class ScrollbarStyle : uint8_t;
enum class TextIndicatorLifetime : uint8_t;
enum class TextIndicatorDismissalAnimation : uint8_t;
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
enum class DOMPasteRequiresInteraction : bool;
enum class ScrollIsAnimated : bool;

struct AppHighlight;
struct AriaNotifyData;
struct DataDetectorElementInfo;
struct DictionaryPopupInfo;
struct ElementContext;
struct FixedContainerEdges;
struct LiveRegionAnnouncementData;
struct ResolvedCaptionDisplaySettingsOptions;
struct TextIndicatorData;
struct ShareDataWithParsedURL;

template <typename> class RectEdges;

#if ENABLE(DRAG_SUPPORT)
struct DragItem;
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
struct PromisedAttachmentInfo;
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
struct TranslationContextMenuInfo;
#endif

using NodeIdentifier = ObjectIdentifier<NodeIdentifierType>;

#if ENABLE(WRITING_TOOLS)
namespace WritingTools {
enum class Action : uint8_t;
enum class RequestedTool : uint16_t;
enum class TextSuggestionState : uint8_t;

struct Context;
struct TextSuggestion;
struct Session;

using TextSuggestionID = WTF::UUID;
using SessionID = WTF::UUID;
}
#endif

}

namespace WebKit {

enum class ColorControlSupportsAlpha : bool;
enum class UndoOrRedo : bool;
enum class ForceSoftwareCapturingViewportSnapshot : bool;
enum class TapHandlingResult : uint8_t;

class ContextMenuContextData;
class DrawingAreaProxy;
class NativeWebGestureEvent;
class NativeWebKeyboardEvent;
class NativeWebMouseEvent;
class NativeWebWheelEvent;
class RemoteLayerTreeNode;
class RemoteLayerTreeTransaction;
class BrowsingWarning;
class UserData;
class ViewSnapshot;
class WebBackForwardListItem;
class WebColorPicker;
class WebContextMenuProxy;
class WebDataListSuggestionsDropdown;
class WebDateTimePicker;
class WebEditCommandProxy;
class WebFrameProxy;
class WebOpenPanelResultListenerProxy;
class WebPageProxy;
class WebPopupMenuProxy;
class WebProcessProxy;

enum class ContinueUnsafeLoad : bool { No, Yes };

struct EditorState;
struct FocusedElementInformation;
struct FrameInfoData;
struct InteractionInformationAtPosition;
struct KeyEventInterpretationContext;
struct MainFrameData;
struct PageData;
struct WebAutocorrectionContext;
struct WebHitTestResultData;

#if ENABLE(TOUCH_EVENTS)
class NativeWebTouchEvent;
class WebTouchEvent;
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

#if PLATFORM(GTK) || PLATFORM(WPE)
class WebKitWebResourceLoadManager;
#endif

class PageClient : public CanMakeWeakPtr<PageClient> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(PageClient);
public:
    virtual ~PageClient() { }

    void ref() { refView(); }
    void deref() { derefView(); }

    // Create a new drawing area proxy for the given page.
    virtual Ref<DrawingAreaProxy> createDrawingAreaProxy(WebProcessProxy&) = 0;

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

    // Return whether the active view is visible.
    virtual bool isActiveViewVisible() = 0;

    // Return whether the main view is visible.
    // This is relevant for page client that can have multiple views.
    virtual bool isMainViewVisible() { return isActiveViewVisible(); }

    // Called when the activity state of the page transitions from non-visible to visible.
    virtual void viewIsBecomingVisible() { }

    // Called when the activity state of the page transitions from visible to non-visible.
    virtual void viewIsBecomingInvisible() { }

#if PLATFORM(COCOA)
    virtual bool canTakeForegroundAssertions() = 0;
#endif

    // Return whether the view is visible, or occluded by another window.
    virtual bool isViewVisibleOrOccluded() { return isActiveViewVisible(); }

    // Return whether the view is in a window.
    virtual bool isViewInWindow() = 0;

    // Return whether the view is visually idle.
    virtual bool isVisuallyIdle() { return !isActiveViewVisible(); }

    virtual WindowKind windowKind() { return isViewInWindow() ? WindowKind::Normal : WindowKind::Unparented; }

    virtual void processDidExit() = 0;
    virtual void processWillSwap() { processDidExit(); }
    virtual void didRelaunchProcess() = 0;
    virtual void processDidUpdateThrottleState() { }
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

#if ENABLE(PDF_HUD)
    virtual void createPDFHUD(PDFPluginIdentifier, WebCore::FrameIdentifier, const WebCore::IntRect&) = 0;
    virtual void updatePDFHUDLocation(PDFPluginIdentifier, const WebCore::IntRect&) = 0;
    virtual void removePDFHUD(PDFPluginIdentifier) = 0;
    virtual void removeAllPDFHUDs() = 0;
#endif

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    virtual void createPDFPageNumberIndicator(PDFPluginIdentifier, const WebCore::IntRect&, size_t pageCount) = 0;
    virtual void updatePDFPageNumberIndicatorLocation(PDFPluginIdentifier, const WebCore::IntRect&) = 0;
    virtual void updatePDFPageNumberIndicatorCurrentPage(PDFPluginIdentifier, size_t pageIndex) = 0;
    virtual void removePDFPageNumberIndicator(PDFPluginIdentifier) = 0;
    virtual void removeAnyPDFPageNumberIndicator() = 0;
#endif

    virtual bool handleRunOpenPanel(const WebPageProxy&, const WebFrameProxy&, const FrameInfoData&, API::OpenPanelParameters&, WebOpenPanelResultListenerProxy&) { return false; }
    virtual bool showShareSheet(WebCore::ShareDataWithParsedURL&&, WTF::CompletionHandler<void (bool)>&&) { return false; }
    virtual void showContactPicker(WebCore::ContactsRequestData&&, WTF::CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&& completionHandler) { completionHandler(std::nullopt); }

    virtual void showDigitalCredentialsPicker(const WebCore::DigitalCredentialsRequestData&, WTF::CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&& completionHandler)
    {
        completionHandler(makeUnexpected(WebCore::ExceptionData { WebCore::ExceptionCode::NotSupportedError, "Digital credentials are not supported."_s }));
    }
    virtual void dismissDigitalCredentialsPicker(WTF::CompletionHandler<void(bool)>&& completionHandler) { completionHandler(true); }
    virtual void dismissAnyOpenPicker() { }

    virtual void didChangeContentSize(const WebCore::IntSize&) = 0;

    virtual void obscuredContentInsetsDidChange() { }

    virtual void showBrowsingWarning(const BrowsingWarning&, CompletionHandler<void(Variant<ContinueUnsafeLoad, URL>&&)>&& completionHandler) { completionHandler(ContinueUnsafeLoad::Yes); }
    virtual void clearBrowsingWarning() { }
    virtual void clearBrowsingWarningIfForMainFrameNavigation() { }

    virtual bool canStartNavigationSwipeAtLastInteractionLocation() const { return true; }
    
#if ENABLE(DRAG_SUPPORT)
#if PLATFORM(GTK)
    virtual void startDrag(WebCore::SelectionData&&, OptionSet<WebCore::DragOperation>, RefPtr<WebCore::ShareableBitmap>&& dragImage, WebCore::IntPoint&& dragImageHotspot) = 0;
#else
    virtual void startDrag(const WebCore::DragItem&, WebCore::ShareableBitmap::Handle&&, const std::optional<WebCore::NodeIdentifier>&) { }
#endif
    virtual void didPerformDragOperation(bool) { }
    virtual void didPerformDragControllerAction() { }
    virtual void didChangeDragCaretRect(const WebCore::IntRect& /*previousCaretRect*/, const WebCore::IntRect& /*caretRect*/) { }
#endif // ENABLE(DRAG_SUPPORT)

    virtual void setCursor(const WebCore::Cursor&) = 0;
    virtual void setCursorHiddenUntilMouseMoves(bool) = 0;

    virtual void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo) = 0;
    virtual void clearAllEditCommands() = 0;
    virtual bool canUndoRedo(UndoOrRedo) = 0;
    virtual void executeUndoRedo(UndoOrRedo) = 0;
    virtual void wheelEventWasNotHandledByWebCore(const NativeWebWheelEvent&) = 0;
#if PLATFORM(COCOA)
    virtual void accessibilityWebProcessTokenReceived(std::span<const uint8_t>, pid_t) = 0;
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

#if PLATFORM(MAC)
    virtual CALayer *headerBannerLayer() const = 0;
    virtual CALayer *footerBannerLayer() const = 0;
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK) || PLATFORM(WPE)
    virtual void selectionDidChange() = 0;
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK) || (PLATFORM(WPE) && USE(SKIA))
    virtual RefPtr<ViewSnapshot> takeViewSnapshot(std::optional<WebCore::IntRect>&&) = 0;
#endif

#if PLATFORM(MAC)
    virtual RefPtr<ViewSnapshot> takeViewSnapshot(std::optional<WebCore::IntRect>&&, ForceSoftwareCapturingViewportSnapshot) = 0;
#endif

#if USE(APPKIT)
    virtual void setPromisedDataForImage(const String& pasteboardName, Ref<WebCore::FragmentedSharedBuffer>&& imageBuffer, const String& filename, const String& extension, const String& title, const String& url, const String& visibleURL, RefPtr<WebCore::FragmentedSharedBuffer>&& archiveBuffer, const String& originIdentifier) = 0;
#endif

    virtual WebCore::FloatRect convertToDeviceSpace(const WebCore::FloatRect&) = 0;
    virtual WebCore::FloatRect convertToUserSpace(const WebCore::FloatRect&) = 0;
    virtual WebCore::IntPoint screenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::FloatRect rootViewToWebView(const WebCore::FloatRect& rect) const { return rect; }
    virtual WebCore::FloatPoint webViewToRootView(const WebCore::FloatPoint& point) const { return point; }
    virtual WebCore::IntPoint rootViewToScreen(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToScreen(const WebCore::IntRect&) = 0;
    virtual WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&) = 0;
    virtual WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&) = 0;
#if PLATFORM(IOS_FAMILY)
    virtual void relayAccessibilityNotification(String&&, RetainPtr<NSData>&&) = 0;
    virtual void relayAriaNotifyNotification(const WebCore::AriaNotifyData&) = 0;
    virtual void relayLiveRegionNotification(const WebCore::LiveRegionAnnouncementData&) = 0;
#endif
#if PLATFORM(MAC)
    virtual WebCore::IntRect rootViewToWindow(const WebCore::IntRect&) = 0;
#endif
#if PLATFORM(IOS_FAMILY)
    virtual void didNotHandleTapAsClick(const WebCore::IntPoint&) = 0;
    virtual void didHandleTapAsHover() = 0;
    virtual void didCompleteSyntheticClick() = 0;
#endif

    virtual void runModalJavaScriptDialog(CompletionHandler<void()>&& callback) { callback(); }

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    virtual void didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID) { }
#if ENABLE(GPU_PROCESS)
    virtual void didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID) { }
#endif
#if ENABLE(MODEL_PROCESS)
    virtual void didCreateContextInModelProcessForVisibilityPropagation(LayerHostingContextID) { }
#endif
#if USE(EXTENSIONKIT)
    virtual UIView *createVisibilityPropagationView() { return nullptr; }
    virtual void removeVisibilityPropagationView(UIView *) { }
#endif
#endif // HAVE(VISIBILITY_PROPAGATION_VIEW)

#if ENABLE(GPU_PROCESS)
    virtual void gpuProcessDidFinishLaunching() { }
    virtual void gpuProcessDidExit() { }
#endif

#if ENABLE(MODEL_PROCESS)
    virtual void modelProcessDidFinishLaunching() { }
    virtual void modelProcessDidExit() { }
#endif

    virtual void doneWithKeyEvent(const NativeWebKeyboardEvent&, bool wasEventHandled) = 0;
#if ENABLE(TOUCH_EVENTS)
    virtual void doneWithTouchEvent(const WebTouchEvent&, bool wasEventHandled) = 0;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
    virtual void doneDeferringTouchStart(bool preventNativeGestures) = 0;
    virtual void doneDeferringTouchMove(bool preventNativeGestures) = 0;
    virtual void doneDeferringTouchEnd(bool preventNativeGestures) = 0;
#endif

    virtual RefPtr<WebPopupMenuProxy> createPopupMenuProxy(WebPageProxy&) = 0;
#if ENABLE(CONTEXT_MENUS)
    virtual Ref<WebContextMenuProxy> createContextMenuProxy(WebPageProxy&, FrameInfoData&&, ContextMenuContextData&&, const UserData&) = 0;
    virtual void didShowContextMenu() { }
    virtual void didDismissContextMenu() { }
#endif

    virtual RefPtr<WebColorPicker> createColorPicker(WebPageProxy&, const WebCore::Color& initialColor, const WebCore::IntRect&, ColorControlSupportsAlpha, Vector<WebCore::Color>&&) = 0;

    virtual RefPtr<WebDataListSuggestionsDropdown> createDataListSuggestionsDropdown(WebPageProxy&) = 0;

    virtual RefPtr<WebDateTimePicker> createDateTimePicker(WebPageProxy&) = 0;

#if PLATFORM(COCOA) || PLATFORM(GTK)
    virtual Ref<WebCore::ValidationBubble> createValidationBubble(String&& message, const WebCore::ValidationBubble::Settings&) = 0;
#endif

#if PLATFORM(COCOA)
    virtual CALayer *textIndicatorInstallationLayer() = 0;

    virtual void didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&) = 0;
#endif

#if HAVE(APP_ACCENT_COLORS)
    virtual WebCore::Color accentColor() = 0;
#if PLATFORM(MAC)
    virtual bool appUsesCustomAccentColor() = 0;
#endif
#endif

    virtual bool effectiveAppearanceIsDark() const { return false; }
    virtual bool effectiveUserInterfaceLevelIsElevated() const { return false; }

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&) = 0;
    virtual void exitAcceleratedCompositingMode() = 0;
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&) = 0;
    virtual void didFirstLayerFlush(const LayerTreeContext&) { }

    virtual void takeFocus(WebCore::FocusDirection) { }

    virtual void performSwitchHapticFeedback() { }

#if USE(DICTATION_ALTERNATIVES)
    virtual std::optional<WebCore::DictationContext> addDictationAlternatives(PlatformTextAlternatives *) = 0;
    virtual void replaceDictationAlternatives(PlatformTextAlternatives *, WebCore::DictationContext) = 0;
    virtual void removeDictationAlternatives(WebCore::DictationContext) = 0;
    virtual void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext) = 0;
    virtual Vector<String> dictationAlternatives(WebCore::DictationContext) = 0;
    virtual PlatformTextAlternatives *platformDictationAlternatives(WebCore::DictationContext) = 0;
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

    virtual bool useFormSemanticContext() const = 0;
    
    virtual NSView *viewForPresentingRevealPopover() const = 0;
    RetainPtr<NSView> protectedViewForPresentingRevealPopover() const;

    virtual void showPlatformContextMenu(NSMenu *, WebCore::IntPoint) = 0;

    virtual void startWindowDrag() = 0;
    virtual void setShouldSuppressFirstResponderChanges(bool) = 0;

    virtual RetainPtr<NSView> inspectorAttachmentView() = 0;
    virtual _WKRemoteObjectRegistry *remoteObjectRegistry() = 0;

    virtual void intrinsicContentSizeDidChange(const WebCore::IntSize& intrinsicContentSize) = 0;

    virtual void registerInsertionUndoGrouping() = 0;

    virtual void setEditableElementIsFocused(bool) = 0;
#endif // PLATFORM(MAC)

#if PLATFORM(COCOA)
    virtual void didCommitLayerTree(const RemoteLayerTreeTransaction&, const std::optional<MainFrameData>&, const PageData&, const TransactionID&) = 0;
    virtual void didCommitMainFrameData(const MainFrameData&) = 0;
    virtual void layerTreeCommitComplete() { }

    virtual void scrollingNodeScrollViewDidScroll(WebCore::ScrollingNodeID) = 0;

    virtual CocoaWindow *platformWindow() const = 0;
#endif

    virtual void reconcileEnclosingScrollViewContentOffset(EditorState&) { };

#if PLATFORM(IOS_FAMILY)
    virtual void commitPotentialTapFailed() = 0;
    virtual void didGetTapHighlightGeometries(WebKit::TapIdentifier requestID, const WebCore::Color&, const Vector<WebCore::FloatQuad>& highlightedQuads, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling) = 0;
    virtual bool isPotentialTapInProgress() const = 0;

    virtual void couldNotRestorePageState() = 0;
    virtual void restorePageState(std::optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale) = 0;
    virtual void restorePageCenterAndScale(std::optional<WebCore::FloatPoint> center, double scale) = 0;

    virtual void elementDidFocus(const FocusedElementInformation&, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState> activityStateChanges, API::Object* userData) = 0;
    virtual void updateInputContextAfterBlurringAndRefocusingElement() = 0;
    virtual void didProgrammaticallyClearFocusedElement(WebCore::ElementContext&&) = 0;
    virtual void updateFocusedElementInformation(const FocusedElementInformation&) = 0;
    virtual void elementDidBlur() = 0;
    virtual void focusedElementDidChangeInputMode(WebCore::InputMode) = 0;
    virtual void didUpdateEditorState() = 0;
    virtual bool isFocusingElement() = 0;
    virtual bool interpretKeyEvent(const NativeWebKeyboardEvent&, KeyEventInterpretationContext&&) = 0;
    virtual void positionInformationDidChange(const InteractionInformationAtPosition&) = 0;
    virtual void saveImageToLibrary(Ref<WebCore::SharedBuffer>&&) = 0;
    virtual void showPlaybackTargetPicker(bool hasVideo, const WebCore::IntRect& elementRect, WebCore::RouteSharingPolicy, const String&) = 0;
    virtual void showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition&) = 0;
    virtual void disableDoubleTapGesturesDuringTapIfNecessary(WebKit::TapIdentifier) = 0;
    virtual void handleSmartMagnificationInformationForPotentialTap(WebKit::TapIdentifier, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel, bool nodeIsPluginElement) = 0;
    virtual double minimumZoomScale() const = 0;
    virtual WebCore::FloatRect documentRect() const = 0;
    virtual void scrollingNodeScrollViewWillStartPanGesture(WebCore::ScrollingNodeID) = 0;
    virtual void scrollingNodeScrollWillStartScroll(std::optional<WebCore::ScrollingNodeID>) = 0;
    virtual void scrollingNodeScrollDidEndScroll(std::optional<WebCore::ScrollingNodeID>) = 0;
    virtual Vector<String> mimeTypesWithCustomContentProviders() = 0;

    virtual void hardwareKeyboardAvailabilityChanged() = 0;

    virtual void showInspectorHighlight(const WebCore::InspectorOverlay::Highlight&) = 0;
    virtual void hideInspectorHighlight() = 0;

    virtual void showInspectorIndication() = 0;
    virtual void hideInspectorIndication() = 0;

    virtual void enableInspectorNodeSearch() = 0;
    virtual void disableInspectorNodeSearch() = 0;

    virtual void handleAutocorrectionContext(const WebAutocorrectionContext&) = 0;

#if HAVE(UISCROLLVIEW_ASYNCHRONOUS_SCROLL_EVENT_HANDLING)
    virtual void handleAsynchronousCancelableScrollEvent(WKBaseScrollView *, WKBEScrollViewScrollUpdate *, void (^completion)(BOOL handled)) = 0;
#endif

    virtual bool isSimulatingCompatibilityPointerTouches() const = 0;

    virtual WebCore::FloatBoxExtent computedObscuredInset() const = 0;
    virtual WebCore::Color contentViewBackgroundColor() = 0;
    virtual WebCore::Color insertionPointColor() = 0;
    virtual bool isScreenBeingCaptured() = 0;

    virtual String sceneID() = 0;

    virtual void beginTextRecognitionForFullscreenVideo(WebCore::ShareableBitmap::Handle&&, AVPlayerViewController *) = 0;
    virtual void cancelTextRecognitionForFullscreenVideo(AVPlayerViewController *) = 0;
#endif
    virtual bool isTextRecognitionInFullscreenVideoEnabled() const { return false; }

#if ENABLE(VIDEO)
    virtual void beginTextRecognitionForVideoInElementFullscreen(WebCore::ShareableBitmap::Handle&&, WebCore::FloatRect) { }
    virtual void cancelTextRecognitionForVideoInElementFullscreen() { }
#endif

    // Auxiliary Client Creation
#if ENABLE(FULLSCREEN_API)
    virtual WebFullScreenManagerProxyClient& fullScreenManagerProxyClient() = 0;
    CheckedRef<WebFullScreenManagerProxyClient> checkedFullScreenManagerProxyClient();
    virtual void setFullScreenClientForTesting(std::unique_ptr<WebFullScreenManagerProxyClient>&&) = 0;
#endif

    // Custom representations.
    virtual void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, std::span<const uint8_t>) = 0;

    virtual void navigationGestureDidBegin() = 0;
    virtual void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&) = 0;
    virtual void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&) = 0;
    virtual void navigationGestureDidEnd() = 0;
    virtual void willRecordNavigationSnapshot(WebBackForwardListItem&) = 0;
    virtual void didRemoveNavigationGestureSnapshot() = 0;

    virtual void willBeginViewGesture() { }
    virtual void didEndViewGesture() { }

    virtual void didFirstVisuallyNonEmptyLayoutForMainFrame() = 0;
    virtual void didFinishNavigation(API::Navigation*) = 0;
    virtual void didFailNavigation(API::Navigation*) = 0;
    virtual void didSameDocumentNavigationForMainFrame(SameDocumentNavigationType) = 0;

    virtual void themeColorWillChange() { }
    virtual void themeColorDidChange() { }
#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
    virtual void spatialBackdropSourceWillChange() { }
    virtual void spatialBackdropSourceDidChange() { }
#endif
    virtual void underPageBackgroundColorWillChange() { }
    virtual void underPageBackgroundColorDidChange() { }
    virtual void sampledPageTopColorWillChange() { }
    virtual void sampledPageTopColorDidChange() { }
    virtual void didChangeBackgroundColor() = 0;
    virtual void isPlayingAudioWillChange() = 0;
    virtual void isPlayingAudioDidChange() = 0;

    virtual void pinnedStateWillChange() { }
    virtual void pinnedStateDidChange() { }
    virtual void drawPageBorderForPrinting(WebCore::FloatSize&& size) { }
    virtual bool scrollingUpdatesDisabledForTesting() { return false; }

    virtual bool hasBrowsingWarning() const { return false; }

    virtual void setMouseEventPolicy(WebCore::MouseEventPolicy) { }

    virtual void makeViewBlank(bool) { }

    virtual WebCore::DataOwnerType dataOwnerForPasteboard(PasteboardAccessIntent) const { return WebCore::DataOwnerType::Undefined; }

    virtual bool hasResizableWindows() const { return false; }

#if ENABLE(IMAGE_ANALYSIS)
    virtual void requestTextRecognition(const URL& imageURL, WebCore::ShareableBitmap::Handle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&& completion) { completion({ }); }
    virtual void computeHasVisualSearchResults(const URL&, WebCore::ShareableBitmap&, CompletionHandler<void(bool)>&& completion) { completion(false); }
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    virtual void showMediaControlsContextMenu(WebCore::FloatRect&&, Vector<WebCore::MediaControlsContextMenuItem>&&, const FrameInfoData&, WebCore::HTMLMediaElementIdentifier, CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&& completionHandler) { completionHandler(WebCore::MediaControlsContextMenuItem::invalidID); }
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    
#if PLATFORM(MAC)
    virtual void didPerformImmediateActionHitTest(const WebHitTestResultData&, bool contentPreventsDefault, API::Object*) = 0;
    virtual NSObject *immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult>, uint64_t, RefPtr<API::Object>) = 0;
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
    virtual void videosInElementFullscreenChanged() { }

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    virtual WebCore::WebMediaSessionManager& mediaSessionManager() = 0;
    CheckedRef<WebCore::WebMediaSessionManager> checkedMediaSessionManager();
#endif

    virtual void refView() = 0;
    virtual void derefView() = 0;

    virtual void pageDidScroll(const WebCore::IntPoint&) { }

    virtual void didRestoreScrollPosition() = 0;

    virtual bool windowIsFrontWindowUnderMouse(const NativeWebMouseEvent&) { return false; }

    virtual std::optional<float> computeAutomaticTopObscuredInset() { return std::nullopt; }

    virtual WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() = 0;

    virtual void didChangeLocalInspectorAttachment() { }

#if USE(QUICK_LOOK)
    virtual void requestPasswordForQuickLookDocument(const String& fileName, WTF::Function<void(const String&)>&&) = 0;
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    virtual void willReceiveEditDragSnapshot() = 0;
    virtual void didReceiveEditDragSnapshot(RefPtr<WebCore::TextIndicator>&&) = 0;
#endif

#if ENABLE(MODEL_PROCESS)
    virtual void didReceiveInteractiveModelElement(std::optional<WebCore::NodeIdentifier>) = 0;
#endif

    virtual void requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::DOMPasteRequiresInteraction, const WebCore::IntRect& elementRect, const String& originIdentifier, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&) = 0;

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

#if ENABLE(WRITING_TOOLS) && ENABLE(CONTEXT_MENUS)
    virtual bool canHandleContextMenuWritingTools() const = 0;
    virtual void handleContextMenuWritingTools(WebCore::WritingTools::RequestedTool, WebCore::IntRect) { }
#endif

#if ENABLE(WRITING_TOOLS)
    virtual void proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestionID&, WebCore::IntRect selectionBoundsInRootView) = 0;

    virtual void proofreadingSessionUpdateStateForSuggestionWithID(WebCore::WritingTools::TextSuggestionState, const WebCore::WritingTools::TextSuggestionID&) = 0;

    virtual void writingToolsActiveWillChange() = 0;
    virtual void writingToolsActiveDidChange() = 0;

    virtual void didEndPartialIntelligenceTextAnimation() = 0;
    virtual bool writingToolsTextReplacementsFinished() = 0;

    virtual void addTextAnimationForAnimationID(const WTF::UUID&, const WebCore::TextAnimationData&) = 0;
    virtual void removeTextAnimationForAnimationID(const WTF::UUID&) = 0;
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
    virtual void didCleanupFullscreen() = 0;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    virtual WebKitWebResourceLoadManager* webResourceLoadManager() = 0;
#endif

#if PLATFORM(IOS_FAMILY)
    virtual UIViewController *presentingViewController() const = 0;
#endif

#if HAVE(SPATIAL_TRACKING_LABEL)
    virtual String spatialTrackingLabel() const = 0;
#endif

#if ENABLE(GAMEPAD)
    enum class GamepadsRecentlyAccessed : bool {
        No,
        Yes
    };
    virtual void setGamepadsRecentlyAccessed(GamepadsRecentlyAccessed) { }

#if PLATFORM(VISION)
    virtual void gamepadsConnectedStateChanged() { }
#endif
#endif // ENABLE(GAMEPAD)

    virtual void hasActiveNowPlayingSessionChanged(bool) { }

    virtual void scheduleVisibleContentRectUpdate() { }

#if ENABLE(SCREEN_TIME)
    virtual void didChangeScreenTimeWebpageControllerURL() { };
    virtual void setURLIsPictureInPictureForScreenTime(bool) { };
    virtual void setURLIsPlayingVideoForScreenTime(bool) { };
#endif

#if ENABLE(POINTER_LOCK)
    virtual void beginPointerLockMouseTracking() { }
    virtual void endPointerLockMouseTracking() { }
#endif

#if ENABLE(VIDEO)
    virtual void showCaptionDisplaySettings(WebCore::HTMLMediaElementIdentifier, const WebCore::ResolvedCaptionDisplaySettingsOptions&, CompletionHandler<void(Expected<void, WebCore::ExceptionData>&&)>&&);
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    virtual void canEnterImmersiveElementFromURL(const URL&, CompletionHandler<void(bool)>&& completion) { completion(false); }
#endif
};

} // namespace WebKit
