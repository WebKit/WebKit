/*
 * Copyright (C) 2010-2021 Apple Inc. All rights reserved.
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

#include "APIInjectedBundleEditorClient.h"
#include "APIInjectedBundleFormClient.h"
#include "APIInjectedBundlePageContextMenuClient.h"
#include "APIInjectedBundlePageLoaderClient.h"
#include "APIInjectedBundlePageResourceLoadClient.h"
#include "APIInjectedBundlePageUIClient.h"
#include "APIObject.h"
#include "CallbackID.h"
#include "ContentAsStringIncludesChildFrames.h"
#include "DataReference.h"
#include "DownloadID.h"
#include "DrawingAreaInfo.h"
#include "EditingRange.h"
#include "FocusedElementInformation.h"
#include "GeolocationIdentifier.h"
#include "InjectedBundlePageContextMenuClient.h"
#include "InjectedBundlePageFullScreenClient.h"
#include "InjectedBundlePagePolicyClient.h"
#include "LayerTreeContext.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "OptionalCallbackID.h"
#include "PDFPluginIdentifier.h"
#include "Plugin.h"
#include "PolicyDecision.h"
#include "SandboxExtension.h"
#include "ShareableBitmap.h"
#include "SharedMemory.h"
#include "StorageNamespaceIdentifier.h"
#include "TransactionID.h"
#include "UserData.h"
#include "WebBackForwardListProxy.h"
#include "WebPageMessagesReplies.h"
#include "WebURLSchemeHandler.h"
#include "WebUndoStepID.h"
#include "WebUserContentController.h"
#include "WebsitePoliciesData.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <WebCore/ActivityState.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/DisabledAdaptations.h>
#include <WebCore/DragActions.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/HTMLMenuElement.h>
#include <WebCore/HTMLMenuItemElement.h>
#include <WebCore/IntRect.h>
#include <WebCore/IntSizeHash.h>
#include <WebCore/MediaControlsContextMenuItem.h>
#include <WebCore/Page.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PluginData.h>
#include <WebCore/PointerCharacteristics.h>
#include <WebCore/PointerID.h>
#include <WebCore/SecurityPolicyViolationEvent.h>
#include <WebCore/ShareData.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextManipulationController.h>
#include <WebCore/UserActivity.h>
#include <WebCore/UserContentTypes.h>
#include <WebCore/UserInterfaceLayoutDirection.h>
#include <WebCore/UserScriptTypes.h>
#include <WebCore/VisibilityState.h>
#include <WebCore/WebCoreKeyboardUIMode.h>
#include <memory>
#include <pal/HysteresisActivity.h>
#include <wtf/HashMap.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

#if ENABLE(ACCESSIBILITY) && USE(ATK)
typedef struct _AtkObject AtkObject;
#include <wtf/glib/GRefPtr.h>
#endif

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#include "WebPrintOperationGtk.h"
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "InputMethodState.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "GestureTypes.h"
#include <WebCore/InspectorOverlay.h>
#include <WebCore/IntPointHash.h>
#include <WebCore/WKContentObservation.h>
#endif

#if ENABLE(META_VIEWPORT)
#include <WebCore/ViewportConfiguration.h>
#endif

#if ENABLE(APPLICATION_MANIFEST)
#include <WebCore/ApplicationManifest.h>
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
#include <WebKitAdditions/PlatformTouchEventIOS.h>
#elif ENABLE(TOUCH_EVENTS)
#include <WebCore/PlatformTouchEvent.h>
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
#include <WebKitAdditions/PlatformGestureEventMac.h>
#endif

#if ENABLE(MEDIA_USAGE)
#include <WebCore/MediaSessionIdentifier.h>
#endif

#if PLATFORM(COCOA)
#include "DynamicViewportSizeUpdate.h"
#include <WebCore/VisibleSelection.h>
#include <wtf/RetainPtr.h>
OBJC_CLASS CALayer;
OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS WKAccessibilityWebPageObject;
#endif

#define ENABLE_VIEWPORT_RESIZING PLATFORM(IOS_FAMILY)

namespace WTF {
enum class Critical : uint8_t;
}

namespace API {
class Array;
}

namespace IPC {
class Connection;
class Decoder;
class FormDataReference;
}

namespace WebCore {

class CaptureDevice;
class DocumentLoader;
class DragData;
class FontAttributeChanges;
class FontChanges;
class Frame;
class FrameSelection;
class FrameView;
class GraphicsContext;
class HTMLImageElement;
class HTMLMenuElement;
class HTMLMenuItemElement;
class HTMLPlugInElement;
class IntPoint;
class KeyboardEvent;
class MediaPlaybackTargetContext;
class MediaPlayerRequestInstallMissingPluginsCallback;
class Page;
class PrintContext;
class Range;
class RenderImage;
class ResourceRequest;
class ResourceResponse;
class SelectionData;
class SelectionRect;
class SharedBuffer;
class SubstituteData;
class TextCheckingRequest;
class VisiblePosition;

enum SyntheticClickType : int8_t;
enum class CreateNewGroupForHighlight : bool;
enum class DOMPasteAccessResponse : uint8_t;
enum class DragApplicationFlags : uint8_t;
enum class DragHandlingMethod : uint8_t;
enum class EventMakesGamepadsVisible : bool;
enum class SelectionDirection : uint8_t;
enum class ShouldTreatAsContinuingLoad : bool;
enum class TextIndicatorPresentationTransition : uint8_t;
enum class TextGranularity : uint8_t;
enum class WheelEventProcessingSteps : uint8_t;
enum class WritingDirection : uint8_t;

using PlatformDisplayID = uint32_t;

struct AttributedString;
struct BackForwardItemIdentifier;
struct CompositionHighlight;
struct CompositionUnderline;
struct ContactInfo;
struct ContactsRequestData;
struct DictationAlternative;
struct ElementContext;
struct GlobalFrameIdentifier;
struct GlobalWindowIdentifier;
struct KeypressCommand;
struct MediaUsageInfo;
struct PromisedAttachmentInfo;
struct RequestStorageAccessResult;
struct RunJavaScriptParameters;
struct TextCheckingResult;
struct ViewportArguments;

#if ENABLE(ATTACHMENT_ELEMENT)
class HTMLAttachmentElement;
#endif
}

namespace WebKit {

class DrawingArea;
class FindController;
class GamepadData;
class GeolocationPermissionRequestManager;
class LayerHostingContext;
class MediaDeviceSandboxExtensions;
class MediaKeySystemPermissionRequestManager;
class NotificationPermissionRequestManager;
class PDFPlugin;
class PageBanner;
class PluginView;
class RemoteRenderingBackendProxy;
class RemoteWebInspectorUI;
class TextCheckingControllerProxy;
class UserMediaPermissionRequestManager;
class ViewGestureGeometryCollector;
class WebColorChooser;
class WebContextMenu;
class WebContextMenuItemData;
class WebDataListSuggestionPicker;
class WebDateTimeChooser;
class WebDocumentLoader;
class WebEvent;
class PlaybackSessionManager;
class VideoFullscreenManager;
class WebFrame;
class WebFullScreenManager;
class WebGestureEvent;
class WebImage;
class WebInspector;
class WebInspectorClient;
class WebInspectorUI;
class WebKeyboardEvent;
class WebMouseEvent;
class WebNotificationClient;
class WebOpenPanelResultListener;
class WebPageGroupProxy;
class WebPageInspectorTargetController;
class WebPageOverlay;
class WebPaymentCoordinator;
class WebPopupMenu;
class WebRemoteObjectRegistry;
class WebTouchEvent;
class WebURLSchemeHandlerProxy;
class WebUndoStep;
class WebUserContentController;
class WebWheelEvent;
class RemoteLayerTreeTransaction;

enum class FindOptions : uint16_t;
enum class DragControllerAction : uint8_t;
enum class SyntheticEditingCommandType : uint8_t;

struct BackForwardListItemState;
struct DataDetectionResult;
struct DocumentEditingContext;
struct DocumentEditingContextRequest;
struct EditorState;
struct FrameTreeNodeData;
struct FontInfo;
struct InsertTextOptions;
struct InteractionInformationAtPosition;
struct InteractionInformationRequest;
struct LoadParameters;
struct PrintInfo;
struct TextInputContext;
struct UserMessage;
struct WebAutocorrectionData;
struct WebAutocorrectionContext;
struct WebPageCreationParameters;
struct WebPreferencesStore;
struct WebsitePoliciesData;

#if ENABLE(UI_SIDE_COMPOSITING)
class VisibleContentRectUpdateInfo;
#endif

using SnapshotOptions = uint32_t;
using WKEventModifiers = uint32_t;

class WebPage : public API::ObjectImpl<API::Object::Type::BundlePage>, public IPC::MessageReceiver, public IPC::MessageSender, public CanMakeWeakPtr<WebPage> {
public:
    static Ref<WebPage> create(WebCore::PageIdentifier, WebPageCreationParameters&&);

    virtual ~WebPage();

    void reinitializeWebPage(WebPageCreationParameters&&);

    void close();

    static WebPage& fromCorePage(WebCore::Page&);

    WebCore::Page* corePage() const { return m_page.get(); }
    WebCore::PageIdentifier identifier() const { return m_identifier; }
    StorageNamespaceIdentifier sessionStorageNamespaceIdentifier() const { return makeObjectIdentifier<StorageNamespaceIdentifierType>(m_webPageProxyIdentifier.toUInt64()); }
    PAL::SessionID sessionID() const;
    bool usesEphemeralSession() const;

    void setSize(const WebCore::IntSize&);
    const WebCore::IntSize& size() const { return m_viewSize; }
    WebCore::IntRect bounds() const { return WebCore::IntRect(WebCore::IntPoint(), size()); }

    DrawingArea* drawingArea() const { return m_drawingArea.get(); }

#if ENABLE(ASYNC_SCROLLING)
    WebCore::ScrollingCoordinator* scrollingCoordinator() const;
#endif

    WebPageGroupProxy* pageGroup() const { return m_pageGroup.get(); }

    void scrollMainFrameIfNotAtMaxScrollPosition(const WebCore::IntSize& scrollOffset);

    bool scrollBy(uint32_t scrollDirection, uint32_t scrollGranularity);

    void centerSelectionInVisibleArea();

#if ENABLE(UI_PROCESS_PDF_HUD)
    void createPDFHUD(PDFPlugin&, const WebCore::IntRect&);
    void updatePDFHUDLocation(PDFPlugin&, const WebCore::IntRect&);
    void removePDFHUD(PDFPlugin&);
    void zoomPDFIn(PDFPluginIdentifier);
    void zoomPDFOut(PDFPluginIdentifier);
    void savePDF(PDFPluginIdentifier, CompletionHandler<void(const String&, const URL&, const IPC::DataReference&)>&&);
    void openPDFWithPreview(PDFPluginIdentifier, CompletionHandler<void(const String&, FrameInfoData&&, const IPC::DataReference&, const String&)>&&);
#endif

#if PLATFORM(COCOA)
    void willCommitLayerTree(RemoteLayerTreeTransaction&);
    void didFlushLayerTreeAtTime(MonotonicTime);
#endif

    void layoutIfNeeded();
    void updateRendering();
    void finalizeRenderingUpdate(OptionSet<WebCore::FinalizeRenderingUpdateFlags>);

    void releaseMemory(WTF::Critical);

    enum class LazyCreationPolicy { UseExistingOnly, CreateIfNeeded };

    WebInspector* inspector(LazyCreationPolicy = LazyCreationPolicy::CreateIfNeeded);
    WebInspectorUI* inspectorUI();
    RemoteWebInspectorUI* remoteInspectorUI();
    bool isInspectorPage() { return !!m_inspectorUI || !!m_remoteInspectorUI; }

    void inspectorFrontendCountChanged(unsigned);

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    PlaybackSessionManager& playbackSessionManager();
    void videoControlsManagerDidChange();
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    VideoFullscreenManager& videoFullscreenManager();
#endif

#if PLATFORM(IOS_FAMILY)
    void setAllowsMediaDocumentInlinePlayback(bool);
    bool allowsMediaDocumentInlinePlayback() const { return m_allowsMediaDocumentInlinePlayback; }
#endif

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManager* fullScreenManager();
#endif

    void addConsoleMessage(WebCore::FrameIdentifier, MessageSource, MessageLevel, const String&, uint64_t requestID = 0);
    void sendCSPViolationReport(WebCore::FrameIdentifier, const URL& reportURL, IPC::FormDataReference&&);
    void enqueueSecurityPolicyViolationEvent(WebCore::FrameIdentifier, WebCore::SecurityPolicyViolationEvent::Init&&);

    // -- Called by the DrawingArea.
    // FIXME: We could genericize these into a DrawingArea client interface. Would that be beneficial?
    void drawRect(WebCore::GraphicsContext&, const WebCore::IntRect&);

    // -- Called from WebCore clients.
    bool handleEditingKeyboardEvent(WebCore::KeyboardEvent&);

    void didStartPageTransition();
    void didCompletePageTransition();
    void didCommitLoad(WebFrame*);
    void willReplaceMultipartContent(const WebFrame&);
    void didReplaceMultipartContent(const WebFrame&);
    void didFinishDocumentLoad(WebFrame&);
    void didFinishLoad(WebFrame&);
    void show();
    String userAgent(const URL&) const;
    String platformUserAgent(const URL&) const;
    WebCore::KeyboardUIMode keyboardUIMode();

    bool hoverSupportedByPrimaryPointingDevice() const;
    bool hoverSupportedByAnyAvailablePointingDevice() const;
    Optional<WebCore::PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const;
    OptionSet<WebCore::PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const;

    void didInsertMenuElement(WebCore::HTMLMenuElement&);
    void didRemoveMenuElement(WebCore::HTMLMenuElement&);
    void didInsertMenuItemElement(WebCore::HTMLMenuItemElement&);
    void didRemoveMenuItemElement(WebCore::HTMLMenuItemElement&);

    void animationDidFinishForElement(const WebCore::Element&);

    const String& overrideContentSecurityPolicy() const { return m_overrideContentSecurityPolicy; }

    WebUndoStep* webUndoStep(WebUndoStepID);
    void addWebUndoStep(WebUndoStepID, Ref<WebUndoStep>&&);
    void removeWebEditCommand(WebUndoStepID);
    bool isInRedo() const { return m_isInRedo; }

    bool isAlwaysOnLoggingAllowed() const;
    void setActivePopupMenu(WebPopupMenu*);

    void setHiddenPageDOMTimerThrottlingIncreaseLimit(Seconds limit)
    {
        m_page->setDOMTimerAlignmentIntervalIncreaseLimit(limit);
    }

#if ENABLE(INPUT_TYPE_COLOR)
    WebColorChooser* activeColorChooser() const { return m_activeColorChooser; }
    void setActiveColorChooser(WebColorChooser*);
    void didChooseColor(const WebCore::Color&);
    void didEndColorPicker();
#endif

#if ENABLE(DATALIST_ELEMENT)
    void setActiveDataListSuggestionPicker(WebDataListSuggestionPicker&);
    void didSelectDataListOption(const String&);
    void didCloseSuggestions();
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    void setActiveDateTimeChooser(WebDateTimeChooser&);
    void didChooseDate(const String&);
    void didEndDateTimePicker();
#endif

    WebOpenPanelResultListener* activeOpenPanelResultListener() const { return m_activeOpenPanelResultListener.get(); }
    void setActiveOpenPanelResultListener(Ref<WebOpenPanelResultListener>&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&) override;

    // -- InjectedBundle methods
#if ENABLE(CONTEXT_MENUS)
    void setInjectedBundleContextMenuClient(std::unique_ptr<API::InjectedBundle::PageContextMenuClient>&&);
#endif
    void setInjectedBundleEditorClient(std::unique_ptr<API::InjectedBundle::EditorClient>&&);
    void setInjectedBundleFormClient(std::unique_ptr<API::InjectedBundle::FormClient>&&);
    void setInjectedBundlePageLoaderClient(std::unique_ptr<API::InjectedBundle::PageLoaderClient>&&);
    void initializeInjectedBundlePolicyClient(WKBundlePagePolicyClientBase*);
    void setInjectedBundleResourceLoadClient(std::unique_ptr<API::InjectedBundle::ResourceLoadClient>&&);
    void setInjectedBundleUIClient(std::unique_ptr<API::InjectedBundle::PageUIClient>&&);
#if ENABLE(FULLSCREEN_API)
    void initializeInjectedBundleFullScreenClient(WKBundlePageFullScreenClientBase*);
#endif

#if ENABLE(CONTEXT_MENUS)
    API::InjectedBundle::PageContextMenuClient& injectedBundleContextMenuClient() { return *m_contextMenuClient; }
#endif
    API::InjectedBundle::EditorClient& injectedBundleEditorClient() { return *m_editorClient; }
    API::InjectedBundle::FormClient& injectedBundleFormClient() { return *m_formClient; }
    API::InjectedBundle::PageLoaderClient& injectedBundleLoaderClient() { return *m_loaderClient; }
    InjectedBundlePagePolicyClient& injectedBundlePolicyClient() { return m_policyClient; }
    API::InjectedBundle::ResourceLoadClient& injectedBundleResourceLoadClient() { return *m_resourceLoadClient; }
    API::InjectedBundle::PageUIClient& injectedBundleUIClient() { return *m_uiClient; }
#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient& injectedBundleFullScreenClient() { return m_fullScreenClient; }
#endif

    bool findStringFromInjectedBundle(const String&, OptionSet<FindOptions>);
    void findStringMatchesFromInjectedBundle(const String&, OptionSet<FindOptions>);
    void replaceStringMatchesFromInjectedBundle(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly);

    WebFrame& mainWebFrame() const { return m_mainFrame; }

    WebCore::Frame* mainFrame() const; // May return nullptr.
    WebCore::FrameView* mainFrameView() const; // May return nullptr.

    Optional<WebCore::SimpleRange> currentSelectionAsRange();

#if ENABLE(NETSCAPE_PLUGIN_API)
    RefPtr<Plugin> createPlugin(WebFrame*, WebCore::HTMLPlugInElement*, const Plugin::Parameters&, String& newMIMEType);
#endif

#if ENABLE(WEBGL)
    WebCore::WebGLLoadPolicy webGLPolicyForURL(WebFrame*, const URL&);
    WebCore::WebGLLoadPolicy resolveWebGLPolicyForURL(WebFrame*, const URL&);
#endif
    
    enum class ShouldPerformLayout { Default, Yes };
    EditorState editorState(ShouldPerformLayout = ShouldPerformLayout::Default) const;
    void updateEditorStateAfterLayoutIfEditabilityChanged();

    // options are RenderTreeExternalRepresentationBehavior values.
    String renderTreeExternalRepresentation(unsigned options = 0) const;
    String renderTreeExternalRepresentationForPrinting() const;
    uint64_t renderTreeSize() const;

    void setTracksRepaints(bool);
    bool isTrackingRepaints() const;
    void resetTrackedRepaints();
    Ref<API::Array> trackedRepaintRects();

    void executeEditingCommand(const String& commandName, const String& argument);
    bool isEditingCommandEnabled(const String& commandName);
    void clearMainFrameName();
    void sendClose();

    void suspendForProcessSwap();

    void sendSetWindowFrame(const WebCore::FloatRect&);

    double textZoomFactor() const;
    void setTextZoomFactor(double);
    double pageZoomFactor() const;
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);
    void windowScreenDidChange(WebCore::PlatformDisplayID, Optional<unsigned> nominalFramesPerSecond);
    String dumpHistoryForTesting(const String& directory);
    void clearHistory();

    void accessibilitySettingsDidChange();
    void screenPropertiesDidChange();

    void scalePage(double scale, const WebCore::IntPoint& origin);
    void scalePageInViewCoordinates(double scale, WebCore::IntPoint centerInViewCoordinates);
    double pageScaleFactor() const;
    double totalScaleFactor() const;
    double viewScaleFactor() const;
    void scaleView(double scale);

    void setUseFixedLayout(bool);
    bool useFixedLayout() const { return m_useFixedLayout; }
    bool setFixedLayoutSize(const WebCore::IntSize&);
    WebCore::IntSize fixedLayoutSize() const;

    void listenForLayoutMilestones(OptionSet<WebCore::LayoutMilestone>);

    void setSuppressScrollbarAnimations(bool);
    
    void setEnableVerticalRubberBanding(bool);
    void setEnableHorizontalRubberBanding(bool);
    
    void setBackgroundExtendsBeyondPage(bool);

    void setPaginationMode(uint32_t /* WebCore::Pagination::Mode */);
    void setPaginationBehavesLikeColumns(bool);
    void setPageLength(double);
    void setGapBetweenPages(double);
    void setPaginationLineGridEnabled(bool);
    
    void postInjectedBundleMessage(const String& messageName, const UserData&);

    void setUnderlayColor(const WebCore::Color& color) { m_underlayColor = color; }
    WebCore::Color underlayColor() const { return m_underlayColor; }

    void stopLoading();
    void stopLoadingFrame(WebCore::FrameIdentifier);
    bool defersLoading() const;
    void setDefersLoading(bool deferLoading);

    void enterAcceleratedCompositingMode(WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode();

    void addPluginView(PluginView*);
    void removePluginView(PluginView*);

    bool isVisible() const { return m_activityState.contains(WebCore::ActivityState::IsVisible); }
    bool isVisibleOrOccluded() const { return m_activityState.contains(WebCore::ActivityState::IsVisibleOrOccluded); }

    LayerHostingMode layerHostingMode() const { return m_layerHostingMode; }
    void setLayerHostingMode(LayerHostingMode);

#if PLATFORM(COCOA)
    void updatePluginsActiveAndFocusedState();
    const WebCore::FloatRect& windowFrameInScreenCoordinates() const { return m_windowFrameInScreenCoordinates; }
    const WebCore::FloatRect& windowFrameInUnflippedScreenCoordinates() const { return m_windowFrameInUnflippedScreenCoordinates; }
    const WebCore::FloatRect& viewFrameInWindowCoordinates() const { return m_viewFrameInWindowCoordinates; }

    bool hasCachedWindowFrame() const { return m_hasCachedWindowFrame; }

    void updateHeaderAndFooterLayersForDeviceScaleChange(float scaleFactor);
#endif

#if PLATFORM(MAC)
    void setTopOverhangImage(WebImage*);
    void setBottomOverhangImage(WebImage*);

    void didUpdateRendering();
    
    void setUseSystemAppearance(bool);

    void didEndMagnificationGesture();
#endif

    void effectiveAppearanceDidChange(bool useDarkAppearance, bool useElevatedUserInterfaceLevel);

    bool windowIsFocused() const;
    bool windowAndWebPageAreFocused() const;

#if !PLATFORM(IOS_FAMILY)
    void setHeaderPageBanner(PageBanner*);
    PageBanner* headerPageBanner();
    void setFooterPageBanner(PageBanner*);
    PageBanner* footerPageBanner();

    void hidePageBanners();
    void showPageBanners();
    
    void setHeaderBannerHeightForTesting(int);
    void setFooterBannerHeightForTesting(int);
#endif

    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&);
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&);
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&);
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&);
    
    RefPtr<WebImage> scaledSnapshotWithOptions(const WebCore::IntRect&, double additionalScaleFactor, SnapshotOptions);

    static const WebEvent* currentEvent();

    FindController& findController() { return m_findController.get(); }

#if ENABLE(GEOLOCATION)
    GeolocationPermissionRequestManager& geolocationPermissionRequestManager() { return m_geolocationPermissionRequestManager.get(); }
#endif

#if PLATFORM(IOS_FAMILY)
    void savePageState(WebCore::HistoryItem&);
    void restorePageState(const WebCore::HistoryItem&);
#endif

#if ENABLE(MEDIA_STREAM)
    UserMediaPermissionRequestManager& userMediaPermissionRequestManager() { return m_userMediaPermissionRequestManager; }
    void captureDevicesChanged();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    MediaKeySystemPermissionRequestManager& mediaKeySystemPermissionRequestManager() { return m_mediaKeySystemPermissionRequestManager; }
#endif

    void elementDidFocus(WebCore::Element&);
    void elementDidRefocus(WebCore::Element&);
    void elementDidBlur(WebCore::Element&);
    void focusedElementDidChangeInputMode(WebCore::Element&, WebCore::InputMode);
    void resetFocusedElementForFrame(WebFrame*);
    void updateInputContextAfterBlurringAndRefocusingElementIfNeeded(WebCore::Element&);

    void disabledAdaptationsDidChange(const OptionSet<WebCore::DisabledAdaptations>&);
    void viewportPropertiesDidChange(const WebCore::ViewportArguments&);
    void executeEditCommandWithCallback(const String&, const String& argument, CompletionHandler<void()>&&);
    void selectAll();

    void setCanShowPlaceholder(const WebCore::ElementContext&, bool);

#if PLATFORM(IOS_FAMILY)
    void textInputContextsInRect(WebCore::FloatRect, CompletionHandler<void(const Vector<WebCore::ElementContext>&)>&&);
    void focusTextInputContextAndPlaceCaret(const WebCore::ElementContext&, const WebCore::IntPoint&, CompletionHandler<void(bool)>&&);

    bool shouldRevealCurrentSelectionAfterInsertion() const { return m_shouldRevealCurrentSelectionAfterInsertion; }
    void setShouldRevealCurrentSelectionAfterInsertion(bool);

    void insertTextPlaceholder(const WebCore::IntSize&, CompletionHandler<void(const Optional<WebCore::ElementContext>&)>&&);
    void removeTextPlaceholder(const WebCore::ElementContext&, CompletionHandler<void()>&&);

    WebCore::FloatSize screenSize() const;
    WebCore::FloatSize availableScreenSize() const;
    WebCore::FloatSize overrideScreenSize() const;
    int32_t deviceOrientation() const { return m_deviceOrientation; }
    void didReceiveMobileDocType(bool);
    
    bool screenIsBeingCaptured() const { return m_screenIsBeingCaptured; }
    void setScreenIsBeingCaptured(bool);

    double minimumPageScaleFactor() const;
    double maximumPageScaleFactor() const;
    double maximumPageScaleFactorIgnoringAlwaysScalable() const;
    bool allowsUserScaling() const;
    bool hasStablePageScaleFactor() const { return m_hasStablePageScaleFactor; }

    void attemptSyntheticClick(const WebCore::IntPoint&, OptionSet<WebKit::WebEvent::Modifier>, TransactionID lastLayerTreeTransactionId);
    void potentialTapAtPosition(uint64_t requestID, const WebCore::FloatPoint&, bool shouldRequestMagnificationInformation);
    void commitPotentialTap(OptionSet<WebKit::WebEvent::Modifier>, TransactionID lastLayerTreeTransactionId, WebCore::PointerID);
    void commitPotentialTapFailed();
    void cancelPotentialTap();
    void cancelPotentialTapInFrame(WebFrame&);
    void tapHighlightAtPosition(uint64_t requestID, const WebCore::FloatPoint&);
    void didRecognizeLongPress();
    void handleDoubleTapForDoubleClickAtPoint(const WebCore::IntPoint&, OptionSet<WebKit::WebEvent::Modifier>, TransactionID lastLayerTreeTransactionId);

    void inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint&);
    void inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint&);

    void blurFocusedElement();
    void requestFocusedElementInformation(CompletionHandler<void(const FocusedElementInformation&)>&&);
    void selectWithGesture(const WebCore::IntPoint&, GestureType, GestureRecognizerState, bool isInteractingWithFocusedElement, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&&);
    void updateSelectionWithTouches(const WebCore::IntPoint&, SelectionTouch, bool baseIsStart, CompletionHandler<void(const WebCore::IntPoint&, SelectionTouch, OptionSet<SelectionFlags>)>&&);
    void selectWithTwoTouches(const WebCore::IntPoint& from, const WebCore::IntPoint& to, GestureType, GestureRecognizerState, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&&);
    void extendSelection(WebCore::TextGranularity, CompletionHandler<void()>&&);
    void selectWordBackward();
    void moveSelectionByOffset(int32_t offset, CompletionHandler<void()>&&);
    void selectTextWithGranularityAtPoint(const WebCore::IntPoint&, WebCore::TextGranularity, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void selectPositionAtBoundaryWithDirection(const WebCore::IntPoint&, WebCore::TextGranularity, WebCore::SelectionDirection, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity, WebCore::SelectionDirection, CompletionHandler<void()>&&);
    void selectPositionAtPoint(const WebCore::IntPoint&, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void beginSelectionInDirection(WebCore::SelectionDirection, CompletionHandler<void(bool)>&&);
    void updateSelectionWithExtentPoint(const WebCore::IntPoint&, bool isInteractingWithFocusedElement, RespectSelectionAnchor, CompletionHandler<void(bool)>&&);
    void updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint&, WebCore::TextGranularity, bool isInteractingWithFocusedElement, CompletionHandler<void(bool)>&&);

    void requestDictationContext(CompletionHandler<void(const String&, const String&, const String&)>&&);
    void replaceDictatedText(const String& oldText, const String& newText);
    void replaceSelectedText(const String& oldText, const String& newText);
    void requestAutocorrectionData(const String& textForAutocorrection, CompletionHandler<void(WebAutocorrectionData)>&& reply);
    void applyAutocorrection(const String& correction, const String& originalText, CompletionHandler<void(const String&)>&&);
    void syncApplyAutocorrection(const String& correction, const String& originalText, CompletionHandler<void(bool)>&&);
    void requestAutocorrectionContext();
    void getPositionInformation(const InteractionInformationRequest&, CompletionHandler<void(InteractionInformationAtPosition&&)>&&);
    void requestPositionInformation(const InteractionInformationRequest&);
    void startInteractionWithElementContextOrPosition(Optional<WebCore::ElementContext>&&, WebCore::IntPoint&&);
    void stopInteraction();
    void performActionOnElement(uint32_t action);
    void focusNextFocusedElement(bool isForward, CompletionHandler<void()>&&);
    void autofillLoginCredentials(const String&, const String&);
    void setFocusedElementValue(const String&);
    void setFocusedElementValueAsNumber(double);
    void setFocusedElementSelectedIndex(uint32_t index, bool allowMultipleSelection);
    void setIsShowingInputViewForFocusedElement(bool);
    bool isShowingInputViewForFocusedElement() const { return m_isShowingInputViewForFocusedElement; }
    void updateSelectionAppearance();
    void getSelectionContext(CompletionHandler<void(const String&, const String&, const String&)>&&);
    void handleTwoFingerTapAtPoint(const WebCore::IntPoint&, OptionSet<WebKit::WebEvent::Modifier>, uint64_t requestID);
    void getRectsForGranularityWithSelectionOffset(WebCore::TextGranularity, int32_t, CompletionHandler<void(const Vector<WebCore::SelectionRect>&)>&&);
    void getRectsAtSelectionOffsetWithText(int32_t, const String&, CompletionHandler<void(const Vector<WebCore::SelectionRect>&)>&&);
    void storeSelectionForAccessibility(bool);
    void startAutoscrollAtPosition(const WebCore::FloatPoint&);
    void cancelAutoscroll();
    void requestEvasionRectsAboveSelection(CompletionHandler<void(const Vector<WebCore::FloatRect>&)>&&);

    void contentSizeCategoryDidChange(const String&);

    Seconds eventThrottlingDelay() const;

    void showInspectorHighlight(const WebCore::InspectorOverlay::Highlight&);
    void hideInspectorHighlight();

    void showInspectorIndication();
    void hideInspectorIndication();

    void enableInspectorNodeSearch();
    void disableInspectorNodeSearch();

    bool forceAlwaysUserScalable() const { return m_forceAlwaysUserScalable; }
    void setForceAlwaysUserScalable(bool);

    void updateSelectionWithDelta(int64_t locationDelta, int64_t lengthDelta, CompletionHandler<void()>&&);
    void requestDocumentEditingContext(WebKit::DocumentEditingContextRequest, CompletionHandler<void(WebKit::DocumentEditingContext)>&&);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(IOS_TOUCH_EVENTS)
    void dispatchAsynchronousTouchEvents(Vector<std::pair<WebTouchEvent, CompletionHandler<void(bool)>>, 1>&&);
    void cancelAsynchronousTouchEvents(Vector<std::pair<WebTouchEvent, CompletionHandler<void(bool)>>, 1>&&);
#endif

    bool hasRichlyEditableSelection() const;

    enum class LayerTreeFreezeReason {
        PageTransition          = 1 << 0,
        BackgroundApplication   = 1 << 1,
        ProcessSuspended        = 1 << 2,
        PageSuspended           = 1 << 3,
        Printing                = 1 << 4,
        ProcessSwap             = 1 << 5,
        SwipeAnimation          = 1 << 6,
    };
    void freezeLayerTree(LayerTreeFreezeReason);
    void unfreezeLayerTree(LayerTreeFreezeReason);

    void markLayersVolatile(CompletionHandler<void(bool)>&& completionHandler = { });
    void cancelMarkLayersVolatile();

    void freezeLayerTreeDueToSwipeAnimation();
    void unfreezeLayerTreeDueToSwipeAnimation();

    NotificationPermissionRequestManager* notificationPermissionRequestManager();

    void pageDidScroll();

#if ENABLE(CONTEXT_MENUS)
    WebContextMenu& contextMenu();
    WebContextMenu* contextMenuAtPointInWindow(const WebCore::IntPoint&);
#endif

    bool hasLocalDataForURL(const URL&);

    static bool canHandleRequest(const WebCore::ResourceRequest&);

    class SandboxExtensionTracker {
    public:
        ~SandboxExtensionTracker();

        void invalidate();

        void beginLoad(WebFrame*, SandboxExtension::Handle&&);
        void beginReload(WebFrame*, SandboxExtension::Handle&&);
        void willPerformLoadDragDestinationAction(RefPtr<SandboxExtension>&& pendingDropSandboxExtension);
        void didStartProvisionalLoad(WebFrame*);
        void didCommitProvisionalLoad(WebFrame*);
        void didFailProvisionalLoad(WebFrame*);

    private:
        void setPendingProvisionalSandboxExtension(RefPtr<SandboxExtension>&&);
        bool shouldReuseCommittedSandboxExtension(WebFrame*);

        RefPtr<SandboxExtension> m_pendingProvisionalSandboxExtension;
        RefPtr<SandboxExtension> m_provisionalSandboxExtension;
        RefPtr<SandboxExtension> m_committedSandboxExtension;
    };

    SandboxExtensionTracker& sandboxExtensionTracker() { return m_sandboxExtensionTracker; }

#if PLATFORM(GTK) || PLATFORM(WPE)
    void cancelComposition(const String& text);
    void deleteSurrounding(int64_t offset, unsigned characterCount);
#endif

#if PLATFORM(GTK)
    void collapseSelectionInFrame(WebCore::FrameIdentifier);
    void showEmojiPicker(WebCore::Frame&);

    void getCenterForZoomGesture(const WebCore::IntPoint& centerInViewCoordinates, CompletionHandler<void(WebCore::IntPoint&&)>&&);

    void themeDidChange(String&&);
#endif

    void didApplyStyle();
    void didChangeSelection();
    void didChangeOverflowScrollPosition();
    void didChangeContents();
    void discardedComposition();
    void canceledComposition();
    void didUpdateComposition();
    void didEndUserTriggeredSelectionChanges();

#if PLATFORM(COCOA)
    void registerUIProcessAccessibilityTokens(const IPC::DataReference& elemenToken, const IPC::DataReference& windowToken);
    WKAccessibilityWebPageObject* accessibilityRemoteObject();
    NSObject *accessibilityObjectForMainFramePlugin();
    const WebCore::FloatPoint& accessibilityPosition() const { return m_accessibilityPosition; }
    
    void sendComplexTextInputToPlugin(uint64_t pluginComplexTextInputIdentifier, const String& textInput);

    void setTextAsync(const String&);
    void insertTextAsync(const String& text, const EditingRange& replacementRange, InsertTextOptions&&);
    void hasMarkedText(CompletionHandler<void(bool)>&&);
    void getMarkedRangeAsync(CompletionHandler<void(const EditingRange&)>&&);
    void getSelectedRangeAsync(CompletionHandler<void(const EditingRange&)>&&);
    void characterIndexForPointAsync(const WebCore::IntPoint&, CompletionHandler<void(uint64_t)>&&);
    void firstRectForCharacterRangeAsync(const EditingRange&, CompletionHandler<void(const WebCore::IntRect&, const EditingRange&)>&&);
    void setCompositionAsync(const String& text, const Vector<WebCore::CompositionUnderline>&, const Vector<WebCore::CompositionHighlight>&, const EditingRange& selectionRange, const EditingRange& replacementRange);
    void confirmCompositionAsync();

    void readSelectionFromPasteboard(const String& pasteboardName, CompletionHandler<void(bool&&)>&&);
    void getStringSelectionForPasteboard(CompletionHandler<void(String&&)>&&);
    void getDataSelectionForPasteboard(const String pasteboardType, CompletionHandler<void(SharedMemory::IPCHandle&&)>&&);
    void shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent&, CompletionHandler<void(bool)>&&);
    void acceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent&, CompletionHandler<void(bool)>&&);
    bool performNonEditingBehaviorForSelector(const String&, WebCore::KeyboardEvent*);

    void insertDictatedTextAsync(const String& text, const EditingRange& replacementRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, InsertTextOptions&&);
#endif

#if PLATFORM(MAC)
    void attributedSubstringForCharacterRangeAsync(const EditingRange&, CompletionHandler<void(const WebCore::AttributedString&, const EditingRange&)>&&);
    void fontAtSelection(CompletionHandler<void(const FontInfo&, double, bool)>&&);
#endif

#if PLATFORM(COCOA) && ENABLE(SERVICE_CONTROLS)
    void replaceSelectionWithPasteboardData(const Vector<String>& types, const IPC::DataReference&);
#endif

    void setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length, bool suppressUnderline, const Vector<WebCore::CompositionHighlight>&);
    bool hasCompositionForTesting();
    void confirmCompositionForTesting(const String& compositionString);

#if PLATFORM(COCOA)
    bool isSpeaking();
    void speak(const String&);
    void stopSpeaking();

    void performDictionaryLookupForSelection(WebCore::Frame&, const WebCore::VisibleSelection&, WebCore::TextIndicatorPresentationTransition);
#endif

    bool isSmartInsertDeleteEnabled();
    void setSmartInsertDeleteEnabled(bool);

    bool isSelectTrailingWhitespaceEnabled() const;
    void setSelectTrailingWhitespaceEnabled(bool);

    void replaceSelectionWithText(WebCore::Frame*, const String&);
    void clearSelection();
    void restoreSelectionInFocusedEditableElement();

#if ENABLE(DRAG_SUPPORT) && PLATFORM(GTK)
    void performDragControllerAction(DragControllerAction, const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragOperation> draggingSourceOperationMask, WebCore::SelectionData&&, OptionSet<WebCore::DragApplicationFlags>);
#endif

#if ENABLE(DRAG_SUPPORT) && !PLATFORM(GTK)
    void performDragControllerAction(DragControllerAction, const WebCore::DragData&, SandboxExtension::Handle&&, SandboxExtension::HandleArray&&);
#endif

#if ENABLE(DRAG_SUPPORT)
    void dragEnded(WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, OptionSet<WebCore::DragOperation>);

    void willPerformLoadDragDestinationAction();
    void mayPerformUploadDragDestinationAction();

    void willStartDrag() { ASSERT(!m_isStartingDrag); m_isStartingDrag = true; }
    void didStartDrag();
    void dragCancelled();
    OptionSet<WebCore::DragSourceAction> allowedDragSourceActions() const { return m_allowedDragSourceActions; }
#endif

    void beginPrinting(WebCore::FrameIdentifier, const PrintInfo&);
    void endPrinting();
    void computePagesForPrinting(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&&);
    void computePagesForPrintingImpl(WebCore::FrameIdentifier, const PrintInfo&, Vector<WebCore::IntRect>& pageRects, double& totalScaleFactor, WebCore::FloatBoxExtent& computedMargin);

#if PLATFORM(COCOA)
    void drawRectToImage(WebCore::FrameIdentifier, const PrintInfo&, const WebCore::IntRect&, const WebCore::IntSize&, CompletionHandler<void(const WebKit::ShareableBitmap::Handle&)>&&);
    void drawPagesToPDF(WebCore::FrameIdentifier, const PrintInfo&, uint32_t first, uint32_t count, CompletionHandler<void(const IPC::DataReference&)>&&);
    void drawPagesToPDFImpl(WebCore::FrameIdentifier, const PrintInfo&, uint32_t first, uint32_t count, RetainPtr<CFMutableDataRef>& pdfPageData);
#endif

#if PLATFORM(IOS_FAMILY)
    void computePagesForPrintingiOS(WebCore::FrameIdentifier, const PrintInfo&, Messages::WebPage::ComputePagesForPrintingiOSDelayedReply&&);
    void drawToPDFiOS(WebCore::FrameIdentifier, const PrintInfo&, size_t, Messages::WebPage::DrawToPDFiOSAsyncReply&&);
#endif

    void drawToPDF(WebCore::FrameIdentifier, const Optional<WebCore::FloatRect>&, CompletionHandler<void(const IPC::DataReference&)>&&);

#if PLATFORM(GTK)
    void drawPagesForPrinting(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(const WebCore::ResourceError&)>&&);
#endif

    void addResourceRequest(unsigned long, const WebCore::ResourceRequest&);
    void removeResourceRequest(unsigned long);

    void setMediaVolume(float);
    void setMuted(WebCore::MediaProducer::MutedStateFlags);
    void setMayStartMediaWhenInWindow(bool);
    void stopMediaCapture();

    void updateMainFrameScrollOffsetPinning();

    bool mainFrameHasCustomContentProvider() const;
    void addMIMETypeWithCustomContentProvider(const String&);

    void mainFrameDidLayout();

    bool canRunBeforeUnloadConfirmPanel() const { return m_canRunBeforeUnloadConfirmPanel; }
    void setCanRunBeforeUnloadConfirmPanel(bool canRunBeforeUnloadConfirmPanel) { m_canRunBeforeUnloadConfirmPanel = canRunBeforeUnloadConfirmPanel; }

    bool canRunModal() const { return m_canRunModal; }
    void setCanRunModal(bool canRunModal) { m_canRunModal = canRunModal; }

    void runModal();

    void setDeviceScaleFactor(float);
    float deviceScaleFactor() const;

    void forceRepaintWithoutCallback();

    void unmarkAllMisspellings();
    void unmarkAllBadGrammar();

#if PLATFORM(COCOA)
    void handleAlternativeTextUIResult(const String&);
#endif

    // For testing purpose.
    void simulateMouseDown(int button, WebCore::IntPoint, int clickCount, WKEventModifiers, WallTime);
    void simulateMouseUp(int button, WebCore::IntPoint, int clickCount, WKEventModifiers, WallTime);
    void simulateMouseMotion(WebCore::IntPoint, WallTime);

#if ENABLE(CONTEXT_MENUS)
    void contextMenuShowing() { m_isShowingContextMenu = true; }
#endif

    bool wheelEvent(const WebWheelEvent&, OptionSet<WebCore::WheelEventProcessingSteps>);

    void wheelEventHandlersChanged(bool);
    void recomputeShortCircuitHorizontalWheelEventsState();

#if ENABLE(MAC_GESTURE_EVENTS)
    void gestureEvent(const WebGestureEvent&);
#endif

    void updateVisibilityState(bool isInitialState = false);

#if PLATFORM(IOS_FAMILY)
    void setMaximumUnobscuredSize(const WebCore::FloatSize&);
    void setDeviceOrientation(int32_t);
    void dynamicViewportSizeUpdate(const WebCore::FloatSize& viewLayoutSize, const WebCore::FloatSize& maximumUnobscuredSize, const WebCore::FloatRect& targetExposedContentRect, const WebCore::FloatRect& targetUnobscuredRect, const WebCore::FloatRect& targetUnobscuredRectInScrollViewCoordinates, const WebCore::FloatBoxExtent& targetUnobscuredSafeAreaInsets, double scale, int32_t deviceOrientation, double minimumEffectiveDeviceWidth, DynamicViewportSizeUpdateID);
    bool scaleWasSetByUIProcess() const { return m_scaleWasSetByUIProcess; }
    void willStartUserTriggeredZooming();
    void applicationWillResignActive();
    void applicationDidEnterBackground(bool isSuspendedUnderLock);
    void applicationDidFinishSnapshottingAfterEnteringBackground();
    void applicationWillEnterForeground(bool isSuspendedUnderLock);
    void applicationDidBecomeActive();
    void applicationDidEnterBackgroundForMedia(bool isSuspendedUnderLock);
    void applicationWillEnterForegroundForMedia(bool isSuspendedUnderLock);
    void didFinishContentChangeObserving(WKContentChange);

    bool platformPrefersTextLegibilityBasedZoomScaling() const;

    void hardwareKeyboardAvailabilityChanged(bool keyboardIsAttached);
    bool hardwareKeyboardIsAttached() const { return m_keyboardIsAttached; }

    void updateStringForFind(const String&);
    
    bool canShowWhileLocked() const { return m_canShowWhileLocked; }
#endif

#if ENABLE(META_VIEWPORT)
    void setViewportConfigurationViewLayoutSize(const WebCore::FloatSize&, double scaleFactor, double minimumEffectiveDeviceWidth);
    void setOverrideViewportArguments(const Optional<WebCore::ViewportArguments>&);
    const WebCore::ViewportConfiguration& viewportConfiguration() const { return m_viewportConfiguration; }

    void setUseTestingViewportConfiguration(bool useTestingViewport) { m_useTestingViewportConfiguration = useTestingViewport; }
    bool isUsingTestingViewportConfiguration() const { return m_useTestingViewportConfiguration; }
#endif

#if ENABLE(UI_SIDE_COMPOSITING)
    Optional<float> scaleFromUIProcess(const VisibleContentRectUpdateInfo&) const;
    void updateVisibleContentRects(const VisibleContentRectUpdateInfo&, MonotonicTime oldestTimestamp);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    bool dispatchTouchEvent(const WebTouchEvent&);
#endif

    bool shouldUseCustomContentProviderForResponse(const WebCore::ResourceResponse&);

    bool asynchronousPluginInitializationEnabled() const { return m_asynchronousPluginInitializationEnabled; }
    void setAsynchronousPluginInitializationEnabled(bool enabled) { m_asynchronousPluginInitializationEnabled = enabled; }
    bool asynchronousPluginInitializationEnabledForAllPlugins() const { return m_asynchronousPluginInitializationEnabledForAllPlugins; }
    void setAsynchronousPluginInitializationEnabledForAllPlugins(bool enabled) { m_asynchronousPluginInitializationEnabledForAllPlugins = enabled; }
    bool artificialPluginInitializationDelayEnabled() const { return m_artificialPluginInitializationDelayEnabled; }
    void setArtificialPluginInitializationDelayEnabled(bool enabled) { m_artificialPluginInitializationDelayEnabled = enabled; }

#if PLATFORM(COCOA)
    bool pdfPluginEnabled() const { return m_pdfPluginEnabled; }
    void setPDFPluginEnabled(bool enabled) { m_pdfPluginEnabled = enabled; }

    NSDictionary *dataDetectionContext() const { return m_dataDetectionContext.get(); }
#endif

#if ENABLE(PDFKIT_PLUGIN) && !ENABLE(UI_PROCESS_PDF_HUD)
    void savePDFToFileInDownloadsFolder(const String& suggestedFilename, const URL& originatingURL, const uint8_t* data, unsigned long size);
    void savePDFToTemporaryFolderAndOpenWithNativeApplication(const String& suggestedFilename, FrameInfoData&&, const uint8_t* data, unsigned long size, const String& pdfUUID);
#endif

    bool mainFrameIsScrollable() const { return m_mainFrameIsScrollable; }

    void setAlwaysShowsHorizontalScroller(bool);
    void setAlwaysShowsVerticalScroller(bool);

    bool alwaysShowsHorizontalScroller() const { return m_alwaysShowsHorizontalScroller; };
    bool alwaysShowsVerticalScroller() const { return m_alwaysShowsVerticalScroller; };

    void setMinimumSizeForAutoLayout(const WebCore::IntSize&);
    WebCore::IntSize minimumSizeForAutoLayout() const { return m_minimumSizeForAutoLayout; }

    void setSizeToContentAutoSizeMaximumSize(const WebCore::IntSize&);
    WebCore::IntSize sizeToContentAutoSizeMaximumSize() const { return m_sizeToContentAutoSizeMaximumSize; }

    void setAutoSizingShouldExpandToViewHeight(bool shouldExpand);
    bool autoSizingShouldExpandToViewHeight() { return m_autoSizingShouldExpandToViewHeight; }

    void setViewportSizeForCSSViewportUnits(Optional<WebCore::IntSize>);
    Optional<WebCore::IntSize> viewportSizeForCSSViewportUnits() const { return m_viewportSizeForCSSViewportUnits; }

    bool canShowMIMEType(const String& MIMEType) const;
    bool canShowResponse(const WebCore::ResourceResponse&) const;

    void addTextCheckingRequest(uint64_t requestID, Ref<WebCore::TextCheckingRequest>&&);
    void didFinishCheckingText(uint64_t requestID, const Vector<WebCore::TextCheckingResult>&);
    void didCancelCheckingText(uint64_t requestID);

#if ENABLE(DATA_DETECTION)
    void setDataDetectionResults(NSArray *);
    void detectDataInAllFrames(uint64_t, CompletionHandler<void(const DataDetectionResult&)>&&);
    void removeDataDetectedLinks(CompletionHandler<void(const DataDetectionResult&)>&&);
#endif

    unsigned extendIncrementalRenderingSuppression();
    void stopExtendingIncrementalRenderingSuppression(unsigned token);
    bool shouldExtendIncrementalRenderingSuppression() { return !m_activeRenderingSuppressionTokens.isEmpty(); }

    WebCore::ScrollPinningBehavior scrollPinningBehavior() { return m_scrollPinningBehavior; }
    void setScrollPinningBehavior(uint32_t /* WebCore::ScrollPinningBehavior */ pinning);

    Optional<WebCore::ScrollbarOverlayStyle> scrollbarOverlayStyle() { return m_scrollbarOverlayStyle; }
    void setScrollbarOverlayStyle(Optional<uint32_t /* WebCore::ScrollbarOverlayStyle */> scrollbarStyle);

    Ref<WebCore::DocumentLoader> createDocumentLoader(WebCore::Frame&, const WebCore::ResourceRequest&, const WebCore::SubstituteData&);
    void updateCachedDocumentLoader(WebDocumentLoader&, WebCore::Frame&);

    void getBytecodeProfile(CompletionHandler<void(const String&)>&&);
    void getSamplingProfilerOutput(CompletionHandler<void(const String&)>&&);
    
#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    void handleTelephoneNumberClick(const String& number, const WebCore::IntPoint&);
    void handleSelectionServiceClick(WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&);
#endif

    void didChangeScrollOffsetForFrame(WebCore::Frame*);

    void setMainFrameProgressCompleted(bool completed) { m_mainFrameProgressCompleted = completed; }
    bool shouldDispatchFakeMouseMoveEvents() const { return m_shouldDispatchFakeMouseMoveEvents; }

    void postMessage(const String& messageName, API::Object* messageBody);
    void postSynchronousMessageForTesting(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData);
    void postMessageIgnoringFullySynchronousMode(const String& messageName, API::Object* messageBody);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void setInputMethodState(WebCore::Element*);
#endif

    void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&);

#if ENABLE(VIDEO) && USE(GSTREAMER)
    void requestInstallMissingMediaPlugins(const String& details, const String& description, WebCore::MediaPlayerRequestInstallMissingPluginsCallback&);
#endif

    void addUserScript(String&& source, InjectedBundleScriptWorld&, WebCore::UserContentInjectedFrames, WebCore::UserScriptInjectionTime);
    void addUserStyleSheet(const String& source, WebCore::UserContentInjectedFrames);
    void removeAllUserContent();

    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>);

    void didRestoreScrollPosition();

    bool isControlledByAutomation() const;
    void setControlledByAutomation(bool);

    void connectInspector(const String& targetId, Inspector::FrontendChannel::ConnectionType);
    void disconnectInspector(const String& targetId);
    void sendMessageToTargetBackend(const String& targetId, const String& message);

    void insertNewlineInQuotedContent();

#if USE(OS_STATE)
    WallTime loadCommitTime() const { return m_loadCommitTime; }
#endif

#if ENABLE(GAMEPAD)
    void gamepadActivity(const Vector<GamepadData>&, WebCore::EventMakesGamepadsVisible);
#endif
    
#if ENABLE(POINTER_LOCK)
    void didAcquirePointerLock();
    void didNotAcquirePointerLock();
    void didLosePointerLock();
#endif

    void didGetLoadDecisionForIcon(bool decision, CallbackID, CompletionHandler<void(const IPC::SharedBufferDataReference&)>&&);
    void setUseIconLoadingClient(bool);

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    void didConcludeEditDrag();
    void didConcludeDrop();
#endif

    void didFinishLoadingImageForElement(WebCore::HTMLImageElement&);

    WebURLSchemeHandlerProxy* urlSchemeHandlerForScheme(const String&);
    void stopAllURLSchemeTasks();

    Optional<double> cpuLimit() const { return m_cpuLimit; }

    static PluginView* pluginViewForFrame(WebCore::Frame*);

    void themeColorChanged() { m_pendingThemeColorChange = true; }
    void flushPendingThemeColorChange();

    void pageExtendedBackgroundColorDidChange() { m_pendingPageExtendedBackgroundColorChange = true; }
    void flushPendingPageExtendedBackgroundColorChange();

    void flushPendingEditorStateUpdate();

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    void hasStorageAccess(WebCore::RegistrableDomain&& subFrameDomain, WebCore::RegistrableDomain&& topFrameDomain, WebFrame&, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(WebCore::RegistrableDomain&& subFrameDomain, WebCore::RegistrableDomain&& topFrameDomain, WebFrame&, WebCore::StorageAccessScope, CompletionHandler<void(WebCore::RequestStorageAccessResult)>&&);
    bool hasPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain) const;
    void addDomainWithPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain);
    void clearPageLevelStorageAccess();
    void wasLoadedWithDataTransferFromPrevalentResource();
    void didLoadFromRegistrableDomain(WebCore::RegistrableDomain&&);
    void clearLoadedSubresourceDomains();
    void getLoadedSubresourceDomains(CompletionHandler<void(Vector<WebCore::RegistrableDomain>)>&&);
    const HashSet<WebCore::RegistrableDomain>& loadedSubresourceDomains() const { return m_loadedSubresourceDomains; }
#endif

#if ENABLE(DEVICE_ORIENTATION)
    void shouldAllowDeviceOrientationAndMotionAccess(WebCore::FrameIdentifier, FrameInfoData&&, bool mayPrompt, CompletionHandler<void(WebCore::DeviceOrientationOrMotionPermissionState)>&&);
#endif

    void showShareSheet(WebCore::ShareDataWithParsedURL&, CompletionHandler<void(bool)>&& callback);
    void showContactPicker(const WebCore::ContactsRequestData&, CompletionHandler<void(Optional<Vector<WebCore::ContactInfo>>&&)>&&);
    
#if ENABLE(ATTACHMENT_ELEMENT)
    void insertAttachment(const String& identifier, Optional<uint64_t>&& fileSize, const String& fileName, const String& contentType, CompletionHandler<void()>&&);
    void updateAttachmentAttributes(const String& identifier, Optional<uint64_t>&& fileSize, const String& contentType, const String& fileName, const IPC::DataReference& enclosingImageData, CompletionHandler<void()>&&);
    void updateAttachmentIcon(const String& identifier, const ShareableBitmap::Handle& qlThumbnailHandle);
#endif

#if ENABLE(APPLICATION_MANIFEST)
    void getApplicationManifest(CompletionHandler<void(const Optional<WebCore::ApplicationManifest>&)>&&);
    void didFinishLoadingApplicationManifest(uint64_t, const Optional<WebCore::ApplicationManifest>&);
#endif

#if USE(WPE_RENDERER)
    int hostFileDescriptor() const { return m_hostFileDescriptor.fileDescriptor(); }
#endif

    void updateCurrentModifierState(OptionSet<WebCore::PlatformEvent::Modifier> modifiers);

    UserContentControllerIdentifier userContentControllerIdentifier() const { return m_userContentController->identifier(); }

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() const { return m_userInterfaceLayoutDirection; }

    bool isSuspended() const { return m_isSuspended; }

    void didReceiveWebPageMessage(IPC::Connection&, IPC::Decoder&);

    template<typename T>
    SendSyncResult sendSyncWithDelayedReply(T&& message, typename T::Reply&& reply)
    {
        cancelGesturesBlockedOnSynchronousReplies();
        return sendSync(WTFMove(message), WTFMove(reply), Seconds::infinity(), IPC::SendSyncOption::InformPlatformProcessWillSuspend);
    }

    WebCore::DOMPasteAccessResponse requestDOMPasteAccess(const String& originIdentifier);
    WebCore::IntRect rectForElementAtInteractionLocation() const;

    const Optional<WebCore::Color>& backgroundColor() const { return m_backgroundColor; }

    void suspendAllMediaBuffering();
    void resumeAllMediaBuffering();

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

    RefPtr<WebCore::Element> elementForContext(const WebCore::ElementContext&) const;
    Optional<WebCore::ElementContext> contextForElement(WebCore::Element&) const;

    void startTextManipulations(Vector<WebCore::TextManipulationController::ExclusionRule>&&, CompletionHandler<void()>&&);
    void completeTextManipulation(const Vector<WebCore::TextManipulationController::ManipulationItem>&, CompletionHandler<void(bool allFailed, const Vector<WebCore::TextManipulationController::ManipulationFailure>&)>&&);

#if ENABLE(APPLE_PAY)
    WebPaymentCoordinator* paymentCoordinator();
#endif

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    TextCheckingControllerProxy& textCheckingController() { return m_textCheckingControllerProxy.get(); }
#endif

#if PLATFORM(COCOA)
    void setRemoteObjectRegistry(WebRemoteObjectRegistry*);
    WebRemoteObjectRegistry* remoteObjectRegistry();
#endif

    WebPageProxyIdentifier webPageProxyIdentifier() const { return m_webPageProxyIdentifier; }

    void scheduleIntrinsicContentSizeUpdate(const WebCore::IntSize&);
    void flushPendingIntrinsicContentSizeUpdate();
    void updateIntrinsicContentSizeIfNeeded(const WebCore::IntSize&);

    void scheduleFullEditorStateUpdate();
    bool isThrottleable() const;

    bool userIsInteracting() const { return m_userIsInteracting; }
    void setUserIsInteracting(bool userIsInteracting) { m_userIsInteracting = userIsInteracting; }

    bool firstFlushAfterCommit() const { return m_firstFlushAfterCommit; }
    void setFirstFlushAfterCommit(bool f) { m_firstFlushAfterCommit = f; }

#if PLATFORM(IOS_FAMILY)
    // This excludes layout overflow, includes borders.
    static WebCore::IntRect rootViewBoundsForElement(const WebCore::Element&);
    // These include layout overflow for overflow:visible elements, but exclude borders.
    static WebCore::IntRect absoluteInteractionBoundsForElement(const WebCore::Element&);
    static WebCore::IntRect rootViewInteractionBoundsForElement(const WebCore::Element&);

    InteractionInformationAtPosition positionInformation(const InteractionInformationRequest&);
    
#endif // PLATFORM(IOS_FAMILY)

#if USE(QUICK_LOOK)
    void didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti);
    void didFinishLoadForQuickLookDocumentInMainFrame(const WebCore::SharedBuffer&);
    void requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&&);
#endif

    const String& overriddenMediaType() const { return m_overriddenMediaType; }
    void setOverriddenMediaType(const String&);

    void updateCORSDisablingPatterns(Vector<String>&&);

#if ENABLE(IPC_TESTING_API)
    bool ipcTestingAPIEnabled() const { return m_ipcTestingAPIEnabled; }
    uint64_t webPageProxyID() const { return messageSenderDestinationID(); }
    uint64_t visitedLinkTableID() const { return m_visitedLinkTableID; }
#endif

    void getProcessDisplayName(CompletionHandler<void(String&&)>&&);

    WebCore::AllowsContentJavaScript allowsContentJavaScriptFromMostRecentNavigation() const { return m_allowsContentJavaScriptFromMostRecentNavigation; }
    void setAllowsContentJavaScriptFromMostRecentNavigation(WebCore::AllowsContentJavaScript allows) { m_allowsContentJavaScriptFromMostRecentNavigation = allows; }

    void getAllFrames(CompletionHandler<void(FrameTreeNodeData&&)>&&);

#if ENABLE(APP_BOUND_DOMAINS)
    void notifyPageOfAppBoundBehavior();
    void setIsNavigatingToAppBoundDomain(Optional<NavigatingToAppBoundDomain>, WebFrame*);
    bool needsInAppBrowserPrivacyQuirks() { return m_needsInAppBrowserPrivacyQuirks; }
#endif

#if ENABLE(MEDIA_USAGE)
    void addMediaUsageManagerSession(WebCore::MediaSessionIdentifier, const String&, const URL&);
    void updateMediaUsageManagerSessionState(WebCore::MediaSessionIdentifier, const WebCore::MediaUsageInfo&);
    void removeMediaUsageManagerSession(WebCore::MediaSessionIdentifier);
#endif

#if ENABLE(IMAGE_EXTRACTION)
    void requestImageExtraction(WebCore::Element&);
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)
    void showMediaControlsContextMenu(WebCore::FloatRect&&, Vector<WebCore::MediaControlsContextMenuItem>&&, CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&);
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS)

#if PLATFORM(WIN)
    uint64_t nativeWindowHandle() { return m_nativeWindowHandle; }
#endif

    static void updatePreferencesGenerated(const WebPreferencesStore&);

    void synchronizeCORSDisablingPatternsWithNetworkProcess();

#if ENABLE(GPU_PROCESS)
    RemoteRenderingBackendProxy& ensureRemoteRenderingBackendProxy();
#endif

#if ENABLE(APP_HIGHLIGHTS)
    bool createAppHighlightInSelectedRange(WebCore::CreateNewGroupForHighlight);
    void restoreAppHighlights(const Vector<SharedMemory::IPCHandle>&&);
#endif

    void dispatchWheelEventWithoutScrolling(const WebWheelEvent&, CompletionHandler<void(bool)>&&);

#if ENABLE(PDFKIT_PLUGIN)
    bool shouldUsePDFPlugin(const String& contentType, StringView path) const;
#endif

    void setLastNavigationWasAppBound(bool wasAppBound) { m_lastNavigationWasAppBound = wasAppBound; }
    void lastNavigationWasAppBound(CompletionHandler<void(bool)>&&);

private:
    WebPage(WebCore::PageIdentifier, WebPageCreationParameters&&);

    void updateThrottleState();

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    void platformInitialize();
    void platformReinitialize();
    void platformDetach();
    void getPlatformEditorState(WebCore::Frame&, EditorState&) const;
    bool platformNeedsLayoutForEditorState(const WebCore::Frame&) const;
    void platformWillPerformEditingCommand();
    void sendEditorStateUpdate();
    void getPlatformEditorStateCommon(const WebCore::Frame&, EditorState&) const;

#if HAVE(TOUCH_BAR)
    void sendTouchBarMenuDataAddedUpdate(WebCore::HTMLMenuElement&);
    void sendTouchBarMenuDataRemovedUpdate(WebCore::HTMLMenuElement&);
    void sendTouchBarMenuItemDataAddedUpdate(WebCore::HTMLMenuItemElement&);
    void sendTouchBarMenuItemDataRemovedUpdate(WebCore::HTMLMenuItemElement&);
#endif

    void didReceiveSyncWebPageMessage(IPC::Connection&, IPC::Decoder&, std::unique_ptr<IPC::Encoder>&);

#if PLATFORM(IOS_FAMILY)
    void updateViewportSizeForCSSViewportUnits();

    void getFocusedElementInformation(FocusedElementInformation&);
    void platformInitializeAccessibility();
    void generateSyntheticEditingCommand(SyntheticEditingCommandType);
    void handleSyntheticClick(WebCore::Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebKit::WebEvent::Modifier>, WebCore::PointerID = WebCore::mousePointerID);
    void completeSyntheticClick(WebCore::Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebKit::WebEvent::Modifier>, WebCore::SyntheticClickType, WebCore::PointerID = WebCore::mousePointerID);
    void sendTapHighlightForNodeIfNecessary(uint64_t requestID, WebCore::Node*);
    WebCore::VisiblePosition visiblePositionInFocusedNodeForPoint(const WebCore::Frame&, const WebCore::IntPoint&, bool isInteractingWithFocusedElement);
    Optional<WebCore::SimpleRange> rangeForGranularityAtPoint(WebCore::Frame&, const WebCore::IntPoint&, WebCore::TextGranularity, bool isInteractingWithFocusedElement);
    void setFocusedFrameBeforeSelectingTextAtLocation(const WebCore::IntPoint&);
    void dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch, const WebCore::IntPoint&);

    void sendPositionInformation(InteractionInformationAtPosition&&);
    RefPtr<ShareableBitmap> shareableBitmapSnapshotForNode(WebCore::Element&);
    WebAutocorrectionContext autocorrectionContext();
    bool applyAutocorrectionInternal(const String& correction, const String& originalText);
#endif

#if ENABLE(META_VIEWPORT)
    void resetViewportDefaultConfiguration(WebFrame* mainFrame, bool hasMobileDocType = false);
    enum class ZoomToInitialScale { No, Yes };
    void viewportConfigurationChanged(ZoomToInitialScale = ZoomToInitialScale::No);
    bool shouldIgnoreMetaViewport() const;
#endif

#if ENABLE(TEXT_AUTOSIZING)
    void textAutoSizingAdjustmentTimerFired();
    void resetIdempotentTextAutosizingIfNeeded(double previousInitialScale);
#endif
    void resetTextAutosizing();

#if ENABLE(VIEWPORT_RESIZING)
    void shrinkToFitContent(ZoomToInitialScale = ZoomToInitialScale::No);
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    void requestDragStart(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask);
    void requestAdditionalItemsForDragSession(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask);
    void insertDroppedImagePlaceholders(const Vector<WebCore::IntSize>&, CompletionHandler<void(const Vector<WebCore::IntRect>&, Optional<WebCore::TextIndicatorData>)>&& reply);
    void computeAndSendEditDragSnapshot();
#endif

#if !PLATFORM(COCOA) && !PLATFORM(WPE)
    static const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
#endif

    bool performDefaultBehaviorForKeyEvent(const WebKeyboardEvent&);
    bool handleKeyEventByRelinquishingFocusToChrome(const WebCore::KeyboardEvent&);

#if PLATFORM(MAC)
    bool executeKeypressCommandsInternal(const Vector<WebCore::KeypressCommand>&, WebCore::KeyboardEvent*);
#endif

    void testProcessIncomingSyncMessagesWhenWaitingForSyncReply(Messages::WebPage::TestProcessIncomingSyncMessagesWhenWaitingForSyncReplyDelayedReply&&);

    void updateDrawingAreaLayerTreeFreezeState();
    bool markLayersVolatileImmediatelyIfPossible();
    void layerVolatilityTimerFired();
    void callVolatilityCompletionHandlers(bool succeeded);

    String sourceForFrame(WebFrame*);

    void loadDataImpl(uint64_t navigationID, bool shouldTreatAsContinuingLoad, Optional<WebsitePoliciesData>&&, Ref<WebCore::SharedBuffer>&&, const String& MIMEType, const String& encodingName, const URL& baseURL, const URL& failingURL, const UserData&, Optional<NavigatingToAppBoundDomain>, WebCore::ShouldOpenExternalURLsPolicy = WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow);

    // Actions
    void tryClose(CompletionHandler<void(bool)>&&);
    void platformDidReceiveLoadParameters(const LoadParameters&);
    void loadRequest(LoadParameters&&);
    NO_RETURN void loadRequestWaitingForProcessLaunch(LoadParameters&&, URL&&, WebPageProxyIdentifier, bool);
    void loadData(LoadParameters&&);
    void loadAlternateHTML(LoadParameters&&);
    void loadSimulatedRequestAndResponse(LoadParameters&&, WebCore::ResourceResponse&&);
    void navigateToPDFLinkWithSimulatedClick(const String& url, WebCore::IntPoint documentPoint, WebCore::IntPoint screenPoint);
    void getPDFFirstPageSize(WebCore::FrameIdentifier, CompletionHandler<void(WebCore::FloatSize)>&&);
    void reload(uint64_t navigationID, uint32_t reloadOptions, SandboxExtension::Handle&&);
    void goToBackForwardItem(uint64_t navigationID, const WebCore::BackForwardItemIdentifier&, WebCore::FrameLoadType, WebCore::ShouldTreatAsContinuingLoad, Optional<WebsitePoliciesData>&&);
    void tryRestoreScrollPosition();
    void setInitialFocus(bool forward, bool isKeyboardEventValid, const WebKeyboardEvent&, CompletionHandler<void()>&&);
    void updateIsInWindow(bool isInitialState = false);
    void visibilityDidChange();
    void setActivityState(OptionSet<WebCore::ActivityState::Flag>, ActivityStateChangeID, CompletionHandler<void()>&&);
    void validateCommand(const String&, CompletionHandler<void(bool, int32_t)>&&);
    void executeEditCommand(const String&, const String&);
    void setEditable(bool);

    void didChangeSelectionOrOverflowScrollPosition();

    void increaseListLevel();
    void decreaseListLevel();
    void changeListType();

    void setBaseWritingDirection(WebCore::WritingDirection);

    void setNeedsFontAttributes(bool);

    void mouseEvent(const WebMouseEvent&, Optional<SandboxExtension::HandleArray>&& sandboxExtensions);
    void keyEvent(const WebKeyboardEvent&);

#if ENABLE(IOS_TOUCH_EVENTS)
    void touchEventSync(const WebTouchEvent&, CompletionHandler<void(bool)>&&);
    void resetPotentialTapSecurityOrigin();
    void updatePotentialTapSecurityOrigin(const WebTouchEvent&, bool wasHandled);
#elif ENABLE(TOUCH_EVENTS)
    void touchEvent(const WebTouchEvent&);
#endif

    void cancelPointer(WebCore::PointerID, const WebCore::IntPoint&);
    void touchWithIdentifierWasRemoved(WebCore::PointerID);

#if ENABLE(CONTEXT_MENUS)
    void contextMenuHidden() { m_isShowingContextMenu = false; }
#endif
#if ENABLE(CONTEXT_MENU_EVENT)
    void contextMenuForKeyEvent();
#endif

    static bool scroll(WebCore::Page*, WebCore::ScrollDirection, WebCore::ScrollGranularity);
    static bool logicalScroll(WebCore::Page*, WebCore::ScrollLogicalDirection, WebCore::ScrollGranularity);

    void loadURLInFrame(URL&&, const String& referrer, WebCore::FrameIdentifier);
    void loadDataInFrame(IPC::DataReference&&, String&& MIMEType, String&& encodingName, URL&& baseURL, WebCore::FrameIdentifier);

    enum class WasRestoredByAPIRequest { No, Yes };
    void restoreSessionInternal(const Vector<BackForwardListItemState>&, WasRestoredByAPIRequest, WebBackForwardListProxy::OverwriteExistingItem);
    void restoreSession(const Vector<BackForwardListItemState>&);
    void didRemoveBackForwardItem(const WebCore::BackForwardItemIdentifier&);
    void updateBackForwardListForReattach(const Vector<WebKit::BackForwardListItemState>&);
    void setCurrentHistoryItemForReattach(WebKit::BackForwardListItemState&&);

    void requestFontAttributesAtSelectionStart(CompletionHandler<void(const WebCore::FontAttributes&)>&&);

#if ENABLE(REMOTE_INSPECTOR)
    void setIndicating(bool);
#endif

    void setBackgroundColor(const Optional<WebCore::Color>&);

#if PLATFORM(COCOA)
    void setTopContentInsetFenced(float, const WTF::MachSendRight&);
#endif
    void setTopContentInset(float);

    void viewWillStartLiveResize();
    void viewWillEndLiveResize();

    void getContentsAsString(ContentAsStringIncludesChildFrames, CompletionHandler<void(const String&)>&&);
#if PLATFORM(COCOA)
    void getContentsAsAttributedString(CompletionHandler<void(const WebCore::AttributedString&)>&&);
#endif
#if ENABLE(MHTML)
    void getContentsAsMHTMLData(CompletionHandler<void(const IPC::SharedBufferDataReference&)>&& callback);
#endif
    void getMainResourceDataOfFrame(WebCore::FrameIdentifier, CompletionHandler<void(const Optional<IPC::SharedBufferDataReference>&)>&&);
    void getResourceDataFromFrame(WebCore::FrameIdentifier, const String& resourceURL, CompletionHandler<void(const Optional<IPC::SharedBufferDataReference>&)>&&);
    void getRenderTreeExternalRepresentation(CompletionHandler<void(const String&)>&&);
    void getSelectionOrContentsAsString(CompletionHandler<void(const String&)>&&);
    void getSelectionAsWebArchiveData(CompletionHandler<void(const Optional<IPC::DataReference>&)>&&);
    void getSourceForFrame(WebCore::FrameIdentifier, CompletionHandler<void(const String&)>&&);
    void getWebArchiveOfFrame(WebCore::FrameIdentifier, CompletionHandler<void(const Optional<IPC::DataReference>&)>&&);
    void runJavaScript(WebFrame*, WebCore::RunJavaScriptParameters&&, ContentWorldIdentifier, CompletionHandler<void(const IPC::DataReference&, const Optional<WebCore::ExceptionDetails>&)>&&);
    void runJavaScriptInFrameInScriptWorld(WebCore::RunJavaScriptParameters&&, Optional<WebCore::FrameIdentifier>, const std::pair<ContentWorldIdentifier, String>& worldData, CompletionHandler<void(const IPC::DataReference&, const Optional<WebCore::ExceptionDetails>&)>&&);
    void forceRepaint(CompletionHandler<void()>&&);
    void takeSnapshot(WebCore::IntRect snapshotRect, WebCore::IntSize bitmapSize, uint32_t options, CompletionHandler<void(const WebKit::ShareableBitmap::Handle&)>&&);

    void preferencesDidChange(const WebPreferencesStore&);
    void updatePreferences(const WebPreferencesStore&);
    void updateSettingsGenerated(const WebPreferencesStore&);

#if PLATFORM(IOS_FAMILY)
    bool parentProcessHasServiceWorkerEntitlement() const;
    void disableServiceWorkerEntitlement();
    void clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&&);
#else
    bool parentProcessHasServiceWorkerEntitlement() const { return true; }
    void disableServiceWorkerEntitlement() { }
    void clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&& completionHandler) { completionHandler(); }
#endif

    void didReceivePolicyDecision(WebCore::FrameIdentifier, uint64_t listenerID, PolicyDecision&&, const SandboxExtension::HandleArray&);
    void continueWillSubmitForm(WebCore::FrameIdentifier, uint64_t listenerID);
    void setUserAgent(const String&);
    void setCustomTextEncodingName(const String&);
    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();

#if PLATFORM(COCOA)
    void performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    void performDictionaryLookupOfCurrentSelection();
    void performDictionaryLookupForRange(WebCore::Frame&, const WebCore::SimpleRange&, NSDictionary *options, WebCore::TextIndicatorPresentationTransition);
    WebCore::DictionaryPopupInfo dictionaryPopupInfoForRange(WebCore::Frame&, const WebCore::SimpleRange&, NSDictionary *options, WebCore::TextIndicatorPresentationTransition);
#if ENABLE(PDFKIT_PLUGIN)
    WebCore::DictionaryPopupInfo dictionaryPopupInfoForSelectionInPDFPlugin(PDFSelection *, PDFPlugin&, NSDictionary *options, WebCore::TextIndicatorPresentationTransition);
#endif

    void windowAndViewFramesChanged(const WebCore::FloatRect& windowFrameInScreenCoordinates, const WebCore::FloatRect& windowFrameInUnflippedScreenCoordinates, const WebCore::FloatRect& viewFrameInWindowCoordinates, const WebCore::FloatPoint& accessibilityViewCoordinates);

    RetainPtr<PDFDocument> pdfDocumentForPrintingFrame(WebCore::Frame*);
    void computePagesForPrintingPDFDocument(WebCore::FrameIdentifier, const PrintInfo&, Vector<WebCore::IntRect>& resultPageRects);
    void drawPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, const WebCore::IntRect&);
    void drawPagesToPDFFromPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, uint32_t first, uint32_t count);
#endif

#if HAVE(APP_ACCENT_COLORS)
    void setAccentColor(WebCore::Color);
#endif

    void setMainFrameIsScrollable(bool);

    void unapplyEditCommand(WebUndoStepID commandID);
    void reapplyEditCommand(WebUndoStepID commandID);
    void didRemoveEditCommand(WebUndoStepID commandID);

    void findString(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount, CompletionHandler<void(bool)>&&);
    void findStringMatches(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount);
    void getImageForFindMatch(uint32_t matchIndex);
    void selectFindMatch(uint32_t matchIndex);
    void indicateFindMatch(uint32_t matchIndex);
    void hideFindUI();
    void countStringMatches(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount);
    void replaceMatches(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly, CompletionHandler<void(uint64_t)>&&);

#if USE(COORDINATED_GRAPHICS)
    void sendViewportAttributesChanged(const WebCore::ViewportArguments&);
#endif

    void didChangeSelectedIndexForActivePopupMenu(int32_t newIndex);
    void setTextForActivePopupMenu(int32_t index);

#if PLATFORM(GTK)
    void failedToShowPopupMenu();
#endif

    void didChooseFilesForOpenPanel(const Vector<String>& files, const Vector<String>& replacementFiles);
    void didCancelForOpenPanel();

#if PLATFORM(IOS_FAMILY)
    void didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>&, const String& displayString, const IPC::DataReference& iconData, WebKit::SandboxExtension::Handle&&, WebKit::SandboxExtension::Handle&&);
    bool isTransparentOrFullyClipped(const WebCore::Element&) const;
#endif

#if ENABLE(SANDBOX_EXTENSIONS)
    void extendSandboxForFilesFromOpenPanel(SandboxExtension::HandleArray&&);
#endif

    void didReceiveGeolocationPermissionDecision(GeolocationIdentifier, const String& authorizationToken);

    void didReceiveNotificationPermissionDecision(uint64_t notificationID, bool allowed);

#if ENABLE(MEDIA_STREAM)
    void userMediaAccessWasGranted(uint64_t userMediaID, WebCore::CaptureDevice&& audioDeviceUID, WebCore::CaptureDevice&& videoDeviceUID, String&& mediaDeviceIdentifierHashSalt, SandboxExtension::Handle&&, CompletionHandler<void()>&&);
    void userMediaAccessWasDenied(uint64_t userMediaID, uint64_t reason, String&& invalidConstraint);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void mediaKeySystemWasGranted(uint64_t mediaKeySystemID, CompletionHandler<void()>&&);
    void mediaKeySystemWasDenied(uint64_t mediaKeySystemID, String&& message);
#endif

    void requestMediaPlaybackState(CompletionHandler<void(WebKit::MediaPlaybackState)>&&);

    void pauseAllMediaPlayback(CompletionHandler<void()>&&);
    void suspendAllMediaPlayback(CompletionHandler<void()>&&);
    void resumeAllMediaPlayback(CompletionHandler<void()>&&);

    void advanceToNextMisspelling(bool startBeforeSelection);
    void changeSpellingToWord(const String& word);

#if USE(APPKIT)
    void uppercaseWord();
    void lowercaseWord();
    void capitalizeWord();
#endif

    bool shouldDispatchSyntheticMouseEventsWhenModifyingSelection() const;
    void platformDidSelectAll();
    
    void setHasResourceLoadClient(bool);
    void setCanUseCredentialStorage(bool);

#if ENABLE(CONTEXT_MENUS)
    void didSelectItemFromActiveContextMenu(const WebContextMenuItemData&);
#endif

    void changeSelectedIndex(int32_t index);
    void setCanStartMediaTimerFired();

    static bool platformCanHandleRequest(const WebCore::ResourceRequest&);

    static PluginView* focusedPluginViewForFrame(WebCore::Frame&);

    void reportUsedFeatures();

    void updateWebsitePolicies(WebsitePoliciesData&&);
    void notifyUserScripts();

    void changeFont(WebCore::FontChanges&&);
    void changeFontAttributes(WebCore::FontAttributeChanges&&);

#if PLATFORM(MAC)
    void performImmediateActionHitTestAtLocation(WebCore::FloatPoint);
    Optional<std::tuple<WebCore::SimpleRange, NSDictionary *>> lookupTextAtLocation(WebCore::FloatPoint);
    void immediateActionDidUpdate();
    void immediateActionDidCancel();
    void immediateActionDidComplete();

    void dataDetectorsDidPresentUI(WebCore::PageOverlay::PageOverlayID);
    void dataDetectorsDidChangeUI(WebCore::PageOverlay::PageOverlayID);
    void dataDetectorsDidHideUI(WebCore::PageOverlay::PageOverlayID);

    void handleAcceptedCandidate(WebCore::TextCheckingResult);
#endif

#if PLATFORM(COCOA)
    void requestActiveNowPlayingSessionInfo(CompletionHandler<void(bool, bool, const String&, double, double, uint64_t)>&&);
    RetainPtr<NSData> accessibilityRemoteTokenData() const;
    void accessibilityTransferRemoteToken(RetainPtr<NSData>);
#endif

    void setShouldDispatchFakeMouseMoveEvents(bool dispatch) { m_shouldDispatchFakeMouseMoveEvents = dispatch; }

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    void playbackTargetSelected(WebCore::PlaybackTargetClientContextIdentifier, const WebCore::MediaPlaybackTargetContext& outputDevice) const;
    void playbackTargetAvailabilityDidChange(WebCore::PlaybackTargetClientContextIdentifier, bool);
    void setShouldPlayToPlaybackTarget(WebCore::PlaybackTargetClientContextIdentifier, bool);
    void playbackTargetPickerWasDismissed(WebCore::PlaybackTargetClientContextIdentifier);
#endif

    void clearWheelEventTestMonitor();

    void setShouldScaleViewToFitDocument(bool);

    void pageStoppedScrolling();

#if ENABLE(VIDEO) && USE(GSTREAMER)
    void didEndRequestInstallMissingMediaPlugins(uint32_t result);
#endif

    void setUserInterfaceLayoutDirection(uint32_t);

    bool canPluginHandleResponse(const WebCore::ResourceResponse&);

    void simulateDeviceOrientationChange(double alpha, double beta, double gamma);

#if USE(SYSTEM_PREVIEW)
    void systemPreviewActionTriggered(WebCore::SystemPreviewInfo, const String& message);
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    void speakingErrorOccurred();
    void boundaryEventOccurred(bool wordBoundary, unsigned charIndex);
    void voicesDidChange();
#endif

    void frameBecameRemote(WebCore::FrameIdentifier, WebCore::GlobalFrameIdentifier&& remoteFrameIdentifier, WebCore::GlobalWindowIdentifier&& remoteWindowIdentifier);

    void registerURLSchemeHandler(uint64_t identifier, const String& scheme);

    void urlSchemeTaskDidPerformRedirection(uint64_t handlerIdentifier, uint64_t taskIdentifier, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    void urlSchemeTaskDidReceiveResponse(uint64_t handlerIdentifier, uint64_t taskIdentifier, const WebCore::ResourceResponse&);
    void urlSchemeTaskDidReceiveData(uint64_t handlerIdentifier, uint64_t taskIdentifier, const IPC::DataReference&);
    void urlSchemeTaskDidComplete(uint64_t handlerIdentifier, uint64_t taskIdentifier, const WebCore::ResourceError&);

    void setIsTakingSnapshotsForApplicationSuspension(bool);
    void setNeedsDOMWindowResizeEvent();

    void setIsSuspended(bool);

    RefPtr<WebImage> snapshotAtSize(const WebCore::IntRect&, const WebCore::IntSize& bitmapSize, SnapshotOptions);
    RefPtr<WebImage> snapshotNode(WebCore::Node&, SnapshotOptions, unsigned maximumPixelCount = std::numeric_limits<unsigned>::max());
#if PLATFORM(COCOA)
    RetainPtr<CFDataRef> pdfSnapshotAtSize(WebCore::IntRect, WebCore::IntSize bitmapSize, SnapshotOptions);
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    RefPtr<WebCore::HTMLAttachmentElement> attachmentElementWithIdentifier(const String& identifier) const;
#endif

    bool canShowMIMEType(const String&, const Function<bool(const String&, WebCore::PluginData::AllowedPluginTypes)>& supportsPlugin) const;

    void cancelGesturesBlockedOnSynchronousReplies();

    bool shouldDispatchUpdateAfterFocusingElement(const WebCore::Element&) const;

    void updateMockAccessibilityElementAfterCommittingLoad();

    void paintSnapshotAtSize(const WebCore::IntRect&, const WebCore::IntSize&, SnapshotOptions, WebCore::Frame&, WebCore::FrameView&, WebCore::GraphicsContext&);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void sendMessageToWebExtension(UserMessage&&);
    void sendMessageToWebExtensionWithReply(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&);
#endif

    void platformDidScalePage();

    Vector<RefPtr<SandboxExtension>> consumeSandboxExtensions(SandboxExtension::HandleArray&&);
    void revokeSandboxExtensions(Vector<RefPtr<SandboxExtension>>& sandboxExtensions);

    void setSelectionRange(const WebCore::IntPoint&, WebCore::TextGranularity, bool);
    
    void consumeNetworkExtensionSandboxExtensions(const SandboxExtension::HandleArray&);

    WebCore::PageIdentifier m_identifier;

    std::unique_ptr<WebCore::Page> m_page;
    Ref<WebFrame> m_mainFrame;

    RefPtr<WebPageGroupProxy> m_pageGroup;

    String m_userAgent;

    WebCore::IntSize m_viewSize;
    std::unique_ptr<DrawingArea> m_drawingArea;
    DrawingAreaType m_drawingAreaType;

    HashSet<PluginView*> m_pluginViews;
    bool m_hasSeenPlugin { false };

    HashMap<uint64_t, RefPtr<WebCore::TextCheckingRequest>> m_pendingTextCheckingRequestMap;

    bool m_useFixedLayout { false };

    WebCore::Color m_underlayColor;

#if ENABLE(UI_PROCESS_PDF_HUD)
    HashMap<PDFPluginIdentifier, WeakPtr<PDFPlugin>> m_pdfPlugInsWithHUD;
#endif

    WTF::Function<void()> m_selectionChangedHandler;
    bool m_isInRedo { false };
    bool m_isClosed { false };
    bool m_tabToLinks { false };
    
    bool m_asynchronousPluginInitializationEnabled { false };
    bool m_asynchronousPluginInitializationEnabledForAllPlugins { false };
    bool m_artificialPluginInitializationDelayEnabled { false };
    bool m_mainFrameIsScrollable { true };

    bool m_alwaysShowsHorizontalScroller { false };
    bool m_alwaysShowsVerticalScroller { false };

    bool m_shouldRenderCanvasInGPUProcess { false };
    bool m_shouldRenderDOMInGPUProcess { false };
    bool m_shouldPlayMediaInGPUProcess { false };
#if ENABLE(WEBGL)
    bool m_shouldRenderWebGLInGPUProcess { false };
#endif
#if ENABLE(APP_BOUND_DOMAINS)
    bool m_needsInAppBrowserPrivacyQuirks { false };
#endif

    // The layer hosting mode.
    LayerHostingMode m_layerHostingMode;

#if PLATFORM(COCOA)
    bool m_pdfPluginEnabled { false };
    bool m_hasCachedWindowFrame { false };

    // The frame of the containing window in screen coordinates.
    WebCore::FloatRect m_windowFrameInScreenCoordinates;

    // The frame of the containing window in unflipped screen coordinates.
    WebCore::FloatRect m_windowFrameInUnflippedScreenCoordinates;

    // The frame of the view in window coordinates.
    WebCore::FloatRect m_viewFrameInWindowCoordinates;

    // The accessibility position of the view.
    WebCore::FloatPoint m_accessibilityPosition;
    
    RetainPtr<WKAccessibilityWebPageObject> m_mockAccessibilityElement;
#endif

#if ENABLE(PLATFORM_DRIVEN_TEXT_CHECKING)
    UniqueRef<TextCheckingControllerProxy> m_textCheckingControllerProxy;
#endif

#if PLATFORM(COCOA) || (PLATFORM(GTK) && !USE(GTK4))
    std::unique_ptr<ViewGestureGeometryCollector> m_viewGestureGeometryCollector;
#endif

#if PLATFORM(COCOA)
    RetainPtr<NSDictionary> m_dataDetectionContext;
#endif

#if ENABLE(ACCESSIBILITY) && USE(ATK)
    GRefPtr<AtkObject> m_accessibilityObject;
#endif

#if PLATFORM(WIN)
    uint64_t m_nativeWindowHandle { 0 };
#endif

#if !PLATFORM(IOS_FAMILY)
    RefPtr<PageBanner> m_headerBanner;
    RefPtr<PageBanner> m_footerBanner;
#endif

    RunLoop::Timer<WebPage> m_setCanStartMediaTimer;
    bool m_mayStartMediaWhenInWindow { false };

    HashMap<WebUndoStepID, RefPtr<WebUndoStep>> m_undoStepMap;

#if ENABLE(CONTEXT_MENUS)
    std::unique_ptr<API::InjectedBundle::PageContextMenuClient> m_contextMenuClient;
#endif
    std::unique_ptr<API::InjectedBundle::EditorClient> m_editorClient;
    std::unique_ptr<API::InjectedBundle::FormClient> m_formClient;
    std::unique_ptr<API::InjectedBundle::PageLoaderClient> m_loaderClient;
    InjectedBundlePagePolicyClient m_policyClient;
    std::unique_ptr<API::InjectedBundle::ResourceLoadClient> m_resourceLoadClient;
    std::unique_ptr<API::InjectedBundle::PageUIClient> m_uiClient;
#if ENABLE(FULLSCREEN_API)
    InjectedBundlePageFullScreenClient m_fullScreenClient;
#endif

    UniqueRef<FindController> m_findController;

    RefPtr<WebInspector> m_inspector;
    RefPtr<WebInspectorUI> m_inspectorUI;
    RefPtr<RemoteWebInspectorUI> m_remoteInspectorUI;
    std::unique_ptr<WebPageInspectorTargetController> m_inspectorTargetController;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr<PlaybackSessionManager> m_playbackSessionManager;
    RefPtr<VideoFullscreenManager> m_videoFullscreenManager;
#endif

#if PLATFORM(IOS_FAMILY)
    bool m_allowsMediaDocumentInlinePlayback { false };
    Optional<WebCore::SimpleRange> m_startingGestureRange;
#endif

#if ENABLE(FULLSCREEN_API)
    RefPtr<WebFullScreenManager> m_fullScreenManager;
#endif

    RefPtr<WebPopupMenu> m_activePopupMenu;

#if ENABLE(CONTEXT_MENUS)
    RefPtr<WebContextMenu> m_contextMenu;
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    WebColorChooser* m_activeColorChooser { nullptr };
#endif

#if ENABLE(DATALIST_ELEMENT)
    WeakPtr<WebDataListSuggestionPicker> m_activeDataListSuggestionPicker;
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    WeakPtr<WebDateTimeChooser> m_activeDateTimeChooser;
#endif

    RefPtr<WebOpenPanelResultListener> m_activeOpenPanelResultListener;
    RefPtr<NotificationPermissionRequestManager> m_notificationPermissionRequestManager;

    Ref<WebUserContentController> m_userContentController;

#if ENABLE(GEOLOCATION)
    UniqueRef<GeolocationPermissionRequestManager> m_geolocationPermissionRequestManager;
#endif

#if ENABLE(MEDIA_STREAM)
    UniqueRef<UserMediaPermissionRequestManager> m_userMediaPermissionRequestManager;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    UniqueRef<MediaKeySystemPermissionRequestManager> m_mediaKeySystemPermissionRequestManager;
#endif

    std::unique_ptr<WebCore::PrintContext> m_printContext;
#if PLATFORM(GTK)
    RefPtr<WebPrintOperationGtk> m_printOperation;
#endif

    SandboxExtensionTracker m_sandboxExtensionTracker;

    RefPtr<SandboxExtension> m_pendingDropSandboxExtension;
    Vector<RefPtr<SandboxExtension>> m_pendingDropExtensionsForFileUpload;

    PAL::HysteresisActivity m_pageScrolledHysteresis;

    bool m_canRunBeforeUnloadConfirmPanel { false };

    bool m_canRunModal { false };
    bool m_isRunningModal { false };

#if ENABLE(DRAG_SUPPORT)
    bool m_isStartingDrag { false };
    OptionSet<WebCore::DragSourceAction> m_allowedDragSourceActions { WebCore::anyDragSourceAction() };
#endif

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    HashSet<RefPtr<WebCore::HTMLImageElement>> m_pendingImageElementsForDropSnapshot;
    Optional<WebCore::SimpleRange> m_rangeForDropSnapshot;
#endif

    bool m_cachedMainFrameIsPinnedToLeftSide { true };
    bool m_cachedMainFrameIsPinnedToRightSide { true };
    bool m_cachedMainFrameIsPinnedToTopSide { true };
    bool m_cachedMainFrameIsPinnedToBottomSide { true };
    bool m_canShortCircuitHorizontalWheelEvents { false };
    bool m_hasWheelEventHandlers { false };

    unsigned m_cachedPageCount { 0 };

    HashSet<unsigned long> m_trackedNetworkResourceRequestIdentifiers;

    WebCore::IntSize m_minimumSizeForAutoLayout;
    WebCore::IntSize m_sizeToContentAutoSizeMaximumSize;
    bool m_autoSizingShouldExpandToViewHeight { false };
    Optional<WebCore::IntSize> m_viewportSizeForCSSViewportUnits;

    bool m_userIsInteracting { false };
    bool m_hasEverFocusedElementDueToUserInteractionSincePageTransition { false };

#if HAVE(TOUCH_BAR)
    bool m_isTouchBarUpdateSupressedForHiddenContentEditable { false };
    bool m_isNeverRichlyEditableForTouchBar { false };
#endif
    OptionSet<WebCore::ActivityState::Flag> m_lastActivityStateChanges;

#if ENABLE(CONTEXT_MENUS)
    bool m_isShowingContextMenu { false };
#endif

    RefPtr<WebCore::Element> m_focusedElement;
    RefPtr<WebCore::Element> m_recentlyBlurredElement;
    bool m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement { false };
    bool m_pendingThemeColorChange { false };
    bool m_pendingPageExtendedBackgroundColorChange { false };
    bool m_hasPendingEditorStateUpdate { false };

#if ENABLE(IOS_TOUCH_EVENTS)
    CompletionHandler<void(bool)> m_pendingSynchronousTouchEventReply;
#endif

#if ENABLE(META_VIEWPORT)
    WebCore::ViewportConfiguration m_viewportConfiguration;
    bool m_useTestingViewportConfiguration { false };
    bool m_forceAlwaysUserScalable { false };
#endif

#if PLATFORM(IOS_FAMILY)
    Optional<WebCore::SimpleRange> m_currentWordRange;
    RefPtr<WebCore::Node> m_interactionNode;
    WebCore::IntPoint m_lastInteractionLocation;

    bool m_isShowingInputViewForFocusedElement { false };
    bool m_hasHandledSyntheticClick { false };
    
    enum SelectionAnchor { Start, End };
    SelectionAnchor m_selectionAnchor { Start };

    RefPtr<WebCore::Node> m_potentialTapNode;
    WebCore::FloatPoint m_potentialTapLocation;
    RefPtr<WebCore::SecurityOrigin> m_potentialTapSecurityOrigin;

    bool m_hasReceivedVisibleContentRectsAfterDidCommitLoad { false };
    bool m_hasRestoredExposedContentRectAfterDidCommitLoad { false };
    bool m_scaleWasSetByUIProcess { false };
    bool m_userHasChangedPageScaleFactor { false };
    bool m_hasStablePageScaleFactor { true };
    bool m_isInStableState { true };
    bool m_shouldRevealCurrentSelectionAfterInsertion { true };
    bool m_screenIsBeingCaptured { false };
    MonotonicTime m_oldestNonStableUpdateVisibleContentRectsTimestamp;
    Seconds m_estimatedLatency { 0 };
    WebCore::FloatSize m_screenSize;
    WebCore::FloatSize m_availableScreenSize;
    WebCore::FloatSize m_overrideScreenSize;

    Optional<WebCore::SimpleRange> m_initialSelection;
    WebCore::VisibleSelection m_storedSelectionForAccessibility { WebCore::VisibleSelection() };
    WebCore::FloatSize m_maximumUnobscuredSize;
    int32_t m_deviceOrientation { 0 };
    bool m_keyboardIsAttached { false };
    bool m_canShowWhileLocked { false };
    bool m_inDynamicSizeUpdate { false };
    HashMap<std::pair<WebCore::IntSize, double>, WebCore::IntPoint> m_dynamicSizeUpdateHistory;
    RefPtr<WebCore::Node> m_pendingSyntheticClickNode;
    WebCore::FloatPoint m_pendingSyntheticClickLocation;
    WebCore::FloatRect m_previousExposedContentRect;
    OptionSet<WebKit::WebEvent::Modifier> m_pendingSyntheticClickModifiers;
    WebCore::PointerID m_pendingSyntheticClickPointerId { 0 };
    FocusedElementIdentifier m_currentFocusedElementIdentifier { 0 };
    Optional<DynamicViewportSizeUpdateID> m_pendingDynamicViewportSizeUpdateID;
    double m_lastTransactionPageScaleFactor { 0 };
    TransactionID m_lastTransactionIDWithScaleChange;

    CompletionHandler<void(InteractionInformationAtPosition&&)> m_pendingSynchronousPositionInformationReply;
    Optional<std::pair<TransactionID, double>> m_lastLayerTreeTransactionIdAndPageScaleBeforeScalingPage;
#endif
#if PLATFORM(MAC)
    bool m_didUpdateRenderingAfterCommittingLoad { false };
#endif

    WebCore::Timer m_layerVolatilityTimer;
    Vector<CompletionHandler<void(bool)>> m_markLayersAsVolatileCompletionHandlers;
    bool m_isSuspendedUnderLock { false };

    HashSet<String, ASCIICaseInsensitiveHash> m_mimeTypesWithCustomContentProviders;
    Optional<WebCore::Color> m_backgroundColor { WebCore::Color::white };

    HashSet<unsigned> m_activeRenderingSuppressionTokens;
    unsigned m_maximumRenderingSuppressionToken { 0 };
    
    WebCore::ScrollPinningBehavior m_scrollPinningBehavior { WebCore::DoNotPin };
    Optional<WebCore::ScrollbarOverlayStyle> m_scrollbarOverlayStyle;

    bool m_useAsyncScrolling { false };

    OptionSet<WebCore::ActivityState::Flag> m_activityState;

    bool m_isAppNapEnabled { true };
    UserActivity m_userActivity;

    uint64_t m_pendingNavigationID { 0 };
    Optional<WebsitePoliciesData> m_pendingWebsitePolicies;

    bool m_mainFrameProgressCompleted { false };
    bool m_shouldDispatchFakeMouseMoveEvents { true };
    bool m_isSelectingTextWhileInsertingAsynchronously { false };

    enum class EditorStateIsContentEditable { No, Yes, Unset };
    mutable EditorStateIsContentEditable m_lastEditorStateWasContentEditable { EditorStateIsContentEditable::Unset };

#if PLATFORM(GTK) || PLATFORM(WPE)
    Optional<InputMethodState> m_inputMethodState;
#endif

#if ENABLE(VIDEO) && USE(GSTREAMER)
    RefPtr<WebCore::MediaPlayerRequestInstallMissingPluginsCallback> m_installMediaPluginsCallback;
#endif

#if USE(OS_STATE)
    WallTime m_loadCommitTime;
#endif

    WebCore::UserInterfaceLayoutDirection m_userInterfaceLayoutDirection { WebCore::UserInterfaceLayoutDirection::LTR };

    const String m_overrideContentSecurityPolicy;
    const Optional<double> m_cpuLimit;

#if USE(WPE_RENDERER)
    IPC::Attachment m_hostFileDescriptor;
#endif

    HashMap<String, RefPtr<WebURLSchemeHandlerProxy>> m_schemeToURLSchemeHandlerProxyMap;
    HashMap<uint64_t, WebURLSchemeHandlerProxy*> m_identifierToURLSchemeHandlerProxyMap;

    HashMap<uint64_t, Function<void(bool granted)>> m_storageAccessResponseCallbackMap;

#if ENABLE(APPLICATION_MANIFEST)
    HashMap<uint64_t, CompletionHandler<void(const Optional<WebCore::ApplicationManifest>&)>> m_applicationManifestFetchCallbackMap;
#endif

    OptionSet<LayerTreeFreezeReason> m_layerTreeFreezeReasons;
    bool m_isSuspended { false };
    bool m_needsFontAttributes { false };
    bool m_firstFlushAfterCommit { false };
#if PLATFORM(COCOA)
    WeakPtr<WebRemoteObjectRegistry> m_remoteObjectRegistry;
#endif
    WebPageProxyIdentifier m_webPageProxyIdentifier;
    Optional<WebCore::IntSize> m_pendingIntrinsicContentSize;
    WebCore::IntSize m_lastSentIntrinsicContentSize;
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    std::unique_ptr<LayerHostingContext> m_contextForVisibilityPropagation;
#endif
#if ENABLE(TEXT_AUTOSIZING)
    WebCore::Timer m_textAutoSizingAdjustmentTimer;
#endif

#if ENABLE(RESOURCE_LOAD_STATISTICS)
    HashMap<WebCore::RegistrableDomain, WebCore::RegistrableDomain> m_domainsWithPageLevelStorageAccess;
    HashSet<WebCore::RegistrableDomain> m_loadedSubresourceDomains;
#endif

    String m_overriddenMediaType;
    String m_processDisplayName;
    WebCore::AllowsContentJavaScript m_allowsContentJavaScriptFromMostRecentNavigation { WebCore::AllowsContentJavaScript::Yes };

#if PLATFORM(GTK)
    String m_themeName;
#endif
    
#if ENABLE(APP_BOUND_DOMAINS)
    bool m_limitsNavigationsToAppBoundDomains { false };
    bool m_navigationHasOccured { false };
#endif

    bool m_lastNavigationWasAppBound { false };

    bool m_canUseCredentialStorage { true };

    Vector<String> m_corsDisablingPatterns;

#if ENABLE(IPC_TESTING_API)
    bool m_ipcTestingAPIEnabled { false };
    uint64_t m_visitedLinkTableID;
#endif

#if ENABLE(GPU_PROCESS)
    std::unique_ptr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
#endif

#if ENABLE(IMAGE_EXTRACTION)
    WeakHashSet<WebCore::Element> m_elementsWithExtractedImages;
#endif

#if HAVE(STATIC_FONT_REGISTRY)
    RefPtr<SandboxExtension> m_fontExtension;
#endif
};

#if !PLATFORM(IOS_FAMILY)
inline void WebPage::platformWillPerformEditingCommand() { }
inline bool WebPage::platformNeedsLayoutForEditorState(const WebCore::Frame&) const { return false; }
#endif

} // namespace WebKit
