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

#include "APIObject.h"
#include "MessageReceiver.h"
#include "MessageSender.h"
#include "RemoteSnapshotIdentifier.h"
#include "SandboxExtension.h"
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <WebCore/BoxExtents.h>
#include <WebCore/DictionaryPopupInfo.h>
#include <WebCore/DisabledAdaptations.h>
#include <WebCore/DragActions.h>
#include <WebCore/FocusOptions.h>
#include <WebCore/FrameIdentifier.h>
#include <WebCore/FrameLoaderTypes.h>
#include <WebCore/FrameTreeSyncData.h>
#include <WebCore/HighlightVisibility.h>
#include <WebCore/IntDegrees.h>
#include <WebCore/LayerHostingContextIdentifier.h>
#include <WebCore/MediaControlsContextMenuItem.h>
#include <WebCore/MediaKeySystemRequest.h>
#include <WebCore/NodeIdentifier.h>
#include <WebCore/NowPlayingMetadataObserver.h>
#include <WebCore/OwnerPermissionsPolicyData.h>
#include <WebCore/PageIdentifier.h>
#include <WebCore/PageOverlay.h>
#include <WebCore/PlatformLayerIdentifier.h>
#include <WebCore/PlaybackTargetClientContextIdentifier.h>
#include <WebCore/PluginData.h>
#include <WebCore/PointerCharacteristics.h>
#include <WebCore/PointerID.h>
#include <WebCore/RectEdges.h>
#include <WebCore/RegistrableDomain.h>
#include <WebCore/SecurityPolicyViolationEvent.h>
#include <WebCore/ShareData.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/SubstituteData.h>
#include <WebCore/UserContentTypes.h>
#include <WebCore/UserScriptTypes.h>
#include <WebCore/WebCoreKeyboardUIMode.h>
#include <memory>
#include <pal/HysteresisActivity.h>
#include <wtf/CallbackAggregator.h>
#include <wtf/CompletionHandler.h>
#include <wtf/HashMap.h>
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/RefPtr.h>
#include <wtf/RunLoop.h>
#include <wtf/Seconds.h>
#include <wtf/WallTime.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/WTFString.h>

#if USE(ATSPI)
#include <WebCore/AccessibilityRootAtspi.h>
#endif

#if PLATFORM(GTK)
#include "ArgumentCodersGtk.h"
#include "WebPrintOperationGtk.h"
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#include "InputMethodState.h"
#endif

#if PLATFORM(IOS_FAMILY)
#include "DynamicViewportSizeUpdate.h"
#include "GestureTypes.h"
#include <WebCore/InspectorOverlay.h>
#include <WebCore/IntPoint.h>
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

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
#include <WebCore/LinkDecorationFilteringData.h>
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
#include <WebKitAdditions/PlatformGestureEventMac.h>
#endif

#if ENABLE(MEDIA_USAGE)
#include <WebCore/MediaSessionIdentifier.h>
#endif

#if PLATFORM(COCOA)
#include <WebCore/VisibleSelection.h>
#include <wtf/RetainPtr.h>

OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSObject;
OBJC_CLASS PDFDocument;
OBJC_CLASS PDFSelection;
OBJC_CLASS WKAccessibilityWebPageObject;
#endif

#define ENABLE_VIEWPORT_RESIZING PLATFORM(IOS_FAMILY)

namespace WTF {
enum class Critical : bool;
}

namespace API {
class Array;
namespace InjectedBundle {
class EditorClient;
#if ENABLE(CONTEXT_MENUS)
class PageContextMenuClient;
#endif
class EditorClient;
class FormClient;
class PageLoaderClient;
class ResourceLoadClient;
class PageUIClient;
} // namespace InjectedBundle
} // namespace API

namespace IPC {
class Connection;
class Decoder;
class FormDataReference;
class SharedBufferReference;
}

namespace WebCore {

class AXIsolatedTree;
class CachedPage;
class CaptureDevice;
class DocumentLoader;
class DocumentSyncData;
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
class DynamicContentScalingDisplayList;
#endif
class DragData;
class WeakPtrImplWithEventTargetData;
class Exception;
class FontAttributeChanges;
class FontChanges;
class Frame;
class FrameView;
class FrameSelection;
class FrameTreeSyncData;
class GraphicsContext;
class HTMLElement;
class HTMLImageElement;
class HTMLPlugInElement;
class HTMLSelectElement;
class HTMLTextFormControlElement;
class HTMLVideoElement;
class HandleUserInputEventResult;
class HistoryItem;
class IgnoreSelectionChangeForScope;
class ImageBuffer;
class IntPoint;
class IntRect;
class KeyboardEvent;
class LegacyWebArchive;
class LocalFrame;
class LocalFrameView;
class MediaPlaybackTarget;
class MediaSessionCoordinator;
class MediaSessionManagerInterface;
class Page;
class PolicyDecision;
class PrintContext;
class Range;
class RegistrableDomain;
class RemoteFrameGeometryTransformer;
class RenderImage;
class Report;
class ResourceRequest;
class ResourceResponse;
class ScrollingCoordinator;
class SelectionData;
class SelectionGeometry;
class Settings;
class SharedBuffer;
class FragmentedSharedBuffer;
class SubstituteData;
class TextCheckingRequest;
class VisiblePosition;

enum class ActivityState : uint16_t;
enum class AdjustViewSize : bool;
enum class COEPDisposition : bool;
enum class CaretAnimatorType : uint8_t;
enum class CreateNewGroupForHighlight : bool;
enum class DidFilterLinkDecoration : bool;
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
enum class DataDetectorType : uint8_t;
enum class DragApplicationFlags : uint8_t;
enum class DragHandlingMethod : uint8_t;
enum class DeviceOrientationOrMotionPermissionState : uint8_t;
enum class EventHandling : uint8_t;
enum class EventMakesGamepadsVisible : bool;
enum class ExceptionCode : uint8_t;
enum class FinalizeRenderingUpdateFlags : uint8_t;
enum class HasOrShouldIgnoreUserGesture : bool;
enum class HighlightRequestOriginatedInApp : bool;
enum class ImageDecodingError : uint8_t;
enum class InputMode : uint8_t;
enum class IsLoggedIn : uint8_t;
enum class LayerTreeAsTextOptions : uint16_t;
enum class LayoutMilestone : uint16_t;
enum class LinkDecorationFilteringTrigger : uint8_t;
enum class MediaConstraintType : uint8_t;
enum class MediaProducerMediaCaptureKind : uint8_t;
enum class MediaProducerMediaState : uint32_t;
enum class MediaProducerMutedState : uint8_t;
enum class PlatformEventModifier : uint8_t;
enum class PlatformMediaSessionRemoteControlCommandType : uint8_t;
enum class RenderAsTextFlag : uint16_t;
enum class ScheduleLocationChangeResult : uint8_t;
enum class SelectionDirection : uint8_t;
enum class ScrollDirection : uint8_t;
enum class ScrollIsAnimated : bool;
enum class ScrollGranularity : uint8_t;
enum ScrollLogicalDirection : uint8_t;
enum class ScrollPinningBehavior : uint8_t;
enum class ScrollbarMode : uint8_t;
enum class ScrollbarOverlayStyle : uint8_t;
enum class ShouldTreatAsContinuingLoad : uint8_t;
enum class StorageAccessScope : bool;
enum class SyntheticClickResult : uint8_t;
enum class SyntheticClickType : uint8_t;
enum class TextAnimationRunMode : uint8_t;
enum class TextAnimationType : uint8_t;
enum class TextIndicatorPresentationTransition : uint8_t;
enum class TextGranularity : uint8_t;
enum class UserInterfaceLayoutDirection : bool;
enum class ViolationReportType : uint8_t;
enum class WheelEventProcessingSteps : uint8_t;
enum class WheelScrollGestureState : uint8_t;
enum class WritingDirection : uint8_t;
enum class PaginationMode : uint8_t;

struct AXDebugInfo;
struct AppHighlight;
struct AriaNotifyData;
struct AttributedString;
struct BackForwardItemIdentifierType;
struct CharacterRange;
struct CompositionHighlight;
struct CompositionUnderline;
struct ContactInfo;
struct ContactsRequestData;
struct DataDetectorElementInfo;
struct DictationAlternative;
struct DictationContextType;
struct ElementContext;
struct ExceptionData;
struct ExceptionDetails;
struct FocusOptions;
struct FontAttributes;
struct FrameTreeSyncSerializationData;
struct GlobalFrameIdentifier;
struct GlobalWindowIdentifier;
#if ENABLE(ATTACHMENT_ELEMENT)
class HTMLAttachmentElement;
#endif
#if ENABLE(IOS_TOUCH_EVENTS)
class HandleUserInputEventResult;
#endif
struct InteractionRegion;
struct KeypressCommand;
struct LiveRegionAnnouncementData;
struct MarkupExclusionRule;
struct MediaDeviceHashSalts;
struct MediaPlayerClientIdentifierType;
struct MediaUsageInfo;
struct MessageWithMessagePorts;
struct NavigationIdentifierType;
struct NowPlayingInfo;
struct PlatformMediaSessionRemoteCommandArgument;
struct DocumentSyncSerializationData;
struct PromisedAttachmentInfo;
struct RemoteUserInputEventData;
struct RequestStorageAccessResult;
struct ScrollingNodeIDType;
struct SystemPreviewInfo;
struct TargetedElementAdjustment;
struct TargetedElementInfo;
struct TargetedElementRequest;
struct TextAnimationData;
struct TextCheckingResult;
struct TextManipulationControllerExclusionRule;
struct TextManipulationControllerManipulationResult;
struct TextManipulationItem;
struct TextRecognitionOptions;
struct TextRecognitionResult;
#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
struct TranslationContextMenuInfo;
#endif
struct UserMediaRequestIdentifierType;
struct ViewportArguments;

#if HAVE(DIGITAL_CREDENTIALS_UI)
struct DigitalCredentialsRequestData;
struct DigitalCredentialsResponseData;
struct MobileDocumentRequest;
struct OpenID4VPRequest;
#endif

using BackForwardItemIdentifier = ProcessQualified<ObjectIdentifier<BackForwardItemIdentifierType>>;
using DictationContext = ObjectIdentifier<DictationContextType>;
using HTMLMediaElementIdentifier = ObjectIdentifier<MediaPlayerClientIdentifierType>;
using MediaProducerMediaStateFlags = OptionSet<MediaProducerMediaState>;
using MediaProducerMutedStateFlags = OptionSet<MediaProducerMutedState>;
using NavigationIdentifier = ObjectIdentifier<NavigationIdentifierType, uint64_t>;
using PlatformDisplayID = uint32_t;
using ScrollingNodeID = ProcessQualified<ObjectIdentifier<ScrollingNodeIDType>>;
using ScrollOffset = IntPoint;
using UserMediaRequestIdentifier = ObjectIdentifier<UserMediaRequestIdentifierType>;

namespace TextExtraction {
struct ExtractedText;
struct FilterRuleData;
struct FilterRule;
struct InteractionDescription;
struct Interaction;
struct Item;
struct Request;
}

namespace WritingTools {
enum class Action : uint8_t;
enum class EditAction : uint8_t;
enum class ReplacementState : uint8_t;
enum class TextSuggestionState : uint8_t;

struct Context;
struct Replacement;
struct Session;
struct TextSuggestion;

using ReplacementID = WTF::UUID;
using SessionID = WTF::UUID;
using TextSuggestionID = WTF::UUID;
} // namespace WritingTools

} // namespace WebCore

namespace WebKit {

class CallbackID;
class ContextMenuContextData;
class DrawingArea;
class FindController;
class FrameState;
class JavaScriptEvaluationResult;
class GPUProcessConnection;
class GamepadData;
class GeolocationPermissionRequestManager;
class InjectedBundleScriptWorld;
class LayerHostingContext;
class MediaDeviceSandboxExtensions;
class MediaKeySystemPermissionRequestManager;
class MediaPlaybackTargetContextSerialized;
class ModelProcessConnection;
class NotificationPermissionRequestManager;
class PDFPluginBase;
class PageBanner;
#if ENABLE(WEBXR)
class PlatformXRSystemProxy;
#endif
class PluginView;
class RemoteLayerTreeTransaction;
class RemoteMediaSessionCoordinator;
class RemoteRenderingBackendProxy;
class RemoteSnapshotRecorderProxy;
class RemoteWebInspectorUI;
#if ENABLE(REVEAL)
class RevealItem;
#endif
class SandboxExtension;
class SandboxExtensionHandle;
class SharedMemoryHandle;
class TextCheckingControllerProxy;
class TextAnimationController;
class UserMediaPermissionRequestManager;
class ViewGestureGeometryCollector;
class WebColorChooser;
class WebContextMenu;
class WebContextMenuItemData;
class WebDataListSuggestionPicker;
class WebDateTimeChooser;
class WebEvent;
class WebFoundTextRangeController;
class WebHistoryItemClient;
class PlaybackSessionManager;
class UserData;
class VideoPresentationManager;
#if ENABLE(UI_SIDE_COMPOSITING)
class VisibleContentRectUpdateInfo;
#endif
class WebBackForwardListItem;
#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
class WebExtensionControllerProxy;
#endif
class WebFrame;
class WebFullScreenManager;
class WebGestureEvent;
class WebImage;
class WebInspectorBackend;
class WebInspectorBackendClient;
class WebInspectorUI;
class WebKeyboardEvent;
class WebMouseEvent;
class WebNotificationClient;
class WebOpenPanelResultListener;
class WebPageGroupProxy;
class WebPageInspectorTarget;
class WebPageOverlay;
class WebPageTesting;
class WebPaymentCoordinator;
class WebPopupMenu;
class WebRemoteObjectRegistry;
class WebScreenOrientationManager;
class WebTouchEvent;
class WebURLSchemeHandler;
class WebURLSchemeHandlerProxy;
class WebUndoStep;
class WebUserContentController;
class WebWheelEvent;

enum class ContentAsStringIncludesChildFrames : bool;
enum class DragControllerAction : uint8_t;
#if ENABLE(TILED_CA_DRAWING_AREA)
enum class DrawingAreaType : bool;
#endif
enum class FindOptions : uint16_t;
enum class FindDecorationStyle : uint8_t;
enum class ImageOption : uint8_t;
enum class NavigatingToAppBoundDomain : bool;
enum class MediaPlaybackState : uint8_t;
enum class SnapshotOption : uint16_t;
enum class SyntheticEditingCommandType : uint8_t;
enum class TextInteractionSource : uint8_t;
enum class TextRecognitionUpdateResult : uint8_t;
enum class VisitedLinkTableIdentifierType;
enum class WebEventModifier : uint8_t;
enum class WebEventType : uint32_t;

struct ContentWorldData;
struct ContentWorldIdentifierType;
struct CoreIPCAuditToken;
#if (PLATFORM(GTK) || PLATFORM(WPE)) && USE(GBM)
struct RendererBufferFormat;
#endif
struct DataDetectionResult;
struct DeferredDidReceiveMouseEvent;
struct DocumentEditingContext;
struct DocumentEditingContextRequest;
struct DragInitiationResult;
struct EditingRange;
struct EditorState;
struct FrameInfoData;
struct FrameTreeCreationParameters;
struct FrameTreeNodeData;
struct FocusedElementInformation;
struct FrameTreeNodeData;
struct GeolocationIdentifierType;
struct GoToBackForwardItemParameters;
#if PLATFORM(IOS_FAMILY)
struct HardwareKeyboardState;
#endif
struct InsertTextOptions;
struct InteractionInformationAtPosition;
struct InteractionInformationRequest;
struct LoadParameters;
struct MainFrameData;
struct NodeHitTestResult;
struct PDFPluginIdentifierType;
struct PlatformFontInfo;
struct PrintInfo;
struct ProvisionalFrameCreationParameters;
#if PLATFORM(GTK) || PLATFORM(WPE)
struct RenderProcessInfo;
#endif
struct RunJavaScriptParameters;
struct StorageNamespaceIdentifierType;
struct TapIdentifierType;
struct TextAnimationData;
struct TextCheckerRequestType;
struct TextInputContext;
#if ENABLE(IOS_TOUCH_EVENTS)
struct TouchEventData;
#endif
struct TransactionIDType;
struct UserContentControllerIdentifierType;
struct UserMessage;
struct ViewWindowCoordinates;
struct WebAutocorrectionData;
struct WebAutocorrectionContext;
struct WebFoundTextRange;
struct WebHitTestResultData;
struct WebPageCreationParameters;
struct WebPageProxyIdentifierType;
struct WebPreferencesStore;
struct WebURLSchemeHandlerIdentifierType;
struct WebsitePoliciesData;

template<typename T> class MonotonicObjectIdentifier;

using ActivityStateChangeID = uint64_t;
using ContentWorldIdentifier = WebCore::ProcessQualified<ObjectIdentifier<ContentWorldIdentifierType>>;
using GeolocationIdentifier = ObjectIdentifier<GeolocationIdentifierType>;
using ImageBufferBackendHandle = Variant<
    WebCore::ShareableBitmapHandle
#if PLATFORM(COCOA)
    , MachSendRight
#endif
#if ENABLE(RE_DYNAMIC_CONTENT_SCALING)
    , WebCore::DynamicContentScalingDisplayList
#endif
>;
using ImageOptions = OptionSet<ImageOption>;
using PDFPluginIdentifier = ObjectIdentifier<PDFPluginIdentifierType>;
using SnapshotOptions = OptionSet<SnapshotOption>;
using StorageNamespaceIdentifier = ObjectIdentifier<StorageNamespaceIdentifierType>;
using TapIdentifier = ObjectIdentifier<TapIdentifierType>;
using TextCheckerRequestID = ObjectIdentifier<TextCheckerRequestType>;
using TransactionIdentifier = MonotonicObjectIdentifier<TransactionIDType>;
using TransactionID = WebCore::ProcessQualified<TransactionIdentifier>;
using UserContentControllerIdentifier = ObjectIdentifier<UserContentControllerIdentifierType>;
using VisitedLinkTableIdentifier = ObjectIdentifier<VisitedLinkTableIdentifierType>;
using WKEventModifiers = uint32_t;
using WebPageProxyIdentifier = ObjectIdentifier<WebPageProxyIdentifierType>;
using WebURLSchemeHandlerIdentifier = ObjectIdentifier<WebURLSchemeHandlerIdentifierType>;
using WebUndoStepID = uint64_t;

enum class DisallowLayoutViewportHeightExpansionReason : uint8_t {
    ElementFullScreen       = 1 << 0,
    LargeContainer          = 1 << 1,
};

class WebPage final : public API::ObjectImpl<API::Object::Type::BundlePage>, public IPC::MessageReceiver, public IPC::MessageSender {
public:
    static Ref<WebPage> create(WebCore::PageIdentifier, WebPageCreationParameters&&);

    virtual ~WebPage();

    void ref() const final;
    void deref() const final;

    void reinitializeWebPage(WebPageCreationParameters&&);
    void platformReinitializeAccessibilityToken();

    void close();

    static WebPage* fromCorePage(WebCore::Page&);

    WebCore::Page* corePage() const { return m_page.get(); }
    RefPtr<WebCore::Page> protectedCorePage() const;
    WebCore::PageIdentifier identifier() const { return m_identifier; }
    inline StorageNamespaceIdentifier sessionStorageNamespaceIdentifier() const;
    PAL::SessionID sessionID() const;
    bool usesEphemeralSession() const;

    void setSize(const WebCore::IntSize&);
    const WebCore::IntSize& size() const { return m_viewSize; }
    inline WebCore::IntRect bounds() const;

    DrawingArea* drawingArea() const { return m_drawingArea.get(); }
    RefPtr<DrawingArea> protectedDrawingArea() const;

#if ENABLE(ASYNC_SCROLLING)
    WebCore::ScrollingCoordinator* scrollingCoordinator() const;
    RefPtr<WebCore::ScrollingCoordinator> protectedScrollingCoordinator() const;
#endif

    WebPageGroupProxy* pageGroup() const { return m_pageGroup.get(); }

    bool scrollBy(WebCore::ScrollDirection, WebCore::ScrollGranularity);

    void centerSelectionInVisibleArea();

#if ENABLE(PDF_HUD)
    void createPDFHUD(PDFPluginBase&, WebCore::FrameIdentifier, const WebCore::IntRect&);
    void updatePDFHUDLocation(PDFPluginBase&, const WebCore::IntRect&);
    void removePDFHUD(PDFPluginBase&);
#endif

#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    void createPDFPageNumberIndicator(PDFPluginBase&, const WebCore::IntRect&, size_t pageCount);
    void updatePDFPageNumberIndicatorLocation(PDFPluginBase&, const WebCore::IntRect&);
    void updatePDFPageNumberIndicatorCurrentPage(PDFPluginBase&, size_t pageIndex);
    void removePDFPageNumberIndicator(PDFPluginBase&);
#endif

#if ENABLE(PDF_PLUGIN) && PLATFORM(MAC)
    void zoomPDFIn(PDFPluginIdentifier);
    void zoomPDFOut(PDFPluginIdentifier);
    void savePDF(PDFPluginIdentifier, CompletionHandler<void(const String&, const URL&, std::span<const uint8_t>)>&&);
    void openPDFWithPreview(PDFPluginIdentifier, CompletionHandler<void(const String&, std::optional<FrameInfoData>&&, std::span<const uint8_t>)>&&);
#endif

    void frameViewLayoutOrVisualViewportChanged(const WebCore::LocalFrameView&);

#if PLATFORM(COCOA)
    void willCommitLayerTree(RemoteLayerTreeTransaction&, WebCore::FrameIdentifier);
    void willCommitMainFrameData(MainFrameData&, const TransactionID&);
    void didFlushLayerTreeAtTime(MonotonicTime, bool flushSucceeded);
#endif

    void layoutIfNeeded();
    void updateRendering();
    bool hasRootFrames();
    String rootFrameOriginString();
    bool shouldTriggerRenderingUpdate(unsigned rescheduledRenderingUpdateCount) const;
    void finalizeRenderingUpdate(OptionSet<WebCore::FinalizeRenderingUpdateFlags>);

    void willStartRenderingUpdateDisplay();
    void didCompleteRenderingUpdateDisplay();
    // Called after didCompleteRenderingUpdateDisplay, but in the same run loop iteration.
    void didCompleteRenderingFrame();

    void releaseMemory(WTF::Critical);
    void willDestroyDecodedDataForAllImages();

    unsigned remoteImagesCountForTesting() const;

    enum class LazyCreationPolicy { UseExistingOnly, CreateIfNeeded };

    WebInspectorBackend* inspector(LazyCreationPolicy = LazyCreationPolicy::CreateIfNeeded);
    RefPtr<WebInspectorBackend> protectedInspector();
    WebInspectorUI* inspectorUI();
    RemoteWebInspectorUI* remoteInspectorUI();
    bool isInspectorPage() { return !!m_inspectorUI || !!m_remoteInspectorUI; }

    void inspectorFrontendCountChanged(unsigned);

#if PLATFORM(IOS_FAMILY) || (PLATFORM(MAC) && ENABLE(VIDEO_PRESENTATION_MODE))
    PlaybackSessionManager& playbackSessionManager();
    Ref<PlaybackSessionManager> protectedPlaybackSessionManager();
    void videoControlsManagerDidChange();
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    VideoPresentationManager& videoPresentationManager();
    Ref<VideoPresentationManager> protectedVideoPresentationManager();

    void startPlayingPredominantVideo(CompletionHandler<void(bool)>&&);
#endif

    void simulateClickOverFirstMatchingTextInViewportWithUserInteraction(const String& targetText, CompletionHandler<void(bool)>&&);

#if PLATFORM(IOS_FAMILY)
    void setAllowsMediaDocumentInlinePlayback(bool);
    bool allowsMediaDocumentInlinePlayback() const { return m_allowsMediaDocumentInlinePlayback; }
#endif

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManager& fullScreenManager();
    Ref<WebFullScreenManager> protectedFullscreenManager();

    enum class IsInFullscreenMode : bool { No, Yes };
    void isInFullscreenChanged(IsInFullscreenMode);

    void prepareToEnterElementFullScreen();
    void prepareToExitElementFullScreen();
    void closeFullScreen();
#endif

    void addConsoleMessage(WebCore::FrameIdentifier, MessageSource, MessageLevel, const String&, std::optional<WebCore::ResourceLoaderIdentifier> = std::nullopt);
    void enqueueSecurityPolicyViolationEvent(WebCore::FrameIdentifier, WebCore::SecurityPolicyViolationEventInit&&);

    void notifyReportObservers(WebCore::FrameIdentifier, Ref<WebCore::Report>&&);
    void sendReportToEndpoints(WebCore::FrameIdentifier, URL&& baseURL, const Vector<String>& endpointURIs, const Vector<String>& endpointTokens, IPC::FormDataReference&&, WebCore::ViolationReportType);

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
    void didSameDocumentNavigationForFrame(WebFrame&);
    void didNavigateWithinPageForFrame(WebFrame&);
    void show();
    String userAgent(const URL&) const;
    String platformUserAgent(const URL&) const;
    bool hasCustomUserAgent() const { return m_hasCustomUserAgent; }
    WebCore::KeyboardUIMode keyboardUIMode();

    void setMainFrameDocumentVisualUpdatesAllowed(bool);

    bool hasAccessoryMousePointingDevice() const;
    bool hoverSupportedByPrimaryPointingDevice() const;
    bool hoverSupportedByAnyAvailablePointingDevice() const;
    std::optional<WebCore::PointerCharacteristics> pointerCharacteristicsOfPrimaryPointingDevice() const;
    OptionSet<WebCore::PointerCharacteristics> pointerCharacteristicsOfAllAvailablePointingDevices() const;

    void animationDidFinishForElement(const WebCore::Element&);

    const String& overrideContentSecurityPolicy() const { return m_overrideContentSecurityPolicy; }

    WebUndoStep* webUndoStep(WebUndoStepID);
    void addWebUndoStep(WebUndoStepID, Ref<WebUndoStep>&&);
    void removeWebEditCommand(WebUndoStepID);
    bool isInRedo() const { return m_isInRedo; }
    void setIsInRedo(bool isInRedo) { m_isInRedo = isInRedo; }

    void closeCurrentTypingCommand();

    void setActivePopupMenu(WebPopupMenu*);

    inline void setHiddenPageDOMTimerThrottlingIncreaseLimit(Seconds);

    WebColorChooser* activeColorChooser() const;
    void setActiveColorChooser(WebColorChooser*);
    void didChooseColor(const WebCore::Color&);
    void didEndColorPicker();

    void setActiveDataListSuggestionPicker(WebDataListSuggestionPicker&);
    void didSelectDataListOption(const String&);
    void didCloseSuggestions();

    void setActiveDateTimeChooser(WebDateTimeChooser&);
    void didChooseDate(const String&);
    void didEndDateTimePicker();

    WebOpenPanelResultListener* activeOpenPanelResultListener() const { return m_activeOpenPanelResultListener.get(); }
    void setActiveOpenPanelResultListener(Ref<WebOpenPanelResultListener>&&);

    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) override;
    void didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) override;

    // -- InjectedBundle methods
#if ENABLE(CONTEXT_MENUS)
    void setInjectedBundleContextMenuClient(std::unique_ptr<API::InjectedBundle::PageContextMenuClient>&&);
#endif
    void setInjectedBundleEditorClient(std::unique_ptr<API::InjectedBundle::EditorClient>&&);
    void setInjectedBundleFormClient(std::unique_ptr<API::InjectedBundle::FormClient>&&);
    void setInjectedBundlePageLoaderClient(std::unique_ptr<API::InjectedBundle::PageLoaderClient>&&);
    void setInjectedBundleResourceLoadClient(std::unique_ptr<API::InjectedBundle::ResourceLoadClient>&&);
    void setInjectedBundleUIClient(std::unique_ptr<API::InjectedBundle::PageUIClient>&&);

#if ENABLE(CONTEXT_MENUS)
    API::InjectedBundle::PageContextMenuClient& injectedBundleContextMenuClient() { return *m_contextMenuClient; }
#endif
    API::InjectedBundle::EditorClient& injectedBundleEditorClient() { return *m_editorClient; }
    API::InjectedBundle::FormClient& injectedBundleFormClient() { return *m_formClient; }
    API::InjectedBundle::PageLoaderClient& injectedBundleLoaderClient() { return *m_loaderClient; }
    API::InjectedBundle::ResourceLoadClient& injectedBundleResourceLoadClient() { return *m_resourceLoadClient; }
    API::InjectedBundle::PageUIClient& injectedBundleUIClient() { return *m_uiClient; }

    void replaceStringMatchesFromInjectedBundle(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly);

    void setTextIndicator(RefPtr<WebCore::TextIndicator>&&);
    void updateTextIndicator(RefPtr<WebCore::TextIndicator>&&);

    WebFrame& mainWebFrame() const { return m_mainFrame; }

    WebCore::Frame* mainFrame() const;
    WebCore::FrameView* mainFrameView() const;
    WebCore::LocalFrameView* localMainFrameView() const;
    CheckedPtr<WebCore::LocalFrameView> checkedLocalMainFrameView() const;
    RefPtr<WebCore::LocalFrame> localMainFrame() const;
    RefPtr<WebCore::Document> localTopDocument() const;

    void createRemoteSubframe(WebCore::FrameIdentifier parentID, WebCore::FrameIdentifier newChildID, const String& newChildFrameName, Ref<WebCore::FrameTreeSyncData>&&);

    Awaitable<std::optional<FrameTreeNodeData>> getFrameTree();
    void didFinishLoadInAnotherProcess(WebCore::FrameIdentifier);
    void frameWasRemovedInAnotherProcess(WebCore::FrameIdentifier);

    void topDocumentSyncDataChangedInAnotherProcess(const WebCore::DocumentSyncSerializationData&);
    void allTopDocumentSyncDataChangedInAnotherProcess(Ref<WebCore::DocumentSyncData>&&);

    void frameTreeSyncDataChangedInAnotherProcess(WebCore::FrameIdentifier, const WebCore::FrameTreeSyncSerializationData&);
    void allFrameTreeSyncDataChangedInAnotherProcess(WebCore::FrameIdentifier, Ref<WebCore::FrameTreeSyncData>&&);

    std::optional<WebCore::SimpleRange> currentSelectionAsRange();

    enum class ShouldPerformLayout : bool { Default, Yes };
    EditorState editorState(ShouldPerformLayout = ShouldPerformLayout::Default) const;
    void updateEditorStateAfterLayoutIfEditabilityChanged();

    // options are RenderTreeExternalRepresentationBehavior values.
    String renderTreeExternalRepresentation(unsigned options = 0) const;
    String renderTreeExternalRepresentationForPrinting() const;
    uint64_t renderTreeSize() const;

    bool isTrackingRepaints() const;
    Ref<API::Array> trackedRepaintRects();

    void executeEditingCommand(const String& commandName, const String& argument);
    void sendClose();

    void suspendForProcessSwap(CompletionHandler<void(std::optional<bool>)>&&);

    void sendSetWindowFrame(const WebCore::FloatRect&);

    double textZoomFactor() const;
    void didSetTextZoomFactor(double);
    double pageZoomFactor() const;
    void didSetPageZoomFactor(double);
    void windowScreenDidChange(WebCore::PlatformDisplayID, std::optional<unsigned> nominalFramesPerSecond);
    String dumpHistoryForTesting(const String& directory);

    void accessibilitySettingsDidChange();
#if PLATFORM(COCOA)
    void accessibilityManageRemoteElementStatus(bool, int);
#endif
    void enableAccessibilityForAllProcesses();
    void enableAccessibility();

#if PLATFORM(MAC)
    void getAccessibilityWebProcessDebugInfo(CompletionHandler<void(WebCore::AXDebugInfo)>&&);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void clearAccessibilityIsolatedTree();
#endif
#endif // PLATFORM(MAC)

    void screenPropertiesDidChange(bool affectsStyle = true);

    // FIXME(site-isolation): Calls to these should be removed in favour of setting via WebPageProxy.
    void scalePage(double scale, const WebCore::IntPoint& origin);
    void scaleView(double scale);

    double pageScaleFactor() const;
    double totalScaleFactor() const;
    double viewScaleFactor() const;

    void didScalePage(double scale, const WebCore::IntPoint& origin);
    void didScalePageInViewCoordinates(double scale, const WebCore::IntPoint& origin);
    void didScalePageRelativeToScrollPosition(double scale, const WebCore::IntPoint& origin);
    void didScaleView(double scale);

    void setUseFixedLayout(bool);
    bool useFixedLayout() const { return m_useFixedLayout; }
    bool setFixedLayoutSize(const WebCore::IntSize&);
    WebCore::IntSize fixedLayoutSize() const;

    void setDefaultUnobscuredSize(const WebCore::FloatSize&);
    void setMinimumUnobscuredSize(const WebCore::FloatSize&);
    void setMaximumUnobscuredSize(const WebCore::FloatSize&);

    void listenForLayoutMilestones(OptionSet<WebCore::LayoutMilestone>);

    void setSuppressScrollbarAnimations(bool);

    void setHasActiveAnimatedScrolls(bool);

    void setEnableVerticalRubberBanding(bool);
    void setEnableHorizontalRubberBanding(bool);

    void setBackgroundExtendsBeyondPage(bool);

    void setPaginationMode(WebCore::PaginationMode);
    void setPaginationBehavesLikeColumns(bool);
    void setPageLength(double);
    void setGapBetweenPages(double);

    void postInjectedBundleMessage(const String& messageName, const UserData&);

    void setUnderPageBackgroundColorOverride(WebCore::Color&&);

    void setShouldSuppressHDR(bool);

    void setUnderlayColor(const WebCore::Color& color) { m_underlayColor = color; }
    WebCore::Color underlayColor() const { return m_underlayColor; }

    void stopLoading();
    void stopLoadingDueToProcessSwap();
    bool defersLoading() const;

    void enterAcceleratedCompositingMode(WebCore::Frame&, WebCore::GraphicsLayer*);
    void exitAcceleratedCompositingMode(WebCore::Frame&);

#if ENABLE(PDF_PLUGIN)
    void addPluginView(PluginView&);
    void removePluginView(PluginView&);
#endif

    inline bool isVisible() const;
    inline bool isVisibleOrOccluded() const;

    OptionSet<WebCore::ActivityState> activityState() const { return m_activityState; }
    bool isThrottleable() const;

#if PLATFORM(COCOA)
    void updatePluginsActiveAndFocusedState();
    const WebCore::FloatRect& windowFrameInScreenCoordinates() const { return m_windowFrameInScreenCoordinates; }
    const WebCore::FloatRect& windowFrameInUnflippedScreenCoordinates() const { return m_windowFrameInUnflippedScreenCoordinates; }
    const WebCore::FloatRect& viewFrameInWindowCoordinates() const { return m_viewFrameInWindowCoordinates; }

    bool hasCachedWindowFrame() const { return m_hasCachedWindowFrame; }

    void updateHeaderAndFooterLayersForDeviceScaleChange(float scaleFactor);

    bool isTransparentOrFullyClipped(const WebCore::Node&) const;
#endif

    enum class DidUpdateRenderingFlags {
        PaintedLayers = 1 << 0,
        NotifyUIProcess = 1 << 1,
    };
    void didUpdateRendering(OptionSet<DidUpdateRenderingFlags> = { DidUpdateRenderingFlags::PaintedLayers, DidUpdateRenderingFlags::NotifyUIProcess });

    // A "platform rendering update" here describes the work done by the system graphics framework before work is submitted to the system compositor.
    // On macOS, this is a CoreAnimation commit.
    void willStartPlatformRenderingUpdate();
    void didCompletePlatformRenderingUpdate();

#if PLATFORM(MAC)
    void setTopOverhangImage(WebImage*);
    void setBottomOverhangImage(WebImage*);

    void setUseFormSemanticContext(bool);
    void semanticContextDidChange(bool);

    void didBeginMagnificationGesture();
    void didEndMagnificationGesture();
#endif

    void setUseColorAppearance(bool useDarkAppearance, bool useElevatedUserInterfaceLevel);

    bool windowIsFocused() const;
    bool windowAndWebPageAreFocused() const;

#if !PLATFORM(IOS_FAMILY)
    void setHeaderPageBanner(PageBanner*);
    PageBanner* headerPageBanner();
    void setFooterPageBanner(PageBanner*);
    PageBanner* footerPageBanner();

    void hidePageBanners();
    void showPageBanners();
#endif

#if PLATFORM(MAC)
    void setHeaderBannerHeight(int);
    void setFooterBannerHeight(int);
#endif

    WebCore::IntPoint screenToRootView(const WebCore::IntPoint&);
    WebCore::IntPoint rootViewToScreen(const WebCore::IntPoint&);
    WebCore::IntRect rootViewToScreen(const WebCore::IntRect&);
    WebCore::IntPoint accessibilityScreenToRootView(const WebCore::IntPoint&);
    WebCore::IntRect rootViewToAccessibilityScreen(const WebCore::IntRect&);
#if PLATFORM(IOS_FAMILY)
    void relayAccessibilityNotification(String&&, RetainPtr<NSData>&&);
    void relayAriaNotifyNotification(WebCore::AriaNotifyData&&);
    void relayLiveRegionNotification(WebCore::LiveRegionAnnouncementData&&);
#endif

    RefPtr<WebImage> scaledSnapshotWithOptions(const WebCore::IntRect&, double additionalScaleFactor, SnapshotOptions);

    static const WebEvent* currentEvent();

    FindController& findController() { return m_findController.get(); }
    WebFoundTextRangeController& foundTextRangeController() { return m_foundTextRangeController.get(); }

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
    void updateCaptureState(const WebCore::Document&, bool isActive, WebCore::MediaProducerMediaCaptureKind, CompletionHandler<void(std::optional<WebCore::Exception>&&)>&&);
    void voiceActivityDetected();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    MediaKeySystemPermissionRequestManager& mediaKeySystemPermissionRequestManager() { return m_mediaKeySystemPermissionRequestManager; }
#endif

    void copyLinkWithHighlight();

    void elementDidFocus(WebCore::Element&, const WebCore::FocusOptions&);
    void elementDidRefocus(WebCore::Element&, const WebCore::FocusOptions&);
    void elementDidBlur(WebCore::Element&);
    void focusedElementDidChangeInputMode(WebCore::Element&, WebCore::InputMode);
    void focusedSelectElementDidChangeOptions(const WebCore::HTMLSelectElement&);
    void resetFocusedElementForFrame(WebFrame*);
    void updateInputContextAfterBlurringAndRefocusingElementIfNeeded(WebCore::Element&);

    void disabledAdaptationsDidChange(const OptionSet<WebCore::DisabledAdaptations>&);
    void viewportPropertiesDidChange(const WebCore::ViewportArguments&);
    void executeEditCommandWithCallback(const String&, const String& argument, CompletionHandler<void()>&&);
    void selectAll();

    void setCanShowPlaceholder(const WebCore::ElementContext&, bool);

    bool handlesPageScaleGesture();

#if PLATFORM(COCOA)
    void insertTextPlaceholder(const WebCore::IntSize&, CompletionHandler<void(const std::optional<WebCore::ElementContext>&)>&&);
    void removeTextPlaceholder(const WebCore::ElementContext&, CompletionHandler<void()>&&);
#endif

#if PLATFORM(IOS_FAMILY)
    void textInputContextsInRect(WebCore::FloatRect, CompletionHandler<void(const Vector<WebCore::ElementContext>&)>&&);
    void focusTextInputContextAndPlaceCaret(const WebCore::ElementContext&, const WebCore::IntPoint&, CompletionHandler<void(bool)>&&);

    bool shouldRevealCurrentSelectionAfterInsertion() const { return m_shouldRevealCurrentSelectionAfterInsertion; }
    void setShouldRevealCurrentSelectionAfterInsertion(bool);

    WebCore::FloatSize screenSize() const;
    WebCore::FloatSize availableScreenSize() const;
    WebCore::FloatSize overrideScreenSize() const;
    WebCore::FloatSize overrideAvailableScreenSize() const;
    WebCore::IntDegrees deviceOrientation() const { return m_deviceOrientation; }
    void didReceiveMobileDocType(bool);

    bool screenIsBeingCaptured() const { return m_screenIsBeingCaptured; }
    void setScreenIsBeingCaptured(bool);

    void setInsertionPointColor(WebCore::Color);

    double minimumPageScaleFactor() const;
    double maximumPageScaleFactor() const;
    double maximumPageScaleFactorIgnoringAlwaysScalable() const;
    bool allowsUserScaling() const;
    bool hasStablePageScaleFactor() const { return m_hasStablePageScaleFactor; }

    void attemptSyntheticClick(const WebCore::IntPoint&, OptionSet<WebKit::WebEventModifier>, TransactionID lastLayerTreeTransactionId);
    Awaitable<std::optional<WebCore::RemoteUserInputEventData>> potentialTapAtPosition(std::optional<WebCore::FrameIdentifier>, WebKit::TapIdentifier, WebCore::FloatPoint, bool shouldRequestMagnificationInformation);
    Awaitable<std::optional<WebCore::FrameIdentifier>> commitPotentialTap(std::optional<WebCore::FrameIdentifier>, OptionSet<WebKit::WebEventModifier>, TransactionID lastLayerTreeTransactionId, WebCore::PointerID);
    void commitPotentialTapFailed();
    void didHandleTapAsHover();
    void cancelPotentialTap();
    void cancelPotentialTapInFrame(WebFrame&);
    void tapHighlightAtPosition(WebKit::TapIdentifier, const WebCore::FloatPoint&);
    void didRecognizeLongPress();
    void handleDoubleTapForDoubleClickAtPoint(const WebCore::IntPoint&, OptionSet<WebKit::WebEventModifier>, TransactionID lastLayerTreeTransactionId);

    void inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint&);
    void inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint&);

    void blurFocusedElement();
    void requestFocusedElementInformation(CompletionHandler<void(const std::optional<FocusedElementInformation>&)>&&);
    void updateFocusedElementInformation();
    void selectWithGesture(const WebCore::IntPoint&, GestureType, GestureRecognizerState, bool isInteractingWithFocusedElement, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&&);
    void updateSelectionWithTouches(const WebCore::IntPoint&, SelectionTouch, bool baseIsStart, CompletionHandler<void(const WebCore::IntPoint&, SelectionTouch, OptionSet<SelectionFlags>)>&&);
    void selectWithTwoTouches(const WebCore::IntPoint& from, const WebCore::IntPoint& to, GestureType, GestureRecognizerState, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&&);
    void extendSelection(WebCore::TextGranularity, CompletionHandler<void()>&&);
    void extendSelectionForReplacement(CompletionHandler<void()>&&);
    void selectWordBackward();
    void moveSelectionByOffset(int32_t offset, CompletionHandler<void()>&&);
    void selectTextWithGranularityAtPoint(const WebCore::IntPoint&, WebCore::TextGranularity, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void selectPositionAtBoundaryWithDirection(const WebCore::IntPoint&, WebCore::TextGranularity, WebCore::SelectionDirection, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity, WebCore::SelectionDirection, CompletionHandler<void()>&&);
    void selectPositionAtPoint(const WebCore::IntPoint&, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void beginSelectionInDirection(WebCore::SelectionDirection, CompletionHandler<void(bool)>&&);
    void updateSelectionWithExtentPoint(const WebCore::IntPoint&, bool isInteractingWithFocusedElement, RespectSelectionAnchor, CompletionHandler<void(bool)>&&);
    void updateSelectionWithExtentPointAndBoundary(const WebCore::IntPoint&, WebCore::TextGranularity, bool isInteractingWithFocusedElement, TextInteractionSource, CompletionHandler<void(bool)>&&);
    void didReleaseAllTouchPoints();
    void clearSelectionAfterTappingSelectionHighlightIfNeeded(WebCore::FloatPoint location);
#if ENABLE(REVEAL)
    RevealItem revealItemForCurrentSelection();
    void requestRVItemInCurrentSelectedRange(CompletionHandler<void(const RevealItem&)>&&);
    void prepareSelectionForContextMenuWithLocationInView(WebCore::IntPoint, CompletionHandler<void(bool, const RevealItem&)>&&);
#endif
    void willInsertFinalDictationResult();
    void didInsertFinalDictationResult();
    bool shouldRemoveDictationAlternativesAfterEditing() const;
    void replaceDictatedText(const String& oldText, const String& newText);
    void replaceSelectedText(const String& oldText, const String& newText);
    void requestAutocorrectionData(const String& textForAutocorrection, CompletionHandler<void(WebAutocorrectionData)>&& reply);
    void applyAutocorrection(const String& correction, const String& originalText, bool isCandidate, CompletionHandler<void(const String&)>&&);
    void syncApplyAutocorrection(const String& correction, const String& originalText, bool isCandidate, CompletionHandler<void(bool)>&&);
    void handleAutocorrectionContextRequest();
    void preemptivelySendAutocorrectionContext();
    void requestPositionInformation(const InteractionInformationRequest&);
    void startInteractionWithElementContextOrPosition(std::optional<WebCore::ElementContext>&&, WebCore::IntPoint&&);
    void stopInteraction();
    void performActionOnElement(uint32_t action, const String& authorizationToken, CompletionHandler<void()>&&);
    void performActionOnElements(uint32_t action, const Vector<WebCore::ElementContext>& elements);
    void focusNextFocusedElement(bool isForward, CompletionHandler<void()>&&);
    void autofillLoginCredentials(const String&, const String&);
    void setFocusedElementValue(const WebCore::ElementContext&, const String&);
    void setFocusedElementSelectedIndex(const WebCore::ElementContext&, uint32_t index, bool allowMultipleSelection);
    void setIsShowingInputViewForFocusedElement(bool);
    bool isShowingInputViewForFocusedElement() const { return m_isShowingInputViewForFocusedElement; }
    void updateSelectionAppearance();
    void getSelectionContext(CompletionHandler<void(const String&, const String&, const String&)>&&);
    void handleTwoFingerTapAtPoint(const WebCore::IntPoint&, OptionSet<WebKit::WebEventModifier>, WebKit::TapIdentifier);
    void getRectsForGranularityWithSelectionOffset(WebCore::TextGranularity, int32_t, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&&);
    void getRectsAtSelectionOffsetWithText(int32_t, const String&, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&&);
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
    void requestDocumentEditingContext(WebKit::DocumentEditingContextRequest&&, CompletionHandler<void(WebKit::DocumentEditingContext&&)>&&);
    bool shouldAllowSingleClickToChangeSelection(WebCore::Node& targetNode, const WebCore::VisibleSelection& newSelection);
    bool shouldDrawVisuallyContiguousBidiSelection() const;
#endif // PLATFORM(IOS_FAMILY)

    void willChangeSelectionForAccessibility() { m_isChangingSelectionForAccessibility = true; }
    void didChangeSelectionForAccessibility() { m_isChangingSelectionForAccessibility = false; }

#if PLATFORM(IOS_FAMILY) && ENABLE(IOS_TOUCH_EVENTS)
    void dispatchAsynchronousTouchEvents(UniqueRef<Vector<TouchEventData, 1>>&&);
    void cancelAsynchronousTouchEvents(UniqueRef<Vector<TouchEventData, 1>>&&);
#endif

    bool hasRichlyEditableSelection() const;

    enum class LayerTreeFreezeReason {
        PageTransition                   = 1 << 0,
        BackgroundApplication            = 1 << 1,
        ProcessSuspended                 = 1 << 2,
        PageSuspended                    = 1 << 3,
        Printing                         = 1 << 4,
        ProcessSwap                      = 1 << 5,
        SwipeAnimation                   = 1 << 6,
        DocumentVisualUpdatesNotAllowed  = 1 << 7,
#if ENABLE(QUICKLOOK_FULLSCREEN)
        OutOfProcessFullscreen           = 1 << 8,
#endif
    };
    void freezeLayerTree(LayerTreeFreezeReason);
    void unfreezeLayerTree(LayerTreeFreezeReason);

    void updateFrameScrollingMode(WebCore::FrameIdentifier, WebCore::ScrollbarMode);

    void markLayersVolatile(CompletionHandler<void(bool)>&& completionHandler = { });
    void cancelMarkLayersVolatile();

    void swipeAnimationDidStart();
    void swipeAnimationDidEnd();

    NotificationPermissionRequestManager* notificationPermissionRequestManager();
    RefPtr<NotificationPermissionRequestManager> protectedNotificationPermissionRequestManager();

    void pageDidScroll();

#if ENABLE(CONTEXT_MENUS)
    WebContextMenu& contextMenu();
    Ref<WebContextMenu> protectedContextMenu();
    RefPtr<WebContextMenu> contextMenuAtPointInWindow(WebCore::FrameIdentifier, const WebCore::DoublePoint&);
#endif

    static bool canHandleRequest(const WebCore::ResourceRequest&);

    class SandboxExtensionTracker {
    public:
        ~SandboxExtensionTracker();

        void invalidate();

        void beginLoad(SandboxExtensionHandle&&);
        void beginReload(WebFrame*, SandboxExtensionHandle&&);
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
    void showEmojiPicker(WebCore::LocalFrame&);
#endif

    void didApplyStyle();
    void didScrollSelection();
    void didChangeSelection(WebCore::LocalFrame&);
    void didChangeContents();
    void discardedComposition(const WebCore::Document&);
    void canceledComposition();
    void didUpdateComposition();
    void didEndUserTriggeredSelectionChanges();

    void navigateServiceWorkerClient(WebCore::ScriptExecutionContextIdentifier, const URL&, CompletionHandler<void(WebCore::ScheduleLocationChangeResult)>&&);

#if PLATFORM(COCOA)
    enum class ShouldInitializeNSAccessibility : bool { No, Yes };
    void platformInitializeAccessibility(ShouldInitializeNSAccessibility);
    void registerUIProcessAccessibilityTokens(std::span<const uint8_t> elementToken, std::span<const uint8_t> windowToken);
    void registerRemoteFrameAccessibilityTokens(pid_t, std::span<const uint8_t>, WebCore::FrameIdentifier);
    WKAccessibilityWebPageObject* accessibilityRemoteObject();
    RetainPtr<WKAccessibilityWebPageObject> protectedAccessibilityRemoteObject();
    WebCore::IntPoint accessibilityRemoteFrameOffset();
    void createMockAccessibilityElement(pid_t);
#if ENABLE(ACCESSIBILITY_ISOLATED_TREE)
    void cacheAXPosition(const WebCore::FloatPoint&);
    void cacheAXSize(const WebCore::IntSize&);
    void setIsolatedTree(Ref<WebCore::AXIsolatedTree>&&);
#endif
    NSObject *accessibilityObjectForMainFramePlugin();
    bool shouldFallbackToWebContentAXObjectForMainFramePlugin() const;
    const WebCore::FloatPoint& accessibilityPosition() const { return m_accessibilityPosition; }

    void setTextAsync(const String&);
    void insertTextAsync(const String& text, const EditingRange& replacementRange, InsertTextOptions&&);
    void hasMarkedText(CompletionHandler<void(bool)>&&);
    void getMarkedRangeAsync(CompletionHandler<void(const EditingRange&)>&&);
    void getSelectedRangeAsync(CompletionHandler<void(const EditingRange&, const EditingRange&)>&&);
    void characterIndexForPointAsync(const WebCore::IntPoint&, CompletionHandler<void(uint64_t)>&&);
    void firstRectForCharacterRangeAsync(const EditingRange&, CompletionHandler<void(const WebCore::IntRect&, const EditingRange&)>&&);
    void setCompositionAsync(const String& text, const Vector<WebCore::CompositionUnderline>&, const Vector<WebCore::CompositionHighlight>&, const HashMap<String, Vector<WebCore::CharacterRange>>&, const EditingRange& selectionRange, const EditingRange& replacementRange);
    void setWritingSuggestion(const String& text, const EditingRange& selection);
    void confirmCompositionAsync();

    void readSelectionFromPasteboard(const String& pasteboardName, CompletionHandler<void(bool&&)>&&);
    void getStringSelectionForPasteboard(CompletionHandler<void(String&&)>&&);
    void getDataSelectionForPasteboard(const String pasteboardType, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
    void shouldDelayWindowOrderingEvent(const WebKit::WebMouseEvent&, CompletionHandler<void(bool)>&&);
    bool performNonEditingBehaviorForSelector(const String&, WebCore::KeyboardEvent*);

#if ENABLE(MULTI_REPRESENTATION_HEIC)
    void insertMultiRepresentationHEIC(std::span<const uint8_t>, const String&);
#endif

    void insertDictatedTextAsync(const String& text, const EditingRange& replacementRange, const Vector<WebCore::DictationAlternative>& dictationAlternativeLocations, InsertTextOptions&&);
    void addDictationAlternative(const String& text, WebCore::DictationContext, CompletionHandler<void(bool)>&&);
    void dictationAlternativesAtSelection(CompletionHandler<void(Vector<WebCore::DictationContext>&&)>&&);
    void clearDictationAlternatives(Vector<WebCore::DictationContext>&&);
#endif // PLATFORM(COCOA)

#if PLATFORM(MAC)
    void setCaretAnimatorType(WebCore::CaretAnimatorType);
    void setCaretBlinkingSuspended(bool);
    void attributedSubstringForCharacterRangeAsync(const EditingRange&, CompletionHandler<void(const WebCore::AttributedString&, const EditingRange&)>&&);
    void requestAcceptsFirstMouse(int eventNumber, const WebKit::WebMouseEvent&);
#endif

#if PLATFORM(COCOA)
    void replaceSelectionWithPasteboardData(const Vector<String>& types, std::span<const uint8_t>);
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    void replaceImageForRemoveBackground(const WebCore::ElementContext&, const Vector<String>& types, std::span<const uint8_t>);
#endif

    void setCompositionForTesting(const String& compositionString, uint64_t from, uint64_t length, bool suppressUnderline, const Vector<WebCore::CompositionHighlight>&, const HashMap<String, Vector<WebCore::CharacterRange>>&);
    bool hasCompositionForTesting();
    void confirmCompositionForTesting(const String& compositionString);
    String frameTextForTestingIncludingSubframes(bool includingSubframes);

#if PLATFORM(COCOA)
    bool isSpeaking() const;

    void performDictionaryLookupForSelection(WebCore::LocalFrame&, const WebCore::VisibleSelection&, WebCore::TextIndicatorPresentationTransition);
#endif

    bool isStoppingLoadingDueToProcessSwap() const { return m_isStoppingLoadingDueToProcessSwap; }

    bool isIOSurfaceLosslessCompressionEnabled() const;

    bool isSmartInsertDeleteEnabled();
    void setSmartInsertDeleteEnabled(bool);

    bool isWebTransportEnabled() const;

    bool isSelectTrailingWhitespaceEnabled() const;
    void setSelectTrailingWhitespaceEnabled(bool);

    void replaceSelectionWithText(WebCore::LocalFrame*, const String&);
    void clearSelection();
    void restoreSelectionInFocusedEditableElement();

#if ENABLE(DRAG_SUPPORT) && PLATFORM(GTK)
    void performDragControllerAction(DragControllerAction, const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragOperation> draggingSourceOperationMask, WebCore::SelectionData&&, OptionSet<WebCore::DragApplicationFlags>, CompletionHandler<void(std::optional<WebCore::DragOperation>, WebCore::DragHandlingMethod, bool, unsigned, WebCore::IntRect, WebCore::IntRect, std::optional<WebCore::RemoteUserInputEventData>)>&&);
#endif

#if ENABLE(DRAG_SUPPORT) && !PLATFORM(GTK)
    void performDragControllerAction(std::optional<WebCore::FrameIdentifier>, DragControllerAction, WebCore::DragData&&, CompletionHandler<void(std::optional<WebCore::DragOperation>, WebCore::DragHandlingMethod, bool, unsigned, WebCore::IntRect, WebCore::IntRect, std::optional<WebCore::RemoteUserInputEventData>)>&&);
    void performDragOperation(WebCore::DragData&&, SandboxExtensionHandle&&, Vector<SandboxExtensionHandle>&&, CompletionHandler<void(bool)>&&);
#endif

#if ENABLE(DRAG_SUPPORT)
    void dragEnded(std::optional<WebCore::FrameIdentifier>, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, OptionSet<WebCore::DragOperation>, CompletionHandler<void(std::optional<WebCore::RemoteUserInputEventData>)>&&);

    void willPerformLoadDragDestinationAction();
    void mayPerformUploadDragDestinationAction();

    void willStartDrag() { ASSERT(!m_isStartingDrag); m_isStartingDrag = true; }
    void didStartDrag();
    void dragCancelled();
    OptionSet<WebCore::DragSourceAction> allowedDragSourceActions() const { return m_allowedDragSourceActions; }
#if ENABLE(MODEL_PROCESS)
    void modelDragEnded(WebCore::NodeIdentifier);
#endif
#endif

#if ENABLE(MODEL_PROCESS)
    void requestInteractiveModelElementAtPoint(WebCore::IntPoint clientPosition);
    void stageModeSessionDidUpdate(std::optional<WebCore::NodeIdentifier>, const WebCore::TransformationMatrix&);
    void stageModeSessionDidEnd(std::optional<WebCore::NodeIdentifier>);
#endif

    void beginPrinting(WebCore::FrameIdentifier, const PrintInfo&);
    void beginPrintingDuringDOMPrintOperation(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo) { beginPrinting(frameID, printInfo); }
    void endPrinting(CompletionHandler<void()>&& = [] { });
    void endPrintingDuringDOMPrintOperation(CompletionHandler<void()>&& completionHandler) { endPrinting(WTFMove(completionHandler)); }
    void computePagesForPrinting(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&&);
    void computePagesForPrintingDuringDOMPrintOperation(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&& completionHandler) { computePagesForPrinting(frameID, printInfo, WTFMove(completionHandler)); }
    void computePagesForPrintingImpl(WebCore::FrameIdentifier, const PrintInfo&, Vector<WebCore::IntRect>& pageRects, double& totalScaleFactor, WebCore::FloatBoxExtent& computedMargin);
#if PLATFORM(COCOA)
    void drawToPDF(const std::optional<WebCore::FloatRect>&, bool allowTransparentBackground,  CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
    void drawRectToImage(WebCore::FrameIdentifier, const PrintInfo&, const WebCore::IntRect&, const WebCore::IntSize&, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&);
    void drawRectToImageDuringDOMPrintOperation(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, const WebCore::IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&& completionHandler) { drawRectToImage(frameID, printInfo, rect, imageSize, WTFMove(completionHandler)); }
    void drawPagesToPDF(WebCore::FrameIdentifier, const PrintInfo&, uint32_t first, uint32_t count, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
    void drawPagesToPDFDuringDOMPrintOperation(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&& completionHandler) { drawPagesToPDF(frameID, printInfo, first, count, WTFMove(completionHandler)); }
    void drawPagesToPDFImpl(WebCore::FrameIdentifier, const PrintInfo&, uint32_t first, uint32_t count, RefPtr<WebCore::SharedBuffer>& pdfPageData);

    void drawPrintContextPagesToGraphicsContext(WebCore::GraphicsContext&, const WebCore::FloatRect& pageRect, uint32_t first, uint32_t count);

    void drawPrintingRectToSnapshot(RemoteSnapshotIdentifier, WebCore::FrameIdentifier, const PrintInfo&, const WebCore::IntRect&, const WebCore::IntSize&, CompletionHandler<void(bool)>&&);
    void drawPrintingRectToSnapshotDuringDOMPrintOperation(RemoteSnapshotIdentifier snapshotIdentifier, WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, const WebCore::IntRect& rect, const WebCore::IntSize& imageSize, CompletionHandler<void(bool)>&& completionHandler) { drawPrintingRectToSnapshot(snapshotIdentifier, frameID, printInfo, rect, imageSize, WTFMove(completionHandler)); }
    void drawPrintingPagesToSnapshot(RemoteSnapshotIdentifier, WebCore::FrameIdentifier, const PrintInfo&, uint32_t first, uint32_t count, CompletionHandler<void(std::optional<WebCore::FloatSize>)>&&);
    void drawPrintingPagesToSnapshotDuringDOMPrintOperation(RemoteSnapshotIdentifier snapshotIdentifier, WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, uint32_t first, uint32_t count, CompletionHandler<void(std::optional<WebCore::FloatSize>)>&& completionHandler) { drawPrintingPagesToSnapshot(snapshotIdentifier, frameID, printInfo, first, count, WTFMove(completionHandler)); }
#endif

#if PLATFORM(IOS_FAMILY)
    void computePagesForPrintingiOS(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(uint64_t)>&&);
    void drawToPDFiOS(WebCore::FrameIdentifier, const PrintInfo&, uint64_t, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
    void drawToImage(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&)>&&);

    void drawPrintingPagesToSnapshotiOS(RemoteSnapshotIdentifier, WebCore::FrameIdentifier, const PrintInfo&, uint64_t, CompletionHandler<void(std::optional<WebCore::FloatSize>)>&&);
    void drawPrintingToSnapshotiOS(RemoteSnapshotIdentifier, WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(std::optional<WebCore::FloatSize>)>&&);
#endif

#if PLATFORM(GTK)
    void drawPagesForPrinting(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(std::optional<WebCore::SharedMemoryHandle>&&, WebCore::ResourceError&&)>&&);
    void drawPagesForPrintingDuringDOMPrintOperation(WebCore::FrameIdentifier frameID, const PrintInfo& printInfo, CompletionHandler<void(std::optional<WebCore::SharedMemoryHandle>&&, WebCore::ResourceError&&)>&& completionHandler) { drawPagesForPrinting(frameID, printInfo, WTFMove(completionHandler)); }
#endif

    // Starts the process of drawing the whole page to a snapshot.
    // The completion is either error or rect of the content. If the nullopt rect was passed in, the snapshot rect is resolved from the content.
    void drawToSnapshot(const std::optional<WebCore::FloatRect>&, bool allowTransparentBackground, RemoteSnapshotIdentifier, CompletionHandler<void(std::optional<WebCore::IntSize>)>&&);

    // Submessage for a frame delivered during web page snapshot draw.
    void drawFrameToSnapshot(WebCore::FrameIdentifier, const WebCore::IntRect&, RemoteSnapshotIdentifier, CompletionHandler<void(bool)>&&);

    void addResourceRequest(WebCore::ResourceLoaderIdentifier, const WebCore::ResourceRequest&, const WebCore::DocumentLoader*, WebCore::LocalFrame*);
    void removeResourceRequest(WebCore::ResourceLoaderIdentifier, WebCore::LocalFrame*);

    void setMediaVolume(float);
    void setMuted(WebCore::MediaProducerMutedStateFlags, CompletionHandler<void()>&&);
    void setMayStartMediaWhenInWindow(bool);
    void stopMediaCapture(WebCore::MediaProducerMediaCaptureKind, CompletionHandler<void()>&&);

    void updateMainFrameScrollOffsetPinning();

    bool mainFrameHasCustomContentProvider() const;

    void mainFrameDidLayout();

    bool canRunBeforeUnloadConfirmPanel() const { return m_canRunBeforeUnloadConfirmPanel; }
    void setCanRunBeforeUnloadConfirmPanel(bool canRunBeforeUnloadConfirmPanel) { m_canRunBeforeUnloadConfirmPanel = canRunBeforeUnloadConfirmPanel; }

    bool canRunModal() const { return m_canRunModal; }
    void setCanRunModal(bool canRunModal) { m_canRunModal = canRunModal; }

    void runModal();

    void setDeviceScaleFactor(float);
    float deviceScaleFactor() const;

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    void setIntrinsicDeviceScaleFactor(float f) { m_intrinsicDeviceScaleFactor = f; }
    float intrinsicDeviceScaleFactor() const { return m_intrinsicDeviceScaleFactor; }
#endif

    void updateRenderingWithForcedRepaintWithoutCallback();

    void unmarkAllMisspellings();
    void unmarkAllBadGrammar();

#if PLATFORM(COCOA)
    void handleAlternativeTextUIResult(const String&);
#endif

    void handleWheelEvent(WebCore::FrameIdentifier, const WebWheelEvent&, const OptionSet<WebCore::WheelEventProcessingSteps>&, std::optional<bool> willStartSwipe, CompletionHandler<void(std::optional<WebCore::ScrollingNodeID>, std::optional<WebCore::WheelScrollGestureState>, bool handled, std::optional<WebCore::RemoteUserInputEventData>)>&&);
    std::pair<WebCore::HandleUserInputEventResult, OptionSet<WebCore::EventHandling>> wheelEvent(const WebCore::FrameIdentifier&, const WebWheelEvent&, OptionSet<WebCore::WheelEventProcessingSteps>);

    void wheelEventHandlersChanged(bool);
    void recomputeShortCircuitHorizontalWheelEventsState();

#if ENABLE(MAC_GESTURE_EVENTS)
    void gestureEvent(WebCore::FrameIdentifier, const WebGestureEvent&, CompletionHandler<void(std::optional<WebEventType>, bool, std::optional<WebCore::RemoteUserInputEventData>)>&&);
#endif

#if PLATFORM(IOS_FAMILY)
    void setDeviceOrientation(WebCore::IntDegrees);
    void dynamicViewportSizeUpdate(const DynamicViewportSizeUpdate&);
    bool scaleWasSetByUIProcess() const { return m_scaleWasSetByUIProcess; }
    void willStartUserTriggeredZooming();
    void didEndUserTriggeredZooming();
    void applicationWillResignActive();
    void applicationDidEnterBackground(bool isSuspendedUnderLock);
    void applicationDidFinishSnapshottingAfterEnteringBackground();
    void applicationWillEnterForeground(bool isSuspendedUnderLock);
    void applicationDidBecomeActive();
    void applicationDidEnterBackgroundForMedia(bool isSuspendedUnderLock);
    void applicationWillEnterForegroundForMedia(bool isSuspendedUnderLock);
    void didFinishContentChangeObserving(WebCore::FrameIdentifier, WKContentChange);

    bool platformPrefersTextLegibilityBasedZoomScaling() const;

    void hardwareKeyboardAvailabilityChanged(HardwareKeyboardState);
    bool hardwareKeyboardIsAttached() const { return m_keyboardIsAttached; }

    void updateStringForFind(const String&);

    bool canShowWhileLocked() const;

    void shouldDismissKeyboardAfterTapAtPoint(WebCore::FloatPoint, CompletionHandler<void(bool)>&&);
#endif

    void processWillSuspend();
    void processDidResume();
    void didReceiveRemoteCommand(WebCore::PlatformMediaSessionRemoteControlCommandType, const WebCore::PlatformMediaSessionRemoteCommandArgument&);

#if PLATFORM(COCOA)
    void processSystemWillSleep() const;
    void processSystemDidWake() const;
#endif

#if ENABLE(META_VIEWPORT)
    void setViewportConfigurationViewLayoutSize(const WebCore::FloatSize&, double layoutSizeScaleFactorFromClient, double minimumEffectiveDeviceWidth);
    void setOverrideViewportArguments(const std::optional<WebCore::ViewportArguments>&);
    const WebCore::ViewportConfiguration& viewportConfiguration() const { return m_viewportConfiguration; }

    void setUseTestingViewportConfiguration(bool useTestingViewport) { m_useTestingViewportConfiguration = useTestingViewport; }
    bool isUsingTestingViewportConfiguration() const { return m_useTestingViewportConfiguration; }
#endif

#if ENABLE(UI_SIDE_COMPOSITING)
    std::optional<float> scaleFromUIProcess(const VisibleContentRectUpdateInfo&) const;
    void updateVisibleContentRects(const VisibleContentRectUpdateInfo&, MonotonicTime oldestTimestamp);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    Expected<bool, WebCore::RemoteFrameGeometryTransformer> dispatchTouchEvent(WebCore::FrameIdentifier, const WebTouchEvent&);
#endif

    bool shouldUseCustomContentProviderForResponse(const WebCore::ResourceResponse&);

#if PLATFORM(COCOA)
    bool pdfPluginEnabled() const { return m_pdfPluginEnabled; }
    void setPDFPluginEnabled(bool enabled) { m_pdfPluginEnabled = enabled; }

    bool selectionFlippingEnabled() const { return m_selectionFlippingEnabled; }
    void setSelectionFlippingEnabled(bool enabled) { m_selectionFlippingEnabled = enabled; }

    std::optional<double> dataDetectionReferenceDate() const { return m_dataDetectionReferenceDate; }
#endif

    bool mainFrameIsScrollable() const { return m_mainFrameIsScrollable; }

    void setAlwaysShowsHorizontalScroller(bool);
    void setAlwaysShowsVerticalScroller(bool);

    bool alwaysShowsHorizontalScroller() const { return m_alwaysShowsHorizontalScroller; };
    bool alwaysShowsVerticalScroller() const { return m_alwaysShowsVerticalScroller; };

    void scrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin);
    void setContentOffset(std::optional<int> x, std::optional<int> y, WebCore::ScrollIsAnimated);
    void scrollToEdge(WebCore::RectEdges<bool>, WebCore::ScrollIsAnimated);

    void setMinimumSizeForAutoLayout(const WebCore::IntSize&);
    WebCore::IntSize minimumSizeForAutoLayout() const { return m_minimumSizeForAutoLayout; }

    void setSizeToContentAutoSizeMaximumSize(const WebCore::IntSize&);
    WebCore::IntSize sizeToContentAutoSizeMaximumSize() const { return m_sizeToContentAutoSizeMaximumSize; }

    void setAutoSizingShouldExpandToViewHeight(bool shouldExpand);
    bool autoSizingShouldExpandToViewHeight() { return m_autoSizingShouldExpandToViewHeight; }

    void setViewportSizeForCSSViewportUnits(std::optional<WebCore::FloatSize>);
    std::optional<WebCore::FloatSize> viewportSizeForCSSViewportUnits() const { return m_viewportSizeForCSSViewportUnits; }

    bool canShowMIMEType(const String& MIMEType) const;
    bool canShowResponse(const WebCore::ResourceResponse&) const;

    void addTextCheckingRequest(TextCheckerRequestID, Ref<WebCore::TextCheckingRequest>&&);
    void didFinishCheckingText(TextCheckerRequestID, const Vector<WebCore::TextCheckingResult>&);
    void didCancelCheckingText(TextCheckerRequestID);

#if ENABLE(DATA_DETECTION)
    void setDataDetectionResults(NSArray *);
    void detectDataInAllFrames(OptionSet<WebCore::DataDetectorType>, CompletionHandler<void(DataDetectionResult&&)>&&);
    void removeDataDetectedLinks(CompletionHandler<void(DataDetectionResult&&)>&&);
    void handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&);
#endif

    unsigned extendIncrementalRenderingSuppression();
    void stopExtendingIncrementalRenderingSuppression(unsigned token);
    bool shouldExtendIncrementalRenderingSuppression() { return !m_activeRenderingSuppressionTokens.isEmpty(); }

    WebCore::ScrollPinningBehavior scrollPinningBehavior();
    void setScrollPinningBehavior(WebCore::ScrollPinningBehavior);

    std::optional<WebCore::ScrollbarOverlayStyle> scrollbarOverlayStyle() { return m_scrollbarOverlayStyle; }
    void setScrollbarOverlayStyle(std::optional<WebCore::ScrollbarOverlayStyle> scrollbarStyle);

    Ref<WebCore::DocumentLoader> createDocumentLoader(WebCore::LocalFrame&, WebCore::ResourceRequest&&, WebCore::SubstituteData&&);
    void updateCachedDocumentLoader(WebCore::DocumentLoader&, WebCore::LocalFrame&);

    void getBytecodeProfile(CompletionHandler<void(const String&)>&&);
    void getSamplingProfilerOutput(CompletionHandler<void(const String&)>&&);

#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)
    void handleTelephoneNumberClick(const String& number, const WebCore::IntPoint&, const WebCore::IntRect&);
    void handleSelectionServiceClick(WebCore::FrameIdentifier, WebCore::FrameSelection&, const Vector<String>& telephoneNumbers, const WebCore::IntPoint&);
    void handleImageServiceClick(WebCore::FrameIdentifier, const WebCore::IntPoint&, WebCore::Image&, WebCore::HTMLImageElement&);
    void handlePDFServiceClick(WebCore::FrameIdentifier, const WebCore::IntPoint&, WebCore::HTMLAttachmentElement&);
#endif

    void didChangeScrollOffsetForFrame(WebCore::LocalFrame&);

    void setMainFrameProgressCompleted(bool completed) { m_mainFrameProgressCompleted = completed; }
    bool shouldDispatchFakeMouseMoveEvents() const { return m_shouldDispatchFakeMouseMoveEvents; }

    void postMessage(const String& messageName, API::Object* messageBody);
    void postSynchronousMessageForTesting(const String& messageName, API::Object* messageBody, RefPtr<API::Object>& returnData);
    void postMessageIgnoringFullySynchronousMode(const String& messageName, API::Object* messageBody);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void setInputMethodState(WebCore::Element*);
#endif

    void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&);

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
    void setOrientationForMediaCapture(uint64_t rotation);
    void setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted);
    void triggerMockCaptureConfigurationChange(bool forCamera, bool forMicrophone, bool forDisplay);
#endif

    void addUserScript(String&& source, InjectedBundleScriptWorld&, WebCore::UserContentInjectedFrames = WebCore::UserContentInjectedFrames::InjectInAllFrames, WebCore::UserScriptInjectionTime = WebCore::UserScriptInjectionTime::DocumentStart, WebCore::UserContentMatchParentFrame = WebCore::UserContentMatchParentFrame::Never);
    void addUserStyleSheet(const String& source, WebCore::UserContentInjectedFrames);
    void removeAllUserContent();

    void dispatchDidReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>);

    void didRestoreScrollPosition();

    bool isControlledByAutomation() const;
    void setControlledByAutomation(bool);

    void connectInspector(Inspector::FrontendChannel::ConnectionType);
    void disconnectInspector();
    void sendMessageToTargetBackend(const String& message);

    void insertNewlineInQuotedContent();

#if USE(OS_STATE)
    WallTime loadCommitTime() const { return m_loadCommitTime; }
#endif

#if ENABLE(GAMEPAD)
    void gamepadActivity(const Vector<std::optional<GamepadData>>&, WebCore::EventMakesGamepadsVisible);
    void gamepadsRecentlyAccessed();
#if PLATFORM(VISION)
    void allowGamepadAccess();
#endif
#endif

#if ENABLE(POINTER_LOCK)
    void didAcquirePointerLock();
    void didNotAcquirePointerLock();
    void didLosePointerLock();
#endif

    void didGetLoadDecisionForIcon(bool decision, CallbackID, CompletionHandler<void(const IPC::SharedBufferReference&)>&&);

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    void didConcludeEditDrag();
    void didConcludeDrop();
#endif

    void didFinishLoadingImageForElement(WebCore::HTMLImageElement&);

#if ENABLE(MODEL_PROCESS)
    void setHasModelElement(bool);
#endif

    WebURLSchemeHandlerProxy* urlSchemeHandlerForScheme(StringView);
    void stopAllURLSchemeTasks();

    std::optional<double> cpuLimit() const { return m_cpuLimit; }

#if ENABLE(PDF_PLUGIN)
    static PluginView* focusedPluginViewForFrame(WebCore::LocalFrame&);
    static PluginView* pluginViewForFrame(WebCore::LocalFrame*);
    PluginView* mainFramePlugIn() const;
#endif

    void themeColorChanged() { m_pendingThemeColorChange = true; }
#if PLATFORM(MAC) || PLATFORM(WPE) || PLATFORM(GTK)
    void flushPendingThemeColorChange();
#endif

    void pageExtendedBackgroundColorDidChange() { m_pendingPageExtendedBackgroundColorChange = true; }
    void flushPendingPageExtendedBackgroundColorChange();

    void sampledPageTopColorChanged() { m_pendingSampledPageTopColorChange = true; }
    void flushPendingSampledPageTopColorChange();

#if ENABLE(WEB_PAGE_SPATIAL_BACKDROP)
    void spatialBackdropSourceChanged();
#endif

#if ENABLE(MODEL_ELEMENT_IMMERSIVE)
    void canEnterImmersiveElement(const WebCore::Element&, CompletionHandler<void(bool)>&&);
#endif

    void flushPendingEditorStateUpdate();

    void loadAndDecodeImage(WebCore::ResourceRequest&&, std::optional<WebCore::FloatSize> sizeConstraint, uint64_t, CompletionHandler<void(Expected<Ref<WebCore::ShareableBitmap>, WebCore::ResourceError>&&)>&&);
#if PLATFORM(COCOA)
    void getInformationFromImageData(const Vector<uint8_t>&, CompletionHandler<void(Expected<std::pair<String, Vector<WebCore::IntSize>>, WebCore::ImageDecodingError>&&)>&&);
    void createBitmapsFromImageData(Ref<WebCore::SharedBuffer>&&, const Vector<unsigned>&, CompletionHandler<void(Vector<Ref<WebCore::ShareableBitmap>>&&)>&&);
    void decodeImageData(Ref<WebCore::SharedBuffer>&&, std::optional<WebCore::FloatSize>, CompletionHandler<void(RefPtr<WebCore::ShareableBitmap>&&)>&&);
#endif

    void hasStorageAccess(WebCore::RegistrableDomain&& subFrameDomain, WebCore::RegistrableDomain&& topFrameDomain, WebFrame&, CompletionHandler<void(bool)>&&);
    void requestStorageAccess(WebCore::RegistrableDomain&& subFrameDomain, WebCore::RegistrableDomain&& topFrameDomain, WebFrame&, WebCore::StorageAccessScope, WebCore::HasOrShouldIgnoreUserGesture, CompletionHandler<void(WebCore::RequestStorageAccessResult)>&&);
    void setLoginStatus(WebCore::RegistrableDomain&&, WebCore::IsLoggedIn, CompletionHandler<void()>&&);
    void isLoggedIn(WebCore::RegistrableDomain&&, CompletionHandler<void(bool)>&&);
    bool hasPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain) const;
    void addDomainWithPageLevelStorageAccess(const WebCore::RegistrableDomain& topLevelDomain, const WebCore::RegistrableDomain& resourceDomain);
    void clearPageLevelStorageAccess();
    void revokeFrameSpecificStorageAccess();
    void wasLoadedWithDataTransferFromPrevalentResource();
    void didLoadFromRegistrableDomain(WebCore::RegistrableDomain&&);
    void clearLoadedSubresourceDomains();
    void getLoadedSubresourceDomains(CompletionHandler<void(Vector<WebCore::RegistrableDomain>)>&&);
    const HashSet<WebCore::RegistrableDomain>& loadedSubresourceDomains() const;

#if ENABLE(DEVICE_ORIENTATION)
    void shouldAllowDeviceOrientationAndMotionAccess(WebCore::FrameIdentifier, FrameInfoData&&, bool mayPrompt, CompletionHandler<void(WebCore::DeviceOrientationOrMotionPermissionState)>&&);
#endif

    void showShareSheet(WebCore::ShareDataWithParsedURL&&, CompletionHandler<void(bool)>&& callback);
    void showContactPicker(WebCore::ContactsRequestData&&, CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&);

#if HAVE(DIGITAL_CREDENTIALS_UI)
    void showDigitalCredentialsPicker(const WebCore::DigitalCredentialsRequestData&, CompletionHandler<void(Expected<WebCore::DigitalCredentialsResponseData, WebCore::ExceptionData>&&)>&&);
    void dismissDigitalCredentialsPicker(CompletionHandler<void(bool)>&&);
#endif

#if ENABLE(ATTACHMENT_ELEMENT)
    void insertAttachment(const String& identifier, std::optional<uint64_t>&& fileSize, const String& fileName, const String& contentType, CompletionHandler<void()>&&);
    void updateAttachmentAttributes(const String& identifier, std::optional<uint64_t>&& fileSize, const String& contentType, const String& fileName, const IPC::SharedBufferReference& associatedElementData, CompletionHandler<void()>&&);
    void updateAttachmentIcon(const String& identifier, std::optional<WebCore::ShareableBitmap::Handle>&& icon, const WebCore::FloatSize&);
    void requestAttachmentIcon(const String& identifier, const WebCore::FloatSize&);
#endif

#if ENABLE(APPLICATION_MANIFEST)
    void getApplicationManifest(CompletionHandler<void(const std::optional<WebCore::ApplicationManifest>&)>&&);
#endif

    void getTextFragmentMatch(CompletionHandler<void(const String&)>&&);

#if USE(WPE_RENDERER)
    UnixFileDescriptor hostFileDescriptor() const { return m_hostFileDescriptor.duplicate(); }
#endif

    void updateCurrentModifierState(OptionSet<WebCore::PlatformEventModifier> modifiers);

    inline UserContentControllerIdentifier userContentControllerIdentifier() const;

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    WebExtensionControllerProxy* webExtensionControllerProxy() const { return m_webExtensionController.get(); }
    RefPtr<WebExtensionControllerProxy> protectedWebExtensionControllerProxy() const;
#endif

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection() const { return m_userInterfaceLayoutDirection; }

    bool isSuspended() const { return m_isSuspended; }

    bool dispatchMessage(IPC::Connection&, IPC::Decoder&);

    template<typename T>
    SendSyncResult<T> sendSyncWithDelayedReply(T&& message, OptionSet<IPC::SendSyncOption> sendSyncOptions = { })
    {
        cancelCurrentInteractionInformationRequest();
        return sendSync(std::forward<T>(message), Seconds::infinity(), sendSyncOptions);
    }

    WebCore::DOMPasteAccessResponse requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, WebCore::FrameIdentifier, const String& originIdentifier);
    WebCore::IntRect rectForElementAtInteractionLocation() const;

    const std::optional<WebCore::Color>& backgroundColor() const { return m_backgroundColor; }

    void suspendAllMediaBuffering();
    void resumeAllMediaBuffering();

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

    RefPtr<WebCore::Element> elementForContext(const WebCore::ElementContext&) const;
    std::optional<WebCore::ElementContext> contextForElement(const WebCore::Element&) const;

    void startTextManipulations(Vector<WebCore::TextManipulationControllerExclusionRule>&&, bool includesSubframes, CompletionHandler<void()>&&);
    void completeTextManipulation(const Vector<WebCore::TextManipulationItem>&, CompletionHandler<void(const WebCore::TextManipulationControllerManipulationResult&)>&&);

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

    bool userIsInteracting() const { return m_userIsInteracting; }
    void setUserIsInteracting(bool userIsInteracting) { m_userIsInteracting = userIsInteracting; }

    static void adjustSettingsForLockdownMode(WebCore::Settings&, const WebPreferencesStore*);

#if PLATFORM(IOS_FAMILY)
    // This excludes layout overflow, includes borders.
    static WebCore::IntRect rootViewBounds(const WebCore::Node&);
    // These include layout overflow for overflow:visible elements, but exclude borders.
    static WebCore::IntRect absoluteInteractionBounds(const WebCore::Node&);
    static WebCore::IntRect rootViewInteractionBounds(const WebCore::Node&);

    InteractionInformationAtPosition positionInformation(const InteractionInformationRequest&);

    void setSceneIdentifier(String&&);
#endif // PLATFORM(IOS_FAMILY)

#if USE(QUICK_LOOK)
    void didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti);
    void didFinishLoadForQuickLookDocumentInMainFrame(const WebCore::FragmentedSharedBuffer&);
    void requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&&);
#endif

    const AtomString& overriddenMediaType() const { return m_overriddenMediaType; }
    void setOverriddenMediaType(const String&);

    void updateCORSDisablingPatterns(Vector<String>&&);

#if ENABLE(IPC_TESTING_API)
    bool ipcTestingAPIEnabled() const { return m_ipcTestingAPIEnabled; }
    uint64_t webPageProxyID() const { return messageSenderDestinationID(); }
    VisitedLinkTableIdentifier visitedLinkTableID() const { return m_visitedLinkTableID; }
#endif

    void getProcessDisplayName(CompletionHandler<void(String&&)>&&);

    WebCore::AllowsContentJavaScript allowsContentJavaScriptFromMostRecentNavigation() const { return m_allowsContentJavaScriptFromMostRecentNavigation; }
    void setAllowsContentJavaScriptFromMostRecentNavigation(WebCore::AllowsContentJavaScript allows) { m_allowsContentJavaScriptFromMostRecentNavigation = allows; }

#if ENABLE(APP_BOUND_DOMAINS)
    void notifyPageOfAppBoundBehavior();
    void setIsNavigatingToAppBoundDomain(std::optional<NavigatingToAppBoundDomain>, WebFrame&);
    bool needsInAppBrowserPrivacyQuirks() { return m_needsInAppBrowserPrivacyQuirks; }
#endif

#if ENABLE(MEDIA_USAGE)
    void addMediaUsageManagerSession(WebCore::MediaSessionIdentifier, const String&, const URL&);
    void updateMediaUsageManagerSessionState(WebCore::MediaSessionIdentifier, const WebCore::MediaUsageInfo&);
    void removeMediaUsageManagerSession(WebCore::MediaSessionIdentifier);
#endif

    void isPlayingMediaDidChange(WebCore::MediaProducerMediaStateFlags);

    std::pair<URL, WebCore::DidFilterLinkDecoration> applyLinkDecorationFilteringWithResult(const URL&, WebCore::LinkDecorationFilteringTrigger);
    URL applyLinkDecorationFiltering(const URL& url, WebCore::LinkDecorationFilteringTrigger trigger) { return applyLinkDecorationFilteringWithResult(url, trigger).first; }
    URL allowedQueryParametersForAdvancedPrivacyProtections(const URL&);

#if ENABLE(IMAGE_ANALYSIS)
    void requestTextRecognition(WebCore::Element&, WebCore::TextRecognitionOptions&&, CompletionHandler<void(RefPtr<WebCore::Element>&&)>&& = { });
    void updateWithTextRecognitionResult(const WebCore::TextRecognitionResult&, const WebCore::ElementContext&, const WebCore::FloatPoint& location, CompletionHandler<void(TextRecognitionUpdateResult)>&&);
    void startVisualTranslation(const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier);
#endif

    void requestImageBitmap(const WebCore::ElementContext&, CompletionHandler<void(std::optional<WebCore::ShareableBitmap::Handle>&&, const String& sourceMIMEType)>&&);

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
    void handleContextMenuTranslation(const WebCore::TranslationContextMenuInfo&);
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    void showMediaControlsContextMenu(WebCore::FloatRect&&, Vector<WebCore::MediaControlsContextMenuItem>&&, WebCore::HTMLMediaElementIdentifier, CompletionHandler<void(WebCore::MediaControlsContextMenuItem::ID)>&&);
#endif // ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    uint64_t nativeWindowHandle() { return m_nativeWindowHandle; }
#endif

    static void updatePreferencesGenerated(const WebPreferencesStore&);
    static void updateSettingsGenerated(const WebPreferencesStore&, WebCore::Settings&);

    void synchronizeCORSDisablingPatternsWithNetworkProcess();

#if ENABLE(GPU_PROCESS)
    void gpuProcessConnectionDidBecomeAvailable(GPUProcessConnection&);
    void gpuProcessConnectionWasDestroyed();
    RemoteRenderingBackendProxy& ensureRemoteRenderingBackendProxy();
    Ref<RemoteRenderingBackendProxy> ensureProtectedRemoteRenderingBackendProxy();
#endif

#if ENABLE(MODEL_PROCESS)
    void modelProcessConnectionDidBecomeAvailable(ModelProcessConnection&);
#endif

    void createTextFragmentDirectiveFromSelection(CompletionHandler<void(URL&&)>&&);
    void getTextFragmentRanges(CompletionHandler<void(const Vector<EditingRange>&&)>&&);

#if ENABLE(APP_HIGHLIGHTS)
    WebCore::CreateNewGroupForHighlight highlightIsNewGroup() const;
    WebCore::HighlightRequestOriginatedInApp highlightRequestOriginatedInApp() const;
    WebCore::HighlightVisibility appHighlightsVisiblility() const { return m_appHighlightsVisible; }

    void createAppHighlightInSelectedRange(WebCore::CreateNewGroupForHighlight, WebCore::HighlightRequestOriginatedInApp, CompletionHandler<void(WebCore::AppHighlight&&)>&&);
    void restoreAppHighlightsAndScrollToIndex(Vector<WebCore::SharedMemoryHandle>&&, const std::optional<unsigned> index);
    void setAppHighlightsVisibility(const WebCore::HighlightVisibility);
#endif

    void didAddOrRemoveViewportConstrainedObjects();

#if PLATFORM(IOS_FAMILY)
    void dispatchWheelEventWithoutScrolling(WebCore::FrameIdentifier, const WebWheelEvent&, CompletionHandler<void(bool)>&&);
#endif

#if ENABLE(PDF_PLUGIN)
    bool shouldUsePDFPlugin(const String& contentType, StringView path) const;
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    void createMediaSessionCoordinator(const String&, CompletionHandler<void(bool)>&&);
#endif

    void setLastNavigationWasAppInitiated(bool wasAppBound) { m_lastNavigationWasAppInitiated = wasAppBound; }
    void lastNavigationWasAppInitiated(CompletionHandler<void(bool)>&&);

    bool isParentProcessAWebBrowser() const;

#if ENABLE(TEXT_AUTOSIZING)
    void textAutosizingUsesIdempotentModeChanged();
#endif

#if ENABLE(META_VIEWPORT)
    double baseViewportLayoutSizeScaleFactor() const { return m_baseViewportLayoutSizeScaleFactor; }
#endif

#if ENABLE(WEBXR)
    PlatformXRSystemProxy& xrSystemProxy();
#endif

    void prepareToRunModalJavaScriptDialog();

#if ENABLE(ARKIT_INLINE_PREVIEW)
    bool useARKitForModel() const { return m_useARKitForModel; };
#endif
#if HAVE(SCENEKIT)
    bool useSceneKitForModel() const { return m_useSceneKitForModel; };
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    void modelInlinePreviewDidLoad(WebCore::PlatformLayerIdentifier);
    void modelInlinePreviewDidFailToLoad(WebCore::PlatformLayerIdentifier, const WebCore::ResourceError&);
#endif

#if ENABLE(IMAGE_ANALYSIS) && ENABLE(VIDEO)
    void beginTextRecognitionForVideoInElementFullScreen(const WebCore::HTMLVideoElement&);
    void cancelTextRecognitionForVideoInElementFullScreen();
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    void shouldAllowRemoveBackground(const WebCore::ElementContext&, CompletionHandler<void(bool)>&&) const;
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    void setIsWindowResizingEnabled(bool);
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    void setInteractionRegionsEnabled(bool);
#endif

    bool needsScrollGeometryUpdates() { return m_needsScrollGeometryUpdates; }
    void setNeedsScrollGeometryUpdates(bool needsUpdates) { m_needsScrollGeometryUpdates = needsUpdates; }

    void startDeferringResizeEvents();
    void flushDeferredResizeEvents();

    void startDeferringScrollEvents();
    void flushDeferredScrollEvents();

    void startDeferringIntersectionObservations();
    void flushDeferredIntersectionObservations();

    void flushDeferredDidReceiveMouseEvent();

    void generateTestReport(String&& message, String&& group);

    bool isUsingUISideCompositing() const;

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    void updateImageAnimationEnabled();
    void pauseAllAnimations(CompletionHandler<void()>&&);
    void playAllAnimations(CompletionHandler<void()>&&);
    void isAnyAnimationAllowedToPlayDidChange(bool /* anyAnimationCanPlay */);
#endif

#if ENABLE(ACCESSIBILITY_NON_BLINKING_CURSOR)
    void updatePrefersNonBlinkingCursor();
#endif

    bool shouldSkipDecidePolicyForResponse(const WebCore::ResourceResponse&) const;
    void setSkipDecidePolicyForResponseIfPossible(bool value) { m_skipDecidePolicyForResponseIfPossible = value; }

#if PLATFORM(IOS_FAMILY)
    bool isInStableState() const { return m_isInStableState; }
#endif

    WebCore::FloatSize screenSizeForFingerprintingProtections(const WebCore::LocalFrame&, WebCore::FloatSize defaultSize) const;

    const Logger& logger() const;
    uint64_t logIdentifier() const;

#if PLATFORM(GTK) || PLATFORM(WPE)
#if USE(GBM)
    const Vector<RendererBufferFormat>& preferredBufferFormats() const { return m_preferredBufferFormats; }
#endif
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    const String& mediaEnvironment() const { return m_mediaEnvironment; }
    void setMediaEnvironment(const String&);
#endif

#if ENABLE(WRITING_TOOLS)
    void proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(const WebCore::WritingTools::TextSuggestionID&, WebCore::IntRect);

    void proofreadingSessionUpdateStateForSuggestionWithID(WebCore::WritingTools::TextSuggestionState, const WebCore::WritingTools::TextSuggestionID&);

    void addTextAnimationForAnimationID(const WTF::UUID&, const WebCore::TextAnimationData&, const RefPtr<WebCore::TextIndicator>, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& = { });

    void removeTextAnimationForAnimationID(const WTF::UUID&);

    void removeInitialTextAnimationForActiveWritingToolsSession();
    void addInitialTextAnimationForActiveWritingToolsSession();
    void addSourceTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, bool finished, const WebCore::CharacterRange&, const String&, CompletionHandler<void(WebCore::TextAnimationRunMode)>&&);
    void addDestinationTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, const std::optional<WebCore::CharacterRange>&, const String&);
    void saveSnapshotOfTextPlaceholderForAnimation(const WebCore::SimpleRange&);
    void clearAnimationsForActiveWritingToolsSession();

    void createTextIndicatorForTextAnimationID(const WTF::UUID&, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&&);

    void didEndPartialIntelligenceTextAnimation();
#endif

#if PLATFORM(COCOA)
    void createTextIndicatorForElementWithID(const String& elementID, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&&);
#endif

    void startObservingNowPlayingMetadata();
    void stopObservingNowPlayingMetadata();

    void didAdjustVisibilityWithSelectors(Vector<String>&&);

    void takeSnapshotForTargetedElement(WebCore::NodeIdentifier, WebCore::ScriptExecutionContextIdentifier, CompletionHandler<void(std::optional<WebCore::ShareableBitmapHandle>&&)>&&);

    void hasActiveNowPlayingSessionChanged(bool);

    OptionSet<LayerTreeFreezeReason> layerTreeFreezeReasons() const { return m_layerTreeFreezeReasons; }

#if ENABLE(CONTEXT_MENUS)
    void showContextMenuFromFrame(const FrameInfoData&, const ContextMenuContextData&, const UserData&);
#endif
    void loadRequest(LoadParameters&&);

    void setObscuredContentInsets(const WebCore::FloatBoxExtent&);

    void updateOpener(WebCore::FrameIdentifier, WebCore::FrameIdentifier);
    void setFramePrinting(WebCore::FrameIdentifier, bool printing, WebCore::FloatSize pageSize, WebCore::FloatSize originalPageSize, float maximumShrinkRatio, WebCore::AdjustViewSize shouldAdjustViewSize);

    WebHistoryItemClient& historyItemClient() const { return m_historyItemClient.get(); }

    bool isAlwaysOnLoggingAllowed() const;

    void callAfterPendingSyntheticClick(CompletionHandler<void(WebCore::SyntheticClickResult)>&&);

    void didDispatchClickEvent(const WebCore::PlatformMouseEvent&, WebCore::Node&);

    bool isClosed() const { return m_isClosed; }

#if HAVE(AUDIT_TOKEN)
    void setPresentingApplicationAuditTokenAndBundleIdentifier(CoreIPCAuditToken&&, String&& bundleIdentifier);
#endif

    void didProgrammaticallyClearTextFormControl(const WebCore::HTMLTextFormControlElement&);

#if USE(UICONTEXTMENU)
    void willBeginContextMenuInteraction();
    void didEndContextMenuInteraction();
    bool hasActiveContextMenuInteraction() const { return m_hasActiveContextMenuInteraction; }
#endif

    bool shouldSendConsoleLogsToUIProcessForTesting() const { return m_shouldSendConsoleLogsToUIProcessForTesting; }

    void setNeedsFixedContainerEdgesUpdate() { m_needsFixedContainerEdgesUpdate = true; }

    RefPtr<WebCore::MediaSessionManagerInterface> mediaSessionManager() const;
    WebCore::MediaSessionManagerInterface* mediaSessionManagerIfExists() const;

#if ENABLE(MODEL_ELEMENT)
    bool shouldDisableModelLoadDelaysForTesting() const;
#endif

    std::unique_ptr<FrameInfoData> takeMainFrameNavigationInitiator();

#if PLATFORM(MAC)
    void setOverflowHeightForTopScrollEdgeEffect(double value) { m_overflowHeightForTopScrollEdgeEffect = value; }
#endif

    bool statusBarIsVisible() const { return m_statusBarIsVisible; }
    void setStatusBarIsVisible(bool visible) { m_statusBarIsVisible = visible; }
    bool menuBarIsVisible() const { return m_menuBarIsVisible; }
    void setMenuBarIsVisible(bool visible) { m_menuBarIsVisible = visible; }
    bool toolbarsAreVisible() const { return m_toolbarsAreVisible; }
    void setToolbarsAreVisible(bool visible) { m_toolbarsAreVisible = visible; }

    RefPtr<WebCore::ShareableBitmap> shareableBitmapSnapshotForNode(WebCore::Node&);

    void paintRemoteFrameContents(WebCore::FrameIdentifier, const WebCore::IntRect&, WebCore::GraphicsContext&);

#if ENABLE(VIDEO)
    void showCaptionDisplaySettingsPreview(WebCore::HTMLMediaElementIdentifier);
    void hideCaptionDisplaySettingsPreview(WebCore::HTMLMediaElementIdentifier);
#endif

private:
    WebPage(WebCore::PageIdentifier, WebPageCreationParameters&&);

    void constructFrameTree(WebFrame& parent, const FrameTreeCreationParameters&);

    void updateThrottleState();

    // IPC::MessageSender
    IPC::Connection* messageSenderConnection() const override;
    uint64_t messageSenderDestinationID() const override;

    void platformInitialize(const WebPageCreationParameters&);
    void platformDetach();
    void getPlatformEditorState(WebCore::LocalFrame&, EditorState&) const;
    bool requiresPostLayoutDataForEditorState(const WebCore::LocalFrame&) const;
    void platformWillPerformEditingCommand();
    void sendEditorStateUpdate();

    void getPlatformEditorStateCommon(const WebCore::LocalFrame&, EditorState&) const;

    void updateSizeForCSSDefaultViewportUnits();
    void updateSizeForCSSSmallViewportUnits();
    void updateSizeForCSSLargeViewportUnits();

    RefPtr<WebCore::LocalFrame> localRootFrame(std::optional<WebCore::FrameIdentifier>);

#if PLATFORM(IOS_FAMILY)
    std::optional<FocusedElementInformation> focusedElementInformation();
    void generateSyntheticEditingCommand(SyntheticEditingCommandType);
    void handleSyntheticClick(std::optional<WebCore::FrameIdentifier>, WebCore::Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebKit::WebEventModifier>, WebCore::PointerID = WebCore::mousePointerID);
    void completeSyntheticClick(std::optional<WebCore::FrameIdentifier>, WebCore::Node& nodeRespondingToClick, const WebCore::FloatPoint& location, OptionSet<WebKit::WebEventModifier>, WebCore::SyntheticClickType, WebCore::PointerID = WebCore::mousePointerID);
    void sendTapHighlightForNodeIfNecessary(WebKit::TapIdentifier, WebCore::Node*, WebCore::FloatPoint);
    WebCore::VisiblePosition visiblePositionInFocusedNodeForPoint(const WebCore::LocalFrame&, const WebCore::IntPoint&, bool isInteractingWithFocusedElement);
    std::optional<WebCore::SimpleRange> rangeForGranularityAtPoint(WebCore::LocalFrame&, const WebCore::IntPoint&, WebCore::TextGranularity, bool isInteractingWithFocusedElement);
    void updateFocusBeforeSelectingTextAtLocation(const WebCore::IntPoint&);
    void setSelectedRangeDispatchingSyntheticMouseEventsIfNeeded(const WebCore::SimpleRange&, WebCore::Affinity);
    void dispatchSyntheticMouseEventsForSelectionGesture(SelectionTouch, const WebCore::IntPoint&);
    void invokePendingSyntheticClickCallback(WebCore::SyntheticClickResult);
    void resetLastSelectedReplacementRangeIfNeeded();

    void sendPositionInformation(InteractionInformationAtPosition&&);
    WebAutocorrectionContext autocorrectionContext();
    bool applyAutocorrectionInternal(const String& correction, const String& originalText, bool isCandidate);
    void clearSelectionAfterTapIfNeeded();
    void scheduleLayoutViewportHeightExpansionUpdate();
    void scheduleEditorStateUpdateAfterAnimationIfNeeded(const WebCore::Element&);
    void computeEnclosingLayerID(EditorState&, const WebCore::VisibleSelection&) const;
    bool mainFramePlugInDefersScalingToViewport() const;
    bool mainFramePlugInRejectsZeroViewLayoutSizeUpdates() const;

    void addTextInteractionSources(OptionSet<TextInteractionSource>);
    void removeTextInteractionSources(OptionSet<TextInteractionSource>);
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    void setLinkDecorationFilteringData(Vector<WebCore::LinkDecorationFilteringData>&&);
    void setAllowedQueryParametersForAdvancedPrivacyProtections(Vector<WebCore::LinkDecorationFilteringData>&&);
#endif

#if ENABLE(META_VIEWPORT)
    void resetViewportDefaultConfiguration(WebFrame* mainFrame, bool hasMobileDocType = false);
    enum class ZoomToInitialScale : bool { No, Yes };
    void viewportConfigurationChanged(ZoomToInitialScale = ZoomToInitialScale::No);
    bool shouldIgnoreMetaViewport() const;
#endif

#if ENABLE(TEXT_AUTOSIZING)
    void textAutoSizingAdjustmentTimerFired();
    void resetIdempotentTextAutosizingIfNeeded(double previousInitialScale);
    void updateTextAutosizingEnablementFromInitialScale(double);
#endif
    void resetTextAutosizing();

#if ENABLE(VIEWPORT_RESIZING)
    void shrinkToFitContent(ZoomToInitialScale = ZoomToInitialScale::No);
#endif

#if PLATFORM(IOS_FAMILY)
    void updateLayoutViewportHeightExpansionTimerFired();
#endif

    void addReasonsToDisallowLayoutViewportHeightExpansion(OptionSet<DisallowLayoutViewportHeightExpansionReason>);
    void removeReasonsToDisallowLayoutViewportHeightExpansion(OptionSet<DisallowLayoutViewportHeightExpansionReason>);

#if PLATFORM(IOS_FAMILY) && ENABLE(DRAG_SUPPORT)
    Awaitable<DragInitiationResult> requestDragStart(std::optional<WebCore::FrameIdentifier>, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask);
    Awaitable<DragInitiationResult> requestAdditionalItemsForDragSession(std::optional<WebCore::FrameIdentifier>, WebCore::IntPoint clientPosition, WebCore::IntPoint globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask);
    void insertDroppedImagePlaceholders(const Vector<WebCore::IntSize>&, CompletionHandler<void(const Vector<WebCore::IntRect>&, std::optional<WebCore::TextIndicatorData>)>&& reply);
    void computeAndSendEditDragSnapshot();
#endif

#if !PLATFORM(COCOA) && !PLATFORM(WPE)
    static const char* interpretKeyEvent(const WebCore::KeyboardEvent*);
#endif

    bool handleKeyEventByRelinquishingFocusToChrome(const WebCore::KeyboardEvent&);

#if PLATFORM(MAC)
    bool executeKeypressCommandsInternal(const Vector<WebCore::KeypressCommand>&, WebCore::KeyboardEvent*);
#endif

    void testProcessIncomingSyncMessagesWhenWaitingForSyncReply(CompletionHandler<void(bool)>&&);

    void updateDrawingAreaLayerTreeFreezeState();
    void updateAfterDrawingAreaCreation(const WebPageCreationParameters&);

    enum class MarkLayersVolatileDontRetryReason : uint8_t { None, SuspendedUnderLock, TimedOut };
    void markLayersVolatileOrRetry(MarkLayersVolatileDontRetryReason);
    void layerVolatilityTimerFired();
    void callVolatilityCompletionHandlers(bool succeeded);

    void tryMarkLayersVolatile(CompletionHandler<void(bool)>&&);
    void tryMarkLayersVolatileCompletionHandler(MarkLayersVolatileDontRetryReason, bool didSucceed);

    String sourceForFrame(WebFrame*);

    void startTextManipulationForFrame(WebCore::Frame&);

    void loadDataImpl(std::optional<WebCore::NavigationIdentifier>, WebCore::ShouldTreatAsContinuingLoad, std::optional<WebsitePoliciesData>&&, Ref<WebCore::FragmentedSharedBuffer>&&, WebCore::ResourceRequest&&, WebCore::ResourceResponse&&, URL&& failingURL, const UserData&, std::optional<NavigatingToAppBoundDomain>, WebCore::SubstituteData::SessionHistoryVisibility, WebCore::ShouldOpenExternalURLsPolicy = WebCore::ShouldOpenExternalURLsPolicy::ShouldNotAllow);

    // Actions
    void tryClose(CompletionHandler<void(bool)>&&);
    void platformDidReceiveLoadParameters(const LoadParameters&);
    void createProvisionalFrame(ProvisionalFrameCreationParameters&&);
    void loadDidCommitInAnotherProcess(WebCore::FrameIdentifier, std::optional<WebCore::LayerHostingContextIdentifier>);
    [[noreturn]] void loadRequestWaitingForProcessLaunch(LoadParameters&&, URL&&, WebPageProxyIdentifier, bool);
    void loadData(LoadParameters&&);
    void loadAlternateHTML(LoadParameters&&);
    void loadSimulatedRequestAndResponse(LoadParameters&&, WebCore::ResourceResponse&&);
    void navigateToPDFLinkWithSimulatedClick(const String& url, WebCore::IntPoint documentPoint, WebCore::IntPoint screenPoint);
    void getPDFFirstPageSize(WebCore::FrameIdentifier, CompletionHandler<void(WebCore::FloatSize)>&&);
    void reload(WebCore::NavigationIdentifier, OptionSet<WebCore::ReloadOption> reloadOptions, SandboxExtensionHandle&&);
    void goToBackForwardItem(GoToBackForwardItemParameters&&);
    [[noreturn]] void goToBackForwardItemWaitingForProcessLaunch(GoToBackForwardItemParameters&&, WebKit::WebPageProxyIdentifier);
    void tryRestoreScrollPosition();
    void setInitialFocus(bool forward, bool isKeyboardEventValid, const std::optional<WebKeyboardEvent>&, CompletionHandler<void()>&&);
    void updateIsInWindow(bool isInitialState = false);
    void visibilityDidChange();
    void windowActivityDidChange();
    void setActivityState(OptionSet<WebCore::ActivityState>, ActivityStateChangeID, CompletionHandler<void()>&&);
    void validateCommand(const String&, CompletionHandler<void(bool, int32_t)>&&);
    void executeEditCommand(const String&, const String&);
    void setEditable(bool);

    void didChangeSelectionOrOverflowScrollPosition();

    void increaseListLevel();
    void decreaseListLevel();
    void changeListType();

    void setBaseWritingDirection(WebCore::WritingDirection);

    void setNeedsFontAttributes(bool);

    void mouseEvent(WebCore::FrameIdentifier, const WebMouseEvent&, std::optional<Vector<SandboxExtensionHandle>>&& sandboxExtensions);
    void keyEvent(WebCore::FrameIdentifier, const WebKeyboardEvent&);

    void setLastKnownMousePosition(WebCore::FrameIdentifier, WebCore::IntPoint eventPoint, WebCore::IntPoint globalPoint);

#if ENABLE(IOS_TOUCH_EVENTS)
    void touchEventSync(const WebTouchEvent&, CompletionHandler<void(bool)>&&);
    void didBeginTouchPoint(WebCore::FloatPoint locationInRootView);
    void updatePotentialTapSecurityOrigin(const WebTouchEvent&, bool wasHandled);
#elif ENABLE(TOUCH_EVENTS)
    void touchEvent(const WebTouchEvent&, CompletionHandler<void(std::optional<WebEventType>, bool)>&&);
#endif

    void cancelPointer(WebCore::PointerID, const WebCore::IntPoint&);
    void touchWithIdentifierWasRemoved(WebCore::PointerID);

#if ENABLE(CONTEXT_MENUS)
    void didDismissContextMenu();
#endif
#if ENABLE(CONTEXT_MENU_EVENT)
    void contextMenuForKeyEvent();
#endif

    static bool scroll(WebCore::Page*, WebCore::ScrollDirection, WebCore::ScrollGranularity);
    static bool logicalScroll(WebCore::Page*, WebCore::ScrollLogicalDirection, WebCore::ScrollGranularity);

    void loadURLInFrame(URL&&, const String& referrer, WebCore::FrameIdentifier);
    void loadDataInFrame(std::span<const uint8_t>, String&& MIMEType, String&& encodingName, URL&& baseURL, WebCore::FrameIdentifier);

    void didRemoveBackForwardItem(WebCore::BackForwardItemIdentifier);
    void setCurrentHistoryItemForReattach(Ref<FrameState>&&);

    void requestFontAttributesAtSelectionStart(CompletionHandler<void(const WebCore::FontAttributes&)>&&);

#if ENABLE(REMOTE_INSPECTOR)
    void setIndicating(bool);
#endif

    void setBackgroundColor(const std::optional<WebCore::Color>&);

#if PLATFORM(COCOA)
    void setObscuredContentInsetsFenced(const WebCore::FloatBoxExtent&, const WTF::MachSendRight&);
#endif

    void viewWillStartLiveResize();
    void viewWillEndLiveResize();

    void getContentsAsString(ContentAsStringIncludesChildFrames, CompletionHandler<void(const String&)>&&);
#if PLATFORM(COCOA)
    void getContentsAsAttributedString(CompletionHandler<void(const WebCore::AttributedString&)>&&);
#endif
#if ENABLE(MHTML)
    void getContentsAsMHTMLData(CompletionHandler<void(const IPC::SharedBufferReference&)>&& callback);
#endif
    void getMainResourceDataOfFrame(WebCore::FrameIdentifier, CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&&);
    void getResourceDataFromFrame(WebCore::FrameIdentifier, const String& resourceURL, CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&&);
    void getRenderTreeExternalRepresentation(CompletionHandler<void(const String&)>&&);
    void getSelectionOrContentsAsString(CompletionHandler<void(const String&)>&&);
    void getSourceForFrame(WebCore::FrameIdentifier, CompletionHandler<void(const String&)>&&);
#if PLATFORM(COCOA)
    void getWebArchiveData(CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&&);
    void getWebArchivesForFrames(const Vector<WebCore::FrameIdentifier>& frameIdentifiers, CompletionHandler<void(HashMap<WebCore::FrameIdentifier, Ref<WebCore::LegacyWebArchive>>&&)>&&);
#endif
    void getWebArchiveOfFrameWithFileName(WebCore::FrameIdentifier, const Vector<WebCore::MarkupExclusionRule>&, const String& fileName, CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&&);
    void runJavaScript(WebFrame*, RunJavaScriptParameters&&, ContentWorldIdentifier, bool, CompletionHandler<void(Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>)>&&);
    void runJavaScriptInFrameInScriptWorld(RunJavaScriptParameters&&, std::optional<WebCore::FrameIdentifier>, const ContentWorldData&, bool, CompletionHandler<void(Expected<JavaScriptEvaluationResult, std::optional<WebCore::ExceptionDetails>>)>&&);
    void getAccessibilityTreeData(CompletionHandler<void(const std::optional<IPC::SharedBufferReference>&)>&&);
    void updateRenderingWithForcedRepaint(CompletionHandler<void()>&&);
    void takeSnapshot(WebCore::IntRect snapshotRect, WebCore::IntSize bitmapSize, SnapshotOptions, CompletionHandler<void(std::optional<ImageBufferBackendHandle>&&, WebCore::Headroom)>&&);

    void preferencesDidChange(const WebPreferencesStore&, std::optional<uint64_t> sharedPreferencesVersion);
    void preferencesDidChangeDuringDOMPrintOperation(const WebPreferencesStore& store, std::optional<uint64_t> sharedPreferencesVersion) { preferencesDidChange(store, sharedPreferencesVersion); }
    void updatePreferences(const WebPreferencesStore&);

#if PLATFORM(IOS_FAMILY)
    bool parentProcessHasServiceWorkerEntitlement() const;
    void disableServiceWorkerEntitlement();
    void clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&&);
#else
    bool parentProcessHasServiceWorkerEntitlement() const { return true; }
    void disableServiceWorkerEntitlement() { }
    void clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&& completionHandler) { completionHandler(); }
#endif

    void setUserAgent(String&&);
    void setHasCustomUserAgent(bool);
    void setCustomTextEncodingName(const String&);
    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();

    void suspend(CompletionHandler<void(bool)>&&);
    void resume(CompletionHandler<void(bool)>&&);

#if PLATFORM(COCOA)
    void performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    void performDictionaryLookupForRange(WebCore::LocalFrame&, const WebCore::SimpleRange&, WebCore::TextIndicatorPresentationTransition);
    WebCore::DictionaryPopupInfo dictionaryPopupInfoForRange(WebCore::LocalFrame&, const WebCore::SimpleRange&, WebCore::TextIndicatorPresentationTransition);

    void windowAndViewFramesChanged(const ViewWindowCoordinates&);

    RetainPtr<PDFDocument> pdfDocumentForPrintingFrame(WebCore::LocalFrame*);
    void computePagesForPrintingPDFDocument(WebCore::FrameIdentifier, const PrintInfo&, Vector<WebCore::IntRect>& resultPageRects);
    void drawPDFDocument(CGContextRef, PDFDocument *, const PrintInfo&, const WebCore::IntRect&);
    void drawPagesToPDFFromPDFDocument(WebCore::GraphicsContext&, PDFDocument *, const PrintInfo&, const WebCore::FloatRect&, uint32_t first, uint32_t count);
#endif

    void endPrintingImmediately();

#if ENABLE(META_VIEWPORT)
    bool shouldEnableViewportBehaviorsForResizableWindows() const;
#endif

#if HAVE(APP_ACCENT_COLORS)
    void setAccentColor(WebCore::Color);
#if PLATFORM(MAC)
    void setAppUsesCustomAccentColor(bool);
    bool appUsesCustomAccentColor();
#endif
#endif

    void setMainFrameIsScrollable(bool);

    void unapplyEditCommand(WebUndoStepID commandID);
    void reapplyEditCommand(WebUndoStepID commandID);
    void didRemoveEditCommand(WebUndoStepID commandID);

    void updateLastNodeBeforeWritingSuggestions(const WebCore::KeyboardEvent&);

    void findString(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount, CompletionHandler<void(std::optional<WebCore::FrameIdentifier>, Vector<WebCore::IntRect>&&, uint32_t, int32_t, bool)>&&);
#if ENABLE(IMAGE_ANALYSIS)
    void findStringIncludingImages(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount, CompletionHandler<void(std::optional<WebCore::FrameIdentifier>, Vector<WebCore::IntRect>&&, uint32_t, int32_t, bool)>&&);
#endif
    void findStringMatches(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount, CompletionHandler<void(Vector<Vector<WebCore::IntRect>>, int32_t)>&&);
    void getImageForFindMatch(uint32_t matchIndex);
    void selectFindMatch(uint32_t matchIndex);
    void indicateFindMatch(uint32_t matchIndex);
    void hideFindUI();
    void countStringMatches(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount, CompletionHandler<void(uint32_t)>&&);
    void replaceMatches(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly, CompletionHandler<void(uint64_t)>&&);

    void findTextRangesForStringMatches(const String&, OptionSet<FindOptions>, uint32_t maxMatchCount, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&&);
    void replaceFoundTextRangeWithString(const WebFoundTextRange&, const String&);
    void decorateTextRangeWithStyle(const WebFoundTextRange&, WebKit::FindDecorationStyle);
    void scrollTextRangeToVisible(const WebFoundTextRange&);
    void clearAllDecoratedFoundText();
    void didBeginTextSearchOperation();

    void requestRectForFoundTextRange(const WebFoundTextRange&, CompletionHandler<void(WebCore::FloatRect)>&&);
    void addLayerForFindOverlay(CompletionHandler<void(std::optional<WebCore::PlatformLayerIdentifier>)>&&);
    void removeLayerForFindOverlay(CompletionHandler<void()>&&);

    void didChangeSelectedIndexForActivePopupMenu(int32_t newIndex);
    void setTextForActivePopupMenu(int32_t index);

#if PLATFORM(GTK)
    void failedToShowPopupMenu();
#endif

    void didChooseFilesForOpenPanel(const Vector<String>& files, const Vector<String>& replacementFiles);
    void didCancelForOpenPanel();

#if PLATFORM(IOS_FAMILY)
    void didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>&, const String& displayString, std::span<const uint8_t> iconData);
#endif

#if ENABLE(SANDBOX_EXTENSIONS)
    void extendSandboxForFilesFromOpenPanel(Vector<SandboxExtensionHandle>&&);
#endif

    void didReceiveGeolocationPermissionDecision(GeolocationIdentifier, const String& authorizationToken);

#if ENABLE(MEDIA_STREAM)
    void userMediaAccessWasGranted(WebCore::UserMediaRequestIdentifier, WebCore::CaptureDevice&& audioDeviceUID, WebCore::CaptureDevice&& videoDeviceUID, WebCore::MediaDeviceHashSalts&& mediaDeviceIdentifierHashSalt, Vector<SandboxExtensionHandle>&&, CompletionHandler<void()>&&);
    void userMediaAccessWasDenied(WebCore::UserMediaRequestIdentifier, uint64_t reason, String&& message, WebCore::MediaConstraintType);
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    void mediaKeySystemWasGranted(WebCore::MediaKeySystemRequestIdentifier, String&& mediaKeysHashSalt);
    void mediaKeySystemWasDenied(WebCore::MediaKeySystemRequestIdentifier, String&& message);
#endif

    void requestMediaPlaybackState(CompletionHandler<void(WebKit::MediaPlaybackState)>&&);

    void pauseAllMediaPlayback(CompletionHandler<void()>&&);
    void suspendAllMediaPlayback(CompletionHandler<void()>&&);
    void resumeAllMediaPlayback(CompletionHandler<void()>&&);

    void advanceToNextMisspelling(bool startBeforeSelection);
    void changeSpellingToWord(const String& word);

#if USE(APPKIT)
    void uppercaseWord(WebCore::FrameIdentifier);
    void lowercaseWord(WebCore::FrameIdentifier);
    void capitalizeWord(WebCore::FrameIdentifier);
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

    void reportUsedFeatures();

    void updateWebsitePolicies(WebsitePoliciesData&&);

    void changeFont(WebCore::FontChanges&&);
    void changeFontAttributes(WebCore::FontAttributeChanges&&);

#if PLATFORM(MAC)
    void performImmediateActionHitTestAtLocation(WebCore::FrameIdentifier, WebCore::FloatPoint);
    std::optional<WebCore::SimpleRange> lookupTextAtLocation(WebCore::FrameIdentifier, WebCore::FloatPoint);
    void immediateActionDidUpdate();
    void immediateActionDidCancel();
    void immediateActionDidComplete();

    void dataDetectorsDidPresentUI(WebCore::PageOverlay::PageOverlayID);
    void dataDetectorsDidChangeUI(WebCore::PageOverlay::PageOverlayID);
    void dataDetectorsDidHideUI(WebCore::PageOverlay::PageOverlayID);

    void handleAcceptedCandidate(WebCore::TextCheckingResult);
#endif

    void performHitTestForMouseEvent(const WebMouseEvent&, CompletionHandler<void(WebHitTestResultData&&, OptionSet<WebEventModifier>)>&&);

#if PLATFORM(COCOA)
    void requestActiveNowPlayingSessionInfo(CompletionHandler<void(bool, WebCore::NowPlayingInfo&&)>&&);
    RetainPtr<NSData> accessibilityRemoteTokenData() const;
    void accessibilityTransferRemoteToken(RetainPtr<NSData>);
#endif

    void setShouldDispatchFakeMouseMoveEvents(bool dispatch) { m_shouldDispatchFakeMouseMoveEvents = dispatch; }

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    void playbackTargetSelected(WebCore::PlaybackTargetClientContextIdentifier, MediaPlaybackTargetContextSerialized&&) const;
    void playbackTargetAvailabilityDidChange(WebCore::PlaybackTargetClientContextIdentifier, bool);
    void setShouldPlayToPlaybackTarget(WebCore::PlaybackTargetClientContextIdentifier, bool);
    void playbackTargetPickerWasDismissed(WebCore::PlaybackTargetClientContextIdentifier);
#endif

    void setShouldScaleViewToFitDocument(bool);

    void pageStoppedScrolling();

    void setUserInterfaceLayoutDirection(uint32_t);

    void simulateDeviceOrientationChange(double alpha, double beta, double gamma);

#if USE(SYSTEM_PREVIEW)
    void systemPreviewActionTriggered(WebCore::SystemPreviewInfo, const String& message);
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    void speakingErrorOccurred();
    void boundaryEventOccurred(bool wordBoundary, unsigned charIndex, unsigned charLength);
    void voicesDidChange();
#endif

    void registerURLSchemeHandler(WebURLSchemeHandlerIdentifier, const String& scheme);

    void urlSchemeTaskWillPerformRedirection(WebURLSchemeHandlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&, CompletionHandler<void(WebCore::ResourceRequest&&)>&&);
    void urlSchemeTaskDidPerformRedirection(WebURLSchemeHandlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    void urlSchemeTaskDidReceiveResponse(WebURLSchemeHandlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, WebCore::ResourceResponse&&);
    void urlSchemeTaskDidReceiveData(WebURLSchemeHandlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, Ref<WebCore::SharedBuffer>&&);
    void urlSchemeTaskDidComplete(WebURLSchemeHandlerIdentifier, WebCore::ResourceLoaderIdentifier taskIdentifier, const WebCore::ResourceError&);

    void setIsTakingSnapshotsForApplicationSuspension(bool);
    void setNeedsDOMWindowResizeEvent();

    void setIsSuspended(bool, CompletionHandler<void(std::optional<bool>)>&&);

    RefPtr<WebImage> snapshotAtSize(const WebCore::IntRect&, const WebCore::IntSize& bitmapSize, SnapshotOptions, WebCore::LocalFrame&, WebCore::LocalFrameView&);
    RefPtr<WebImage> snapshotNode(WebCore::Node&, SnapshotOptions, unsigned maximumPixelCount = std::numeric_limits<unsigned>::max());

    void drawMainFrameToPDF(WebCore::LocalFrame&, WebCore::GraphicsContext&, WebCore::IntRect& snapshotRect, bool allowTransparentBackground);
    void pdfSnapshotAtSize(WebCore::LocalFrame&, WebCore::GraphicsContext&, const WebCore::IntRect& snapshotRect, SnapshotOptions);

#if ENABLE(ATTACHMENT_ELEMENT)
    RefPtr<WebCore::HTMLAttachmentElement> attachmentElementWithIdentifier(const String& identifier) const;
#endif

    bool canShowMIMEType(const String&, NOESCAPE const Function<bool(const String&, WebCore::PluginData::AllowedPluginTypes)>& supportsPlugin) const;

    void cancelCurrentInteractionInformationRequest();

    bool shouldDispatchUpdateAfterFocusingElement(const WebCore::Element&) const;

    void updateMockAccessibilityElementAfterCommittingLoad();

    void paintSnapshotAtSize(const WebCore::IntRect&, const WebCore::IntSize&, SnapshotOptions, WebCore::LocalFrame&, WebCore::LocalFrameView&, WebCore::GraphicsContext&);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void sendMessageToWebProcessExtension(UserMessage&&);
    void sendMessageToWebProcessExtensionWithReply(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&);

    void getRenderProcessInfo(CompletionHandler<void(RenderProcessInfo&&)>&&);
#endif

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange(Vector<RendererBufferFormat>&&);
#endif

    void platformDidScalePage();

    Vector<Ref<SandboxExtension>> consumeSandboxExtensions(Vector<SandboxExtensionHandle>&&);
    void revokeSandboxExtensions(Vector<Ref<SandboxExtension>>& sandboxExtensions);

    void setSelectionRange(const WebCore::IntPoint&, WebCore::TextGranularity, bool);

    bool hasPendingEditorStateUpdate() const;
    bool shouldAvoidComputingPostLayoutDataForEditorState() const;

    void useRedirectionForCurrentNavigation(WebCore::ResourceResponse&&);

    void dispatchLoadEventToFrameOwnerElement(WebCore::FrameIdentifier);

    void elementWasFocusedInAnotherProcess(WebCore::FrameIdentifier, WebCore::FocusOptions);
    void frameWasFocusedInAnotherProcess(std::optional<WebCore::FrameIdentifier>&&);

#if ENABLE(WRITING_TOOLS)
    void willBeginWritingToolsSession(const std::optional<WebCore::WritingTools::Session>&, CompletionHandler<void(const Vector<WebCore::WritingTools::Context>&)>&&);

    void didBeginWritingToolsSession(const WebCore::WritingTools::Session&, const Vector<WebCore::WritingTools::Context>&);

    void proofreadingSessionDidReceiveSuggestions(const WebCore::WritingTools::Session&, const Vector<WebCore::WritingTools::TextSuggestion>&, const WebCore::CharacterRange&, const WebCore::WritingTools::Context&, bool finished, CompletionHandler<void()>&&);

    void proofreadingSessionDidUpdateStateForSuggestion(const WebCore::WritingTools::Session&, WebCore::WritingTools::TextSuggestionState, const WebCore::WritingTools::TextSuggestion&, const WebCore::WritingTools::Context&);

    void willEndWritingToolsSession(const WebCore::WritingTools::Session&, bool accepted, CompletionHandler<void()>&&);

    void didEndWritingToolsSession(const WebCore::WritingTools::Session&, bool accepted);

    void compositionSessionDidReceiveTextWithReplacementRange(const WebCore::WritingTools::Session&, const WebCore::AttributedString&, const WebCore::CharacterRange&, const WebCore::WritingTools::Context&, bool finished, CompletionHandler<void()>&&);

    void writingToolsSessionDidReceiveAction(const WebCore::WritingTools::Session&, WebCore::WritingTools::Action);

    void proofreadingSessionSuggestionTextRectsInRootViewCoordinates(const WebCore::CharacterRange&, CompletionHandler<void(Vector<WebCore::FloatRect>&&)>&&) const;
    void updateTextVisibilityForActiveWritingToolsSession(const WebCore::CharacterRange&, bool, const WTF::UUID&, CompletionHandler<void()>&&);
    void textPreviewDataForActiveWritingToolsSession(const WebCore::CharacterRange&, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&&);
    void decorateTextReplacementsForActiveWritingToolsSession(const WebCore::CharacterRange&, CompletionHandler<void()>&&);
    void setSelectionForActiveWritingToolsSession(const WebCore::CharacterRange&, CompletionHandler<void()>&&);

    // Old animation system methods:

    void updateUnderlyingTextVisibilityForTextAnimationID(const WTF::UUID&, bool, CompletionHandler<void()>&&);

    void intelligenceTextAnimationsDidComplete();
#endif

    void remotePostMessage(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData>&& targetOrigin, const WebCore::MessageWithMessagePorts&);
    void renderTreeAsTextForTesting(WebCore::FrameIdentifier, uint64_t baseIndent, OptionSet<WebCore::RenderAsTextFlag>, CompletionHandler<void(String&&)>&&);
    void layerTreeAsTextForTesting(WebCore::FrameIdentifier, uint64_t baseIndent, OptionSet<WebCore::LayerTreeAsTextOptions>, CompletionHandler<void(String&&)>&&);
    void frameTextForTesting(WebCore::FrameIdentifier, CompletionHandler<void(String&&)>&&);
    void bindRemoteAccessibilityFrames(int processIdentifier, WebCore::FrameIdentifier, Vector<uint8_t>, CompletionHandler<void(Vector<uint8_t>, int)>&&);
    void updateRemotePageAccessibilityOffset(WebCore::FrameIdentifier, WebCore::IntPoint);
    void resolveAccessibilityHitTestForTesting(WebCore::FrameIdentifier, const WebCore::IntPoint&, CompletionHandler<void(String)>&&);

    void requestAllTextAndRects(CompletionHandler<void(Vector<std::pair<String, WebCore::FloatRect>>&&)>&&);

    void requestTargetedElement(WebCore::TargetedElementRequest&&, CompletionHandler<void(Vector<WebCore::TargetedElementInfo>&&)>&&);
    void requestAllTargetableElements(float, CompletionHandler<void(Vector<Vector<WebCore::TargetedElementInfo>>&&)>&&);

    void requestTextExtraction(WebCore::TextExtraction::Request&&, CompletionHandler<void(WebCore::TextExtraction::Item&&)>&&);
    void handleTextExtractionInteraction(WebCore::TextExtraction::Interaction&&, CompletionHandler<void(bool, String&&)>&&);
    void describeTextExtractionInteraction(WebCore::TextExtraction::Interaction&&, CompletionHandler<void(WebCore::TextExtraction::InteractionDescription&&)>&&);
    void takeSnapshotOfExtractedText(WebCore::TextExtraction::ExtractedText&&, CompletionHandler<void(RefPtr<WebCore::TextIndicator>&&)>&&);
    void updateTextExtractionFilterRules(Vector<WebCore::TextExtraction::FilterRuleData>&&);
    void applyTextExtractionFilter(const String& input, std::optional<WebCore::NodeIdentifier>&& containerNode, CompletionHandler<void(const String&)>&&);

#if HAVE(SANDBOX_STATE_FLAGS)
    static void setHasLaunchedWebContentProcess();
#endif

    template<typename T> T contentsToRootView(WebCore::FrameIdentifier, T);
    template<typename T> T rootViewToContents(WebCore::FrameIdentifier, T);
    void contentsToRootViewRect(WebCore::FrameIdentifier, WebCore::FloatRect, CompletionHandler<void(WebCore::FloatRect)>&&);
    void contentsToRootViewPoint(WebCore::FrameIdentifier, WebCore::FloatPoint, CompletionHandler<void(WebCore::FloatPoint)>&&);
    void remoteDictionaryPopupInfoToRootView(WebCore::FrameIdentifier, WebCore::DictionaryPopupInfo, CompletionHandler<void(WebCore::DictionaryPopupInfo)>&&);

    void hitTestAtPoint(WebCore::FrameIdentifier, WebCore::FloatPoint, CompletionHandler<void(NodeHitTestResult)>&&);

    void resetVisibilityAdjustmentsForTargetedElements(const Vector<std::pair<WebCore::NodeIdentifier, WebCore::ScriptExecutionContextIdentifier>>&, CompletionHandler<void(bool)>&&);
    void adjustVisibilityForTargetedElements(Vector<WebCore::TargetedElementAdjustment>&&, CompletionHandler<void(bool)>&&);
    void numberOfVisibilityAdjustmentRects(CompletionHandler<void(uint64_t)>&&);

#if HAVE(SPATIAL_TRACKING_LABEL)
    void setDefaultSpatialTrackingLabel(const String&);
#endif

#if PLATFORM(COCOA)
    WebCore::BoxSideSet sidesRequiringFixedContainerEdges() const;
#endif

    void frameNameWasChangedInAnotherProcess(WebCore::FrameIdentifier, const String& frameName);

    WebPageInspectorTarget& ensureInspectorTarget();

    struct Internals;
    const UniqueRef<Internals> m_internals;

    const WebCore::PageIdentifier m_identifier;

    RefPtr<WebCore::Page> m_page;

    WebCore::IntSize m_viewSize;
    RefPtr<DrawingArea> m_drawingArea;

    RefPtr<WebPageTesting> m_webPageTesting;

    const Ref<WebFrame> m_mainFrame;

    RefPtr<WebPageGroupProxy> m_pageGroup;

    String m_userAgent;
    bool m_hasCustomUserAgent { false };

#if ENABLE(TILED_CA_DRAWING_AREA)
    DrawingAreaType m_drawingAreaType;
#endif

    HashMap<TextCheckerRequestID, RefPtr<WebCore::TextCheckingRequest>> m_pendingTextCheckingRequestMap;

    WebCore::FloatSize m_defaultUnobscuredSize;
    WebCore::FloatSize m_minimumUnobscuredSize;
    WebCore::FloatSize m_maximumUnobscuredSize;

    WebCore::Color m_underlayColor;

#if ENABLE(PDF_PLUGIN)
    SingleThreadWeakHashSet<PluginView> m_pluginViews;
#endif
#if ENABLE(PDF_HUD)
    HashMap<PDFPluginIdentifier, WeakPtr<PDFPluginBase>> m_pdfPlugInsWithHUD;
#endif
#if ENABLE(PDF_PAGE_NUMBER_INDICATOR)
    std::pair<Markable<PDFPluginIdentifier>, WeakPtr<PDFPluginBase>> m_pdfPlugInWithPageNumberIndicator;
#endif

    WTF::Function<void()> m_selectionChangedHandler;

    bool m_useFixedLayout { false };
    bool m_isInRedo { false };
    bool m_isClosed { false };
    bool m_tabToLinks { false };

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
    const bool m_shouldSendConsoleLogsToUIProcessForTesting { false };

#if PLATFORM(COCOA)
    bool m_pdfPluginEnabled { false };
    bool m_hasCachedWindowFrame { false };
    bool m_selectionFlippingEnabled { false };

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
    const UniqueRef<TextCheckingControllerProxy> m_textCheckingControllerProxy;
#endif

#if PLATFORM(COCOA) || PLATFORM(GTK)
    RefPtr<ViewGestureGeometryCollector> m_viewGestureGeometryCollector;
#endif

#if PLATFORM(COCOA)
    std::optional<double> m_dataDetectionReferenceDate;
#endif

#if USE(ATSPI)
    RefPtr<WebCore::AccessibilityRootAtspi> m_accessibilityRootObject;
#endif

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    float m_intrinsicDeviceScaleFactor { 1 };
    uint64_t m_nativeWindowHandle { 0 };
#endif

#if !PLATFORM(IOS_FAMILY)
    RefPtr<PageBanner> m_headerBanner;
    RefPtr<PageBanner> m_footerBanner;
#endif

    RunLoop::Timer m_setCanStartMediaTimer;
    bool m_mayStartMediaWhenInWindow { false };

    HashMap<WebUndoStepID, RefPtr<WebUndoStep>> m_undoStepMap;

#if ENABLE(CONTEXT_MENUS)
    std::unique_ptr<API::InjectedBundle::PageContextMenuClient> m_contextMenuClient;
#endif
    std::unique_ptr<API::InjectedBundle::EditorClient> m_editorClient;
    std::unique_ptr<API::InjectedBundle::FormClient> m_formClient;
    std::unique_ptr<API::InjectedBundle::PageLoaderClient> m_loaderClient;
    std::unique_ptr<API::InjectedBundle::ResourceLoadClient> m_resourceLoadClient;
    std::unique_ptr<API::InjectedBundle::PageUIClient> m_uiClient;

    const UniqueRef<FindController> m_findController;

    const UniqueRef<WebFoundTextRangeController> m_foundTextRangeController;

    RefPtr<WebInspectorBackend> m_inspector;
    RefPtr<WebInspectorUI> m_inspectorUI;
    RefPtr<RemoteWebInspectorUI> m_remoteInspectorUI;
    std::unique_ptr<WebPageInspectorTarget> m_inspectorTarget;

#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr<PlaybackSessionManager> m_playbackSessionManager;
    RefPtr<VideoPresentationManager> m_videoPresentationManager;
#endif

#if PLATFORM(IOS_FAMILY)
    bool m_allowsMediaDocumentInlinePlayback { false };
    std::optional<WebCore::SimpleRange> m_startingGestureRange;
#endif

#if ENABLE(FULLSCREEN_API)
    RefPtr<WebFullScreenManager> m_fullScreenManager;
    IsInFullscreenMode m_isInFullscreenMode { IsInFullscreenMode::No };
#endif

    RefPtr<WebPopupMenu> m_activePopupMenu;

#if ENABLE(CONTEXT_MENUS)
    RefPtr<WebContextMenu> m_contextMenu;
#endif

    WeakPtr<WebColorChooser> m_activeColorChooser;

    WeakPtr<WebDataListSuggestionPicker> m_activeDataListSuggestionPicker;

    WeakPtr<WebDateTimeChooser> m_activeDateTimeChooser;

    RefPtr<WebOpenPanelResultListener> m_activeOpenPanelResultListener;
    RefPtr<NotificationPermissionRequestManager> m_notificationPermissionRequestManager;

    const Ref<WebUserContentController> m_userContentController;

#if ENABLE(WK_WEB_EXTENSIONS) && PLATFORM(COCOA)
    RefPtr<WebExtensionControllerProxy> m_webExtensionController;
#endif

    const UniqueRef<WebScreenOrientationManager> m_screenOrientationManager;

#if ENABLE(GEOLOCATION)
    const UniqueRef<GeolocationPermissionRequestManager> m_geolocationPermissionRequestManager;
#endif

#if ENABLE(MEDIA_STREAM)
    const UniqueRef<UserMediaPermissionRequestManager> m_userMediaPermissionRequestManager;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    const UniqueRef<MediaKeySystemPermissionRequestManager> m_mediaKeySystemPermissionRequestManager;
#endif

    RefPtr<WebCore::PrintContext> m_printContext;
    bool m_inActivePrintContextAccessScope { false };
    bool m_shouldEndPrintingImmediately { false };

    class PrintContextAccessScope {
    public:
        PrintContextAccessScope(WebPage& webPage)
            : m_webPage { webPage }
            , m_wasInActivePrintContextAccessScope { webPage.m_inActivePrintContextAccessScope }
        {
            m_webPage->m_inActivePrintContextAccessScope = true;
        }

        ~PrintContextAccessScope()
        {
            m_webPage->m_inActivePrintContextAccessScope = m_wasInActivePrintContextAccessScope;
            if (!m_wasInActivePrintContextAccessScope && m_webPage->m_shouldEndPrintingImmediately)
                m_webPage->endPrintingImmediately();
        }
    private:
        const Ref<WebPage> m_webPage;
        const bool m_wasInActivePrintContextAccessScope;
    };

    friend class PrintContextAccessScope;

#if PLATFORM(GTK)
    std::unique_ptr<WebPrintOperationGtk> m_printOperation;
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
    std::optional<WebCore::SimpleRange> m_rangeForDropSnapshot;
#endif

    WebCore::RectEdges<bool> m_cachedMainFramePinnedState { true, true, true, true };
    bool m_canShortCircuitHorizontalWheelEvents { false };
    bool m_hasWheelEventHandlers { false };

    unsigned m_cachedPageCount { 0 };

    struct DeferredDidReceiveMouseEvent {
        std::optional<WebEventType> type;
        bool handled { false };
    };
    std::optional<DeferredDidReceiveMouseEvent> m_deferredDidReceiveMouseEvent;

    HashMap<WebCore::FrameIdentifier, HashSet<WebCore::ResourceLoaderIdentifier>> m_networkResourceRequestIdentifiersForPageLoadTiming;
    HashSet<WebCore::ResourceLoaderIdentifier> m_trackedNetworkResourceRequestIdentifiers;

    WebCore::IntSize m_minimumSizeForAutoLayout;
    WebCore::IntSize m_sizeToContentAutoSizeMaximumSize;
    bool m_autoSizingShouldExpandToViewHeight { false };
    std::optional<WebCore::FloatSize> m_viewportSizeForCSSViewportUnits;

    bool m_userIsInteracting { false };
    bool m_hasEverDisplayedContextMenu { false };

    enum class UserInteractionFlag : uint8_t {
        FocusedElement      = 1 << 0,
        SelectedRange       = 1 << 1,
    };
    OptionSet<UserInteractionFlag> m_userInteractionsSincePageTransition;
#if HAVE(TOUCH_BAR)
    bool m_requiresUserActionForEditingControlsManager { false };
    bool m_isTouchBarUpdateSuppressedForHiddenContentEditable { false };
    bool m_isNeverRichlyEditableForTouchBar { false };
#endif
    OptionSet<WebCore::ActivityState> m_lastActivityStateChanges;

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    bool m_isWindowResizingEnabled { false };
#endif

#if PLATFORM(MAC)
    double m_overflowHeightForTopScrollEdgeEffect { 0 };
#endif

    bool m_needsScrollGeometryUpdates { false };

    RefPtr<WebCore::Element> m_focusedElement;
    RefPtr<WebCore::Element> m_recentlyBlurredElement;
    bool m_hasPendingInputContextUpdateAfterBlurringAndRefocusingElement { false };
    bool m_pendingThemeColorChange { false };
    bool m_pendingPageExtendedBackgroundColorChange { false };
    bool m_pendingSampledPageTopColorChange { false };

    enum class PendingEditorStateUpdateStatus : uint8_t {
        NotScheduled,
        Scheduled,
        ScheduledDuringAccessibilitySelectionChange,
    };
    PendingEditorStateUpdateStatus m_pendingEditorStateUpdateStatus { PendingEditorStateUpdateStatus::NotScheduled };
    bool m_needsEditorStateVisualDataUpdate { false };

#if ENABLE(META_VIEWPORT)
    WebCore::ViewportConfiguration m_viewportConfiguration;
    double m_baseViewportLayoutSizeScaleFactor { 1 };
    bool m_useTestingViewportConfiguration { false };
    bool m_forceAlwaysUserScalable { false };
#endif

#if PLATFORM(IOS_FAMILY)
    std::optional<WebCore::SimpleRange> m_currentWordRange;
    RefPtr<WebCore::Node> m_interactionNode;
    WebCore::DoublePoint m_lastInteractionLocation;

    bool m_isShowingInputViewForFocusedElement { false };
    bool m_wasShowingInputViewForFocusedElementDuringLastPotentialTap { false };
    bool m_completingSyntheticClick { false };
    bool m_hasHandledSyntheticClick { false };

    enum class SelectionAnchor : bool { Start, End };
    SelectionAnchor m_selectionAnchor { SelectionAnchor::Start };

    enum class BidiSelectionFlippingState : uint8_t {
        NotFlipping,
        FlippingToStart,
        FlippingToEnd
    };
    BidiSelectionFlippingState m_bidiSelectionFlippingState { BidiSelectionFlippingState::NotFlipping };

    RefPtr<WebCore::Node> m_potentialTapNode;
    WebCore::FloatPoint m_potentialTapLocation;
    RefPtr<WebCore::SecurityOrigin> m_potentialTapSecurityOrigin;
    CompletionHandler<void(WebCore::SyntheticClickResult)> m_pendingSyntheticClickCallback;

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
    WebCore::FloatSize m_overrideAvailableScreenSize;

    std::optional<WebCore::SimpleRange> m_initialSelection;
    std::optional<WebCore::WeakSimpleRange> m_lastSelectedReplacementRange;
    WebCore::IntDegrees m_deviceOrientation { 0 };
    bool m_keyboardIsAttached { false };
    bool m_inDynamicSizeUpdate { false };
    RefPtr<WebCore::Node> m_pendingSyntheticClickNode;
    WebCore::FloatPoint m_pendingSyntheticClickLocation;
    WebCore::FloatRect m_previousExposedContentRect;
    OptionSet<WebKit::WebEventModifier> m_pendingSyntheticClickModifiers;
    WebCore::PointerID m_pendingSyntheticClickPointerId { 0 };
    std::optional<DynamicViewportSizeUpdateID> m_pendingDynamicViewportSizeUpdateID;
    double m_lastTransactionPageScaleFactor { 0 };

    WebCore::DeferrableOneShotTimer m_updateFocusedElementInformationTimer;

    CompletionHandler<void(InteractionInformationAtPosition&&)> m_pendingSynchronousPositionInformationReply;
    bool m_sendAutocorrectionContextAfterFocusingElement { false };
    std::unique_ptr<WebCore::IgnoreSelectionChangeForScope> m_ignoreSelectionChangeScopeForDictation;

    bool m_isMobileDoctype { false };
    bool m_hasAnyActiveTouchPoints { false };
    OptionSet<TextInteractionSource> m_activeTextInteractionSources;
    std::optional<WebCore::FloatPoint> m_lastTouchLocationBeforeTap;
#endif // PLATFORM(IOS_FAMILY)

#if USE(UICONTEXTMENU)
    bool m_hasActiveContextMenuInteraction { false };
#endif

    WebCore::Timer m_layerVolatilityTimer;
    Seconds m_layerVolatilityTimerInterval;
    Vector<CompletionHandler<void(bool)>> m_markLayersAsVolatileCompletionHandlers;
    bool m_isSuspendedUnderLock { false };

    HashSet<String, ASCIICaseInsensitiveHash> m_mimeTypesWithCustomContentProviders;
    std::optional<WebCore::Color> m_backgroundColor { WebCore::Color::white };

    HashSet<unsigned> m_activeRenderingSuppressionTokens;
    unsigned m_maximumRenderingSuppressionToken { 0 };

    std::optional<WebCore::ScrollbarOverlayStyle> m_scrollbarOverlayStyle;

    bool m_useAsyncScrolling { false };

    OptionSet<WebCore::ActivityState> m_activityState;

    bool m_isAppNapEnabled { true };

    Markable<WebCore::NavigationIdentifier> m_pendingNavigationID;

    bool m_mainFrameProgressCompleted { false };
    bool m_shouldDispatchFakeMouseMoveEvents { true };
    bool m_isSelectingTextWhileInsertingAsynchronously { false };
    bool m_isChangingSelectionForAccessibility { false };

    enum class EditorStateIsContentEditable { No, Yes, Unset };
    mutable EditorStateIsContentEditable m_lastEditorStateWasContentEditable { EditorStateIsContentEditable::Unset };

#if PLATFORM(GTK) || PLATFORM(WPE)
    std::optional<InputMethodState> m_inputMethodState;
#endif

#if USE(OS_STATE)
    WallTime m_loadCommitTime;
#endif

    WebCore::UserInterfaceLayoutDirection m_userInterfaceLayoutDirection;

    const String m_overrideContentSecurityPolicy;
    const std::optional<double> m_cpuLimit;

#if USE(WPE_RENDERER)
    UnixFileDescriptor m_hostFileDescriptor;
#endif

    HashMap<String, RefPtr<WebURLSchemeHandlerProxy>> m_schemeToURLSchemeHandlerProxyMap;
    HashMap<WebURLSchemeHandlerIdentifier, WeakRef<WebURLSchemeHandlerProxy>> m_identifierToURLSchemeHandlerProxyMap;

    HashMap<uint64_t, Function<void(bool granted)>> m_storageAccessResponseCallbackMap;

    OptionSet<LayerTreeFreezeReason> m_layerTreeFreezeReasons;
    bool m_isSuspended { false };
    bool m_needsFontAttributes { false };
    bool m_firstFlushAfterCommit { false };
    bool m_needsFixedContainerEdgesUpdate { true };
#if PLATFORM(COCOA)
    WeakPtr<WebRemoteObjectRegistry> m_remoteObjectRegistry;
#endif
    WebPageProxyIdentifier m_webPageProxyIdentifier;
    std::optional<WebCore::IntSize> m_pendingIntrinsicContentSize;
    WebCore::IntSize m_lastSentIntrinsicContentSize;
#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    std::unique_ptr<LayerHostingContext> m_contextForVisibilityPropagation;
#endif
#if ENABLE(TEXT_AUTOSIZING)
    WebCore::Timer m_textAutoSizingAdjustmentTimer;
#endif

    AtomString m_overriddenMediaType;
    String m_processDisplayName;
    WebCore::AllowsContentJavaScript m_allowsContentJavaScriptFromMostRecentNavigation { WebCore::AllowsContentJavaScript::Yes };
#if ENABLE(GPU_PROCESS)
    struct RemoteSnapshotState {
        RemoteSnapshotIdentifier identifier;
        UniqueRef<RemoteSnapshotRecorderProxy> recorder;
        Ref<MainRunLoopSuccessCallbackAggregator> callback;
    };
    std::optional<RemoteSnapshotState> m_remoteSnapshotState;
#endif
#if PLATFORM(GTK)
    WebCore::Color m_accentColor;
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
#if USE(GBM)
    Vector<RendererBufferFormat> m_preferredBufferFormats;
#endif
#endif

#if ENABLE(APP_BOUND_DOMAINS)
    bool m_limitsNavigationsToAppBoundDomains { false };
    bool m_navigationHasOccured { false };
#endif

    bool m_lastNavigationWasAppInitiated { true };

    bool m_canUseCredentialStorage { true };

    bool m_didUpdateRenderingAfterCommittingLoad { false };
    bool m_isStoppingLoadingDueToProcessSwap { false };
    bool m_skipDecidePolicyForResponseIfPossible { false };

#if ENABLE(ARKIT_INLINE_PREVIEW)
    bool m_useARKitForModel { false };
#endif
#if HAVE(SCENEKIT)
    bool m_useSceneKitForModel { false };
#endif

#if HAVE(APP_ACCENT_COLORS)
    bool m_appUsesCustomAccentColor { false };
#endif

    OptionSet<DisallowLayoutViewportHeightExpansionReason> m_disallowLayoutViewportHeightExpansionReasons;
#if PLATFORM(IOS_FAMILY)
    WebCore::DeferrableOneShotTimer m_updateLayoutViewportHeightExpansionTimer;
    bool m_shouldRescheduleLayoutViewportHeightExpansionTimer { false };
#endif

    WeakPtr<WebCore::Node, WebCore::WeakPtrImplWithEventTargetData> m_lastNodeBeforeWritingSuggestions;

    bool m_textManipulationIncludesSubframes { false };

    Vector<String> m_corsDisablingPatterns;

    std::unique_ptr<WebCore::CachedPage> m_cachedPage;

#if ENABLE(IPC_TESTING_API)
    bool m_ipcTestingAPIEnabled { false };
    VisitedLinkTableIdentifier m_visitedLinkTableID;
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    RefPtr<WebCore::MediaSessionCoordinator> m_mediaSessionCoordinator;
    RefPtr<RemoteMediaSessionCoordinator> m_remoteMediaSessionCoordinator;
#endif

#if ENABLE(GPU_PROCESS)
    RefPtr<RemoteRenderingBackendProxy> m_remoteRenderingBackendProxy;
#endif

#if ENABLE(IMAGE_ANALYSIS)
    Vector<std::pair<WeakPtr<WebCore::HTMLElement, WebCore::WeakPtrImplWithEventTargetData>, Vector<CompletionHandler<void(RefPtr<WebCore::Element>&&)>>>> m_elementsPendingTextRecognition;
#endif

#if ENABLE(WEBXR)
    const std::unique_ptr<PlatformXRSystemProxy> m_xrSystemProxy;
#endif

#if ENABLE(APP_HIGHLIGHTS)
    WebCore::HighlightVisibility m_appHighlightsVisible { WebCore::HighlightVisibility::Hidden };
#endif

    const Ref<WebHistoryItemClient> m_historyItemClient;

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    WeakHashSet<WebCore::Element, WebCore::WeakPtrImplWithEventTargetData> m_elementsToExcludeFromRemoveBackground;
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    String m_mediaEnvironment;
#endif

#if ENABLE(WRITING_TOOLS)
    const UniqueRef<TextAnimationController> m_textAnimationController;
#endif

    Vector<WebCore::TextExtraction::FilterRule> m_textExtractionFilterRules;

    RefPtr<WebCore::NowPlayingMetadataObserver> m_nowPlayingMetadataObserver;
    std::unique_ptr<FrameInfoData> m_mainFrameNavigationInitiator;

    mutable RefPtr<Logger> m_logger;

    bool m_statusBarIsVisible { true };
    bool m_menuBarIsVisible { true };
    bool m_toolbarsAreVisible { true };
};

#if !PLATFORM(IOS_FAMILY)
inline void WebPage::platformWillPerformEditingCommand() { }
inline bool WebPage::requiresPostLayoutDataForEditorState(const WebCore::LocalFrame&) const { return false; }
inline void WebPage::prepareToRunModalJavaScriptDialog() { }
inline void WebPage::didProgrammaticallyClearTextFormControl(const WebCore::HTMLTextFormControlElement&) { }
#endif

#if !ENABLE(IOS_TOUCH_EVENTS)
inline void WebPage::didDispatchClickEvent(const WebCore::PlatformMouseEvent&, WebCore::Node&) { }
#endif

#if !PLATFORM(MAC)
inline bool WebPage::shouldAvoidComputingPostLayoutDataForEditorState() const { return false; }
#endif

#if !PLATFORM(COCOA)
inline URL WebPage::allowedQueryParametersForAdvancedPrivacyProtections(const URL& url) { return url; }
#endif

#if PLATFORM(IOS_FAMILY)
bool scalesAreEssentiallyEqual(float, float);
#endif

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebPage) \
    static bool isType(const API::Object& object) { return object.type() == API::Object::Type::BundlePage; } \
SPECIALIZE_TYPE_TRAITS_END()
