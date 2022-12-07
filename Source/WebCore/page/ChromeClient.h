/*
 * Copyright (C) 2006-2018 Apple, Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies).
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "AXObjectCache.h"
#include "AutoplayEvent.h"
#include "ContactInfo.h"
#include "Cursor.h"
#include "DatabaseDetails.h"
#include "DeviceOrientationOrMotionPermissionState.h"
#include "DisabledAdaptations.h"
#include "DisplayRefreshMonitor.h"
#include "DocumentStorageAccess.h"
#include "FocusDirection.h"
#include "FrameLoader.h"
#include "GraphicsContext.h"
#include "GraphicsLayer.h"
#include "HTMLMediaElementEnums.h"
#include "HighlightVisibility.h"
#include "HostWindow.h"
#include "Icon.h"
#include "ImageBuffer.h"
#include "InputMode.h"
#include "MediaControlsContextMenuItem.h"
#include "MediaProducer.h"
#include "PointerCharacteristics.h"
#include "PopupMenu.h"
#include "PopupMenuClient.h"
#include "RegistrableDomain.h"
#include "RenderEmbeddedObject.h"
#include "ScrollTypes.h"
#include "ScrollingCoordinator.h"
#include "SearchPopupMenu.h"
#include "WebCoreKeyboardUIMode.h"
#include "WorkerClient.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <pal/graphics/WebGPU/WebGPU.h>
#include <wtf/Assertions.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Forward.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Seconds.h>
#include <wtf/URL.h>

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
#include "MediaPlaybackTargetContext.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "PlatformLayer.h"
#include "WKContentObservation.h"
#define NSResponder WAKResponder
#ifndef __OBJC__
class WAKResponder;
#else
@class WAKResponder;
#endif
#endif

#if ENABLE(MEDIA_USAGE)
#include "MediaSessionIdentifier.h"
#include "MediaUsageInfo.h"
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
class HTMLModelElement;
#endif

#if ENABLE(WEBXR)
#include "PlatformXR.h"
#endif

OBJC_CLASS NSResponder;

namespace WebCore {

class AccessibilityObject;
class ColorChooser;
class ColorChooserClient;
class DataListSuggestionPicker;
class DataListSuggestionsClient;
class DateTimeChooser;
class DateTimeChooserClient;
class Element;
class FileChooser;
class FileIconLoader;
class FloatRect;
class Frame;
class Geolocation;
class GraphicsLayer;
class GraphicsLayerFactory;
class HTMLImageElement;
class HTMLInputElement;
class HTMLMediaElement;
class HTMLVideoElement;
class HitTestResult;
class IntRect;
class NavigationAction;
class Node;
class Page;
class PopupMenuClient;
class SecurityOrigin;
class ViewportConstraints;
class Widget;

#if ENABLE(VIDEO) && USE(GSTREAMER)
class MediaPlayerRequestInstallMissingPluginsCallback;
#endif

#if ENABLE(WEBGL)
class GraphicsContextGL;
struct GraphicsContextGLAttributes;
#endif

struct AppHighlight;
struct ApplePayAMSUIRequest;
struct ContactsRequestData;
struct ContentRuleListResults;
struct DataDetectorElementInfo;
struct DateTimeChooserParameters;
struct FocusOptions;
struct GraphicsDeviceAdapter;
struct MockWebAuthenticationConfiguration;
struct SecurityOriginData;
struct ShareDataWithParsedURL;
struct TextIndicatorData;
struct TextRecognitionOptions;
struct ViewportArguments;
struct WindowFeatures;

enum class CookieConsentDecisionResult : uint8_t;
enum class ModalContainerControlType : uint8_t;
enum class ModalContainerDecision : uint8_t;
enum class RouteSharingPolicy : uint8_t;

class ChromeClient {
public:
    virtual void chromeDestroyed() = 0;

    virtual void setWindowRect(const FloatRect&) = 0;
    virtual FloatRect windowRect() = 0;

    virtual FloatRect pageRect() = 0;

    virtual void focus() = 0;
    virtual void unfocus() = 0;

    virtual bool canTakeFocus(FocusDirection) = 0;
    virtual void takeFocus(FocusDirection) = 0;

    virtual void focusedElementChanged(Element*) = 0;
    virtual void focusedFrameChanged(Frame*) = 0;

    // The Frame pointer provides the ChromeClient with context about which
    // Frame wants to create the new Page. Also, the newly created window
    // should not be shown to the user until the ChromeClient of the newly
    // created Page has its show method called.
    // The ChromeClient should not load the request.
    virtual Page* createWindow(Frame&, const WindowFeatures&, const NavigationAction&) = 0;
    virtual void show() = 0;

    virtual bool canRunModal() = 0;
    virtual void runModal() = 0;

    virtual void setToolbarsVisible(bool) = 0;
    virtual bool toolbarsVisible() = 0;

    virtual void setStatusbarVisible(bool) = 0;
    virtual bool statusbarVisible() = 0;

    virtual void setScrollbarsVisible(bool) = 0;
    virtual bool scrollbarsVisible() = 0;

    virtual void setMenubarVisible(bool) = 0;
    virtual bool menubarVisible() = 0;

    virtual void setResizable(bool) = 0;

    virtual void addMessageToConsole(MessageSource, MessageLevel, const String& message, unsigned lineNumber, unsigned columnNumber, const String& sourceID) = 0;
    virtual void addMessageWithArgumentsToConsole(MessageSource, MessageLevel, const String& message, Span<const String> messageArguments, unsigned lineNumber, unsigned columnNumber, const String& sourceID) { UNUSED_PARAM(message); UNUSED_PARAM(messageArguments); UNUSED_PARAM(lineNumber); UNUSED_PARAM(columnNumber); UNUSED_PARAM(sourceID); }

    virtual bool canRunBeforeUnloadConfirmPanel() = 0;
    virtual bool runBeforeUnloadConfirmPanel(const String& message, Frame&) = 0;

    virtual void closeWindow() = 0;

    virtual void runJavaScriptAlert(Frame&, const String&) = 0;
    virtual bool runJavaScriptConfirm(Frame&, const String&) = 0;
    virtual bool runJavaScriptPrompt(Frame&, const String& message, const String& defaultValue, String& result) = 0;
    virtual void setStatusbarText(const String&) = 0;
    virtual KeyboardUIMode keyboardUIMode() = 0;

    virtual bool hoverSupportedByPrimaryPointingDevice() const = 0;
    virtual bool hoverSupportedByAnyAvailablePointingDevice() const = 0;
    virtual std::optional<PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const = 0;
    virtual OptionSet<PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const = 0;

    virtual void invalidateRootView(const IntRect&) = 0;
    virtual void invalidateContentsAndRootView(const IntRect&) = 0;
    virtual void invalidateContentsForSlowScroll(const IntRect&) = 0;
    virtual void scroll(const IntSize&, const IntRect&, const IntRect&) = 0;

    virtual IntPoint screenToRootView(const IntPoint&) const = 0;
    virtual IntRect rootViewToScreen(const IntRect&) const = 0;
    virtual IntPoint accessibilityScreenToRootView(const IntPoint&) const = 0;
    virtual IntRect rootViewToAccessibilityScreen(const IntRect&) const = 0;

    virtual void didFinishLoadingImageForElement(HTMLImageElement&) = 0;

    virtual PlatformPageClient platformPageClient() const = 0;

    virtual void setCursor(const Cursor&) = 0;
    virtual void setCursorHiddenUntilMouseMoves(bool) = 0;
    virtual bool supportsSettingCursor() { return true; }

    virtual bool shouldUseMouseEventForSelection(const PlatformMouseEvent&) { return true; }

    virtual FloatSize screenSize() const { return const_cast<ChromeClient&>(*this).windowRect().size(); }
    virtual FloatSize availableScreenSize() const { return const_cast<ChromeClient&>(*this).windowRect().size(); }
    virtual FloatSize overrideScreenSize() const { return const_cast<ChromeClient&>(*this).windowRect().size(); }

    virtual void dispatchDisabledAdaptationsDidChange(const OptionSet<DisabledAdaptations>&) const { }
    virtual void dispatchViewportPropertiesDidChange(const ViewportArguments&) const { }

    virtual void contentsSizeChanged(Frame&, const IntSize&) const = 0;
    virtual void intrinsicContentsSizeChanged(const IntSize&) const = 0;

    virtual void scrollContainingScrollViewsToRevealRect(const IntRect&) const { }; // Currently only Mac has a non empty implementation.
    virtual void scrollMainFrameToRevealRect(const IntRect&) const { };

    virtual bool shouldUnavailablePluginMessageBeButton(RenderEmbeddedObject::PluginUnavailabilityReason) const { return false; }
    virtual void unavailablePluginButtonClicked(Element&, RenderEmbeddedObject::PluginUnavailabilityReason) const { }
    virtual void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags, const String& toolTip, TextDirection) = 0;

    virtual void print(Frame&, const StringWithDirection&) = 0;

    virtual Color underlayColor() const { return Color(); }

    virtual void themeColorChanged() const { }
    virtual void pageExtendedBackgroundColorDidChange() const { }
    virtual void sampledPageTopColorChanged() const { }
    
#if ENABLE(APP_HIGHLIGHTS)
    virtual WebCore::HighlightVisibility appHighlightsVisiblility() const { return HighlightVisibility::Hidden; };
#endif

    virtual void exceededDatabaseQuota(Frame&, const String& databaseName, DatabaseDetails) = 0;

    // Callback invoked when the application cache fails to save a cache object
    // because storing it would grow the database file past its defined maximum
    // size or past the amount of free space on the device. 
    // The chrome client would need to take some action such as evicting some
    // old caches.
    virtual void reachedMaxAppCacheSize(int64_t spaceNeeded) = 0;

    // Callback invoked when the application cache origin quota is reached. This
    // means that the resources attempting to be cached via the manifest are
    // more than allowed on this origin. This callback allows the chrome client
    // to take action, such as prompting the user to ask to increase the quota
    // for this origin. The totalSpaceNeeded parameter is the total amount of
    // storage, in bytes, needed to store the new cache along with all of the
    // other existing caches for the origin that would not be replaced by
    // the new cache.
    virtual void reachedApplicationCacheOriginQuota(SecurityOrigin&, int64_t totalSpaceNeeded) = 0;

    virtual std::unique_ptr<WorkerClient> createWorkerClient(SerialFunctionDispatcher&) { return nullptr; }

#if ENABLE(IOS_TOUCH_EVENTS)
    virtual void didPreventDefaultForEvent() = 0;
#endif

    virtual Seconds eventThrottlingDelay() { return 0_s; };

#if PLATFORM(IOS_FAMILY)
    virtual void didReceiveMobileDocType(bool) = 0;
    virtual void setNeedsScrollNotifications(Frame&, bool) = 0;
    virtual void didFinishContentChangeObserving(Frame&, WKContentChange) = 0;
    virtual void notifyRevealedSelectionByScrollingFrame(Frame&) = 0;

    enum LayoutType { NormalLayout, Scroll };
    virtual void didLayout(LayoutType = NormalLayout) = 0;
    virtual void didStartOverflowScroll() = 0;
    virtual void didEndOverflowScroll() = 0;

    // FIXME: Remove this functionality. This functionality was added to workaround an issue (<rdar://problem/5973875>)
    // where the unconfirmed text in a text area would be removed when a person clicks in the text area before a
    // suggestion is shown. We should fix this issue in <rdar://problem/5975559>.
    virtual void suppressFormNotifications() = 0;
    virtual void restoreFormNotifications() = 0;

    virtual void didFlushCompositingLayers() { }

    virtual bool fetchCustomFixedPositionLayoutRect(IntRect&) { return false; }

    virtual void updateViewportConstrainedLayers(HashMap<PlatformLayer*, std::unique_ptr<ViewportConstraints>>&, const HashMap<PlatformLayer*, PlatformLayer*>&) { }

    virtual void addOrUpdateScrollingLayer(Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer, const IntSize& scrollSize, bool allowHorizontalScrollbar, bool allowVerticalScrollbar) = 0;
    virtual void removeScrollingLayer(Node*, PlatformLayer* scrollingLayer, PlatformLayer* contentsLayer) = 0;

    virtual void webAppOrientationsUpdated() = 0;
    virtual void showPlaybackTargetPicker(bool hasVideo, RouteSharingPolicy, const String&) = 0;

    virtual bool showDataDetectorsUIForElement(const Element&, const Event&) = 0;
#endif

#if ENABLE(ORIENTATION_EVENTS)
    virtual int deviceOrientation() const = 0;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    virtual std::unique_ptr<ColorChooser> createColorChooser(ColorChooserClient&, const Color&) = 0;
#endif

#if ENABLE(DATALIST_ELEMENT)
    virtual std::unique_ptr<DataListSuggestionPicker> createDataListSuggestionPicker(DataListSuggestionsClient&) = 0;
    virtual bool canShowDataListSuggestionLabels() const = 0;
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    virtual std::unique_ptr<DateTimeChooser> createDateTimeChooser(DateTimeChooserClient&) = 0;
#endif

#if ENABLE(APP_HIGHLIGHTS)
    virtual void storeAppHighlight(WebCore::AppHighlight&&) const = 0;
#endif

    virtual void setTextIndicator(const TextIndicatorData&) const = 0;

    virtual void runOpenPanel(Frame&, FileChooser&) = 0;
    virtual void showShareSheet(ShareDataWithParsedURL&, CompletionHandler<void(bool)>&& callback) { callback(false); }
    virtual void showContactPicker(const ContactsRequestData&, CompletionHandler<void(std::optional<Vector<ContactInfo>>&&)>&& callback) { callback(std::nullopt); }
    
    // Asynchronous request to load an icon for specified filenames.
    virtual void loadIconForFiles(const Vector<String>&, FileIconLoader&) = 0;
        
    virtual void elementDidFocus(Element&, const FocusOptions&) { }
    virtual void elementDidBlur(Element&) { }
    virtual void elementDidRefocus(Element&, const FocusOptions&) { }

    virtual void focusedElementDidChangeInputMode(Element&, InputMode) { }

    virtual bool shouldPaintEntireContents() const { return false; }
    virtual bool hasStablePageScaleFactor() const { return true; }

    // Allows ports to customize the type of graphics layers created by this page.
    virtual GraphicsLayerFactory* graphicsLayerFactory() const { return nullptr; }
    
    virtual DisplayRefreshMonitorFactory* displayRefreshMonitorFactory() const { return nullptr; }

    virtual RefPtr<ImageBuffer> createImageBuffer(const FloatSize&, RenderingMode, RenderingPurpose, float, const DestinationColorSpace&, PixelFormat, bool avoidBackendSizeCheck = false) const { UNUSED_PARAM(avoidBackendSizeCheck); return nullptr; }

#if ENABLE(WEBGL)
    WEBCORE_EXPORT virtual RefPtr<GraphicsContextGL> createGraphicsContextGL(const GraphicsContextGLAttributes&) const;
#endif

    virtual RefPtr<PAL::WebGPU::GPU> createGPUForWebGPU() const { return nullptr; }

    // Pass nullptr as the GraphicsLayer to detatch the root layer.
    virtual void attachRootGraphicsLayer(Frame&, GraphicsLayer*) = 0;
    virtual void attachViewOverlayGraphicsLayer(GraphicsLayer*) = 0;
    // Sets a flag to specify that the next time content is drawn to the window,
    // the changes appear on the screen in synchrony with updates to GraphicsLayers.
    virtual void setNeedsOneShotDrawingSynchronization() = 0;

    virtual bool shouldTriggerRenderingUpdate(unsigned) const { return true; }
    // Makes a rendering update happen soon, typically in the current runloop.
    virtual void triggerRenderingUpdate() = 0;
    // Schedule a rendering update that coordinates with display refresh. Returns true if scheduled. (This is only used by SVGImageChromeClient.)
    virtual bool scheduleRenderingUpdate() { return false; }

    virtual unsigned remoteImagesCountForTesting() const { return 0; }

    // Returns whether or not the client can render the composited layer,
    // regardless of the settings.
    virtual bool allowsAcceleratedCompositing() const { return true; }

    enum CompositingTrigger {
        ThreeDTransformTrigger = 1 << 0,
        VideoTrigger = 1 << 1,
        PluginTrigger = 1 << 2,
        CanvasTrigger = 1 << 3,
        AnimationTrigger = 1 << 4,
        FilterTrigger = 1 << 5,
        ScrollableNonMainFrameTrigger = 1 << 6,
        AnimatedOpacityTrigger = 1 << 7,
        AllTriggers = 0xFFFFFFFF
    };
    typedef unsigned CompositingTriggerFlags;

    // Returns a bitfield indicating conditions that can trigger the compositor.
    virtual CompositingTriggerFlags allowedCompositingTriggers() const { return static_cast<CompositingTriggerFlags>(AllTriggers); }
    
    // Returns true if layer tree updates are disabled.
    virtual bool layerTreeStateIsFrozen() const { return false; }

    virtual RefPtr<ScrollingCoordinator> createScrollingCoordinator(Page&) const { return nullptr; }

#if PLATFORM(WIN) && USE(AVFOUNDATION)
    virtual GraphicsDeviceAdapter* graphicsDeviceAdapter() const { return nullptr; }
#endif

    virtual bool canEnterVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) const { return false; }
    virtual bool supportsVideoFullscreen(HTMLMediaElementEnums::VideoFullscreenMode) { return false; }
    virtual bool supportsVideoFullscreenStandby() { return false; }

#if ENABLE(VIDEO_PRESENTATION_MODE)
    virtual void prepareForVideoFullscreen() { }
#endif

#if ENABLE(VIDEO)
    virtual void enterVideoFullscreenForVideoElement(HTMLVideoElement&, HTMLMediaElementEnums::VideoFullscreenMode, bool standby) { UNUSED_PARAM(standby); }
    virtual void setUpPlaybackControlsManager(HTMLMediaElement&) { }
    virtual void clearPlaybackControlsManager() { }
    virtual void playbackControlsMediaEngineChanged() { }
#endif

#if ENABLE(MEDIA_USAGE)
    virtual void addMediaUsageManagerSession(MediaSessionIdentifier, const String&, const URL&) { }
    virtual void updateMediaUsageManagerSessionState(MediaSessionIdentifier, const MediaUsageInfo&) { }
    virtual void removeMediaUsageManagerSession(MediaSessionIdentifier) { }
#endif

    virtual void exitVideoFullscreenForVideoElement(HTMLVideoElement&, CompletionHandler<void(bool)>&& completionHandler = [](bool) { }) { completionHandler(true); }
    virtual void exitVideoFullscreenToModeWithoutAnimation(HTMLVideoElement&, HTMLMediaElementEnums::VideoFullscreenMode) { }
    virtual bool requiresFullscreenForVideoPlayback() { return false; } 

#if ENABLE(FULLSCREEN_API)
    virtual bool supportsFullScreenForElement(const Element&, bool) { return false; }
    virtual void enterFullScreenForElement(Element&) { }
    virtual void exitFullScreenForElement(Element*) { }
    virtual void setRootFullScreenLayer(GraphicsLayer*) { }
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    virtual void setMockVideoPresentationModeEnabled(bool) { }
#endif

#if USE(COORDINATED_GRAPHICS)
    virtual IntRect visibleRectForTiledBackingStore() const { return IntRect(); }
#endif

#if PLATFORM(COCOA)
    virtual NSResponder *firstResponder() { return nullptr; }
    virtual void makeFirstResponder(NSResponder *) { }
    // Focuses on the containing view associated with this page.
    virtual void makeFirstResponder() { }
    virtual void assistiveTechnologyMakeFirstResponder() { }
#endif

    virtual bool testProcessIncomingSyncMessagesWhenWaitingForSyncReply() { return true; }

#if PLATFORM(IOS_FAMILY)
    // FIXME: Come up with a more descriptive name for this function and make it platform independent (if possible).
    virtual bool isStopping() = 0;
#endif

    virtual void enableSuddenTermination() { }
    virtual void disableSuddenTermination() { }

    virtual void contentRuleListNotification(const URL&, const ContentRuleListResults&) { };

#if PLATFORM(WIN)
    virtual void setLastSetCursorToCurrentCursor() = 0;
    virtual void AXStartFrameLoad() = 0;
    virtual void AXFinishFrameLoad() = 0;
#endif

    virtual bool selectItemWritingDirectionIsNatural() = 0;
    virtual bool selectItemAlignmentFollowsMenuWritingDirection() = 0;
    // Checks if there is an opened popup, called by RenderMenuList::showPopup().
    virtual RefPtr<PopupMenu> createPopupMenu(PopupMenuClient&) const = 0;
    virtual RefPtr<SearchPopupMenu> createSearchPopupMenu(PopupMenuClient&) const = 0;

    virtual void postAccessibilityNotification(AccessibilityObject&, AXObjectCache::AXNotification) { }

    virtual void notifyScrollerThumbIsVisibleInRect(const IntRect&) { }
    virtual void recommendedScrollbarStyleDidChange(ScrollbarStyle) { }

    virtual std::optional<ScrollbarOverlayStyle> preferredScrollbarOverlayStyle() { return std::nullopt; }

    virtual void wheelEventHandlersChanged(bool hasHandlers) = 0;
        
    virtual bool isSVGImageChromeClient() const { return false; }

#if ENABLE(POINTER_LOCK)
    virtual bool requestPointerLock() { return false; }
    virtual void requestPointerUnlock() { }
#endif

    virtual FloatSize minimumWindowSize() const { return FloatSize(100, 100); };

    virtual bool isEmptyChromeClient() const { return false; }

    virtual String plugInStartLabelTitle(const String& mimeType) const { UNUSED_PARAM(mimeType); return String(); }
    virtual String plugInStartLabelSubtitle(const String& mimeType) const { UNUSED_PARAM(mimeType); return String(); }
    virtual String plugInExtraStyleSheet() const { return String(); }
    virtual String plugInExtraScript() const { return String(); }

    virtual void didAssociateFormControls(const Vector<RefPtr<Element>>&, Frame&) { };
    virtual bool shouldNotifyOnFormChanges() { return false; };

    virtual void didAddHeaderLayer(GraphicsLayer&) { }
    virtual void didAddFooterLayer(GraphicsLayer&) { }

    virtual bool shouldUseTiledBackingForFrameView(const FrameView&) const { return false; }

    virtual void isPlayingMediaDidChange(MediaProducerMediaStateFlags) { }
    virtual void handleAutoplayEvent(AutoplayEvent, OptionSet<AutoplayEventFlags>) { }

#if ENABLE(WEB_CRYPTO)
    virtual bool wrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const { return false; }
    virtual bool unwrapCryptoKey(const Vector<uint8_t>&, Vector<uint8_t>&) const { return false; }
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
    virtual void handleTelephoneNumberClick(const String&, const IntPoint&, const IntRect&) { }
#endif

#if ENABLE(DATA_DETECTION)
    virtual void handleClickForDataDetectionResult(const DataDetectorElementInfo&, const IntPoint&) { }
#endif

#if ENABLE(SERVICE_CONTROLS)
    virtual void handleSelectionServiceClick(FrameSelection&, const Vector<String>&, const IntPoint&) { }
    virtual bool hasRelevantSelectionServices(bool /*isTextOnly*/) const { return false; }
    virtual void handleImageServiceClick(const IntPoint&, Image&, HTMLImageElement&) { }
    virtual void handlePDFServiceClick(const IntPoint&, HTMLAttachmentElement&) { }
#endif

    virtual URL sanitizeForCopyOrShare(const URL& url) const { return url; }

    virtual bool shouldDispatchFakeMouseMoveEvents() const { return true; }

    virtual void handleAutoFillButtonClick(HTMLInputElement&) { }

    virtual void inputElementDidResignStrongPasswordAppearance(HTMLInputElement&) { };

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    virtual void addPlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier) { }
    virtual void removePlaybackTargetPickerClient(PlaybackTargetClientContextIdentifier) { }
    virtual void showPlaybackTargetPicker(PlaybackTargetClientContextIdentifier, const IntPoint&, bool /*isVideo*/) { }
    virtual void playbackTargetPickerClientStateDidChange(PlaybackTargetClientContextIdentifier, MediaProducerMediaStateFlags) { }
    virtual void setMockMediaPlaybackTargetPickerEnabled(bool)  { }
    virtual void setMockMediaPlaybackTargetPickerState(const String&, MediaPlaybackTargetContext::MockState) { }
    virtual void mockMediaPlaybackTargetPickerDismissPopup() { }
#endif

    virtual void imageOrMediaDocumentSizeChanged(const IntSize&) { }

#if ENABLE(VIDEO) && USE(GSTREAMER)
    virtual void requestInstallMissingMediaPlugins(const String& /*details*/, const String& /*description*/, MediaPlayerRequestInstallMissingPluginsCallback&) { }
#endif

    virtual void didInvalidateDocumentMarkerRects() { }

    virtual void reportProcessCPUTime(Seconds, ActivityStateForCPUSampling) { }
    virtual RefPtr<Icon> createIconForFiles(const Vector<String>& /* filenames */) = 0;

#if ENABLE(TRACKING_PREVENTION)
    virtual void hasStorageAccess(RegistrableDomain&& /*subFrameDomain*/, RegistrableDomain&& /*topFrameDomain*/, Frame&, CompletionHandler<void(bool)>&& completionHandler) { completionHandler(false); }
    virtual void requestStorageAccess(RegistrableDomain&& subFrameDomain, RegistrableDomain&& topFrameDomain, Frame&, StorageAccessScope scope, CompletionHandler<void(RequestStorageAccessResult)>&& completionHandler) { completionHandler({ StorageAccessWasGranted::No, StorageAccessPromptWasShown::No, scope, WTFMove(topFrameDomain), WTFMove(subFrameDomain) }); }
    virtual bool hasPageLevelStorageAccess(const RegistrableDomain& /*topLevelDomain*/, const RegistrableDomain& /*resourceDomain*/) const { return false; }
#endif

#if ENABLE(DEVICE_ORIENTATION)
    virtual void shouldAllowDeviceOrientationAndMotionAccess(Frame&, bool /* mayPrompt */, CompletionHandler<void(DeviceOrientationOrMotionPermissionState)>&& callback) { callback(DeviceOrientationOrMotionPermissionState::Denied); }
#endif

    virtual void didInsertMenuElement(HTMLMenuElement&) { }
    virtual void didRemoveMenuElement(HTMLMenuElement&) { }
    virtual void didInsertMenuItemElement(HTMLMenuItemElement&) { }
    virtual void didRemoveMenuItemElement(HTMLMenuItemElement&) { }

    virtual void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel) { }

    virtual bool userIsInteracting() const { return false; }
    virtual void setUserIsInteracting(bool) { }

#if ENABLE(WEB_AUTHN)
    virtual void setMockWebAuthenticationConfiguration(const MockWebAuthenticationConfiguration&) { }
#endif

    virtual void animationDidFinishForElement(const Element&) { }

#if PLATFORM(MAC)
    virtual void changeUniversalAccessZoomFocus(const IntRect&, const IntRect&) { }
#endif

#if ENABLE(IMAGE_ANALYSIS)
    virtual void requestTextRecognition(Element&, TextRecognitionOptions&&, CompletionHandler<void(RefPtr<Element>&&)>&& completion = { })
    {
        if (completion)
            completion({ });
    }
#endif
    virtual bool needsImageOverlayControllerForSelectionPainting() const { return false; }

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    virtual void showMediaControlsContextMenu(FloatRect&&, Vector<MediaControlsContextMenuItem>&&, CompletionHandler<void(MediaControlsContextMenuItem::ID)>&& completionHandler) { completionHandler(MediaControlsContextMenuItem::invalidID); }
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if ENABLE(WEBXR)
    virtual void enumerateImmersiveXRDevices(CompletionHandler<void(const PlatformXR::Instance::DeviceList&)>&& completionHandler) { PlatformXR::Instance::singleton().enumerateImmersiveXRDevices(WTFMove(completionHandler)); }
    virtual void requestPermissionOnXRSessionFeatures(const SecurityOriginData&, PlatformXR::SessionMode, const PlatformXR::Device::FeatureList& granted, const PlatformXR::Device::FeatureList& /* consentRequired */, const PlatformXR::Device::FeatureList& /* consentOptional */, CompletionHandler<void(std::optional<PlatformXR::Device::FeatureList>&&)>&& completionHandler) { completionHandler(granted); }
#endif

#if ENABLE(TEXT_AUTOSIZING)
    virtual void textAutosizingUsesIdempotentModeChanged() { }
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    virtual void startApplePayAMSUISession(const URL&, const ApplePayAMSUIRequest&, CompletionHandler<void(std::optional<bool>&&)>&& completionHandler) { completionHandler(std::nullopt); }
    virtual void abortApplePayAMSUISession() { }
#endif

    virtual void requestCookieConsent(CompletionHandler<void(CookieConsentDecisionResult)>&&) = 0;

    virtual const AtomString& searchStringForModalContainerObserver() const { return nullAtom(); }
    virtual void classifyModalContainerControls(Vector<String>&& texts, CompletionHandler<void(Vector<ModalContainerControlType>&&)>&&) = 0;

    virtual void decidePolicyForModalContainer(OptionSet<ModalContainerControlType>, CompletionHandler<void(ModalContainerDecision)>&&) = 0;

protected:
    WEBCORE_EXPORT ChromeClient();
    WEBCORE_EXPORT virtual ~ChromeClient();
};

} // namespace WebCore
