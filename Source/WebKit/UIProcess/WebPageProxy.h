/**
 * Copyright (C) 2010-2023 Apple Inc. All rights reserved.
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
#include <WebCore/FrameIdentifier.h>
#include <wtf/CheckedRef.h>
#include <wtf/CompletionHandler.h>
#include <wtf/OptionSet.h>
#include <wtf/ProcessID.h>
#include <wtf/UniqueRef.h>
#include <wtf/WeakHashSet.h>

namespace API {
class Attachment;
class ContentWorld;
class ContextMenuClient;
class Data;
class DataTask;
class DiagnosticLoggingClient;
class Error;
class FindClient;
class FindMatchesClient;
class FormClient;
class FullscreenClient;
class HistoryClient;
class HitTestResult;
class IconLoadingClient;
class LoaderClient;
class Navigation;
class NavigationAction;
class NavigationClient;
class NavigationResponse;
class PageConfiguration;
class PolicyClient;
class ResourceLoadClient;
class SerializedScriptValue;
class UIClient;
class URL;
class URLRequest;
class WebsitePolicies;
}

namespace Inspector {
enum class InspectorTargetType : uint8_t;
}

namespace IPC {
class Decoder;
class FormDataReference;
class SharedBufferReference;
template<typename> struct ConnectionSendSyncResult;
using DataReference = std::span<const uint8_t>;
}

namespace PAL {
class SessionID;
enum class HysteresisState : bool;
}

namespace WebCore {

class AuthenticationChallenge;
class CaptureDevice;
class CertificateInfo;
class Color;
class ContentFilterUnblockHandler;
class Cursor;
class DataSegment;
class DestinationColorSpace;
class DragData;
class FloatPoint;
class FloatQuad;
class FloatRect;
class FloatSize;
class FontAttributeChanges;
class FontChanges;
class GraphicsLayer;
class ImageBufferBackend;
class InspectorOverlay;
class IntPoint;
class IntRect;
class IntSize;
class LayoutPoint;
class LayoutSize;
class NotificationResources;
class PlatformSpeechSynthesisUtterance;
class PlatformSpeechSynthesizer;
class PrivateClickMeasurement;
class Region;
class RegistrableDomain;
class ResourceError;
class ResourceLoader;
class ResourceRequest;
class ResourceResponse;
class RunLoopObserver;
class SecurityOrigin;
class SecurityOriginData;
class SelectionData;
class SelectionGeometry;
class SharedBuffer;
class SleepDisabler;
class SpeechRecognitionRequest;
class SubstituteData;
class TextCheckingRequestData;
class ValidationBubble;

enum class LayoutMilestone : uint16_t;
enum class PaginationMode : uint8_t;
enum class ScrollDirection : uint8_t;
enum ScrollbarOverlayStyle : uint8_t;

enum class ActivityState : uint16_t;
enum class AdvancedPrivacyProtections : uint16_t;
enum class AlternativeTextType : uint8_t;
enum class ArchiveError : uint8_t;
enum class AutocorrectionResponse : uint8_t;
enum class AutoplayEvent : uint8_t;
enum class AutoplayEventFlags : uint8_t;
enum class BrowsingContextGroupSwitchDecision : uint8_t;
enum class CaretAnimatorType : uint8_t;
enum class CookieConsentDecisionResult : uint8_t;
enum class CreateNewGroupForHighlight : bool;
enum class DOMPasteAccessCategory : uint8_t;
enum class DOMPasteAccessResponse : uint8_t;
enum class DataDetectorType : uint8_t;
enum class DataOwnerType : uint8_t;
enum class DeviceOrientationOrMotionPermissionState : uint8_t;
enum class DiagnosticLoggingDomain : uint8_t;
enum class DragControllerAction : uint8_t;
enum class DragHandlingMethod : uint8_t;
enum class DragOperation : uint8_t;
enum class DragSourceAction : uint8_t;
enum class EventMakesGamepadsVisible : bool;
enum class FocusDirection : uint8_t;
enum class FrameLoadType : uint8_t;
enum class HasInsecureContent : bool;
enum class HighlightRequestOriginatedInApp : bool;
enum class HighlightVisibility : bool;
enum class InputMode : uint8_t;
enum class LayoutViewportConstraint : bool;
enum class LockBackForwardList : bool;
enum class MediaPlaybackTargetContextMockState : uint8_t;
enum class MediaProducerMediaCaptureKind : uint8_t;
enum class MediaProducerMediaState : uint32_t;
enum class MediaProducerMutedState : uint8_t;
enum class ModalContainerControlType : uint8_t;
enum class ModalContainerDecision : uint8_t;
enum class MouseEventPolicy : uint8_t;
enum class PermissionName : uint8_t;
enum class PermissionState : uint8_t;
enum class PlatformEventModifier : uint8_t;
enum class PolicyAction : uint8_t;
enum class ReasonForDismissingAlternativeText : uint8_t;
enum class ReloadOption : uint8_t;
enum class RenderAsTextFlag : uint16_t;
enum class RouteSharingPolicy : uint8_t;
enum class ScreenOrientationType : uint8_t;
enum class ScrollGranularity : uint8_t;
enum class ScrollIsAnimated : bool;
enum class ScrollPinningBehavior : uint8_t;
enum class SelectionDirection : uint8_t;
enum class SessionHistoryVisibility : bool;
enum class ShouldOpenExternalURLsPolicy : uint8_t;
enum class ShouldSample : bool;
enum class ShouldTreatAsContinuingLoad : uint8_t;
enum class TextCheckingType : uint8_t;
enum class TextGranularity : uint8_t;
enum class TrackingType : uint8_t;
enum class UserInterfaceLayoutDirection : bool;
enum class WheelEventProcessingSteps : uint8_t;
enum class WheelScrollGestureState : uint8_t;
enum class WillContinueLoading : bool;
enum class WillInternallyHandleFailure : bool;
enum class WindowProxyProperty : uint8_t;
enum class WritingDirection : uint8_t;

struct AppHighlight;
struct ApplePayAMSUIRequest;
struct ApplicationManifest;
struct AttributedString;
struct BackForwardItemIdentifierType;
struct CaptureDeviceWithCapabilities;
struct CaptureSourceOrError;
struct CharacterRange;
struct ClientOrigin;
struct CompositionHighlight;
struct CompositionUnderline;
struct ContactInfo;
struct ContactsRequestData;
struct ContentRuleListResults;
struct DataDetectorElementInfo;
struct DataListSuggestionInformation;
struct DateTimeChooserParameters;
struct DiagnosticLoggingDictionary;
struct DictationContextType;
struct DictionaryPopupInfo;
struct DragItem;
struct ElementContext;
struct ExceptionDetails;
struct FileChooserSettings;
struct FontAttributes;
struct GrammarDetail;
struct HTMLMediaElementIdentifierType;
struct HTMLModelElementCamera;
struct ImageBufferParameters;
struct InspectorOverlayHighlight;
struct LinkIcon;
struct LinkDecorationFilteringData;
struct MarkupExclusionRule;
struct MediaControlsContextMenuItem;
struct MediaDeviceHashSalts;
struct MediaKeySystemRequestIdentifierType;
struct MediaPlayerIdentifierType;
struct MediaSessionIdentifierType;
struct MediaStreamRequest;
struct MediaUsageInfo;
struct MessageWithMessagePorts;
struct MockWebAuthenticationConfiguration;
struct NotificationData;
struct OrganizationStorageAccessPromptQuirk;
struct PageIdentifierType;
struct PermissionDescriptor;
struct PlatformLayerIdentifierType;
struct PlaybackTargetClientContextIdentifierType;
struct PromisedAttachmentInfo;
struct RecentSearch;
struct RemoteUserInputEventData;
struct RunJavaScriptParameters;
struct SerializedAttachmentData;
struct ShareDataWithParsedURL;
struct SleepDisablerIdentifierType;
struct SpeechRecognitionError;
struct SystemPreviewInfo;
struct TextAlternativeWithRange;
struct TextCheckingResult;
struct TextIndicatorData;
struct TextManipulationControllerExclusionRule;
struct TextManipulationControllerManipulationFailure;
struct TextManipulationItem;
struct TextRecognitionResult;
struct TranslationContextMenuInfo;
struct UserMediaRequestIdentifierType;
struct ViewportAttributes;
struct ViewportArguments;
struct WheelEventHandlingResult;
struct WindowFeatures;

template<typename> class ProcessQualified;
template<typename> class RectEdges;

using BackForwardItemIdentifier = ProcessQualified<ObjectIdentifier<BackForwardItemIdentifierType>>;
using DictationContext = ObjectIdentifier<DictationContextType>;
using FloatBoxExtent = RectEdges<float>;
using FramesPerSecond = unsigned;
using IntDegrees = int32_t;
using HTMLMediaElementIdentifier = ObjectIdentifier<HTMLMediaElementIdentifierType>;
using MediaControlsContextMenuItemID = uint64_t;
using MediaKeySystemRequestIdentifier = ObjectIdentifier<MediaKeySystemRequestIdentifierType>;
using MediaPlayerIdentifier = ObjectIdentifier<MediaPlayerIdentifierType>;
using MediaProducerMediaStateFlags = OptionSet<MediaProducerMediaState>;
using MediaProducerMutedStateFlags = OptionSet<MediaProducerMutedState>;
using MediaSessionIdentifier = ObjectIdentifier<MediaSessionIdentifierType>;
using PageIdentifier = ObjectIdentifier<PageIdentifierType>;
using PlatformDisplayID = uint32_t;
using PlatformLayerIdentifier = ProcessQualified<ObjectIdentifier<PlatformLayerIdentifierType>>;
using PlaybackTargetClientContextIdentifier = ObjectIdentifier<PlaybackTargetClientContextIdentifierType>;
using PointerID = uint32_t;
using ResourceLoaderIdentifier = AtomicObjectIdentifier<ResourceLoader>;
using ScrollingNodeID = uint64_t;
using SleepDisablerIdentifier = ObjectIdentifier<SleepDisablerIdentifierType>;
using UserMediaRequestIdentifier = ObjectIdentifier<UserMediaRequestIdentifierType>;

} // namespace WebCore

OBJC_CLASS AMSUIEngagementTask;
OBJC_CLASS CALayer;
OBJC_CLASS NSArray;
OBJC_CLASS NSDictionary;
OBJC_CLASS NSEvent;
OBJC_CLASS NSFileWrapper;
OBJC_CLASS NSMenu;
OBJC_CLASS NSObject;
OBJC_CLASS NSString;
OBJC_CLASS NSTextAlternatives;
OBJC_CLASS NSView;
OBJC_CLASS NSWindow;
OBJC_CLASS QLPreviewPanel;
OBJC_CLASS SYNotesActivationObserver;
OBJC_CLASS WKQLThumbnailLoadOperation;
OBJC_CLASS WKQuickLookPreviewController;
OBJC_CLASS WKWebView;
OBJC_CLASS _WKRemoteObjectRegistry;

struct WKPageInjectedBundleClientBase;
struct wpe_view_backend;

#if PLATFORM(WPE)
typedef struct _WPEView WPEView;
#endif

namespace WebKit {

class AudioSessionRoutingArbitratorProxy;
class AuthenticationChallengeProxy;
class BrowsingContextGroup;
class CallbackID;
class ContextMenuContextData;
class DownloadProxy;
class DrawingAreaProxy;
class GamepadData;
class GeolocationPermissionRequestManagerProxy;
class LayerTreeContext;
class LinkDecorationFilteringDataObserver;
class MediaCapability;
class MediaKeySystemPermissionRequestManagerProxy;
class MediaSessionCoordinatorProxyPrivate;
class MediaUsageManager;
class ModelElementController;
class NativeWebGestureEvent;
class NativeWebKeyboardEvent;
class NativeWebMouseEvent;
class NativeWebTouchEvent;
class NativeWebWheelEvent;
class NetworkIssueReporter;
class PageClient;
class PageLoadState;
class PageLoadStateObserverBase;
class PlatformXRSystem;
class PlaybackSessionManagerProxy;
class ProcessThrottlerActivity;
class ProcessThrottlerTimedActivity;
class ProvisionalPageProxy;
class RemoteLayerTreeHost;
class RemoteLayerTreeNode;
class RemoteLayerTreeScrollingPerformanceData;
class RemoteLayerTreeTransaction;
class RemoteMediaSessionCoordinatorProxy;
class RemotePageProxy;
class RemoteScrollingCoordinatorProxy;
class RevealItem;
class SandboxExtensionHandle;
class SecKeyProxyStore;
class ShareableBitmap;
class ShareableBitmapHandle;
class ShareableResourceHandle;
class SharedMemory;
class SharedMemoryHandle;
class SpeechRecognitionPermissionManager;
class SuspendedPageProxy;
class SystemPreviewController;
class UserData;
class UserMediaPermissionRequestManagerProxy;
class UserMediaPermissionRequestProxy;
class VideoPresentationManagerProxy;
class ViewSnapshot;
class VisibleContentRectUpdateInfo;
class VisitedLinkStore;
class WebAuthenticatorCoordinatorProxy;
class WebBackForwardCache;
class WebBackForwardList;
class WebBackForwardListItem;
class WebColorPickerClient;
class WebContextMenuItemData;
class WebContextMenuProxy;
class WebDateTimePicker;
class WebDeviceOrientationUpdateProviderProxy;
class WebEditCommandProxy;
class WebExtensionController;
class WebFramePolicyListenerProxy;
class WebFrameProxy;
class WebFullScreenManagerProxy;
class WebInspectorUIProxy;
class WebKeyboardEvent;
class WebMouseEvent;
class WebNavigationState;
class WebOpenPanelResultListenerProxy;
class WebPageDebuggable;
class WebPageGroup;
class WebPageInjectedBundleClient;
class WebPageInspectorController;
class WebPageProxyMessageReceiverRegistration;
class WebPopupMenuProxy;
class WebPopupMenuProxyClient;
class WebPreferences;
class WebProcessProxy;
class WebScreenOrientationManagerProxy;
class WebTouchEvent;
class WebURLSchemeHandler;
class WebUserContentControllerProxy;
class WebViewDidMoveToWindowObserver;
class WebWheelEvent;
class WebWheelEventCoalescer;
class WebsiteDataStore;

struct AppPrivacyReportTestingData;
struct BackForwardListItemState;
struct DataDetectionResult;
struct DocumentEditingContext;
struct DocumentEditingContextRequest;
struct DynamicViewportSizeUpdate;
struct EditingRange;
struct EditorState;
struct FocusedElementInformation;
struct FrameInfoData;
struct FrameTreeCreationParameters;
struct FrameTreeNodeData;
struct GPUProcessPreferencesForWebProcess;
struct GeolocationIdentifierType;
struct InputMethodState;
struct InsertTextOptions;
struct InteractionInformationAtPosition;
struct InteractionInformationRequest;
struct LoadParameters;
struct ModelIdentifier;
struct NavigationActionData;
struct NetworkResourceLoadIdentifierType;
struct PDFContextMenu;
struct PDFPluginIdentifierType;
struct PlatformPopupMenuData;
struct PolicyDecision;
struct PrintInfo;
struct ResourceLoadInfo;
struct SessionState;
struct TapIdentifierType;
struct TextCheckerRequestType;
struct TransactionIDType;
struct URLSchemeTaskParameters;
struct UserMessage;
struct ViewWindowCoordinates;
struct WebAutocorrectionContext;
struct WebAutocorrectionData;
struct WebBackForwardListCounts;
struct WebFoundTextRange;
struct WebHitTestResultData;
struct WebNavigationDataStore;
struct WebPageCreationParameters;
struct WebPageProxyIdentifierType;
struct WebPopupItem;
struct WebPreferencesStore;
struct WebSpeechSynthesisVoice;
struct WebsitePoliciesData;
#if PLATFORM(WPE) && USE(GBM)
struct DMABufRendererBufferFormat;
#endif

enum class ContentAsStringIncludesChildFrames : bool;
enum class DragControllerAction : uint8_t;
enum class FindDecorationStyle : uint8_t;
enum class FindOptions : uint16_t;
enum class GestureRecognizerState : uint8_t;
enum class GestureType : uint8_t;
enum class TextRecognitionUpdateResult : uint8_t;
enum class MediaPlaybackState : uint8_t;
enum class NavigatingToAppBoundDomain : bool;
enum class NegotiatedLegacyTLS : bool;
enum class PasteboardAccessIntent : bool;
enum class ProcessSwapRequestedByClient : bool;
enum class ProcessTerminationReason : uint8_t;
enum class QuickLookPreviewActivity : uint8_t;
enum class RespectSelectionAnchor : bool;
enum class SOAuthorizationLoadPolicy : bool;
enum class SameDocumentNavigationType : uint8_t;
enum class SelectionFlags : uint8_t;
enum class SelectionTouch : uint8_t;
enum class ShouldDelayClosingUntilFirstLayerFlush : bool;
enum class SyntheticEditingCommandType : uint8_t;
enum class TextRecognitionUpdateResult : uint8_t;
enum class UndoOrRedo : bool;
enum class WasNavigationIntercepted : bool;
enum class WebContentMode : uint8_t;
enum class WebEventModifier : uint8_t;
enum class WebEventType : uint8_t;
enum class WindowKind : uint8_t;

template<typename> class MonotonicObjectIdentifier;

using ActivityStateChangeID = uint64_t;
using GeolocationIdentifier = ObjectIdentifier<GeolocationIdentifierType>;
using LayerHostingContextID = uint32_t;
using NetworkResourceLoadIdentifier = ObjectIdentifier<NetworkResourceLoadIdentifierType>;
using PDFPluginIdentifier = ObjectIdentifier<PDFPluginIdentifierType>;
using PlaybackSessionContextIdentifier = WebCore::HTMLMediaElementIdentifier;
using SnapshotOptions = uint32_t;
using SpeechRecognitionPermissionRequestCallback = CompletionHandler<void(std::optional<WebCore::SpeechRecognitionError>&&)>;
using SpellDocumentTag = int64_t;
using TapIdentifier = ObjectIdentifier<TapIdentifierType>;
using TextCheckerRequestID = ObjectIdentifier<TextCheckerRequestType>;
using TransactionID = MonotonicObjectIdentifier<TransactionIDType>;
using WebPageProxyIdentifier = ObjectIdentifier<WebPageProxyIdentifierType>;
using WebURLSchemeHandlerIdentifier = ObjectIdentifier<WebURLSchemeHandler>;
using WebUndoStepID = uint64_t;

class WebPageProxy final : public API::ObjectImpl<API::Object::Type::Page>, public IPC::MessageReceiver, public IPC::MessageSender, public CanMakeCheckedPtr {
public:
    static Ref<WebPageProxy> create(PageClient&, WebProcessProxy&, Ref<API::PageConfiguration>&&);
    virtual ~WebPageProxy();

    using IPC::MessageReceiver::weakPtrFactory;
    using IPC::MessageReceiver::WeakValueType;
    using IPC::MessageReceiver::WeakPtrImplType;

    static void forMostVisibleWebPageIfAny(PAL::SessionID, const WebCore::SecurityOriginData&, CompletionHandler<void(WebPageProxy*)>&&);

    const API::PageConfiguration& configuration() const;
    Ref<API::PageConfiguration> protectedConfiguration() const;

    using Identifier = WebPageProxyIdentifier;

    Identifier identifier() const;
    WebCore::PageIdentifier webPageID() const;
    WebCore::PageIdentifier webPageIDInProcessForDomain(const WebCore::RegistrableDomain&) const;

    PAL::SessionID sessionID() const;

    WebFrameProxy* mainFrame() const { return m_mainFrame.get(); }
    WebFrameProxy* openerFrame() const { return m_openerFrame.get(); }
    WebFrameProxy* focusedFrame() const { return m_focusedFrame.get(); }

    DrawingAreaProxy* drawingArea() const { return m_drawingArea.get(); }
    DrawingAreaProxy* provisionalDrawingArea() const;

    WebNavigationState& navigationState() { return *m_navigationState; }

    WebsiteDataStore& websiteDataStore() { return m_websiteDataStore; }
    Ref<WebsiteDataStore> protectedWebsiteDataStore() const;

    void addPreviouslyVisitedPath(const String&);

    bool isLockdownModeExplicitlySet() const { return m_isLockdownModeExplicitlySet; }
    bool shouldEnableLockdownMode() const;

#if ENABLE(GPU_PROCESS)
    GPUProcessPreferencesForWebProcess preferencesForGPUProcess() const;
#endif

    bool hasSameGPUProcessPreferencesAs(const API::PageConfiguration&) const;
    bool hasSameGPUProcessPreferencesAs(const WebPageProxy& page) const { return hasSameGPUProcessPreferencesAs(page.configuration()); }

    void processIsNoLongerAssociatedWithPage(WebProcessProxy&);

#if ENABLE(DATA_DETECTION)
    NSArray *dataDetectionResults() { return m_dataDetectionResults.get(); }
    void detectDataInAllFrames(OptionSet<WebCore::DataDetectorType>, CompletionHandler<void(const DataDetectionResult&)>&&);
    void removeDataDetectedLinks(CompletionHandler<void(const DataDetectionResult&)>&&);
#endif
        
#if ENABLE(ASYNC_SCROLLING) && PLATFORM(COCOA)
    RemoteScrollingCoordinatorProxy* scrollingCoordinatorProxy() const { return m_scrollingCoordinatorProxy.get(); }
#endif

    WebBackForwardList& backForwardList() { return m_backForwardList; }
    Ref<WebBackForwardList> protectedBackForwardList() const;

    bool addsVisitedLinks() const { return m_addsVisitedLinks; }
    void setAddsVisitedLinks(bool addsVisitedLinks) { m_addsVisitedLinks = addsVisitedLinks; }
    VisitedLinkStore& visitedLinkStore() { return m_visitedLinkStore; }
    Ref<VisitedLinkStore> protectedVisitedLinkStore();

    void exitFullscreenImmediately();
    void fullscreenMayReturnToInline();

    void suspend(CompletionHandler<void(bool)>&&);
    void resume(CompletionHandler<void(bool)>&&);
    bool isSuspended() const { return m_isSuspended; }

    WebInspectorUIProxy* inspector() const;
    RefPtr<WebInspectorUIProxy> protectedInspector() const;

    GeolocationPermissionRequestManagerProxy& geolocationPermissionRequestManager();

    void resourceLoadDidSendRequest(ResourceLoadInfo&&, WebCore::ResourceRequest&&);
    void resourceLoadDidPerformHTTPRedirection(ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceRequest&&);
    void resourceLoadDidReceiveChallenge(ResourceLoadInfo&&, WebCore::AuthenticationChallenge&&);
    void resourceLoadDidReceiveResponse(ResourceLoadInfo&&, WebCore::ResourceResponse&&);
    void resourceLoadDidCompleteWithError(ResourceLoadInfo&&, WebCore::ResourceResponse&&, WebCore::ResourceError&&);

    void didChangeInspectorFrontendCount(uint32_t count) { m_inspectorFrontendCount = count; }
    unsigned inspectorFrontendCount() const { return m_inspectorFrontendCount; }
    bool hasInspectorFrontend() const { return m_inspectorFrontendCount > 0; }

    bool isControlledByAutomation() const { return m_controlledByAutomation; }
    void setControlledByAutomation(bool);

    WebPageInspectorController& inspectorController() { return *m_inspectorController; }

#if PLATFORM(IOS_FAMILY)
    void showInspectorIndication();
    void hideInspectorIndication();
#endif

    void createInspectorTarget(const String& targetId, Inspector::InspectorTargetType);
    void destroyInspectorTarget(const String& targetId);
    void sendMessageToInspectorFrontend(const String& targetId, const String& message);

    void getAllFrames(CompletionHandler<void(FrameTreeNodeData&&)>&&);
    void getAllFrameTrees(CompletionHandler<void(Vector<FrameTreeNodeData>&&)>&&);

#if ENABLE(REMOTE_INSPECTOR)
    void setIndicating(bool);
    bool inspectable() const;
    void setInspectable(bool);
    String remoteInspectionNameOverride() const;
    void setRemoteInspectionNameOverride(const String&);
    void remoteInspectorInformationDidChange();
#endif

    void loadServiceWorker(const URL&, bool usingModules, CompletionHandler<void(bool success)>&&);

    WebUserContentControllerProxy& userContentController() { return m_userContentController.get(); }

#if ENABLE(WK_WEB_EXTENSIONS)
    WebExtensionController* webExtensionController();
#endif

    bool hasSleepDisabler() const;

#if ENABLE(FULLSCREEN_API)
    WebFullScreenManagerProxy* fullScreenManager();

    API::FullscreenClient& fullscreenClient() const { return *m_fullscreenClient; }
    void setFullscreenClient(std::unique_ptr<API::FullscreenClient>&&);
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    PlaybackSessionManagerProxy* playbackSessionManager();
    VideoPresentationManagerProxy* videoPresentationManager();
    void setMockVideoPresentationModeEnabled(bool);
#endif

#if PLATFORM(IOS_FAMILY)
    bool allowsMediaDocumentInlinePlayback() const;
    void setAllowsMediaDocumentInlinePlayback(bool);

    void willOpenAppLink();
#endif

#if USE(SYSTEM_PREVIEW)
    SystemPreviewController* systemPreviewController() { return m_systemPreviewController.get(); }
    void systemPreviewActionTriggered(const WebCore::SystemPreviewInfo&, const String&);
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
    ModelElementController* modelElementController() { return m_modelElementController.get(); }
    void modelElementGetCamera(ModelIdentifier, CompletionHandler<void(Expected<WebCore::HTMLModelElementCamera, WebCore::ResourceError>)>&&);
    void modelElementSetCamera(ModelIdentifier, WebCore::HTMLModelElementCamera, CompletionHandler<void(bool)>&&);
    void modelElementIsPlayingAnimation(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void modelElementSetAnimationIsPlaying(ModelIdentifier, bool, CompletionHandler<void(bool)>&&);
    void modelElementIsLoopingAnimation(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void modelElementSetIsLoopingAnimation(ModelIdentifier, bool, CompletionHandler<void(bool)>&&);
    void modelElementAnimationDuration(ModelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&&);
    void modelElementAnimationCurrentTime(ModelIdentifier, CompletionHandler<void(Expected<Seconds, WebCore::ResourceError>)>&&);
    void modelElementSetAnimationCurrentTime(ModelIdentifier, Seconds, CompletionHandler<void(bool)>&&);
    void modelElementHasAudio(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void modelElementIsMuted(ModelIdentifier, CompletionHandler<void(Expected<bool, WebCore::ResourceError>)>&&);
    void modelElementSetIsMuted(ModelIdentifier, bool, CompletionHandler<void(bool)>&&);
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)
    void takeModelElementFullscreen(ModelIdentifier);
    void modelElementSetInteractionEnabled(ModelIdentifier, bool);
    void modelInlinePreviewDidLoad(WebCore::PlatformLayerIdentifier);
    void modelInlinePreviewDidFailToLoad(WebCore::PlatformLayerIdentifier, const WebCore::ResourceError&);
#endif
#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    void modelElementCreateRemotePreview(const String&, const WebCore::FloatSize&, CompletionHandler<void(Expected<std::pair<String, uint32_t>, WebCore::ResourceError>)>&&);
    void modelElementLoadRemotePreview(const String&, const URL&, CompletionHandler<void(std::optional<WebCore::ResourceError>&&)>&&);
    void modelElementDestroyRemotePreview(const String&);
    void modelElementSizeDidChange(const String&, WebCore::FloatSize, CompletionHandler<void(Expected<MachSendRight, WebCore::ResourceError>)>&&);
    void handleMouseDownForModelElement(const String&, const WebCore::LayoutPoint&, MonotonicTime);
    void handleMouseMoveForModelElement(const String&, const WebCore::LayoutPoint&, MonotonicTime);
    void handleMouseUpForModelElement(const String&, const WebCore::LayoutPoint&, MonotonicTime);
    void modelInlinePreviewUUIDs(CompletionHandler<void(Vector<String>&&)>&&);
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    void startApplePayAMSUISession(URL&&, WebCore::ApplePayAMSUIRequest&&, CompletionHandler<void(std::optional<bool>&&)>&&);
    void abortApplePayAMSUISession();
#endif

#if ENABLE(CONTEXT_MENUS)
    API::ContextMenuClient& contextMenuClient() { return *m_contextMenuClient; }
    void setContextMenuClient(std::unique_ptr<API::ContextMenuClient>&&);
#endif

    API::FindClient& findClient() { return *m_findClient; }
    void setFindClient(std::unique_ptr<API::FindClient>&&);
    API::FindMatchesClient& findMatchesClient() { return *m_findMatchesClient; }
    void setFindMatchesClient(std::unique_ptr<API::FindMatchesClient>&&);
    API::DiagnosticLoggingClient* diagnosticLoggingClient() { return m_diagnosticLoggingClient.get(); }
    void setDiagnosticLoggingClient(std::unique_ptr<API::DiagnosticLoggingClient>&&);
    void setFormClient(std::unique_ptr<API::FormClient>&&);
    void setNavigationClient(UniqueRef<API::NavigationClient>&&);
    void setHistoryClient(UniqueRef<API::HistoryClient>&&);
    void setLoaderClient(std::unique_ptr<API::LoaderClient>&&);
    void setPolicyClient(std::unique_ptr<API::PolicyClient>&&);
    void setInjectedBundleClient(const WKPageInjectedBundleClientBase*);
    void setResourceLoadClient(std::unique_ptr<API::ResourceLoadClient>&&);

    API::UIClient& uiClient() { return *m_uiClient; }
    void setUIClient(std::unique_ptr<API::UIClient>&&);

    API::IconLoadingClient& iconLoadingClient() { return *m_iconLoadingClient; }
    void setIconLoadingClient(std::unique_ptr<API::IconLoadingClient>&&);

    void setPageLoadStateObserver(std::unique_ptr<PageLoadStateObserverBase>&&);

    void initializeWebPage();
    void setDrawingArea(std::unique_ptr<DrawingAreaProxy>&&);

    WeakPtr<SecKeyProxyStore> secKeyProxyStore(const WebCore::AuthenticationChallenge&);

    void makeViewBlankIfUnpaintedSinceLastLoadCommit();
        
    void close();
    bool tryClose();
    bool isClosed() const { return m_isClosed; }

    void setOpenedByDOM() { m_openedByDOM = true; }
    bool openedByDOM() const { return m_openedByDOM; }

    bool hasCommittedAnyProvisionalLoads() const { return m_hasCommittedAnyProvisionalLoads; }

    bool preferFasterClickOverDoubleTap() const { return m_preferFasterClickOverDoubleTap; }

    void closePage();

    void addPlatformLoadParameters(WebProcessProxy&, LoadParameters&);
    RefPtr<API::Navigation> loadRequest(WebCore::ResourceRequest&&);
    RefPtr<API::Navigation> loadRequest(WebCore::ResourceRequest&&, WebCore::ShouldOpenExternalURLsPolicy, API::Object* userData = nullptr);
    RefPtr<API::Navigation> loadFile(const String& fileURL, const String& resourceDirectoryURL, bool isAppInitiated = true, API::Object* userData = nullptr);
    RefPtr<API::Navigation> loadData(const IPC::DataReference&, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData = nullptr);
    RefPtr<API::Navigation> loadData(const IPC::DataReference&, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, WebCore::ShouldOpenExternalURLsPolicy);
    RefPtr<API::Navigation> loadSimulatedRequest(WebCore::ResourceRequest&&, WebCore::ResourceResponse&&, const IPC::DataReference&);
    void loadAlternateHTML(Ref<WebCore::DataSegment>&&, const String& encoding, const URL& baseURL, const URL& unreachableURL, API::Object* userData = nullptr);
    void navigateToPDFLinkWithSimulatedClick(const String& url, WebCore::IntPoint documentPoint, WebCore::IntPoint screenPoint);

    void simulateDeviceOrientationChange(double alpha, double beta, double gamma);

    void stopLoading();
    RefPtr<API::Navigation> reload(OptionSet<WebCore::ReloadOption>);

    RefPtr<API::Navigation> goForward();
    RefPtr<API::Navigation> goBack();

    RefPtr<API::Navigation> goToBackForwardItem(WebBackForwardListItem&);
    void tryRestoreScrollPosition();
    void didChangeBackForwardList(WebBackForwardListItem* addedItem, Vector<Ref<WebBackForwardListItem>>&& removed);
    void willGoToBackForwardListItem(const WebCore::BackForwardItemIdentifier&, bool inBackForwardCache);

    bool shouldKeepCurrentBackForwardListItemInList(WebBackForwardListItem&);

    bool willHandleHorizontalScrollEvents() const;

    void updateWebsitePolicies(WebsitePoliciesData&&);
    void notifyUserScripts();
    bool userScriptsNeedNotification() const;

    bool canShowMIMEType(const String& mimeType);

    String currentURL() const;

    float topContentInset() const { return m_topContentInset; }
    void setTopContentInset(float);

    // Corresponds to the web content's `<meta name="theme-color">` or application manifest's `"theme_color"`.
    WebCore::Color themeColor() const;

    WebCore::Color underlayColor() const;
    void setUnderlayColor(const WebCore::Color&);

    void triggerBrowsingContextGroupSwitchForNavigation(uint64_t navigationID, WebCore::BrowsingContextGroupSwitchDecision, const WebCore::RegistrableDomain& responseDomain, NetworkResourceLoadIdentifier existingNetworkResourceLoadIdentifierToResume, CompletionHandler<void(bool success)>&&);

    // At this time, m_pageExtendedBackgroundColor can be set via pageExtendedBackgroundColorDidChange() which is a message
    // from the UIProcess, or by didCommitLayerTree(). When PLATFORM(MAC) adopts UI side compositing, we should get rid of
    // the message entirely.
    WebCore::Color pageExtendedBackgroundColor() const;

    WebCore::Color sampledPageTopColor() const;

    WebCore::Color underPageBackgroundColor() const;
    WebCore::Color underPageBackgroundColorOverride() const;
    void setUnderPageBackgroundColorOverride(WebCore::Color&&);

    void viewWillStartLiveResize();
    void viewWillEndLiveResize();

    void setInitialFocus(bool forward, bool isKeyboardEventValid, const std::optional<WebKeyboardEvent>&, CompletionHandler<void()>&&);
    
    void clearSelection();
    void restoreSelectionInFocusedEditableElement();

    PageClient& pageClient() const;

    void setViewNeedsDisplay(const WebCore::Region&);
    void requestScroll(const WebCore::FloatPoint& scrollPosition, const WebCore::IntPoint& scrollOrigin, WebCore::ScrollIsAnimated);
    
    WebCore::FloatPoint viewScrollPosition() const;
    
    void setHasActiveAnimatedScrolls(bool isRunning);

    void setPrivateClickMeasurement(std::nullopt_t);
    void setPrivateClickMeasurement(WebCore::PrivateClickMeasurement&&);
    void setPrivateClickMeasurement(WebCore::PrivateClickMeasurement&&, String sourceDescription, String purchaser);

    struct EventAttribution {
        uint8_t sourceID;
        String destinationDomain;
        String sourceDescription;
        String purchaser;
    };
    std::optional<EventAttribution> privateClickMeasurementEventAttribution() const;

    enum class ActivityStateChangeDispatchMode : bool { Deferrable, Immediate };
    enum class ActivityStateChangeReplyMode : bool { Asynchronous, Synchronous };
    void activityStateDidChange(OptionSet<WebCore::ActivityState> mayHaveChanged, ActivityStateChangeDispatchMode = ActivityStateChangeDispatchMode::Deferrable, ActivityStateChangeReplyMode = ActivityStateChangeReplyMode::Asynchronous);
    bool isInWindow() const;
    void waitForDidUpdateActivityState(ActivityStateChangeID);
    void didUpdateActivityState() { m_waitingForDidUpdateActivityState = false; }

    void layerHostingModeDidChange();

    WebCore::IntSize viewSize() const;
    bool isViewVisible() const;
    bool isViewFocused() const;
    bool isViewWindowActive() const;

    WindowKind windowKind() const;

    void addMIMETypeWithCustomContentProvider(const String& mimeType);

    void selectAll();
    void executeEditCommand(const String& commandName, const String& argument = String());
    void executeEditCommand(const String& commandName, const String& argument, CompletionHandler<void()>&&);
    void validateCommand(const String& commandName, CompletionHandler<void(bool, int32_t)>&&);

    const EditorState& editorState() const;
    bool canDelete() const { return hasSelectedRange() && isContentEditable(); }
    bool hasSelectedRange() const;
    bool isContentEditable() const;

    void increaseListLevel();
    void decreaseListLevel();
    void changeListType();

    void setBaseWritingDirection(WebCore::WritingDirection);

    bool maintainsInactiveSelection() const;
    void setMaintainsInactiveSelection(bool);
    void setEditable(bool);
    bool isEditable() const { return m_isEditable; }

    void activateMediaStreamCaptureInPage();
    bool isMediaStreamCaptureMuted() const;
    void setMediaStreamCaptureMuted(bool);
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    void isConnectedToHardwareConsoleDidChange();
#endif
    bool isAllowedToChangeMuteState() const;

    void requestFontAttributesAtSelectionStart(CompletionHandler<void(const WebCore::FontAttributes&)>&&);

    void setCanShowPlaceholder(const WebCore::ElementContext&, bool);

#if ENABLE(UI_SIDE_COMPOSITING)
    void updateVisibleContentRects(const VisibleContentRectUpdateInfo&, bool sendEvenIfUnchanged);
#endif
        
    void adjustLayersForLayoutViewport(const WebCore::FloatPoint& scrollPosition, const WebCore::FloatRect& layoutViewport, double scale);

#if PLATFORM(IOS_FAMILY)
    void textInputContextsInRect(WebCore::FloatRect, CompletionHandler<void(const Vector<WebCore::ElementContext>&)>&&);
    void focusTextInputContextAndPlaceCaret(const WebCore::ElementContext&, const WebCore::IntPoint&, CompletionHandler<void(bool)>&&);

    void setShouldRevealCurrentSelectionAfterInsertion(bool);
        
    void setScreenIsBeingCaptured(bool);

    void insertTextPlaceholder(const WebCore::IntSize&, CompletionHandler<void(const std::optional<WebCore::ElementContext>&)>&&);
    void removeTextPlaceholder(const WebCore::ElementContext&, CompletionHandler<void()>&&);

    double displayedContentScale() const;
    const WebCore::FloatRect& exposedContentRect() const;
    const WebCore::FloatRect& unobscuredContentRect() const;
    bool inStableState() const;
    const WebCore::FloatRect& unobscuredContentRectRespectingInputViewBounds() const;
    // When visual viewports are enabled, this is the layout viewport rect.
    const WebCore::FloatRect& layoutViewportRect() const;

    void resendLastVisibleContentRects();

    WebCore::FloatRect computeLayoutViewportRect(const WebCore::FloatRect& unobscuredContentRect, const WebCore::FloatRect& unobscuredContentRectRespectingInputViewBounds, const WebCore::FloatRect& currentLayoutViewportRect, double displayedContentScale, WebCore::LayoutViewportConstraint) const;
    WebCore::FloatRect unconstrainedLayoutViewportRect() const;

    void scrollingNodeScrollViewWillStartPanGesture(WebCore::ScrollingNodeID);
    void scrollingNodeScrollViewDidScroll(WebCore::ScrollingNodeID);
    void scrollingNodeScrollWillStartScroll(WebCore::ScrollingNodeID);
    void scrollingNodeScrollDidEndScroll(WebCore::ScrollingNodeID);

    WebCore::FloatSize overrideScreenSize();

    void dynamicViewportSizeUpdate(const DynamicViewportSizeUpdate&);

    void setViewportConfigurationViewLayoutSize(const WebCore::FloatSize&, double scaleFactor, double minimumEffectiveDeviceWidth);
    void setSceneIdentifier(String&&);
    void setDeviceOrientation(WebCore::IntDegrees);
    WebCore::IntDegrees deviceOrientation() const { return m_deviceOrientation; }
    void setOverrideViewportArguments(const std::optional<WebCore::ViewportArguments>&);
    void willCommitLayerTree(TransactionID);

    void selectWithGesture(WebCore::IntPoint, GestureType, GestureRecognizerState, bool isInteractingWithFocusedElement, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&&);
    void updateSelectionWithTouches(WebCore::IntPoint, SelectionTouch, bool baseIsStart, CompletionHandler<void(const WebCore::IntPoint&, SelectionTouch, OptionSet<SelectionFlags>)>&&);
    void selectWithTwoTouches(WebCore::IntPoint from, WebCore::IntPoint to, GestureType, GestureRecognizerState, CompletionHandler<void(const WebCore::IntPoint&, GestureType, GestureRecognizerState, OptionSet<SelectionFlags>)>&&);
    void extendSelection(WebCore::TextGranularity, CompletionHandler<void()>&&);
    void extendSelection(WebCore::TextGranularity);
    void selectWordBackward();
    void extendSelectionForReplacement(CompletionHandler<void()>&&);
    void moveSelectionByOffset(int32_t offset, CompletionHandler<void()>&&);
    void selectTextWithGranularityAtPoint(WebCore::IntPoint, WebCore::TextGranularity, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void selectPositionAtPoint(WebCore::IntPoint, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void selectPositionAtBoundaryWithDirection(WebCore::IntPoint, WebCore::TextGranularity, WebCore::SelectionDirection, bool isInteractingWithFocusedElement, CompletionHandler<void()>&&);
    void moveSelectionAtBoundaryWithDirection(WebCore::TextGranularity, WebCore::SelectionDirection, CompletionHandler<void()>&&);
    void beginSelectionInDirection(WebCore::SelectionDirection, CompletionHandler<void(bool)>&&);
    void updateSelectionWithExtentPoint(WebCore::IntPoint, bool isInteractingWithFocusedElement, RespectSelectionAnchor, CompletionHandler<void(bool)>&&);
    void updateSelectionWithExtentPointAndBoundary(WebCore::IntPoint, WebCore::TextGranularity, bool isInteractingWithFocusedElement, CompletionHandler<void(bool)>&&);
    void requestAutocorrectionData(const String& textForAutocorrection, CompletionHandler<void(WebAutocorrectionData)>&&);
    void applyAutocorrection(const String& correction, const String& originalText, bool isCandidate, CompletionHandler<void(const String&)>&&);
    bool applyAutocorrection(const String& correction, const String& originalText, bool isCandidate);
    void requestAutocorrectionContext();
    void handleAutocorrectionContext(const WebAutocorrectionContext&);
#if ENABLE(REVEAL)
    void requestRVItemInCurrentSelectedRange(CompletionHandler<void(const RevealItem&)>&&);
    void prepareSelectionForContextMenuWithLocationInView(WebCore::IntPoint, CompletionHandler<void(bool, const RevealItem&)>&&);
#endif
    void willInsertFinalDictationResult();
    void didInsertFinalDictationResult();
    void replaceDictatedText(const String& oldText, const String& newText);
    void replaceSelectedText(const String& oldText, const String& newText);
    void didReceivePositionInformation(const InteractionInformationAtPosition&);
    void requestPositionInformation(const InteractionInformationRequest&);
    void startInteractionWithPositionInformation(const InteractionInformationAtPosition&);
    void stopInteraction();
    void performActionOnElement(uint32_t action);
    void performActionOnElements(uint32_t action, Vector<WebCore::ElementContext>&&);
    void saveImageToLibrary(SharedMemoryHandle&& imageHandle, const String& authorizationToken);
    void focusNextFocusedElement(bool isForward, CompletionHandler<void()>&&);
    void setFocusedElementValue(const WebCore::ElementContext&, const String&);
    void setFocusedElementSelectedIndex(const WebCore::ElementContext&, uint32_t index, bool allowMultipleSelection = false);
    void applicationDidEnterBackground();
    void applicationDidFinishSnapshottingAfterEnteringBackground();
    void applicationWillEnterForeground();
    bool lastObservedStateWasBackground() const { return m_lastObservedStateWasBackground; }
    void applicationWillResignActive();
    void applicationDidBecomeActive();
    void applicationDidEnterBackgroundForMedia();
    void applicationWillEnterForegroundForMedia();
    void commitPotentialTapFailed();
    void didNotHandleTapAsClick(const WebCore::IntPoint&);
    void didHandleTapAsHover();
    void didCompleteSyntheticClick();
    void disableDoubleTapGesturesDuringTapIfNecessary(TapIdentifier);
    void handleSmartMagnificationInformationForPotentialTap(TapIdentifier, const WebCore::FloatRect& renderRect, bool fitEntireRect, double viewportMinimumScale, double viewportMaximumScale, bool nodeIsRootLevel);
    void contentSizeCategoryDidChange(const String& contentSizeCategory);
    void getSelectionContext(CompletionHandler<void(const String&, const String&, const String&)>&&);
    void handleTwoFingerTapAtPoint(const WebCore::IntPoint&, OptionSet<WebEventModifier>, TapIdentifier requestID);
    void setForceAlwaysUserScalable(bool);
    bool forceAlwaysUserScalable() const { return m_forceAlwaysUserScalable; }
    double layoutSizeScaleFactor() const { return m_viewportConfigurationLayoutSizeScaleFactor; }
    WebCore::FloatSize viewLayoutSize() const;
    double minimumEffectiveDeviceWidth() const { return m_viewportConfigurationMinimumEffectiveDeviceWidth; }
    void setMinimumEffectiveDeviceWidthWithoutViewportConfigurationUpdate(double minimumEffectiveDeviceWidth) { m_viewportConfigurationMinimumEffectiveDeviceWidth = minimumEffectiveDeviceWidth; }
    void setIsScrollingOrZooming(bool);
    void requestRectsForGranularityWithSelectionOffset(WebCore::TextGranularity, uint32_t offset, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&&);
    void requestRectsAtSelectionOffsetWithText(int32_t offset, const String&, CompletionHandler<void(const Vector<WebCore::SelectionGeometry>&)>&&);
    void autofillLoginCredentials(const String& username, const String& password);
    void storeSelectionForAccessibility(bool);
    void startAutoscrollAtPosition(const WebCore::FloatPoint& positionInWindow);
    void cancelAutoscroll();
    void hardwareKeyboardAvailabilityChanged(bool keyboardIsAttached);
    bool isScrollingOrZooming() const { return m_isScrollingOrZooming; }
    void requestEvasionRectsAboveSelection(CompletionHandler<void(const Vector<WebCore::FloatRect>&)>&&);
    void updateSelectionWithDelta(int64_t locationDelta, int64_t lengthDelta, CompletionHandler<void()>&&);
    WebCore::FloatRect selectionBoundingRectInRootViewCoordinates() const;
    void requestDocumentEditingContext(DocumentEditingContextRequest, CompletionHandler<void(DocumentEditingContext)>&&);
    void generateSyntheticEditingCommand(SyntheticEditingCommandType);
    void showDataDetectorsUIForPositionInformation(const InteractionInformationAtPosition&);

    void insertionPointColorDidChange();

#if ENABLE(DRAG_SUPPORT)
    void didHandleDragStartRequest(bool started);
    void didHandleAdditionalDragItemsRequest(bool added);
    void requestDragStart(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask);
    void requestAdditionalItemsForDragSession(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragSourceAction> allowedActionsMask);
    void insertDroppedImagePlaceholders(const Vector<WebCore::IntSize>&, CompletionHandler<void(const Vector<WebCore::IntRect>&, std::optional<WebCore::TextIndicatorData>)>&& reply);
    void willReceiveEditDragSnapshot();
    void didReceiveEditDragSnapshot(std::optional<WebCore::TextIndicatorData>);
    void didConcludeDrop();
#endif
#endif // PLATFORM(IOS_FAMILY)

#if ENABLE(DATA_DETECTION)
    void setDataDetectionResult(const DataDetectionResult&);
    void handleClickForDataDetectionResult(const WebCore::DataDetectorElementInfo&, const WebCore::IntPoint&);
#endif
    void didCommitLayerTree(const RemoteLayerTreeTransaction&);
    void layerTreeCommitComplete();

    bool updateLayoutViewportParameters(const RemoteLayerTreeTransaction&);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void cancelComposition(const String& compositionString);
    void deleteSurrounding(int64_t offset, unsigned characterCount);

    void setInputMethodState(std::optional<InputMethodState>&&);
#endif
        
    void requestScrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin);
    void scrollToRect(const WebCore::FloatRect& targetRect, const WebCore::FloatPoint& origin);

#if PLATFORM(COCOA)
    void windowAndViewFramesChanged(const WebCore::FloatRect& viewFrameInWindowCoordinates, const WebCore::FloatPoint& accessibilityViewCoordinates);
    void setMainFrameIsScrollable(bool);
    bool shouldDelayWindowOrderingForEvent(const WebMouseEvent&);

    void setRemoteLayerTreeRootNode(RemoteLayerTreeNode*);
    CALayer *acceleratedCompositingRootLayer() const;

#if PLATFORM(MAC)
    CALayer *headerBannerLayer() const;
    CALayer *footerBannerLayer() const;
    int headerBannerHeight() const;
    int footerBannerHeight() const;
#endif

    void setTextAsync(const String&);

    void insertTextAsync(const String&, const EditingRange& replacementRange, InsertTextOptions&&);
    void insertDictatedTextAsync(const String&, const EditingRange& replacementRange, const Vector<WebCore::TextAlternativeWithRange>&, InsertTextOptions&&);

    void addDictationAlternative(WebCore::TextAlternativeWithRange&&);
    void dictationAlternativesAtSelection(CompletionHandler<void(Vector<WebCore::DictationContext>&&)>&&);
    void clearDictationAlternatives(Vector<WebCore::DictationContext>&&);

#if USE(DICTATION_ALTERNATIVES)
    NSTextAlternatives *platformDictationAlternatives(WebCore::DictationContext);
#endif

    void hasMarkedText(CompletionHandler<void(bool)>&&);
    void getMarkedRangeAsync(CompletionHandler<void(const EditingRange&)>&&);
    void getSelectedRangeAsync(CompletionHandler<void(const EditingRange&)>&&);
    void characterIndexForPointAsync(const WebCore::IntPoint&, CompletionHandler<void(uint64_t)>&&);
    void firstRectForCharacterRangeAsync(const EditingRange&, CompletionHandler<void(const WebCore::IntRect&, const EditingRange&)>&&);
    void setCompositionAsync(const String& text, const Vector<WebCore::CompositionUnderline>&, const Vector<WebCore::CompositionHighlight>&, const HashMap<String, Vector<WebCore::CharacterRange>>&, const EditingRange& selectionRange, const EditingRange& replacementRange);
    void confirmCompositionAsync();

    void setScrollPerformanceDataCollectionEnabled(bool);
    bool scrollPerformanceDataCollectionEnabled() const { return m_scrollPerformanceDataCollectionEnabled; }
    RemoteLayerTreeScrollingPerformanceData* scrollingPerformanceData() { return m_scrollingPerformanceData.get(); }

    void scheduleActivityStateUpdate();
    void addActivityStateUpdateCompletionHandler(CompletionHandler<void()>&&);
#endif // PLATFORM(COCOA)

    void dispatchActivityStateUpdateForTesting();

    void changeFontAttributes(WebCore::FontAttributeChanges&&);
    void changeFont(WebCore::FontChanges&&);

#if PLATFORM(MAC)
    void setCaretAnimatorType(WebCore::CaretAnimatorType);
    void setCaretBlinkingSuspended(bool);
    void attributedSubstringForCharacterRangeAsync(const EditingRange&, CompletionHandler<void(const WebCore::AttributedString&, const EditingRange&)>&&);

    void startWindowDrag();
    NSWindow *platformWindow();
    void rootViewToWindow(const WebCore::IntRect& viewRect, WebCore::IntRect& windowRect);

    NSView *inspectorAttachmentView();
    _WKRemoteObjectRegistry *remoteObjectRegistry();

    CGRect boundsOfLayerInLayerBackedWindowCoordinates(CALayer *) const;
#endif // PLATFORM(MAC)

#if PLATFORM(GTK)
    GtkWidget* viewWidget();
#endif

#if PLATFORM(GTK) && HAVE(APP_ACCENT_COLORS)
    void accentColorDidChange();
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
    WPEView* wpeView() const;
#endif

    const std::optional<WebCore::Color>& backgroundColor() const;
    void setBackgroundColor(const std::optional<WebCore::Color>&);

#if USE(GRAPHICS_LAYER_TEXTURE_MAPPER) || USE(GRAPHICS_LAYER_WC)
    uint64_t viewWidget();
#endif

#if USE(LIBWPE)
    struct wpe_view_backend* viewBackend();
#endif

    bool isProcessingMouseEvents() const;
    void processNextQueuedMouseEvent();
    void handleMouseEventReply(WebEventType, bool, const std::optional<WebCore::RemoteUserInputEventData>&, std::optional<Vector<SandboxExtensionHandle>>&&);
    void sendMouseEvent(const WebCore::FrameIdentifier&, const NativeWebMouseEvent&, std::optional<Vector<SandboxExtensionHandle>>&&);
    void handleMouseEvent(const NativeWebMouseEvent&);
    void dispatchMouseDidMoveOverElementAsynchronously(const NativeWebMouseEvent&);

    void doAfterProcessingAllPendingMouseEvents(Function<void()>&&);
    void didFinishProcessingAllPendingMouseEvents();
    void flushPendingMouseEventCallbacks();

    bool isProcessingWheelEvents() const;
    void handleNativeWheelEvent(const NativeWebWheelEvent&);
    void continueWheelEventHandling(const WebWheelEvent&, const WebCore::WheelEventHandlingResult&, std::optional<bool> willStartSwipe);
    void wheelEventHandlingCompleted(bool wasHandled);

    bool isProcessingKeyboardEvents() const;
    void sendKeyEvent(const NativeWebKeyboardEvent&);
    bool handleKeyboardEvent(const NativeWebKeyboardEvent&);
#if PLATFORM(WIN)
    void dispatchPendingCharEvents(const NativeWebKeyboardEvent&);
#endif

#if ENABLE(MAC_GESTURE_EVENTS)
    void handleGestureEvent(const NativeWebGestureEvent&);
#endif

#if ENABLE(IOS_TOUCH_EVENTS)
    void resetPotentialTapSecurityOrigin();
    void handlePreventableTouchEvent(NativeWebTouchEvent&);
    void handleUnpreventableTouchEvent(const NativeWebTouchEvent&);

#elif ENABLE(TOUCH_EVENTS)
    void handleTouchEvent(const NativeWebTouchEvent&);
#endif

#if PLATFORM(MAC)
    void showFontPanel();
    void showStylesPanel();
    void showColorPanel();
#endif

    void cancelPointer(WebCore::PointerID, const WebCore::IntPoint&);
    void touchWithIdentifierWasRemoved(WebCore::PointerID);

    void scrollBy(WebCore::ScrollDirection, WebCore::ScrollGranularity);
    void centerSelectionInVisibleArea();

    const String& toolTip() const { return m_toolTip; }

    const String& userAgent() const { return m_userAgent; }
    String userAgentForURL(const URL&);
    void setApplicationNameForUserAgent(const String&);
    const String& applicationNameForUserAgent() const { return m_applicationNameForUserAgent; }
    void setApplicationNameForDesktopUserAgent(const String& applicationName) { m_applicationNameForDesktopUserAgent = applicationName; }
    const String& applicationNameForDesktopUserAgent() const { return m_applicationNameForDesktopUserAgent; }
    void setCustomUserAgent(const String&);
    const String& customUserAgent() const { return m_customUserAgent; }
    static String standardUserAgent(const String& applicationName = String());
#if PLATFORM(IOS_FAMILY)
    String predictedUserAgentForRequest(const WebCore::ResourceRequest&) const;
#else
    String predictedUserAgentForRequest(const WebCore::ResourceRequest&) const { return userAgent(); }
#endif

    bool supportsTextEncoding() const;
    void setCustomTextEncodingName(const String&);
    String customTextEncodingName() const { return m_customTextEncodingName; }

    bool areActiveDOMObjectsAndAnimationsSuspended() const { return m_areActiveDOMObjectsAndAnimationsSuspended; }
    void resumeActiveDOMObjectsAndAnimations();
    void suspendActiveDOMObjectsAndAnimations();

    double estimatedProgress() const;

    SessionState sessionState(Function<bool(WebBackForwardListItem&)>&& = nullptr) const;
    RefPtr<API::Navigation> restoreFromSessionState(SessionState, bool navigate);

    bool supportsTextZoom() const;
    double textZoomFactor() const { return m_textZoomFactor; }
    void setTextZoomFactor(double);
    double pageZoomFactor() const;
    void setPageZoomFactor(double);
    void setPageAndTextZoomFactors(double pageZoomFactor, double textZoomFactor);

    void scalePage(double scale, const WebCore::IntPoint& origin);
    void scalePageInViewCoordinates(double scale, const WebCore::IntPoint& centerInViewCoordinates);
    double pageScaleFactor() const;
    double viewScaleFactor() const { return m_viewScaleFactor; }
    void scaleView(double scale);
    void setShouldScaleViewToFitDocument(bool);
    
    float deviceScaleFactor() const;
    void setIntrinsicDeviceScaleFactor(float);
    std::optional<float> customDeviceScaleFactor() const { return m_customDeviceScaleFactor; }
    void setCustomDeviceScaleFactor(float);

    void accessibilitySettingsDidChange();

    void windowScreenDidChange(WebCore::PlatformDisplayID);
    std::optional<WebCore::PlatformDisplayID> displayID() const { return m_displayID; }

#if PLATFORM(IOS_FAMILY)
    WebCore::PlatformDisplayID generateDisplayIDFromPageID() const;
#endif

    void setUseFixedLayout(bool);
    void setFixedLayoutSize(const WebCore::IntSize&);
    bool useFixedLayout() const { return m_useFixedLayout; };
    const WebCore::IntSize& fixedLayoutSize() const;

    void setDefaultUnobscuredSize(const WebCore::FloatSize&);
    WebCore::FloatSize defaultUnobscuredSize() const;
    void setMinimumUnobscuredSize(const WebCore::FloatSize&);
    WebCore::FloatSize minimumUnobscuredSize() const;
    void setMaximumUnobscuredSize(const WebCore::FloatSize&);
    WebCore::FloatSize maximumUnobscuredSize() const;

    void setViewExposedRect(std::optional<WebCore::FloatRect>);
    std::optional<WebCore::FloatRect> viewExposedRect() const;

    void setAlwaysShowsHorizontalScroller(bool);
    void setAlwaysShowsVerticalScroller(bool);
    bool alwaysShowsHorizontalScroller() const { return m_alwaysShowsHorizontalScroller; }
    bool alwaysShowsVerticalScroller() const { return m_alwaysShowsVerticalScroller; }

    void listenForLayoutMilestones(OptionSet<WebCore::LayoutMilestone>);

    bool hasHorizontalScrollbar() const { return m_mainFrameHasHorizontalScrollbar; }
    bool hasVerticalScrollbar() const { return m_mainFrameHasVerticalScrollbar; }

    void setSuppressScrollbarAnimations(bool);
    bool areScrollbarAnimationsSuppressed() const { return m_suppressScrollbarAnimations; }

    WebCore::RectEdges<bool> pinnedState() const;

    WebCore::RectEdges<bool> rubberBandableEdgesRespectingHistorySwipe() const;
    WebCore::RectEdges<bool> rubberBandableEdges() const;
    void setRubberBandableEdges(WebCore::RectEdges<bool>);
    void setRubberBandsAtLeft(bool);
    void setRubberBandsAtRight(bool);
    void setRubberBandsAtTop(bool);
    void setRubberBandsAtBottom(bool);

    void setShouldUseImplicitRubberBandControl(bool shouldUseImplicitRubberBandControl) { m_shouldUseImplicitRubberBandControl = shouldUseImplicitRubberBandControl; }
    bool shouldUseImplicitRubberBandControl() const { return m_shouldUseImplicitRubberBandControl; }
        
    void setEnableVerticalRubberBanding(bool);
    bool verticalRubberBandingIsEnabled() const;
    void setEnableHorizontalRubberBanding(bool);
    bool horizontalRubberBandingIsEnabled() const;
        
    void setBackgroundExtendsBeyondPage(bool);
    bool backgroundExtendsBeyondPage() const;

    void setPaginationMode(WebCore::PaginationMode);
    WebCore::PaginationMode paginationMode() const { return m_paginationMode; }
    void setPaginationBehavesLikeColumns(bool);
    bool paginationBehavesLikeColumns() const { return m_paginationBehavesLikeColumns; }
    void setPageLength(double);
    double pageLength() const { return m_pageLength; }
    void setGapBetweenPages(double);
    double gapBetweenPages() const { return m_gapBetweenPages; }
    unsigned pageCount() const { return m_pageCount; }

    void isJITEnabled(CompletionHandler<void(bool)>&&);

#if PLATFORM(MAC)
    void setUseSystemAppearance(bool);
    bool useSystemAppearance() const { return m_useSystemAppearance; }

    bool useFormSemanticContext() const;
    void semanticContextDidChange();

    WebCore::DestinationColorSpace colorSpace();
#endif

    void effectiveAppearanceDidChange();
    bool useDarkAppearance() const;
    bool useElevatedUserInterfaceLevel() const;

    WebCore::DataOwnerType dataOwnerForPasteboard(PasteboardAccessIntent) const;

#if PLATFORM(COCOA)
    // Called by the web process through a message.
    void registerWebProcessAccessibilityToken(const IPC::DataReference&);
    // Called by the UI process when it is ready to send its tokens to the web process.
    void registerUIProcessAccessibilityTokens(const IPC::DataReference& elemenToken, const IPC::DataReference& windowToken);
    void replaceSelectionWithPasteboardData(const Vector<String>& types, const IPC::DataReference&);
    bool readSelectionFromPasteboard(const String& pasteboardName);
    String stringSelectionForPasteboard();
    RefPtr<WebCore::SharedBuffer> dataSelectionForPasteboard(const String& pasteboardType);
    void makeFirstResponder();
    void assistiveTechnologyMakeFirstResponder();
#endif

    void pageScaleFactorDidChange(double);
    void pluginScaleFactorDidChange(double);
    void pluginZoomFactorDidChange(double);

    // Find.
    void findString(const String&, OptionSet<FindOptions>, unsigned maxMatchCount);
    void findString(const String&, OptionSet<FindOptions>, unsigned maxMatchCount, CompletionHandler<void(bool)>&&);
    void findStringMatches(const String&, OptionSet<FindOptions>, unsigned maxMatchCount);
    void findRectsForStringMatches(const String&, OptionSet<FindOptions>, unsigned maxMatchCount, CompletionHandler<void(Vector<WebCore::FloatRect>&&)>&&);
    void getImageForFindMatch(int32_t matchIndex);
    void selectFindMatch(int32_t matchIndex);
    void indicateFindMatch(int32_t matchIndex);
    void didGetImageForFindMatch(WebCore::ImageBufferParameters&&, ShareableBitmapHandle&& contentImageHandle, uint32_t matchIndex);
    void hideFindUI();
    void hideFindIndicator();
    void countStringMatches(const String&, OptionSet<FindOptions>, unsigned maxMatchCount);
    void replaceMatches(Vector<uint32_t>&& matchIndices, const String& replacementText, bool selectionOnly, CompletionHandler<void(uint64_t)>&&);
    void didCountStringMatches(const String&, uint32_t matchCount);
    void setTextIndicator(const WebCore::TextIndicatorData&, uint64_t /* WebCore::TextIndicatorLifetime */ lifetime = 0 /* Permanent */);
    void setTextIndicatorAnimationProgress(float);
    void clearTextIndicator();

    void didFindString(const String&, const Vector<WebCore::IntRect>&, uint32_t matchCount, int32_t matchIndex, bool didWrapAround);
    void didFailToFindString(const String&);
    void didFindStringMatches(const String&, const Vector<Vector<WebCore::IntRect>>& matchRects, int32_t firstIndexAfterSelection);

    void findTextRangesForStringMatches(const String&, OptionSet<FindOptions>, unsigned maxMatchCount, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&&);
    void replaceFoundTextRangeWithString(const WebFoundTextRange&, const String&);
    void decorateTextRangeWithStyle(const WebFoundTextRange&, FindDecorationStyle);
    void scrollTextRangeToVisible(const WebFoundTextRange&);
    void clearAllDecoratedFoundText();
    void didBeginTextSearchOperation();

    void requestRectForFoundTextRange(const WebFoundTextRange&, CompletionHandler<void(WebCore::FloatRect)>&&);
    void addLayerForFindOverlay(CompletionHandler<void(WebCore::PlatformLayerIdentifier)>&&);
    void removeLayerForFindOverlay(CompletionHandler<void()>&&);

    void getContentsAsString(ContentAsStringIncludesChildFrames, CompletionHandler<void(const String&)>&&);
#if PLATFORM(COCOA)
    void getContentsAsAttributedString(CompletionHandler<void(const WebCore::AttributedString&)>&&);
#endif
    void getBytecodeProfile(CompletionHandler<void(const String&)>&&);
    void getSamplingProfilerOutput(CompletionHandler<void(const String&)>&&);

#if ENABLE(MHTML)
    void getContentsAsMHTMLData(CompletionHandler<void(API::Data*)>&&);
#endif
    void saveResources(WebFrameProxy*, const Vector<WebCore::MarkupExclusionRule>&, const String& directory, const String& suggestedMainResourceName, CompletionHandler<void(Expected<void, WebCore::ArchiveError>)>&&);
    void getMainResourceDataOfFrame(WebFrameProxy*, CompletionHandler<void(API::Data*)>&&);
    void getResourceDataFromFrame(WebFrameProxy&, API::URL*, CompletionHandler<void(API::Data*)>&&);
    void getRenderTreeExternalRepresentation(CompletionHandler<void(const String&)>&&);
    void getSelectionOrContentsAsString(CompletionHandler<void(const String&)>&&);
    void getSelectionAsWebArchiveData(CompletionHandler<void(API::Data*)>&&);
    void getSourceForFrame(WebFrameProxy*, CompletionHandler<void(const String&)>&&);
    void getWebArchiveOfFrame(WebFrameProxy*, CompletionHandler<void(API::Data*)>&&);
    void runJavaScriptInMainFrame(WebCore::RunJavaScriptParameters&&, CompletionHandler<void(Expected<RefPtr<API::SerializedScriptValue>, WebCore::ExceptionDetails>&&)>&&);
    void runJavaScriptInFrameInScriptWorld(WebCore::RunJavaScriptParameters&&, std::optional<WebCore::FrameIdentifier>, API::ContentWorld&, CompletionHandler<void(Expected<RefPtr<API::SerializedScriptValue>, WebCore::ExceptionDetails>&&)>&&);
    void getAccessibilityTreeData(CompletionHandler<void(API::Data*)>&&);
    void forceRepaint(CompletionHandler<void()>&&);

    float headerHeightForPrinting(WebFrameProxy&);
    float footerHeightForPrinting(WebFrameProxy&);
    void drawHeaderForPrinting(WebFrameProxy&, WebCore::FloatRect&&);
    void drawFooterForPrinting(WebFrameProxy&, WebCore::FloatRect&&);
    void drawPageBorderForPrinting(WebFrameProxy&, WebCore::FloatSize&&);

#if PLATFORM(COCOA)
    void performDictionaryLookupAtLocation(const WebCore::FloatPoint&);
    void performDictionaryLookupOfCurrentSelection();
#endif

    class PolicyDecisionSender;
    enum class WillContinueLoadInNewProcess : bool { No, Yes };
    void receivedPolicyDecision(WebCore::PolicyAction, API::Navigation*, RefPtr<API::WebsitePolicies>&&, std::variant<Ref<API::NavigationResponse>, Ref<API::NavigationAction>>&&, Ref<PolicyDecisionSender>&&, WillContinueLoadInNewProcess, std::optional<SandboxExtensionHandle>);
    void receivedNavigationPolicyDecision(WebProcessProxy&, WebProcessProxy&, WebCore::PolicyAction, API::Navigation*, Ref<API::NavigationAction>&&, ProcessSwapRequestedByClient, WebFrameProxy&, const FrameInfoData&, WasNavigationIntercepted, Ref<PolicyDecisionSender>&&);

    void backForwardRemovedItem(const WebCore::BackForwardItemIdentifier&);

#if ENABLE(DRAG_SUPPORT)    
    // Drag and drop support.
    void dragEntered(WebCore::DragData&, const String& dragStorageName = String());
    void dragUpdated(WebCore::DragData&, const String& dragStorageName = String());
    void dragExited(WebCore::DragData&);
    void performDragOperation(WebCore::DragData&, const String& dragStorageName, SandboxExtensionHandle&&, Vector<SandboxExtensionHandle>&&);

    void didPerformDragControllerAction(std::optional<WebCore::DragOperation>, WebCore::DragHandlingMethod, bool mouseIsOverFileInput, unsigned numberOfItemsToBeAccepted, const WebCore::IntRect& insertionRect, const WebCore::IntRect& editableElementRect);
    void dragEnded(const WebCore::IntPoint& clientPosition, const WebCore::IntPoint& globalPosition, OptionSet<WebCore::DragOperation>, const std::optional<WebCore::FrameIdentifier>& = std::nullopt);
    void didStartDrag();
    void dragCancelled();
    void setDragCaretRect(const WebCore::IntRect&);
#if PLATFORM(COCOA)
    void startDrag(const WebCore::DragItem&, ShareableBitmapHandle&& dragImageHandle);
    void setPromisedDataForImage(const String& pasteboardName, SharedMemoryHandle&& imageHandle, const String& filename, const String& extension,
        const String& title, const String& url, const String& visibleURL, SharedMemoryHandle&& archiveHandle, const String& originIdentifier);
#endif
#if PLATFORM(GTK)
    void startDrag(WebCore::SelectionData&&, OptionSet<WebCore::DragOperation>, std::optional<ShareableBitmapHandle>&& dragImage, WebCore::IntPoint&& dragImageHotspot);
#endif
#endif

    void processDidBecomeUnresponsive();
    void processDidBecomeResponsive();
    void resetStateAfterProcessTermination(ProcessTerminationReason);
    void provisionalProcessDidTerminate();
    void dispatchProcessDidTerminate(ProcessTerminationReason);
    void willChangeProcessIsResponsive();
    void didChangeProcessIsResponsive();

#if PLATFORM(IOS_FAMILY)
    void processWillBecomeSuspended();
    void processWillBecomeForeground();
#endif

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    void didCreateContextInWebProcessForVisibilityPropagation(LayerHostingContextID);
    LayerHostingContextID contextIDForVisibilityPropagationInWebProcess() const { return m_contextIDForVisibilityPropagationInWebProcess; }
#if ENABLE(GPU_PROCESS)
    void didCreateContextInGPUProcessForVisibilityPropagation(LayerHostingContextID);
    LayerHostingContextID contextIDForVisibilityPropagationInGPUProcess() const { return m_contextIDForVisibilityPropagationInGPUProcess; }
#endif
#endif

#if ENABLE(GPU_PROCESS)
    void gpuProcessDidFinishLaunching();
    void gpuProcessExited(ProcessTerminationReason);
#endif

    virtual void enterAcceleratedCompositingMode(const LayerTreeContext&);
    virtual void exitAcceleratedCompositingMode();
    virtual void updateAcceleratedCompositingMode(const LayerTreeContext&);
    void didFirstLayerFlush(const LayerTreeContext&);

    void addEditCommand(WebEditCommandProxy&);
    void removeEditCommand(WebEditCommandProxy&);
    void registerEditCommand(Ref<WebEditCommandProxy>&&, UndoOrRedo);

    bool canUndo();
    bool canRedo();

#if PLATFORM(COCOA)
    void registerKeypressCommandName(const String& name) { m_knownKeypressCommandNames.add(name); }
    bool isValidKeypressCommandName(const String& name) const { return m_knownKeypressCommandNames.contains(name); }
#endif

    WebProcessProxy& ensureRunningProcess();
    WebProcessProxy& process() const { return m_process; }
    Ref<WebProcessProxy> protectedProcess() const;
    ProcessID processID() const;

    ProcessID gpuProcessID() const;

    WebBackForwardCache& backForwardCache() const;

    const WebPreferences& preferences() const { return m_preferences; }
    WebPreferences& preferences() { return m_preferences; }
    Ref<WebPreferences> protectedPreferences() const;

    void setPreferences(WebPreferences&);

    WebPageGroup& pageGroup() { return m_pageGroup; }
    Ref<WebPageGroup> protectedPageGroup() const;

    bool hasRunningProcess() const;
    void launchInitialProcessIfNecessary();

#if ENABLE(DRAG_SUPPORT)
    std::optional<WebCore::DragOperation> currentDragOperation() const { return m_currentDragOperation; }
    WebCore::DragHandlingMethod currentDragHandlingMethod() const;
    bool currentDragIsOverFileInput() const { return m_currentDragIsOverFileInput; }
    unsigned currentDragNumberOfFilesToBeAccepted() const { return m_currentDragNumberOfFilesToBeAccepted; }
    WebCore::IntRect currentDragCaretRect() const;
    WebCore::IntRect currentDragCaretEditableElementRect() const;
    void resetCurrentDragInformation();
#endif

    void preferencesDidChange();

#if ENABLE(CONTEXT_MENUS)
    // Called by the WebContextMenuProxy.
    void didShowContextMenu();
    void didDismissContextMenu();
    void contextMenuItemSelected(const WebContextMenuItemData&);
    void handleContextMenuKeyEvent();
#endif

#if ENABLE(CONTEXT_MENU_EVENT)
    void dispatchAfterCurrentContextMenuEvent(CompletionHandler<void(bool)>&&);
#endif

    // Called by the WebOpenPanelResultListenerProxy.
#if PLATFORM(IOS_FAMILY)
    void didChooseFilesForOpenPanelWithDisplayStringAndIcon(const Vector<String>&, const String& displayString, const API::Data* iconData);
#endif
    void didChooseFilesForOpenPanel(const Vector<String>& fileURLs, const Vector<String>& allowedMIMETypes);
    void didCancelForOpenPanel();

    WebPageCreationParameters creationParameters(WebProcessProxy&, DrawingAreaProxy&, RefPtr<API::WebsitePolicies>&& = nullptr);

    void resumeDownload(const API::Data& resumeData, const String& path, CompletionHandler<void(DownloadProxy*)>&&);
    void downloadRequest(WebCore::ResourceRequest&&, CompletionHandler<void(DownloadProxy*)>&&);
    void dataTaskWithRequest(WebCore::ResourceRequest&&, const std::optional<WebCore::SecurityOriginData>& topOrigin, CompletionHandler<void(API::DataTask&)>&&);

    void advanceToNextMisspelling(bool startBeforeSelection);
    void changeSpellingToWord(const String& word);
#if USE(APPKIT)
    void uppercaseWord();
    void lowercaseWord();
    void capitalizeWord();
#endif

#if PLATFORM(COCOA)
    bool isSmartInsertDeleteEnabled() const { return m_isSmartInsertDeleteEnabled; }
    void setSmartInsertDeleteEnabled(bool);
#endif

    void setCanRunModal(bool);
    bool canRunModal();

    void beginPrinting(WebFrameProxy*, const PrintInfo&);
    void endPrinting(CompletionHandler<void()>&& = [] { });
    IPC::AsyncReplyID computePagesForPrinting(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(const Vector<WebCore::IntRect>&, double, const WebCore::FloatBoxExtent&)>&&);
    void getPDFFirstPageSize(WebCore::FrameIdentifier, CompletionHandler<void(WebCore::FloatSize)>&&);
#if PLATFORM(COCOA)
    IPC::AsyncReplyID drawRectToImage(WebFrameProxy*, const PrintInfo&, const WebCore::IntRect&, const WebCore::IntSize&, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&&);
    IPC::AsyncReplyID drawPagesToPDF(WebFrameProxy*, const PrintInfo&, uint32_t first, uint32_t count, CompletionHandler<void(API::Data*)>&&);
    void drawToPDF(WebCore::FrameIdentifier, const std::optional<WebCore::FloatRect>&, bool allowTransparentBackground,  CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
#if PLATFORM(IOS_FAMILY)
    size_t computePagesForPrintingiOS(WebCore::FrameIdentifier, const PrintInfo&);
    IPC::AsyncReplyID drawToImage(WebCore::FrameIdentifier, const PrintInfo&, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&&);
    IPC::AsyncReplyID drawToPDFiOS(WebCore::FrameIdentifier, const PrintInfo&, size_t pageCount, CompletionHandler<void(RefPtr<WebCore::SharedBuffer>&&)>&&);
#endif
#elif PLATFORM(GTK)
    void drawPagesForPrinting(WebFrameProxy*, const PrintInfo&, CompletionHandler<void(std::optional<SharedMemoryHandle>&&, WebCore::ResourceError&&)>&&);
#endif

    const PageLoadState& pageLoadState() const;
    PageLoadState& pageLoadState();

#if PLATFORM(COCOA)
    void handleAlternativeTextUIResult(const String& result);
#endif

    void saveDataToFileInDownloadsFolder(String&& suggestedFilename, String&& mimeType, URL&& originatingURL, API::Data&);
    void savePDFToFileInDownloadsFolder(String&& suggestedFilename, URL&& originatingURL, const IPC::DataReference&);

#if ENABLE(PDF_PLUGIN) && PLATFORM(MAC)
    void savePDFToTemporaryFolderAndOpenWithNativeApplication(const String& suggestedFilename, FrameInfoData&&, const IPC::DataReference&, const String& pdfUUID);
    void showPDFContextMenu(const PDFContextMenu&, PDFPluginIdentifier, CompletionHandler<void(std::optional<int32_t>&&)>&&);
#endif

    WebCore::IntRect visibleScrollerThumbRect() const;

    uint64_t renderTreeSize() const { return m_renderTreeSize; }

    void setMediaVolume(float);
    void setMuted(WebCore::MediaProducerMutedStateFlags);
    void setMuted(WebCore::MediaProducerMutedStateFlags, CompletionHandler<void()>&&);
    bool isAudioMuted() const;
    void setMayStartMediaWhenInWindow(bool);
    bool mayStartMediaWhenInWindow() const { return m_mayStartMediaWhenInWindow; }
    void setMediaCaptureEnabled(bool);
    bool mediaCaptureEnabled() const { return m_mediaCaptureEnabled; }
    void stopMediaCapture(WebCore::MediaProducerMediaCaptureKind);
    void stopMediaCapture(WebCore::MediaProducerMediaCaptureKind, CompletionHandler<void()>&&);

    void pauseAllMediaPlayback(CompletionHandler<void()>&&);
    void suspendAllMediaPlayback(CompletionHandler<void()>&&);
    void resumeAllMediaPlayback(CompletionHandler<void()>&&);
    void requestMediaPlaybackState(CompletionHandler<void(MediaPlaybackState)>&&);

#if ENABLE(POINTER_LOCK)
    void didAllowPointerLock();
    void didDenyPointerLock();
#endif

    void setSuppressVisibilityUpdates(bool flag);
    bool suppressVisibilityUpdates() { return m_suppressVisibilityUpdates; }

#if PLATFORM(IOS_FAMILY)
    void willStartUserTriggeredZooming();

    void potentialTapAtPosition(const WebCore::FloatPoint&, bool shouldRequestMagnificationInformation, TapIdentifier requestID);
    void commitPotentialTap(OptionSet<WebEventModifier>, TransactionID layerTreeTransactionIdAtLastTouchStart, WebCore::PointerID);
    void cancelPotentialTap();
    void tapHighlightAtPosition(const WebCore::FloatPoint&, TapIdentifier requestID);
    void attemptSyntheticClick(const WebCore::FloatPoint&, OptionSet<WebEventModifier>, TransactionID layerTreeTransactionIdAtLastTouchStart);
    void didRecognizeLongPress();
    void handleDoubleTapForDoubleClickAtPoint(const WebCore::IntPoint&, OptionSet<WebEventModifier>, TransactionID layerTreeTransactionIdAtLastTouchStart);

    void inspectorNodeSearchMovedToPosition(const WebCore::FloatPoint&);
    void inspectorNodeSearchEndedAtPosition(const WebCore::FloatPoint&);

    void blurFocusedElement();
    void setIsShowingInputViewForFocusedElement(bool);
#endif

    void postMessageToInjectedBundle(const String& messageName, API::Object* messageBody);

#if ENABLE(INPUT_TYPE_COLOR)
    void setColorPickerColor(const WebCore::Color&);
    void endColorPicker();
#endif

    bool isLayerTreeFrozenDueToSwipeAnimation() const { return m_isLayerTreeFrozenDueToSwipeAnimation; }

    WebCore::IntSize minimumSizeForAutoLayout() const;
    void setMinimumSizeForAutoLayout(const WebCore::IntSize&);

    WebCore::IntSize sizeToContentAutoSizeMaximumSize() const;
    void setSizeToContentAutoSizeMaximumSize(const WebCore::IntSize&);

    bool autoSizingShouldExpandToViewHeight() const { return m_autoSizingShouldExpandToViewHeight; }
    void setAutoSizingShouldExpandToViewHeight(bool);

    void setViewportSizeForCSSViewportUnits(const WebCore::FloatSize&);
    WebCore::FloatSize viewportSizeForCSSViewportUnits() const;

    void didReceiveAuthenticationChallengeProxy(Ref<AuthenticationChallengeProxy>&&, NegotiatedLegacyTLS);
    void negotiatedLegacyTLS();
    void didNegotiateModernTLS(const URL&);

    void didBlockLoadToKnownTracker(const URL&);
    void didApplyLinkDecorationFiltering(const URL&, const URL&);

    SpellDocumentTag spellDocumentTag();

    void didFinishCheckingText(TextCheckerRequestID, const Vector<WebCore::TextCheckingResult>&);
    void didCancelCheckingText(TextCheckerRequestID);
        
    void setScrollPinningBehavior(WebCore::ScrollPinningBehavior);
    WebCore::ScrollPinningBehavior scrollPinningBehavior() const;

    void setOverlayScrollbarStyle(std::optional<WebCore::ScrollbarOverlayStyle>);
    std::optional<WebCore::ScrollbarOverlayStyle> overlayScrollbarStyle() const { return m_scrollbarOverlayStyle; }

    void isLayerTreeFrozen(CompletionHandler<void(bool)>&&);

    // When the state of the window changes such that the WebPage needs immediate update, the UIProcess sends a new
    // ActivityStateChangeID to the WebProcess through the SetActivityState message. The UIProcess will wait till it
    // receives a CommitLayerTree which has an ActivityStateChangeID equal to or greater than the one it sent.
    ActivityStateChangeID takeNextActivityStateChangeID() { return ++m_currentActivityStateChangeID; }

    bool shouldRecordNavigationSnapshots() const { return m_shouldRecordNavigationSnapshots; }
    void setShouldRecordNavigationSnapshots(bool shouldRecordSnapshots) { m_shouldRecordNavigationSnapshots = shouldRecordSnapshots; }
    void recordAutomaticNavigationSnapshot();
    void suppressNextAutomaticNavigationSnapshot() { m_shouldSuppressNextAutomaticNavigationSnapshot = true; }
    void recordNavigationSnapshot(WebBackForwardListItem&);
    void requestFocusedElementInformation(CompletionHandler<void(const std::optional<FocusedElementInformation>&)>&&);

#if PLATFORM(COCOA) || PLATFORM(GTK)
    RefPtr<ViewSnapshot> takeViewSnapshot(std::optional<WebCore::IntRect>&&);
#endif

#if ENABLE(WEB_CRYPTO)
    void wrapCryptoKey(const Vector<uint8_t>&, CompletionHandler<void(bool, Vector<uint8_t>&&)>&&);
    void unwrapCryptoKey(const Vector<uint8_t>&, CompletionHandler<void(bool, Vector<uint8_t>&&)>&&);
#endif

    void takeSnapshot(WebCore::IntRect, WebCore::IntSize bitmapSize, SnapshotOptions, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&)>&&);

    void navigationGestureDidBegin();
    void navigationGestureWillEnd(bool willNavigate, WebBackForwardListItem&);
    void navigationGestureDidEnd(bool willNavigate, WebBackForwardListItem&);
    void navigationGestureDidEnd();
    void navigationGestureSnapshotWasRemoved();
    void willRecordNavigationSnapshot(WebBackForwardListItem&);

    bool isShowingNavigationGestureSnapshot() const { return m_isShowingNavigationGestureSnapshot; }

    bool isPlayingAudio() const;
    bool hasMediaStreaming() const;
    void isPlayingMediaDidChange(WebCore::MediaProducerMediaStateFlags);
    void updateReportedMediaCaptureState();

    enum class CanDelayNotification : bool { No, Yes };
    void updatePlayingMediaDidChange(WebCore::MediaProducerMediaStateFlags, CanDelayNotification = CanDelayNotification::No);
    bool isCapturingAudio() const;
    bool isCapturingVideo() const;
    bool hasActiveAudioStream() const;
    bool hasActiveVideoStream() const;
    WebCore::MediaProducerMediaStateFlags reportedMediaState() const;
    WebCore::MediaProducerMutedStateFlags mutedStateFlags() const;

    void handleAutoplayEvent(WebCore::AutoplayEvent, OptionSet<WebCore::AutoplayEventFlags>);

    void videoControlsManagerDidChange();
    bool hasActiveVideoForControlsManager() const;
    void requestControlledElementID() const;
    void handleControlledElementIDResponse(const String&) const;
    bool isPlayingVideoInEnhancedFullscreen() const;

#if PLATFORM(COCOA)
    void requestActiveNowPlayingSessionInfo(CompletionHandler<void(bool, bool, const String&, double, double, uint64_t)>&&);
#endif

#if PLATFORM(MAC)
    API::HitTestResult* lastMouseMoveHitTestResult() const { return m_lastMouseMoveHitTestResult.get(); }
    void performImmediateActionHitTestAtLocation(WebCore::FloatPoint);

    void immediateActionDidUpdate();
    void immediateActionDidCancel();
    void immediateActionDidComplete();

    NSObject *immediateActionAnimationControllerForHitTestResult(RefPtr<API::HitTestResult>, uint64_t, RefPtr<API::Object>);

    void handleAcceptedCandidate(WebCore::TextCheckingResult);
    void didHandleAcceptedCandidate();

    void setHeaderBannerHeight(int);
    void setFooterBannerHeight(int);

    void didBeginMagnificationGesture();
    void didEndMagnificationGesture();
#endif

    bool scrollingUpdatesDisabledForTesting();

    void installActivityStateChangeCompletionHandler(CompletionHandler<void()>&&);

#if USE(UNIFIED_TEXT_CHECKING)
    void checkTextOfParagraph(const String& text, OptionSet<WebCore::TextCheckingType> checkingTypes, int32_t insertionPoint, CompletionHandler<void(Vector<WebCore::TextCheckingResult>&&)>&&);
#endif
    void getGuessesForWord(const String& word, const String& context, int32_t insertionPoint, CompletionHandler<void(Vector<String>&&)>&&);

    void setShouldDispatchFakeMouseMoveEvents(bool);

    // Diagnostic messages logging.
    void logDiagnosticMessage(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithResult(const String& message, const String& description, uint32_t result, WebCore::ShouldSample);
    void logDiagnosticMessageWithValue(const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample);
    void logDiagnosticMessageWithEnhancedPrivacy(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithValueDictionary(const String& message, const String& description, const WebCore::DiagnosticLoggingDictionary&, WebCore::ShouldSample);
    void logDiagnosticMessageWithDomain(const String& message, WebCore::DiagnosticLoggingDomain);

    void logDiagnosticMessageFromWebProcess(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithResultFromWebProcess(const String& message, const String& description, uint32_t result, WebCore::ShouldSample);
    void logDiagnosticMessageWithValueFromWebProcess(const String& message, const String& description, double value, unsigned significantFigures, WebCore::ShouldSample);
    void logDiagnosticMessageWithEnhancedPrivacyFromWebProcess(const String& message, const String& description, WebCore::ShouldSample);
    void logDiagnosticMessageWithValueDictionaryFromWebProcess(const String& message, const String& description, const WebCore::DiagnosticLoggingDictionary&, WebCore::ShouldSample);
    void logDiagnosticMessageWithDomainFromWebProcess(const String& message, WebCore::DiagnosticLoggingDomain);

    // Performance logging.
    void logScrollingEvent(uint32_t eventType, MonotonicTime, uint64_t);

    // Form validation messages.
    void showValidationMessage(const WebCore::IntRect& anchorClientRect, const String& message);
    void hideValidationMessage();
#if PLATFORM(COCOA) || PLATFORM(GTK)
    WebCore::ValidationBubble* validationBubble() const { return m_validationBubble.get(); } // For testing.
#endif

#if PLATFORM(IOS_FAMILY)
    void setIsKeyboardAnimatingIn(bool isKeyboardAnimatingIn) { m_isKeyboardAnimatingIn = isKeyboardAnimatingIn; }
    bool isKeyboardAnimatingIn() const { return m_isKeyboardAnimatingIn; }

    void setWaitingForPostLayoutEditorStateUpdateAfterFocusingElement(bool waitingForPostLayoutEditorStateUpdateAfterFocusingElement) { m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement = waitingForPostLayoutEditorStateUpdateAfterFocusingElement; }
    bool waitingForPostLayoutEditorStateUpdateAfterFocusingElement() const { return m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement; }

    const Function<bool()>& deviceOrientationUserPermissionHandlerForTesting() const { return m_deviceOrientationUserPermissionHandlerForTesting; };
    void setDeviceOrientationUserPermissionHandlerForTesting(Function<bool()>&& handler) { m_deviceOrientationUserPermissionHandlerForTesting = WTFMove(handler); }
    void setDeviceHasAGXCompilerServiceForTesting() const;

    void statusBarWasTapped();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    void addPlaybackTargetPickerClient(WebCore::PlaybackTargetClientContextIdentifier);
    void removePlaybackTargetPickerClient(WebCore::PlaybackTargetClientContextIdentifier);
    void showPlaybackTargetPicker(WebCore::PlaybackTargetClientContextIdentifier, const WebCore::FloatRect&, bool hasVideo);
    void playbackTargetPickerClientStateDidChange(WebCore::PlaybackTargetClientContextIdentifier, WebCore::MediaProducerMediaStateFlags);
    void setMockMediaPlaybackTargetPickerEnabled(bool);
    void setMockMediaPlaybackTargetPickerState(const String&, WebCore::MediaPlaybackTargetContextMockState);
    void mockMediaPlaybackTargetPickerDismissPopup();
#endif

    void didChangeBackgroundColor();
    void didLayoutForCustomContentProvider();

    // For testing
    void clearWheelEventTestMonitor();
    void callAfterNextPresentationUpdate(CompletionHandler<void()>&&);

    void didReachLayoutMilestone(OptionSet<WebCore::LayoutMilestone>);

    void didRestoreScrollPosition();

    void getLoadDecisionForIcon(const WebCore::LinkIcon&, CallbackID);

    void focusFromServiceWorker(CompletionHandler<void()>&&);
    void setFocus(bool focused);
    void setWindowFrame(const WebCore::FloatRect&);
    void getWindowFrame(CompletionHandler<void(const WebCore::FloatRect&)>&&);
    void getWindowFrameWithCallback(Function<void(WebCore::FloatRect)>&&);

    WebCore::UserInterfaceLayoutDirection userInterfaceLayoutDirection();
    void setUserInterfaceLayoutDirection(WebCore::UserInterfaceLayoutDirection);

    bool hasHadSelectionChangesFromUserInteraction() const { return m_hasHadSelectionChangesFromUserInteraction; }

#if HAVE(TOUCH_BAR)
    bool isTouchBarUpdateSupressedForHiddenContentEditable() const { return m_isTouchBarUpdateSupressedForHiddenContentEditable; }
    bool isNeverRichlyEditableForTouchBar() const { return m_isNeverRichlyEditableForTouchBar; }
#endif

#if ENABLE(GAMEPAD)
    void gamepadActivity(const Vector<std::optional<GamepadData>>&, WebCore::EventMakesGamepadsVisible);
#endif

    void isLoadingChanged();

    void clearUserMediaState();

    void setShouldSkipWaitingForPaintAfterNextViewDidMoveToWindow(bool shouldSkip) { m_shouldSkipWaitingForPaintAfterNextViewDidMoveToWindow = shouldSkip; }

    void setURLSchemeHandlerForScheme(Ref<WebURLSchemeHandler>&&, const String& scheme);
    WebURLSchemeHandler* urlSchemeHandlerForScheme(const String& scheme);

#if PLATFORM(COCOA)
    void createSandboxExtensionsIfNeeded(const Vector<String>& files, SandboxExtensionHandle& fileReadHandle, Vector<SandboxExtensionHandle>& fileUploadHandles);
#endif
    void editorStateChanged(const EditorState&);
    enum class ShouldMergeVisualEditorState : uint8_t { No, Yes, Default };
    bool updateEditorState(const EditorState& newEditorState, ShouldMergeVisualEditorState = ShouldMergeVisualEditorState::Default);
    void scheduleFullEditorStateUpdate();
    void dispatchDidUpdateEditorState();

    void requestStorageAccessConfirm(const WebCore::RegistrableDomain& subFrameDomain, const WebCore::RegistrableDomain& topFrameDomain, WebCore::FrameIdentifier, std::optional<WebCore::OrganizationStorageAccessPromptQuirk>&&, CompletionHandler<void(bool)>&&);
    void didCommitCrossSiteLoadWithDataTransferFromPrevalentResource();
    void getLoadedSubresourceDomains(CompletionHandler<void(Vector<WebCore::RegistrableDomain>&&)>&&);
    void clearLoadedSubresourceDomains();

#if ENABLE(DEVICE_ORIENTATION)
    void shouldAllowDeviceOrientationAndMotionAccess(WebCore::FrameIdentifier, FrameInfoData&&, bool mayPrompt, CompletionHandler<void(WebCore::DeviceOrientationOrMotionPermissionState)>&&);
#endif

#if ENABLE(IMAGE_ANALYSIS)
    void requestTextRecognition(const URL& imageURL, ShareableBitmapHandle&& imageData, const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier, CompletionHandler<void(WebCore::TextRecognitionResult&&)>&&);
    void updateWithTextRecognitionResult(WebCore::TextRecognitionResult&&, const WebCore::ElementContext&, const WebCore::FloatPoint& location, CompletionHandler<void(TextRecognitionUpdateResult)>&&);
    void computeHasVisualSearchResults(const URL& imageURL, ShareableBitmap& imageBitmap, CompletionHandler<void(bool)>&&);
    void startVisualTranslation(const String& sourceLanguageIdentifier, const String& targetLanguageIdentifier);
#endif

#if ENABLE(MEDIA_CONTROLS_CONTEXT_MENUS) && USE(UICONTEXTMENU)
    void showMediaControlsContextMenu(WebCore::FloatRect&&, Vector<WebCore::MediaControlsContextMenuItem>&&, CompletionHandler<void(WebCore::MediaControlsContextMenuItemID)>&&);
#endif

    static RefPtr<WebPageProxy> nonEphemeralWebPageProxy();

#if ENABLE(ATTACHMENT_ELEMENT)
    RefPtr<API::Attachment> attachmentForIdentifier(const String& identifier) const;
    void insertAttachment(Ref<API::Attachment>&&, CompletionHandler<void()>&&);
    void updateAttachmentAttributes(const API::Attachment&, CompletionHandler<void()>&&);
    void serializedAttachmentDataForIdentifiers(const Vector<String>&, CompletionHandler<void(Vector<WebCore::SerializedAttachmentData>&&)>&&);
    void registerAttachmentIdentifier(const String&);
    void didInvalidateDataForAttachment(API::Attachment&);
    enum class ShouldUpdateAttachmentAttributes : bool { No, Yes };
    ShouldUpdateAttachmentAttributes willUpdateAttachmentAttributes(const API::Attachment&);
#endif

#if ENABLE(ATTACHMENT_ELEMENT) && HAVE(QUICKLOOK_THUMBNAILING)
    void updateAttachmentThumbnail(const String&, const RefPtr<ShareableBitmap>&);
    void requestThumbnailWithPath(const String&, const String&);
    void requestThumbnail(const API::Attachment&, const String&);
#endif

#if ENABLE(APPLICATION_MANIFEST)
    void getApplicationManifest(CompletionHandler<void(const std::optional<WebCore::ApplicationManifest>&)>&&);
#endif

    void getTextFragmentMatch(CompletionHandler<void(const String&)>&&);

    const WebPreferencesStore& preferencesStore() const;

    void setDefersLoadingForTesting(bool);

    bool isPageOpenedByDOMShowingInitialEmptyDocument() const;

    WebCore::IntRect syncRootViewToScreen(const WebCore::IntRect& viewRect);

#if ENABLE(DATALIST_ELEMENT)
    void didSelectOption(const String&);
    void didCloseSuggestions();
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    void didChooseDate(StringView);
    void didEndDateTimePicker();
#endif

    void updateCurrentModifierState();

    ProvisionalPageProxy* provisionalPageProxy() const { return m_provisionalPage.get(); }
    void commitProvisionalPage(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool privateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent, WebCore::MouseEventPolicy, const UserData&);
    void destroyProvisionalPage();

    // Logic shared between the WebPageProxy and the ProvisionalPageProxy.
    void didStartProvisionalLoadForFrameShared(Ref<WebProcessProxy>&&, WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, URL&&, URL&& unreachableURL, const UserData&);
    void didFailProvisionalLoadForFrameShared(Ref<WebProcessProxy>&&, WebFrameProxy&, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError&, WebCore::WillContinueLoading, const UserData&, WebCore::WillInternallyHandleFailure);
    void didReceiveServerRedirectForProvisionalLoadForFrameShared(Ref<WebProcessProxy>&&, WebCore::FrameIdentifier, uint64_t navigationID, WebCore::ResourceRequest&&, const UserData&);
    void didPerformServerRedirectShared(Ref<WebProcessProxy>&&, const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didPerformClientRedirectShared(Ref<WebProcessProxy>&&, const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didNavigateWithNavigationDataShared(Ref<WebProcessProxy>&&, const WebNavigationDataStore&, WebCore::FrameIdentifier);
    void didChangeProvisionalURLForFrameShared(Ref<WebProcessProxy>&&, WebCore::FrameIdentifier, uint64_t navigationID, URL&&);
    void decidePolicyForNavigationActionAsyncShared(Ref<WebProcessProxy>&&, WebCore::PageIdentifier, FrameInfoData&&, uint64_t navigationID, NavigationActionData&&, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&&, IPC::FormDataReference&& requestBody, CompletionHandler<void(PolicyDecision&&)>&&);
    void decidePolicyForResponseShared(Ref<WebProcessProxy>&&, WebCore::PageIdentifier, FrameInfoData&&, uint64_t navigationID, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, const String& downloadAttribute, CompletionHandler<void(PolicyDecision&&)>&&);
    void startURLSchemeTaskShared(Ref<WebProcessProxy>&&, WebCore::PageIdentifier, URLSchemeTaskParameters&&);
    void loadDataWithNavigationShared(Ref<WebProcessProxy>&&, WebCore::PageIdentifier, API::Navigation&, const IPC::DataReference&, const String& MIMEType, const String& encoding, const String& baseURL, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain>, std::optional<WebsitePoliciesData>&&, WebCore::ShouldOpenExternalURLsPolicy, WebCore::SessionHistoryVisibility);
    void loadRequestWithNavigationShared(Ref<WebProcessProxy>&&, WebCore::PageIdentifier, API::Navigation&, WebCore::ResourceRequest&&, WebCore::ShouldOpenExternalURLsPolicy, API::Object* userData, WebCore::ShouldTreatAsContinuingLoad, std::optional<NavigatingToAppBoundDomain>, std::optional<WebsitePoliciesData>&&, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume);
    void backForwardAddItemShared(Ref<WebProcessProxy>&&, BackForwardListItemState&&);
    void backForwardGoToItemShared(Ref<WebProcessProxy>&&, const WebCore::BackForwardItemIdentifier&, CompletionHandler<void(const WebBackForwardListCounts&)>&&);
    void decidePolicyForNavigationActionSyncShared(Ref<WebProcessProxy>&&, WebCore::PageIdentifier, FrameInfoData&&, uint64_t navigationID, NavigationActionData&&, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&&, IPC::FormDataReference&& requestBody, CompletionHandler<void(PolicyDecision&&)>&&);
    void didDestroyNavigationShared(Ref<WebProcessProxy>&&, uint64_t navigationID);
#if USE(QUICK_LOOK)
    void requestPasswordForQuickLookDocumentInMainFrameShared(const String& fileName, CompletionHandler<void(const String&)>&&);
#endif
#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoadForFrameShared(Ref<WebProcessProxy>&&, const WebCore::ContentFilterUnblockHandler&, WebCore::FrameIdentifier);
#endif
    void handleMessageShared(const Ref<WebProcessProxy>&, const String& messageName, const WebKit::UserData&);

    void dumpPrivateClickMeasurement(CompletionHandler<void(const String&)>&&);
    void clearPrivateClickMeasurement(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementOverrideTimerForTesting(bool value, CompletionHandler<void()>&&);
    void markAttributedPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementEphemeralMeasurementForTesting(bool value, CompletionHandler<void()>&&);
    void simulatePrivateClickMeasurementSessionRestart(CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenPublicKeyURLForTesting(const URL&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementTokenSignatureURLForTesting(const URL&, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementAttributionReportURLsForTesting(const URL& sourceURL, const URL& destinationURL, CompletionHandler<void()>&&);
    void markPrivateClickMeasurementsAsExpiredForTesting(CompletionHandler<void()>&&);
    void setPCMFraudPreventionValuesForTesting(const String& unlinkableToken, const String& secretToken, const String& signature, const String& keyID, CompletionHandler<void()>&&);
    void setPrivateClickMeasurementAppBundleIDForTesting(const String& appBundleIDForTesting, CompletionHandler<void()>&&);

#if ENABLE(SPEECH_SYNTHESIS)
    void speechSynthesisVoiceList(CompletionHandler<void(Vector<WebSpeechSynthesisVoice>&&)>&&);
    void speechSynthesisSpeak(const String&, const String&, float volume, float rate, float pitch, MonotonicTime startTime, const String& voiceURI, const String& voiceName, const String& voiceLang, bool localService, bool defaultVoice, CompletionHandler<void()>&&);
    void speechSynthesisSetFinishedCallback(CompletionHandler<void()>&&);
    void speechSynthesisCancel();
    void speechSynthesisPause(CompletionHandler<void()>&&);
    void speechSynthesisResume(CompletionHandler<void()>&&);
    void speechSynthesisResetState();
#endif

    void configureLoggingChannel(const String&, WTFLogChannelState, WTFLogLevel);

    void addDidMoveToWindowObserver(WebViewDidMoveToWindowObserver&);
    void removeDidMoveToWindowObserver(WebViewDidMoveToWindowObserver&);
    void webViewDidMoveToWindow();

#if HAVE(APP_SSO)
    void setShouldSuppressSOAuthorizationInNextNavigationPolicyDecision() { m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision = true; }
    void decidePolicyForSOAuthorizationLoad(const String&, CompletionHandler<void(SOAuthorizationLoadPolicy)>&&);
#endif

    Logger& logger();
    const void* logIdentifier() const;

    // IPC::MessageReceiver
    // Implemented in generated WebPageProxyMessageReceiver.cpp
    void didReceiveMessage(IPC::Connection&, IPC::Decoder&) final;
    bool didReceiveSyncMessage(IPC::Connection&, IPC::Decoder&, UniqueRef<IPC::Encoder>&) final;

    void requestStorageSpace(WebCore::FrameIdentifier, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, WTF::CompletionHandler<void(uint64_t)>&&);

    URL currentResourceDirectoryURL() const;

#if ENABLE(MEDIA_STREAM)
    void setMockCaptureDevicesEnabledOverride(std::optional<bool>);
    void willStartCapture(const UserMediaPermissionRequestProxy&, CompletionHandler<void()>&&);
#endif

    void maybeInitializeSandboxExtensionHandle(WebProcessProxy&, const URL&, const URL& resourceDirectoryURL, SandboxExtensionHandle&, bool checkAssumedReadAccessToResourceURL = true);

#if ENABLE(WEB_AUTHN)
    void setMockWebAuthenticationConfiguration(WebCore::MockWebAuthenticationConfiguration&&);
#endif

    using TextManipulationItemCallback = Function<void(const Vector<WebCore::TextManipulationItem>&)>;
    void startTextManipulations(const Vector<WebCore::TextManipulationControllerExclusionRule>&, bool includeSubframes, TextManipulationItemCallback&&, WTF::CompletionHandler<void()>&&);
    void didFindTextManipulationItems(const Vector<WebCore::TextManipulationItem>&);
    void completeTextManipulation(const Vector<WebCore::TextManipulationItem>&, Function<void(bool allFailed, const Vector<WebCore::TextManipulationControllerManipulationFailure>&)>&&);

    const String& overriddenMediaType() const { return m_overriddenMediaType; }
    void setOverriddenMediaType(const String&);

    void setCORSDisablingPatterns(Vector<String>&&);
    const Vector<String>& corsDisablingPatterns() const { return m_corsDisablingPatterns; }

    void getProcessDisplayName(CompletionHandler<void(String&&)>&&);

    void setOrientationForMediaCapture(WebCore::IntDegrees);

#if ENABLE(MEDIA_STREAM) && USE(GSTREAMER)
    void setMockCaptureDevicesInterrupted(bool isCameraInterrupted, bool isMicrophoneInterrupted);
#endif

    bool isHandlingPreventableTouchStart() const { return m_handlingPreventableTouchStartCount; }
    bool isHandlingPreventableTouchMove() const { return m_touchMovePreventionState == EventPreventionState::Waiting; }
    bool isHandlingPreventableTouchEnd() const { return m_handlingPreventableTouchEndCount; }

    bool hasQueuedKeyEvent() const;
    const NativeWebKeyboardEvent& firstQueuedKeyEvent() const;

    void grantAccessToAssetServices();
    void revokeAccessToAssetServices();
    void switchFromStaticFontRegistryToUserFontRegistry();

    void disableURLSchemeCheckInDataDetectors() const;

    void setIsTakingSnapshotsForApplicationSuspension(bool);
    void setNeedsDOMWindowResizeEvent();

#if ENABLE(APP_BOUND_DOMAINS)
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain() const { return m_isNavigatingToAppBoundDomain; }
    void isNavigatingToAppBoundDomainTesting(CompletionHandler<void(bool)>&&);
    void isForcedIntoAppBoundModeTesting(CompletionHandler<void(bool)>&&);
    std::optional<NavigatingToAppBoundDomain> isTopFrameNavigatingToAppBoundDomain() const { return m_isTopFrameNavigatingToAppBoundDomain; }
#else
    std::optional<NavigatingToAppBoundDomain> isNavigatingToAppBoundDomain() const { return std::nullopt; }
#endif

#if PLATFORM(COCOA)
    WebCore::ResourceError errorForUnpermittedAppBoundDomainNavigation(const URL&);
#endif

    void disableServiceWorkerEntitlementInNetworkProcess();
    void clearServiceWorkerEntitlementOverride(CompletionHandler<void()>&&);
        
#if PLATFORM(COCOA)
    void grantAccessToCurrentPasteboardData(const String& pasteboardName);
#endif

#if PLATFORM(MAC)
    NSMenu *activeContextMenu() const;
    RetainPtr<NSEvent> createSyntheticEventForContextMenu(WebCore::FloatPoint) const;
#endif

#if ENABLE(CONTEXT_MENUS)
    void platformDidSelectItemFromActiveContextMenu(const WebContextMenuItemData&);
#endif

#if ENABLE(MEDIA_USAGE)
    MediaUsageManager& mediaUsageManager();
    void addMediaUsageManagerSession(WebCore::MediaSessionIdentifier, const String&, const URL&);
    void updateMediaUsageManagerSessionState(WebCore::MediaSessionIdentifier, const WebCore::MediaUsageInfo&);
    void removeMediaUsageManagerSession(WebCore::MediaSessionIdentifier);
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void failedToEnterFullscreen(PlaybackSessionContextIdentifier);
    void didEnterFullscreen(PlaybackSessionContextIdentifier);
    void didExitFullscreen(PlaybackSessionContextIdentifier);
    void didChangePlaybackRate(PlaybackSessionContextIdentifier);
    void didChangeCurrentTime(PlaybackSessionContextIdentifier);
#else
    void didEnterFullscreen();
    void didExitFullscreen();
#endif

    void setHasExecutedAppBoundBehaviorBeforeNavigation() { m_hasExecutedAppBoundBehaviorBeforeNavigation = true; }

    WebPopupMenuProxy* activePopupMenu() const { return m_activePopupMenu.get(); }

    void preconnectTo(WebCore::ResourceRequest&&);

    bool canUseCredentialStorage() { return m_canUseCredentialStorage; }
    void setCanUseCredentialStorage(bool);

#if ENABLE(PDF_HUD)
    void createPDFHUD(PDFPluginIdentifier, const WebCore::IntRect&);
    void updatePDFHUDLocation(PDFPluginIdentifier, const WebCore::IntRect&);
    void removePDFHUD(PDFPluginIdentifier);
#endif

#if ENABLE(PDF_PLUGIN) && PLATFORM(MAC)
    void pdfZoomIn(PDFPluginIdentifier);
    void pdfZoomOut(PDFPluginIdentifier);
    void pdfSaveToPDF(PDFPluginIdentifier);
    void pdfOpenWithPreview(PDFPluginIdentifier);
#endif

    Seconds mediaCaptureReportingDelay() const { return m_mediaCaptureReportingDelay; }
    void setMediaCaptureReportingDelay(Seconds captureReportingDelay) { m_mediaCaptureReportingDelay = captureReportingDelay; }
    size_t suspendMediaPlaybackCounter() { return m_suspendMediaPlaybackCounter; }

    void requestSpeechRecognitionPermission(WebCore::SpeechRecognitionRequest&, SpeechRecognitionPermissionRequestCallback&&);
    void requestSpeechRecognitionPermissionByDefaultAction(const WebCore::SecurityOriginData&, CompletionHandler<void(bool)>&&);
    void requestUserMediaPermissionForSpeechRecognition(WebCore::FrameIdentifier, const WebCore::SecurityOrigin&, const WebCore::SecurityOrigin&, CompletionHandler<void(bool)>&&);

    void requestMediaKeySystemPermissionByDefaultAction(const WebCore::SecurityOriginData&, CompletionHandler<void(bool)>&&);

    void syncIfMockDevicesEnabledChanged();

#if ENABLE(APP_HIGHLIGHTS)
    void createAppHighlightInSelectedRange(WebCore::CreateNewGroupForHighlight, WebCore::HighlightRequestOriginatedInApp);
    void storeAppHighlight(const WebCore::AppHighlight&);
    void restoreAppHighlightsAndScrollToIndex(const Vector<Ref<SharedMemory>>& highlights, const std::optional<unsigned> index);
    void setAppHighlightsVisibility(const WebCore::HighlightVisibility);
    bool appHighlightsVisibility();
    CGRect appHighlightsOverlayRect();
#endif

#if ENABLE(MEDIA_STREAM)
    WebCore::CaptureSourceOrError createRealtimeMediaSourceForSpeechRecognition();
    void clearUserMediaPermissionRequestHistory(WebCore::PermissionName);
#endif

#if PLATFORM(COCOA) && ENABLE(MEDIA_STREAM)
    void setIndexOfGetDisplayMediaDeviceSelectedForTesting(std::optional<unsigned>);
    void setSystemCanPromptForGetDisplayMediaForTesting(bool);
#endif

#if PLATFORM(MAC)
    void changeUniversalAccessZoomFocus(const WebCore::IntRect&, const WebCore::IntRect&);

    bool acceptsFirstMouse(int eventNumber, const WebMouseEvent&);
#endif

    bool isServiceWorkerPage() const { return m_isServiceWorkerPage; }

#if PLATFORM(IOS_FAMILY)
    void dispatchWheelEventWithoutScrolling(const WebWheelEvent&, CompletionHandler<void(bool)>&&);
#endif

#if ENABLE(CONTEXT_MENUS) && ENABLE(IMAGE_ANALYSIS)
    void handleContextMenuLookUpImage();
#endif

#if ENABLE(CONTEXT_MENUS) && ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    void handleContextMenuCopySubject(const String& preferredMIMEType);
#endif

#if USE(APPKIT)
    void beginPreviewPanelControl(QLPreviewPanel *);
    void endPreviewPanelControl(QLPreviewPanel *);
    void closeSharedPreviewPanelIfNecessary();
#endif

#if HAVE(TRANSLATION_UI_SERVICES) && ENABLE(CONTEXT_MENUS)
    bool canHandleContextMenuTranslation() const;
    void handleContextMenuTranslation(const WebCore::TranslationContextMenuInfo&);
#endif

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    void createMediaSessionCoordinator(Ref<MediaSessionCoordinatorProxyPrivate>&&, CompletionHandler<void(bool)>&&);
#endif

    bool lastNavigationWasAppInitiated() const { return m_lastNavigationWasAppInitiated; }

#if PLATFORM(COCOA)
    void setLastNavigationWasAppInitiated(WebCore::ResourceRequest&);
    void lastNavigationWasAppInitiated(CompletionHandler<void(bool)>&&);
    void appPrivacyReportTestingData(CompletionHandler<void(const AppPrivacyReportTestingData&)>&&);
    void clearAppPrivacyReportTestingData(CompletionHandler<void()>&&);

    NSDictionary *contentsOfUserInterfaceItem(NSString *userInterfaceItem);

    RetainPtr<WKWebView> cocoaView();
    void setCocoaView(WKWebView *);
#endif

    bool shouldAvoidSynchronouslyWaitingToPreventDeadlock() const;

#if ENABLE(IMAGE_ANALYSIS) && PLATFORM(MAC)
    WKQuickLookPreviewController *quickLookPreviewController() const { return m_quickLookPreviewController.get(); }
#endif

    WebProcessProxy* processForRegistrableDomain(const WebCore::RegistrableDomain&);
    RemotePageProxy* remotePageProxyForRegistrableDomain(const WebCore::RegistrableDomain&) const;
    void addRemotePageProxy(const WebCore::RegistrableDomain&, RemotePageProxy&);
    void removeRemotePageProxy(const WebCore::RegistrableDomain&);
    void setRemotePageProxyInOpenerProcess(Ref<RemotePageProxy>&&);
    RefPtr<RemotePageProxy> takeRemotePageProxyInOpenerProcessIfDomainEquals(const WebCore::RegistrableDomain&);
    void addOpenedRemotePageProxy(WebPageProxyIdentifier, Ref<RemotePageProxy>&&);
    void removeOpenedRemotePageProxy(WebPageProxyIdentifier);

    void createRemoteSubframesInOtherProcesses(WebFrameProxy&, const String& frameName);
    void broadcastFrameRemovalToOtherProcesses(IPC::Connection&, WebCore::FrameIdentifier);
    void broadcastMainFrameURLChangeToOtherProcesses(IPC::Connection&, const URL&);

    void addOpenedPage(WebPageProxy&);
    bool hasOpenedPage() const;

    void requestImageBitmap(const WebCore::ElementContext&, CompletionHandler<void(std::optional<ShareableBitmapHandle>&&, const String& sourceMIMEType)>&&);

#if PLATFORM(MAC)
    bool isQuarantinedAndNotUserApproved(const String&);
#endif

    void showNotification(IPC::Connection&, const WebCore::NotificationData&, RefPtr<WebCore::NotificationResources>&&);
    void cancelNotification(const WTF::UUID& notificationID);
    void clearNotifications(const Vector<WTF::UUID>& notificationIDs);
    void didDestroyNotification(const WTF::UUID& notificationID);
    void pageWillLikelyUseNotifications();

#if USE(SYSTEM_PREVIEW)
    void beginSystemPreview(const URL&, const WebCore::SecurityOriginData& topOrigin, const WebCore::SystemPreviewInfo&, CompletionHandler<void()>&&);
    void setSystemPreviewCompletionHandlerForLoadTesting(CompletionHandler<void(bool)>&&);
#endif

    void requestCookieConsent(CompletionHandler<void(WebCore::CookieConsentDecisionResult)>&&);

    void setServiceWorkerOpenWindowCompletionCallback(CompletionHandler<void(std::optional<WebCore::PageIdentifier>)>&&);

#if ENABLE(VIDEO)
    void beginTextRecognitionForVideoInElementFullScreen(WebCore::MediaPlayerIdentifier, WebCore::FloatRect videoBounds);
    void cancelTextRecognitionForVideoInElementFullScreen();
#endif

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    void replaceImageForRemoveBackground(const WebCore::ElementContext&, const Vector<String>& types, const IPC::DataReference&);
    void shouldAllowRemoveBackground(const WebCore::ElementContext&, CompletionHandler<void(bool)>&&);
#endif

#if HAVE(UIKIT_RESIZABLE_WINDOWS)
    void setIsWindowResizingEnabled(bool);
#endif

#if ENABLE(ATTACHMENT_ELEMENT) && PLATFORM(MAC)
    bool updateIconForDirectory(NSFileWrapper *, const String&);
#endif

#if ENABLE(NOTIFICATIONS)
    void clearNotificationPermissionState();
#endif

#if ENABLE(INTERACTION_REGIONS_IN_EVENT_REGION)
    void setInteractionRegionsEnabled(bool);
#endif

    void queryPermission(const WebCore::ClientOrigin&, const WebCore::PermissionDescriptor&, CompletionHandler<void(std::optional<WebCore::PermissionState>)>&&);

    void generateTestReport(const String& message, const String& group);

    void frameCreated(WebCore::FrameIdentifier, WebFrameProxy&);
    void didDestroyFrame(WebCore::FrameIdentifier);
    void disconnectFramesFromPage();

    void didCommitLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& mimeType, bool frameHasCustomContentProvider, WebCore::FrameLoadType, const WebCore::CertificateInfo&, bool usedLegacyTLS, bool wasPrivateRelayed, bool containsPluginDocument, WebCore::HasInsecureContent, WebCore::MouseEventPolicy, const UserData&);

    void didCreateSleepDisabler(WebCore::SleepDisablerIdentifier, const String& reason, bool display);
    void didDestroySleepDisabler(WebCore::SleepDisablerIdentifier);

#if ENABLE(NETWORK_ISSUE_REPORTING)
    void reportNetworkIssue(const URL&);
#endif

    void useRedirectionForCurrentNavigation(const WebCore::ResourceResponse&);

#if ENABLE(ACCESSIBILITY_ANIMATION_CONTROL)
    void pauseAllAnimations(CompletionHandler<void()>&&);
    void playAllAnimations(CompletionHandler<void()>&&);
    bool allowsAnyAnimationToPlay() { return m_allowsAnyAnimationToPlay; }
    void isAnyAnimationAllowedToPlayDidChange(bool anyAnimationCanPlay) { m_allowsAnyAnimationToPlay = anyAnimationCanPlay; }
#endif
    String scrollbarStateForScrollingNodeID(int scrollingNodeID, bool isVertical);

#if ENABLE(WEBXR) && !USE(OPENXR)
    PlatformXRSystem* xrSystem() const;
    void restartXRSessionActivityOnProcessResumeIfNeeded();
#endif

#if ENABLE(INPUT_TYPE_COLOR)
    WebColorPickerClient& colorPickerClient();
#endif

    WebPopupMenuProxyClient& popupMenuClient();

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    OptionSet<WebCore::AdvancedPrivacyProtections> advancedPrivacyProtectionsPolicies() const { return m_advancedPrivacyProtectionsPolicies; }
#endif

#if PLATFORM(WPE) && USE(GBM) && ENABLE(WPE_PLATFORM)
    void preferredBufferFormatsDidChange();
#endif

    WebPageProxyMessageReceiverRegistration& messageReceiverRegistration();

#if HAVE(ESIM_AUTOFILL_SYSTEM_SUPPORT)
    bool shouldAllowAutoFillForCellularIdentifiers() const;
#endif

#if ENABLE(EXTENSION_CAPABILITIES)
    const std::optional<MediaCapability>& mediaCapability() const;
    void updateMediaCapability();
#endif

private:
    WebPageProxy(PageClient&, WebProcessProxy&, Ref<API::PageConfiguration>&&);
    void platformInitialize();

    void addAllMessageReceivers();
    void removeAllMessageReceivers();

    void notifyProcessPoolToPrewarm();
    bool shouldUseBackForwardCache() const;

    bool attachmentElementEnabled();
    bool modelElementEnabled();

    void forEachWebContentProcess(Function<void(WebProcessProxy&, WebCore::PageIdentifier)>&&);

    bool shouldForceForegroundPriorityForClientNavigation() const;

    bool canCreateFrame(WebCore::FrameIdentifier) const;

    RefPtr<API::Navigation> goToBackForwardItem(WebBackForwardListItem&, WebCore::FrameLoadType);

    void updateActivityState();
    void updateActivityState(OptionSet<WebCore::ActivityState> flagsToUpdate);
    void updateThrottleState();
    void updateHiddenPageThrottlingAutoIncreases();

    bool suspendCurrentPageIfPossible(API::Navigation&, RefPtr<WebFrameProxy>&& mainFrame, ProcessSwapRequestedByClient, ShouldDelayClosingUntilFirstLayerFlush);

    enum class ResetStateReason {
        PageInvalidated,
        WebProcessExited,
        NavigationSwap,
    };
    void resetState(ResetStateReason);
    void resetStateAfterProcessExited(ProcessTerminationReason);

    void setUserAgent(String&&);

    // IPC::MessageSender
    bool sendMessage(UniqueRef<IPC::Encoder>&&, OptionSet<IPC::SendOption>) final;
    bool sendMessageWithAsyncReply(UniqueRef<IPC::Encoder>&&, AsyncReplyHandler, OptionSet<IPC::SendOption>) final;

    IPC::Connection* messageSenderConnection() const final;
    uint64_t messageSenderDestinationID() const final;

#if ENABLE(POINTER_LOCK)
    void requestPointerLock();
    void requestPointerUnlock();
#endif

    void didCreateMainFrame(WebCore::FrameIdentifier);
    void didCreateSubframe(WebCore::FrameIdentifier parent, WebCore::FrameIdentifier newFrameID, const String& frameName);

    void didStartProvisionalLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, URL&&, URL&& unreachableURL, const UserData&);
    void didReceiveServerRedirectForProvisionalLoadForFrame(WebCore::FrameIdentifier, uint64_t navigationID, WebCore::ResourceRequest&&, const UserData&);
    void willPerformClientRedirectForFrame(WebCore::FrameIdentifier, const String& url, double delay, WebCore::LockBackForwardList);
    void didCancelClientRedirectForFrame(WebCore::FrameIdentifier);
    void didChangeProvisionalURLForFrame(WebCore::FrameIdentifier, uint64_t navigationID, URL&&);
    void didFailProvisionalLoadForFrame(FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const String& provisionalURL, const WebCore::ResourceError&, WebCore::WillContinueLoading, const UserData&, WebCore::WillInternallyHandleFailure);
    void didFinishDocumentLoadForFrame(WebCore::FrameIdentifier, uint64_t navigationID, const UserData&);
    void didFinishLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const UserData&);
    void didFailLoadForFrame(WebCore::FrameIdentifier, FrameInfoData&&, WebCore::ResourceRequest&&, uint64_t navigationID, const WebCore::ResourceError&, const UserData&);
    void didSameDocumentNavigationForFrame(WebCore::FrameIdentifier, uint64_t navigationID, SameDocumentNavigationType, URL&&, const UserData&);
    void didSameDocumentNavigationForFrameViaJSHistoryAPI(WebCore::FrameIdentifier, SameDocumentNavigationType, URL, NavigationActionData&&, const UserData&);
    void didChangeMainDocument(WebCore::FrameIdentifier);
    void didExplicitOpenForFrame(WebCore::FrameIdentifier, URL&&, String&& mimeType);

    void didReceiveTitleForFrame(WebCore::FrameIdentifier, const String&, const UserData&);
    void didFirstLayoutForFrame(WebCore::FrameIdentifier, const UserData&);
    void didFirstVisuallyNonEmptyLayoutForFrame(WebCore::FrameIdentifier, const UserData&);
    void didDisplayInsecureContentForFrame(WebCore::FrameIdentifier, const UserData&);
    void didRunInsecureContentForFrame(WebCore::FrameIdentifier, const UserData&);
    void mainFramePluginHandlesPageScaleGestureDidChange(bool);
    void didStartProgress();
    void didChangeProgress(double);
    void didFinishProgress();
    void setNetworkRequestsInProgress(bool);

    void updateRemoteFrameSize(WebCore::FrameIdentifier, WebCore::IntSize);

    void didDestroyNavigation(uint64_t navigationID);

    void decidePolicyForNavigationAction(Ref<WebProcessProxy>&&, WebCore::PageIdentifier, WebFrameProxy&, FrameInfoData&&, uint64_t navigationID, NavigationActionData&&, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&&, IPC::FormDataReference&& requestBody, Ref<PolicyDecisionSender>&&);
    void decidePolicyForNavigationActionAsync(FrameInfoData&&, uint64_t navigationID, NavigationActionData&&, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&&, IPC::FormDataReference&& requestBody, CompletionHandler<void(PolicyDecision&&)>&&);
    void decidePolicyForNavigationActionSync(FrameInfoData&&, uint64_t navigationID, NavigationActionData&&, FrameInfoData&& originatingFrameInfo, std::optional<WebPageProxyIdentifier> originatingPageID, const WebCore::ResourceRequest& originalRequest, WebCore::ResourceRequest&&, IPC::FormDataReference&& requestBody, CompletionHandler<void(PolicyDecision&&)>&&);
    void decidePolicyForNewWindowAction(FrameInfoData&&, NavigationActionData&&, WebCore::ResourceRequest&&, const String& frameName, CompletionHandler<void(PolicyDecision&&)>&&);
    void decidePolicyForResponse(IPC::Connection&, FrameInfoData&&, uint64_t navigationID, const WebCore::ResourceResponse&, const WebCore::ResourceRequest&, bool canShowMIMEType, const String& downloadAttribute, CompletionHandler<void(PolicyDecision&&)>&&);
    void beginSafeBrowsingCheck(const URL&, bool, WebFramePolicyListenerProxy&);

    WebContentMode effectiveContentModeAfterAdjustingPolicies(API::WebsitePolicies&, const WebCore::ResourceRequest&);

    void willSubmitForm(WebCore::FrameIdentifier, WebCore::FrameIdentifier sourceFrameID, const Vector<std::pair<String, String>>& textFieldValues, const UserData&, CompletionHandler<void()>&&);

#if ENABLE(CONTENT_EXTENSIONS)
    void contentRuleListNotification(URL&&, WebCore::ContentRuleListResults&&);
#endif

    // History client
    void didNavigateWithNavigationData(const WebNavigationDataStore&, WebCore::FrameIdentifier);
    void didPerformClientRedirect(const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didPerformServerRedirect(const String& sourceURLString, const String& destinationURLString, WebCore::FrameIdentifier);
    void didUpdateHistoryTitle(const String& title, const String& url, WebCore::FrameIdentifier);

    // UI client
    void createNewPage(FrameInfoData&&, WebPageProxyIdentifier originatingPageID, WebCore::ResourceRequest&&, WebCore::WindowFeatures&&, NavigationActionData&&, CompletionHandler<void(std::optional<WebCore::PageIdentifier>, std::optional<WebPageCreationParameters>)>&&);
    void showPage();
    void runJavaScriptAlert(WebCore::FrameIdentifier, FrameInfoData&&, const String&, CompletionHandler<void()>&&);
    void runJavaScriptConfirm(WebCore::FrameIdentifier, FrameInfoData&&, const String&, CompletionHandler<void(bool)>&&);
    void runJavaScriptPrompt(WebCore::FrameIdentifier, FrameInfoData&&, const String&, const String&, CompletionHandler<void(const String&)>&&);
    void setStatusText(const String&);
    void mouseDidMoveOverElement(WebHitTestResultData&&, OptionSet<WebEventModifier>, UserData&&);

    void setToolbarsAreVisible(bool toolbarsAreVisible);
    void getToolbarsAreVisible(CompletionHandler<void(bool)>&&);
    void setMenuBarIsVisible(bool menuBarIsVisible);
    void getMenuBarIsVisible(CompletionHandler<void(bool)>&&);
    void setStatusBarIsVisible(bool statusBarIsVisible);
    void getStatusBarIsVisible(CompletionHandler<void(bool)>&&);
    void getIsViewVisible(bool&);
    void setIsResizable(bool isResizable);
    void screenToRootView(const WebCore::IntPoint& screenPoint, CompletionHandler<void(const WebCore::IntPoint&)>&&);
    void rootViewToScreen(const WebCore::IntRect& viewRect, CompletionHandler<void(const WebCore::IntRect&)>&&);
    void accessibilityScreenToRootView(const WebCore::IntPoint& screenPoint, CompletionHandler<void(WebCore::IntPoint)>&&);
    void rootViewToAccessibilityScreen(const WebCore::IntRect& viewRect, CompletionHandler<void(WebCore::IntRect)>&&);
#if PLATFORM(IOS_FAMILY)
    void relayAccessibilityNotification(const String&, const IPC::DataReference&);
#endif
    void runBeforeUnloadConfirmPanel(WebCore::FrameIdentifier, FrameInfoData&&, const String& message, CompletionHandler<void(bool)>&&);
    void didChangeViewportProperties(const WebCore::ViewportAttributes&);
    void pageDidScroll(const WebCore::IntPoint&);
    void runOpenPanel(WebCore::FrameIdentifier, FrameInfoData&&, const WebCore::FileChooserSettings&);
    bool didChooseFilesForOpenPanelWithImageTranscoding(const Vector<String>& fileURLs, const Vector<String>& allowedMIMETypes);
    void showShareSheet(const WebCore::ShareDataWithParsedURL&, CompletionHandler<void(bool)>&&);
    void showContactPicker(const WebCore::ContactsRequestData&, CompletionHandler<void(std::optional<Vector<WebCore::ContactInfo>>&&)>&&);
    void printFrame(WebCore::FrameIdentifier, const String&, const WebCore::FloatSize&,  CompletionHandler<void()>&&);
    void exceededDatabaseQuota(WebCore::FrameIdentifier, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, CompletionHandler<void(uint64_t)>&&);
    void reachedApplicationCacheOriginQuota(const String& originIdentifier, uint64_t currentQuota, uint64_t totalBytesNeeded, CompletionHandler<void(uint64_t)>&&);

    void requestGeolocationPermissionForFrame(GeolocationIdentifier, FrameInfoData&&);
    void revokeGeolocationAuthorizationToken(const String& authorizationToken);

#if PLATFORM(GTK) || PLATFORM(WPE)
    void sendMessageToWebView(UserMessage&&);
    void sendMessageToWebViewWithReply(UserMessage&&, CompletionHandler<void(UserMessage&&)>&&);

    void didInitiateLoadForResource(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceRequest&&);
    void didSendRequestForResource(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceRequest&&, WebCore::ResourceResponse&&);
    void didReceiveResponseForResource(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceResponse&&);
    void didFinishLoadForResource(WebCore::ResourceLoaderIdentifier, WebCore::FrameIdentifier, WebCore::ResourceError&&);
#endif

    bool shouldClosePreviousPage();

#if ENABLE(MEDIA_STREAM)
    UserMediaPermissionRequestManagerProxy& userMediaPermissionRequestManager();
#endif
    void requestUserMediaPermissionForFrame(WebCore::UserMediaRequestIdentifier, WebCore::FrameIdentifier, const WebCore::SecurityOriginData& userMediaDocumentOriginIdentifier, const WebCore::SecurityOriginData& topLevelDocumentOriginIdentifier, WebCore::MediaStreamRequest&&);
    void enumerateMediaDevicesForFrame(WebCore::FrameIdentifier, const WebCore::SecurityOriginData& userMediaDocumentOriginData, const WebCore::SecurityOriginData& topLevelDocumentOriginData, CompletionHandler<void(const Vector<WebCore::CaptureDeviceWithCapabilities>&, WebCore::MediaDeviceHashSalts&&)>&&);
    void beginMonitoringCaptureDevices();

#if ENABLE(ENCRYPTED_MEDIA)
    MediaKeySystemPermissionRequestManagerProxy& mediaKeySystemPermissionRequestManager();
#endif
    void requestMediaKeySystemPermissionForFrame(WebCore::MediaKeySystemRequestIdentifier, WebCore::FrameIdentifier, const WebCore::SecurityOriginData& topLevelDocumentOriginIdentifier, const String&);

    void runModal();
    void notifyScrollerThumbIsVisibleInRect(const WebCore::IntRect&);
    void recommendedScrollbarStyleDidChange(int32_t newStyle);
    void didChangeScrollbarsForMainFrame(bool hasHorizontalScrollbar, bool hasVerticalScrollbar);
    void didChangeScrollOffsetPinningForMainFrame(WebCore::RectEdges<bool>);
    void didChangePageCount(unsigned);
    void themeColorChanged(const WebCore::Color&);
    void pageExtendedBackgroundColorDidChange(const WebCore::Color&);
    void sampledPageTopColorChanged(const WebCore::Color&);
    WebCore::Color platformUnderPageBackgroundColor() const;
    void setCanShortCircuitHorizontalWheelEvents(bool canShortCircuitHorizontalWheelEvents) { m_canShortCircuitHorizontalWheelEvents = canShortCircuitHorizontalWheelEvents; }

    enum class ProcessLaunchReason {
        InitialProcess,
        ProcessSwap,
        Crash
    };

    void launchProcess(const WebCore::RegistrableDomain&, ProcessLaunchReason);
    void swapToProvisionalPage(std::unique_ptr<ProvisionalPageProxy>);
    void didFailToSuspendAfterProcessSwap();
    void didSuspendAfterProcessSwap();
    void finishAttachingToWebProcess(ProcessLaunchReason);

    RefPtr<API::Navigation> launchProcessForReload();

    void requestNotificationPermission(const String& originString, CompletionHandler<void(bool allowed)>&&);

    void didChangeContentSize(const WebCore::IntSize&);
    void didChangeIntrinsicContentSize(const WebCore::IntSize&);

#if ENABLE(INPUT_TYPE_COLOR)
    void showColorPicker(const WebCore::Color& initialColor, const WebCore::IntRect&, Vector<WebCore::Color>&&);
#endif

#if ENABLE(DATALIST_ELEMENT)
    void showDataListSuggestions(WebCore::DataListSuggestionInformation&&);
    void handleKeydownInDataList(const String&);
    void endDataListSuggestions();
#endif

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    void showDateTimePicker(WebCore::DateTimeChooserParameters&&);
    void endDateTimePicker();
#endif

    void closeOverlayedViews();

    void compositionWasCanceled();
    void setHasHadSelectionChangesFromUserInteraction(bool);

#if HAVE(TOUCH_BAR)
    void setIsTouchBarUpdateSupressedForHiddenContentEditable(bool);
    void setIsNeverRichlyEditableForTouchBar(bool);
#endif

    void requestDOMPasteAccess(WebCore::DOMPasteAccessCategory, const WebCore::IntRect&, const String&, CompletionHandler<void(WebCore::DOMPasteAccessResponse)>&&);
    void willPerformPasteCommand(WebCore::DOMPasteAccessCategory);

    // Back/Forward list management
    void backForwardAddItem(BackForwardListItemState&&);
    void backForwardGoToItem(const WebCore::BackForwardItemIdentifier&, CompletionHandler<void(const WebBackForwardListCounts&)>&&);
    void backForwardListContainsItem(const WebCore::BackForwardItemIdentifier&, CompletionHandler<void(bool)>&&);
    void backForwardItemAtIndex(int32_t index, CompletionHandler<void(std::optional<WebCore::BackForwardItemIdentifier>&&)>&&);
    void backForwardListCounts(CompletionHandler<void(WebBackForwardListCounts&&)>&&);
    void backForwardClear();

    // Undo management
    void registerEditCommandForUndo(WebUndoStepID commandID, const String& label);
    void registerInsertionUndoGrouping();
    void clearAllEditCommands();
    void canUndoRedo(UndoOrRedo, CompletionHandler<void(bool)>&&);
    void executeUndoRedo(UndoOrRedo, CompletionHandler<void()>&&);

    // Keyboard handling
#if PLATFORM(COCOA)
    void executeSavedCommandBySelector(const String& selector, CompletionHandler<void(bool)>&&);
#endif

#if PLATFORM(GTK)
    void getEditorCommandsForKeyEvent(const AtomString&, Vector<String>&);
#endif

#if PLATFORM(GTK) || PLATFORM(WPE)
    void bindAccessibilityTree(const String&);
    OptionSet<WebCore::PlatformEventModifier> currentStateOfModifierKeys();
#endif

#if PLATFORM(GTK)
    void showEmojiPicker(const WebCore::IntRect&, CompletionHandler<void(String)>&&);
#endif

#if PLATFORM(WPE) && USE(GBM)
    Vector<DMABufRendererBufferFormat> preferredBufferFormats() const;
#endif

    // Popup Menu.
    void showPopupMenu(const WebCore::IntRect& rect, uint64_t textDirection, const Vector<WebPopupItem>& items, int32_t selectedIndex, const PlatformPopupMenuData&);
    void hidePopupMenu();

#if ENABLE(CONTEXT_MENUS)
    void showContextMenu(ContextMenuContextData&&, const UserData&);
#endif

#if ENABLE(CONTEXT_MENU_EVENT)
    void processContextMenuCallbacks();
#endif

#if ENABLE(TELEPHONE_NUMBER_DETECTION) && PLATFORM(MAC)
    void showTelephoneNumberMenu(const String& telephoneNumber, const WebCore::IntPoint&, const WebCore::IntRect&);
#endif

    // Search popup results
    void saveRecentSearches(const String&, const Vector<WebCore::RecentSearch>&);
    void loadRecentSearches(const String&, CompletionHandler<void(Vector<WebCore::RecentSearch>&&)>&&);

#if PLATFORM(COCOA)
    // Speech.
    void getIsSpeaking(CompletionHandler<void(bool)>&&);
    void speak(const String&);
    void stopSpeaking();

    // Spotlight.
    void searchWithSpotlight(const String&);

    void searchTheWeb(const String&);

    // Dictionary.
    void didPerformDictionaryLookup(const WebCore::DictionaryPopupInfo&);
#endif

    void stopMakingViewBlankDueToLackOfRenderingUpdateIfNecessary();

    // Spelling and grammar.
    void checkSpellingOfString(const String& text, CompletionHandler<void(int32_t misspellingLocation, int32_t misspellingLength)>&&);
    void checkGrammarOfString(const String& text, CompletionHandler<void(Vector<WebCore::GrammarDetail>&&, int32_t badGrammarLocation, int32_t badGrammarLength)>&&);
    void spellingUIIsShowing(CompletionHandler<void(bool)>&&);
    void updateSpellingUIWithMisspelledWord(const String& misspelledWord);
    void updateSpellingUIWithGrammarString(const String& badGrammarPhrase, const WebCore::GrammarDetail&);
    void learnWord(const String& word);
    void ignoreWord(const String& word);
    void requestCheckingOfString(TextCheckerRequestID, const WebCore::TextCheckingRequestData&, int32_t insertionPoint);

    void takeFocus(WebCore::FocusDirection);
    void setToolTip(const String&);
    void setCursor(const WebCore::Cursor&);
    void setCursorHiddenUntilMouseMoves(bool);

    void discardQueuedMouseEvents();

    void mouseEventHandlingCompleted(std::optional<WebEventType>, bool handled);
    void keyEventHandlingCompleted(std::optional<WebEventType>, bool handled);
    void didReceiveEvent(WebEventType, bool handled);
    void didUpdateRenderingAfterCommittingLoad();
#if PLATFORM(IOS_FAMILY)
    void interpretKeyEvent(const EditorState&, bool isCharEvent, CompletionHandler<void(bool)>&&);
    void showPlaybackTargetPicker(bool hasVideo, const WebCore::IntRect& elementRect, WebCore::RouteSharingPolicy, const String&);

    void updateStringForFind(const String&);

    bool isValidPerformActionOnElementAuthorizationToken(const String& authorizationToken) const;
    bool isDesktopClassBrowsingRecommended(const WebCore::ResourceRequest&) const;
    bool useDesktopClassBrowsing(const API::WebsitePolicies&, const WebCore::ResourceRequest&) const;
    static WebCore::ScreenOrientationType toScreenOrientationType(WebCore::IntDegrees);
#endif

    void focusedFrameChanged(IPC::Connection&, const std::optional<WebCore::FrameIdentifier>&);

    void didFinishLoadingDataForCustomContentProvider(const String& suggestedFilename, const IPC::DataReference&);

#if USE(AUTOMATIC_TEXT_REPLACEMENT)
    void toggleSmartInsertDelete();
    void toggleAutomaticQuoteSubstitution();
    void toggleAutomaticLinkDetection();
    void toggleAutomaticDashSubstitution();
    void toggleAutomaticTextReplacement();
#endif

#if USE(DICTATION_ALTERNATIVES)
    void showDictationAlternativeUI(const WebCore::FloatRect& boundingBoxOfDictatedText, WebCore::DictationContext);
    void removeDictationAlternatives(WebCore::DictationContext);
    void dictationAlternatives(WebCore::DictationContext, CompletionHandler<void(Vector<String>&&)>&&);
#endif

#if PLATFORM(MAC)
    void substitutionsPanelIsShowing(CompletionHandler<void(bool)>&&);
    void showCorrectionPanel(WebCore::AlternativeTextType panelType, const WebCore::FloatRect& boundingBoxOfReplacedString, const String& replacedString, const String& replacementString, const Vector<String>& alternativeReplacementStrings);
    void dismissCorrectionPanel(WebCore::ReasonForDismissingAlternativeText);
    void dismissCorrectionPanelSoon(WebCore::ReasonForDismissingAlternativeText, CompletionHandler<void(String)>&&);
    void recordAutocorrectionResponse(WebCore::AutocorrectionResponse, const String& replacedString, const String& replacementString);

    void setEditableElementIsFocused(bool);

    void handleAcceptsFirstMouse(bool);
#endif // PLATFORM(MAC)

#if PLATFORM(IOS_FAMILY)
    WebCore::FloatSize screenSize();
    WebCore::FloatSize availableScreenSize();
    float textAutosizingWidth();

    void couldNotRestorePageState();
    void restorePageState(std::optional<WebCore::FloatPoint> scrollPosition, const WebCore::FloatPoint& scrollOrigin, const WebCore::FloatBoxExtent& obscuredInsetsOnSave, double scale);
    void restorePageCenterAndScale(std::optional<WebCore::FloatPoint>, double scale);

    void didGetTapHighlightGeometries(TapIdentifier requestID, const WebCore::Color&, const Vector<WebCore::FloatQuad>& geometries, const WebCore::IntSize& topLeftRadius, const WebCore::IntSize& topRightRadius, const WebCore::IntSize& bottomLeftRadius, const WebCore::IntSize& bottomRightRadius, bool nodeHasBuiltInClickHandling);

    void elementDidFocus(const FocusedElementInformation&, bool userIsInteracting, bool blurPreviousNode, OptionSet<WebCore::ActivityState> activityStateChanges, const UserData&);
    void elementDidBlur();
    void updateInputContextAfterBlurringAndRefocusingElement();
    void updateFocusedElementInformation(const FocusedElementInformation&);
    void focusedElementDidChangeInputMode(WebCore::InputMode);
    void didReleaseAllTouchPoints();

    void showInspectorHighlight(const WebCore::InspectorOverlayHighlight&);
    void hideInspectorHighlight();

    void enableInspectorNodeSearch();
    void disableInspectorNodeSearch();
#else
    void didReleaseAllTouchPoints() { }
#endif // PLATFORM(IOS_FAMILY)

    void performDragControllerAction(DragControllerAction, WebCore::DragData&, const std::optional<WebCore::FrameIdentifier>& = std::nullopt);

    void updateBackingStoreDiscardableState();

    void setRenderTreeSize(uint64_t treeSize) { m_renderTreeSize = treeSize; }

    void handleWheelEvent(const WebWheelEvent&);
    void sendWheelEvent(WebCore::FrameIdentifier, const WebWheelEvent&, OptionSet<WebCore::WheelEventProcessingSteps>, WebCore::RectEdges<bool> rubberBandableEdges, std::optional<bool> willStartSwipe, bool wasHandledForScrolling);
    void handleWheelEventReply(const WebWheelEvent&, WebCore::ScrollingNodeID, std::optional<WebCore::WheelScrollGestureState>, bool wasHandledForScrolling, bool wasHandledByWebProcess);

    void cacheWheelEventScrollingAccelerationCurve(const NativeWebWheelEvent&);
    void sendWheelEventScrollingAccelerationCurveIfNecessary(const WebWheelEvent&);

    WebWheelEventCoalescer& wheelEventCoalescer();

#if HAVE(DISPLAY_LINK)
    void wheelEventHysteresisUpdated(PAL::HysteresisState);
    void updateDisplayLinkFrequency();
#endif
    void updateWheelEventActivityAfterProcessSwap();

#if ENABLE(TOUCH_EVENTS)
    void touchEventHandlingCompleted(std::optional<WebEventType>, bool handled);
    void updateTouchEventTracking(const WebTouchEvent&);
    WebCore::TrackingType touchEventTrackingType(const WebTouchEvent&) const;
#endif

#if USE(QUICK_LOOK)
    void didStartLoadForQuickLookDocumentInMainFrame(const String& fileName, const String& uti);
    void didFinishLoadForQuickLookDocumentInMainFrame(ShareableResourceHandle&&);
    void requestPasswordForQuickLookDocumentInMainFrame(const String& fileName, CompletionHandler<void(const String&)>&&);
#endif

#if ENABLE(CONTENT_FILTERING)
    void contentFilterDidBlockLoadForFrame(const WebCore::ContentFilterUnblockHandler&, WebCore::FrameIdentifier);
#endif

    void tryReloadAfterProcessTermination();
    void resetRecentCrashCountSoon();
    void resetRecentCrashCount();

    API::DiagnosticLoggingClient* effectiveDiagnosticLoggingClient(WebCore::ShouldSample);

    void dispatchActivityStateChange();
    void viewDidLeaveWindow();
    void viewDidEnterWindow();

#if PLATFORM(MAC)
    void didPerformImmediateActionHitTest(const WebHitTestResultData&, bool contentPreventsDefault, const UserData&);
#endif

    void useFixedLayoutDidChange(bool useFixedLayout) { m_useFixedLayout = useFixedLayout; }
    void fixedLayoutSizeDidChange(WebCore::IntSize);

    void imageOrMediaDocumentSizeChanged(const WebCore::IntSize&);
#if ENABLE(VIDEO) && USE(GSTREAMER)
    void requestInstallMissingMediaPlugins(const String& details, const String& description);
#endif

    void startURLSchemeTask(URLSchemeTaskParameters&&);
    void stopURLSchemeTask(WebURLSchemeHandlerIdentifier, WebCore::ResourceLoaderIdentifier);
    void loadSynchronousURLSchemeTask(URLSchemeTaskParameters&&, CompletionHandler<void(const WebCore::ResourceResponse&, const WebCore::ResourceError&, Vector<uint8_t>&&)>&&);

    bool checkURLReceivedFromCurrentOrPreviousWebProcess(WebProcessProxy&, const String&);
    bool checkURLReceivedFromCurrentOrPreviousWebProcess(WebProcessProxy&, const URL&);
    void willAcquireUniversalFileReadSandboxExtension(WebProcessProxy&);

    void handleAutoFillButtonClick(const UserData&);

    void didResignInputElementStrongPasswordAppearance(const UserData&);

    void performSwitchHapticFeedback();

    void handleMessage(IPC::Connection&, const String& messageName, const UserData& messageBody);
    void handleSynchronousMessage(IPC::Connection&, const String& messageName, const UserData& messageBody, CompletionHandler<void(UserData&&)>&&);

    void viewIsBecomingVisible();

    void stopAllURLSchemeTasks(WebProcessProxy* = nullptr);

#if ENABLE(ATTACHMENT_ELEMENT)
    void registerAttachmentIdentifierFromData(const String&, const String& contentType, const String& preferredFileName, const IPC::SharedBufferReference&);
    void registerAttachmentIdentifierFromFilePath(const String&, const String& contentType, const String& filePath);
    void registerAttachmentsFromSerializedData(Vector<WebCore::SerializedAttachmentData>&&);
    void cloneAttachmentData(const String& fromIdentifier, const String& toIdentifier);

    void platformRegisterAttachment(Ref<API::Attachment>&&, const String& preferredFileName, const IPC::SharedBufferReference&);
    void platformRegisterAttachment(Ref<API::Attachment>&&, const String& filePath);
    void platformCloneAttachment(Ref<API::Attachment>&& fromAttachment, Ref<API::Attachment>&& toAttachment);

    void didInsertAttachmentWithIdentifier(const String& identifier, const String& source, bool hasEnclosingImage);
    void didRemoveAttachmentWithIdentifier(const String& identifier);
    void didRemoveAttachment(API::Attachment&);
    Ref<API::Attachment> ensureAttachment(const String& identifier);
    void invalidateAllAttachments();

#if PLATFORM(IOS_FAMILY)
    void writePromisedAttachmentToPasteboard(WebCore::PromisedAttachmentInfo&&, const String& authorizationToken);
#endif

    void requestAttachmentIcon(const String& identifier, const String& type, const String& path, const String& title, const WebCore::FloatSize&);

    RefPtr<ShareableBitmap> iconForAttachment(const String& fileName, const String& contentType, const String& title, WebCore::FloatSize&);
#endif

#if ENABLE(ATTACHMENT_ELEMENT) && HAVE(QUICKLOOK_THUMBNAILING)
    void requestThumbnail(WKQLThumbnailLoadOperation *);
#endif

    void reportPageLoadResult(const WebCore::ResourceError&);

    void continueNavigationInNewProcess(API::Navigation&, WebFrameProxy&, std::unique_ptr<SuspendedPageProxy>&&, Ref<WebProcessProxy>&&, ProcessSwapRequestedByClient, WebCore::ShouldTreatAsContinuingLoad, std::optional<NetworkResourceLoadIdentifier> existingNetworkResourceLoadIdentifierToResume);

    void setNeedsFontAttributes(bool);
    void updateFontAttributesAfterEditorStateChange();

    void didAttachToRunningProcess();

    void logFrameNavigation(const WebFrameProxy&, const URL& pageURL, const WebCore::ResourceRequest&, const URL& redirectURL, bool wasPotentiallyInitiatedByUser);

#if ENABLE(WINDOW_PROXY_PROPERTY_ACCESS_NOTIFICATION)
    void didAccessWindowProxyPropertyViaOpenerForFrame(WebCore::FrameIdentifier, const WebCore::SecurityOriginData&, WebCore::WindowProxyProperty);
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    void resetSpeechSynthesizer();
#endif

    void didFinishServiceWorkerPageRegistration(bool success);
    void callLoadCompletionHandlersIfNecessary(bool success);

#if PLATFORM(IOS_FAMILY)
    static bool isInHardwareKeyboardMode();
#endif

    void waitForInitialLinkDecorationFilteringData(WebFramePolicyListenerProxy&);
    void sendCachedLinkDecorationFilteringData();
#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    static Vector<WebCore::LinkDecorationFilteringData>& cachedAllowedQueryParametersForAdvancedPrivacyProtections();
    void updateAllowedQueryParametersForAdvancedPrivacyProtectionsIfNeeded();
#endif

    void clearAudibleActivity();

    void tryCloseTimedOut();
    void makeStorageSpaceRequest(WebCore::FrameIdentifier, const String& originIdentifier, const String& databaseName, const String& displayName, uint64_t currentQuota, uint64_t currentOriginUsage, uint64_t currentDatabaseUsage, uint64_t expectedUsage, CompletionHandler<void(uint64_t)>&&);
        
#if ENABLE(APP_BOUND_DOMAINS)
    bool setIsNavigatingToAppBoundDomainAndCheckIfPermitted(bool isMainFrame, const URL&, std::optional<NavigatingToAppBoundDomain>);
#endif

    void prepareToLoadWebPage(WebProcessProxy&, LoadParameters&);

    void didUpdateEditorState(const EditorState& oldEditorState, const EditorState& newEditorState);

    void runModalJavaScriptDialog(RefPtr<WebFrameProxy>&&, FrameInfoData&&, const String& message, CompletionHandler<void(WebPageProxy&, WebFrameProxy*, FrameInfoData&&, const String&, CompletionHandler<void()>&&)>&&);

#if ENABLE(IMAGE_ANALYSIS) && PLATFORM(MAC)
    void showImageInQuickLookPreviewPanel(ShareableBitmap& imageBitmap, const String& tooltip, const URL& imageURL, QuickLookPreviewActivity);
#endif
        
#if ENABLE(APP_HIGHLIGHTS)
    void setUpHighlightsObserver();
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void updateFullscreenVideoTextRecognition();
    void fullscreenVideoTextRecognitionTimerFired();
#endif

    bool useGPUProcessForDOMRenderingEnabled() const;

    void dispatchLoadEventToFrameOwnerElement(WebCore::FrameIdentifier);

#if ENABLE(EXTENSION_CAPABILITIES)
    void setMediaCapability(std::optional<MediaCapability>&&);
    bool shouldActivateMediaCapability() const;
    bool shouldDeactivateMediaCapability() const;
#endif

    template<typename F> decltype(auto) sendToWebPage(std::optional<WebCore::FrameIdentifier>, F&&);
    template<typename M, typename C> void sendToProcessContainingFrame(std::optional<WebCore::FrameIdentifier>, M&&, C&&);
    template<typename M> void sendToProcessContainingFrame(std::optional<WebCore::FrameIdentifier>, M&&);
    template<typename M> IPC::ConnectionSendSyncResult<M> sendSyncToProcessContainingFrame(std::optional<WebCore::FrameIdentifier>, M&&);

    void sendPreventableTouchEvent(WebCore::FrameIdentifier, const NativeWebTouchEvent&);
    void sendUnpreventableTouchEvent(WebCore::FrameIdentifier, const NativeWebTouchEvent&);

    void broadcastFocusedFrameToOtherProcesses(IPC::Connection&, const WebCore::FrameIdentifier&);

    void closeRemoteFrame(WebCore::FrameIdentifier);
    void focusRemoteFrame(IPC::Connection&, WebCore::FrameIdentifier);
    void postMessageToRemote(WebCore::FrameIdentifier source, const String& sourceOrigin, WebCore::FrameIdentifier target, std::optional<WebCore::SecurityOriginData> targetOrigin, const WebCore::MessageWithMessagePorts&);
    void renderTreeAsText(WebCore::FrameIdentifier, size_t baseIndent, OptionSet<WebCore::RenderAsTextFlag>, CompletionHandler<void(String&&)>&&);

    struct Internals;
    Internals& internals() { return m_internals; }
    const Internals& internals() const { return m_internals; }

    class ProcessActivityState {
    public:
        explicit ProcessActivityState(WebPageProxy&);
        void takeVisibleActivity();
        void takeAudibleActivity();
        void takeCapturingActivity();

        void reset();
        void dropVisibleActivity();
        void dropAudibleActivity();
        void dropCapturingActivity();

        bool hasValidVisibleActivity() const;
        bool hasValidAudibleActivity() const;
        bool hasValidCapturingActivity() const;

#if PLATFORM(IOS_FAMILY)
        void takeOpeningAppLinkActivity();
        void dropOpeningAppLinkActivity();
        bool hasValidOpeningAppLinkActivity() const;
#endif

    private:
        WebPageProxy& m_page;

        std::unique_ptr<ProcessThrottlerActivity> m_isVisibleActivity;
#if PLATFORM(MAC)
        UniqueRef<ProcessThrottlerTimedActivity> m_wasRecentlyVisibleActivity;
#endif
        std::unique_ptr<ProcessThrottlerActivity> m_isAudibleActivity;
        std::unique_ptr<ProcessThrottlerActivity> m_isCapturingActivity;
#if PLATFORM(IOS_FAMILY)
        std::unique_ptr<ProcessThrottlerActivity> m_openingAppLinkActivity;
#endif
    };

    UniqueRef<Internals> m_internals;

    WeakPtr<PageClient> m_pageClient;
    Ref<API::PageConfiguration> m_configuration;

    std::unique_ptr<API::LoaderClient> m_loaderClient;
    std::unique_ptr<API::PolicyClient> m_policyClient;
    UniqueRef<API::NavigationClient> m_navigationClient;
    UniqueRef<API::HistoryClient> m_historyClient;
    std::unique_ptr<API::IconLoadingClient> m_iconLoadingClient;
    std::unique_ptr<API::FormClient> m_formClient;
    std::unique_ptr<API::UIClient> m_uiClient;
    std::unique_ptr<API::FindClient> m_findClient;
    std::unique_ptr<API::FindMatchesClient> m_findMatchesClient;
    std::unique_ptr<API::DiagnosticLoggingClient> m_diagnosticLoggingClient;
    std::unique_ptr<API::ResourceLoadClient> m_resourceLoadClient;
#if ENABLE(CONTEXT_MENUS)
    std::unique_ptr<API::ContextMenuClient> m_contextMenuClient;
#endif
    std::unique_ptr<WebPageInjectedBundleClient> m_injectedBundleClient;
    std::unique_ptr<PageLoadStateObserverBase> m_pageLoadStateObserver;

    std::unique_ptr<WebNavigationState> m_navigationState;
    String m_failingProvisionalLoadURL;
    bool m_isLoadingAlternateHTMLStringForFailingProvisionalLoad { false };

    std::unique_ptr<DrawingAreaProxy> m_drawingArea;
#if PLATFORM(COCOA)
    std::unique_ptr<RemoteLayerTreeHost> m_frozenRemoteLayerTreeHost;
#endif
#if PLATFORM(COCOA) && ENABLE(ASYNC_SCROLLING)
    std::unique_ptr<RemoteScrollingCoordinatorProxy> m_scrollingCoordinatorProxy;
#endif
    Ref<WebProcessProxy> m_process;
    Ref<WebPageGroup> m_pageGroup;
    Ref<WebPreferences> m_preferences;

    Ref<WebUserContentControllerProxy> m_userContentController;

#if ENABLE(WK_WEB_EXTENSIONS)
    RefPtr<WebExtensionController> m_webExtensionController;
    WeakPtr<WebExtensionController> m_weakWebExtensionController;
#endif

    Ref<VisitedLinkStore> m_visitedLinkStore;
    Ref<WebsiteDataStore> m_websiteDataStore;

    RefPtr<WebFrameProxy> m_mainFrame;

    RefPtr<WebFrameProxy> m_openerFrame;

    RefPtr<WebFrameProxy> m_focusedFrame;

    String m_userAgent;
    String m_applicationNameForUserAgent;
    String m_applicationNameForDesktopUserAgent;
    String m_customUserAgent;
    String m_customTextEncodingName;
    String m_overrideContentSecurityPolicy;

    RefPtr<WebInspectorUIProxy> m_inspector;

#if ENABLE(FULLSCREEN_API)
    std::unique_ptr<WebFullScreenManagerProxy> m_fullScreenManager;
    std::unique_ptr<API::FullscreenClient> m_fullscreenClient;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    RefPtr<PlaybackSessionManagerProxy> m_playbackSessionManager;
    RefPtr<VideoPresentationManagerProxy> m_videoPresentationManager;
    bool m_mockVideoPresentationModeEnabled { false };
#endif

#if ENABLE(MEDIA_USAGE)
    std::unique_ptr<MediaUsageManager> m_mediaUsageManager;
#endif

#if PLATFORM(IOS_FAMILY)
    std::optional<WebCore::InputMode> m_pendingInputModeChange;
    WebCore::IntDegrees m_deviceOrientation { 0 };
    bool m_hasNetworkRequestsOnSuspended { false };
    bool m_isKeyboardAnimatingIn { false };
    bool m_isScrollingOrZooming { false };
#endif

#if PLATFORM(MAC)
    bool m_useSystemAppearance { false };
    bool m_acceptsFirstMouse { false };
#endif

#if USE(SYSTEM_PREVIEW)
    std::unique_ptr<SystemPreviewController> m_systemPreviewController;
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW)
    std::unique_ptr<ModelElementController> m_modelElementController;
#endif

#if ENABLE(APPLE_PAY_AMS_UI)
    RetainPtr<AMSUIEngagementTask> m_applePayAMSUISession;
#endif

#if ENABLE(WEB_AUTHN)
    std::unique_ptr<WebAuthenticatorCoordinatorProxy> m_credentialsMessenger;
#endif

#if ENABLE(DATA_DETECTION)
    RetainPtr<NSArray> m_dataDetectionResults;
#endif

    WeakHashSet<WebEditCommandProxy> m_editCommandSet;

#if PLATFORM(COCOA)
    HashSet<String> m_knownKeypressCommandNames;
#endif

    RefPtr<WebPopupMenuProxy> m_activePopupMenu;
#if ENABLE(CONTEXT_MENUS)
    RefPtr<WebContextMenuProxy> m_activeContextMenu;
#endif

#if PLATFORM(MAC)
    RefPtr<API::HitTestResult> m_lastMouseMoveHitTestResult;
#endif

    RefPtr<WebOpenPanelResultListenerProxy> m_openPanelResultListener;

#if ENABLE(MEDIA_STREAM)
    std::unique_ptr<UserMediaPermissionRequestManagerProxy> m_userMediaPermissionRequestManager;
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    std::unique_ptr<MediaKeySystemPermissionRequestManagerProxy> m_mediaKeySystemPermissionRequestManager;
#endif

    bool m_viewWasEverInWindow { false };
#if PLATFORM(MACCATALYST)
    bool m_isListeningForUserFacingStateChangeNotification { false };
#endif
    bool m_allowsMediaDocumentInlinePlayback { false };

    ProcessActivityState m_processActivityState;

    bool m_initialCapitalizationEnabled { false };
    std::optional<double> m_cpuLimit;
    Ref<WebBackForwardList> m_backForwardList;
        
    bool m_maintainsInactiveSelection { false };

    bool m_waitsForPaintAfterViewDidMoveToWindow { false };
    bool m_shouldSkipWaitingForPaintAfterNextViewDidMoveToWindow { false };

    String m_toolTip;

    bool m_isEditable { false };

    double m_textZoomFactor { 1 };
    double m_pageZoomFactor { 1 };
    double m_pageScaleFactor { 1 };
    double m_pluginZoomFactor { 1 };
    double m_pluginScaleFactor { 1 };
    double m_viewScaleFactor { 1 };
    float m_intrinsicDeviceScaleFactor { 1 };
    std::optional<float> m_customDeviceScaleFactor;
    float m_topContentInset { 0 };

    bool m_hasPendingUnderPageBackgroundColorOverrideToDispatch { false };

    bool m_useFixedLayout { false };

    bool m_alwaysShowsHorizontalScroller { false };
    bool m_alwaysShowsVerticalScroller { false };

    bool m_suppressScrollbarAnimations { false };

    WebCore::PaginationMode m_paginationMode { };
    bool m_paginationBehavesLikeColumns { false };
    double m_pageLength { 0 };
    double m_gapBetweenPages { 0 };
        
    // If the process backing the web page is alive and kicking.
    bool m_hasRunningProcess { false };

    // Whether WebPageProxy::close() has been called on this page.
    bool m_isClosed { false };

    // Whether it can run modal child web pages.
    bool m_canRunModal { false };

    bool m_isInPrintingMode { false };
    bool m_isPerformingDOMPrintOperation { false };

    bool m_hasUpdatedRenderingAfterDidCommitLoad { true };

    bool m_hasActiveAnimatedScroll { false };
    bool m_registeredForFullSpeedUpdates { false };

    bool m_shouldSuppressAppLinksInNextNavigationPolicyDecision { false };

#if HAVE(APP_SSO)
    bool m_shouldSuppressSOAuthorizationInNextNavigationPolicyDecision { false };
#endif

    std::unique_ptr<WebWheelEventCoalescer> m_wheelEventCoalescer;

    std::optional<WebCore::PlatformDisplayID> m_displayID;

    enum class EventPreventionState : uint8_t { None, Waiting, Prevented, Allowed };

#if ENABLE(CONTEXT_MENU_EVENT)
    EventPreventionState m_contextMenuPreventionState { EventPreventionState::None };
    Vector<CompletionHandler<void(bool)>> m_contextMenuCallbacks;
#endif

    uint64_t m_handlingPreventableTouchStartCount { 0 };
    uint64_t m_handlingPreventableTouchEndCount { 0 };
    EventPreventionState m_touchMovePreventionState { EventPreventionState::None };

#if ENABLE(DATE_AND_TIME_INPUT_TYPES)
    RefPtr<WebDateTimePicker> m_dateTimePicker;
#endif
#if PLATFORM(COCOA) || PLATFORM(GTK)
    RefPtr<WebCore::ValidationBubble> m_validationBubble;
#endif

    bool m_areActiveDOMObjectsAndAnimationsSuspended { false };
    bool m_addsVisitedLinks { true };

    bool m_controlledByAutomation { false };

    unsigned m_inspectorFrontendCount { 0 };

#if PLATFORM(COCOA)
    bool m_isSmartInsertDeleteEnabled { false };
#endif

    unsigned m_pendingLearnOrIgnoreWordMessageCount { 0 };

    bool m_mainFrameHasCustomContentProvider { false };

#if ENABLE(DRAG_SUPPORT)
    // Current drag destination details are delivered as an asynchronous response,
    // so we preserve them to be used when the next dragging delegate call is made.
    std::optional<WebCore::DragOperation> m_currentDragOperation;
    bool m_currentDragIsOverFileInput { false };
    unsigned m_currentDragNumberOfFilesToBeAccepted { 0 };
#endif

    bool m_mainFrameHasHorizontalScrollbar { false };
    bool m_mainFrameHasVerticalScrollbar { false };

    // Whether horizontal wheel events can be handled directly for swiping purposes.
    bool m_canShortCircuitHorizontalWheelEvents { true };

    bool m_shouldUseImplicitRubberBandControl { false };
        
    bool m_enableVerticalRubberBanding { true };
    bool m_enableHorizontalRubberBanding { true };

    bool m_backgroundExtendsBeyondPage { true };

    bool m_shouldRecordNavigationSnapshots { false };
    bool m_isShowingNavigationGestureSnapshot { false };

    bool m_mainFramePluginHandlesPageScaleGesture { false };
    bool m_shouldReloadDueToCrashWhenVisible { false };

    unsigned m_pageCount { 0 };

    uint64_t m_renderTreeSize { 0 };
    uint64_t m_sessionRestorationRenderTreeSize { 0 };
    bool m_hitRenderTreeSizeThreshold { false };

    bool m_suppressVisibilityUpdates { false };
    bool m_autoSizingShouldExpandToViewHeight { false };

    float m_mediaVolume { 1 };
    bool m_mayStartMediaWhenInWindow { true };
    bool m_mediaPlaybackIsSuspended { false };
    bool m_mediaCaptureEnabled { true };
#if PLATFORM(MAC) || PLATFORM(MACCATALYST)
    bool m_isProcessingIsConnectedToHardwareConsoleDidChangeNotification { false };
    bool m_captureWasMutedDueToDisconnectedHardwareConsole { false };
#endif

    bool m_waitingForDidUpdateActivityState { false };

    bool m_shouldScaleViewToFitDocument { false };
    bool m_shouldSuppressNextAutomaticNavigationSnapshot { false };
    bool m_madeViewBlankDueToLackOfRenderingUpdate { false };

#if PLATFORM(COCOA)
    using TemporaryPDFFileMap = HashMap<String, String>;
    TemporaryPDFFileMap m_temporaryPDFFiles;
    std::unique_ptr<WebCore::RunLoopObserver> m_activityStateChangeDispatcher;

    std::unique_ptr<RemoteLayerTreeScrollingPerformanceData> m_scrollingPerformanceData;
    bool m_scrollPerformanceDataCollectionEnabled { false };

    bool m_hasScheduledActivityStateUpdate { false };
    Vector<CompletionHandler<void()>> m_activityStateUpdateCallbacks;
#endif
#if PLATFORM(MAC)
    std::unique_ptr<ViewWindowCoordinates> m_viewWindowCoordinates;
#endif

    std::optional<WebCore::ScrollbarOverlayStyle> m_scrollbarOverlayStyle;

    ActivityStateChangeID m_currentActivityStateChangeID { };

    bool m_activityStateChangeWantsSynchronousReply { false };
    Vector<CompletionHandler<void()>> m_nextActivityStateChangeCallbacks;

    // Assume animations are allowed to play by default. If this is not the case, we will be notified by the web process.
    bool m_allowsAnyAnimationToPlay { true };

    // To make sure capture indicators are visible long enough, m_reportedMediaCaptureState is the same as m_mediaState except that we might delay a bit transition from capturing to not-capturing.
    static constexpr Seconds DefaultMediaCaptureReportingDelay { 3_s };
    Seconds m_mediaCaptureReportingDelay { DefaultMediaCaptureReportingDelay };

    WebCore::IntDegrees m_orientationForMediaCapture { 0 };

    bool m_hasHadSelectionChangesFromUserInteraction { false };

#if HAVE(TOUCH_BAR)
    bool m_isTouchBarUpdateSupressedForHiddenContentEditable { false };
    bool m_isNeverRichlyEditableForTouchBar { false };
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)
    bool m_requiresTargetMonitoring { false };
#endif

#if ENABLE(META_VIEWPORT)
    bool m_forceAlwaysUserScalable { false };
    double m_viewportConfigurationLayoutSizeScaleFactor { 1 };
    double m_viewportConfigurationMinimumEffectiveDeviceWidth { 0 };
#endif

#if PLATFORM(IOS_FAMILY)
    Function<bool()> m_deviceOrientationUserPermissionHandlerForTesting;
    bool m_waitingForPostLayoutEditorStateUpdateAfterFocusingElement { false };
    bool m_lastObservedStateWasBackground { false };
    HashSet<String> m_performActionOnElementAuthTokens;
#endif

#if ENABLE(POINTER_LOCK)
    bool m_isPointerLockPending { false };
    bool m_isPointerLocked { false };
#endif

    bool m_openedByDOM { false };
    bool m_hasCommittedAnyProvisionalLoads { false };
    bool m_preferFasterClickOverDoubleTap { false };

    HashMap<String, Ref<WebURLSchemeHandler>> m_urlSchemeHandlersByScheme;

#if ENABLE(ATTACHMENT_ELEMENT)
    using IdentifierToAttachmentMap = HashMap<String, Ref<API::Attachment>>;
    IdentifierToAttachmentMap m_attachmentIdentifierToAttachmentMap;
#endif

    const std::unique_ptr<WebPageInspectorController> m_inspectorController;
#if ENABLE(REMOTE_INSPECTOR)
    std::unique_ptr<WebPageDebuggable> m_inspectorDebuggable;
#endif

    std::optional<SpellDocumentTag> m_spellDocumentTag;

    HashSet<String> m_previouslyVisitedPaths;

    unsigned m_recentCrashCount { 0 };

    bool m_needsFontAttributes { false };
    bool m_mayHaveUniversalFileReadSandboxExtension { false };
    bool m_isServiceWorkerPage { false };

    std::unique_ptr<ProvisionalPageProxy> m_provisionalPage;
    std::unique_ptr<SuspendedPageProxy> m_suspendedPageKeptToPreventFlashing;
    WeakPtr<SuspendedPageProxy> m_lastSuspendedPage;

    TextManipulationItemCallback m_textManipulationItemCallback;

#if HAVE(VISIBILITY_PROPAGATION_VIEW)
    LayerHostingContextID m_contextIDForVisibilityPropagationInWebProcess { 0 };
#endif
#if HAVE(VISIBILITY_PROPAGATION_VIEW) && ENABLE(GPU_PROCESS)
    LayerHostingContextID m_contextIDForVisibilityPropagationInGPUProcess { 0 };
#endif

    HashMap<WebViewDidMoveToWindowObserver*, WeakPtr<WebViewDidMoveToWindowObserver>> m_webViewDidMoveToWindowObservers;

    mutable RefPtr<Logger> m_logger;

    std::unique_ptr<SpeechRecognitionPermissionManager> m_speechRecognitionPermissionManager;

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    RefPtr<RemoteMediaSessionCoordinatorProxy> m_mediaSessionCoordinatorProxy;
#endif

    bool m_sessionStateWasRestoredByAPIRequest { false };
    bool m_isQuotaIncreaseDenied { false };
    bool m_isLayerTreeFrozenDueToSwipeAnimation { false };
    
    String m_overriddenMediaType;

    Vector<String> m_corsDisablingPatterns;

    struct InjectedBundleMessage {
        String messageName;
        RefPtr<API::Object> messageBody;
    };
    Vector<InjectedBundleMessage> m_pendingInjectedBundleMessages;
        
#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
    std::unique_ptr<WebDeviceOrientationUpdateProviderProxy> m_webDeviceOrientationUpdateProviderProxy;
#endif

    std::unique_ptr<WebScreenOrientationManagerProxy> m_screenOrientationManager;

#if ENABLE(APP_BOUND_DOMAINS)
    std::optional<NavigatingToAppBoundDomain> m_isNavigatingToAppBoundDomain;
    std::optional<NavigatingToAppBoundDomain> m_isTopFrameNavigatingToAppBoundDomain;
    bool m_ignoresAppBoundDomains { false };
    bool m_limitsNavigationsToAppBoundDomains { false };
#endif

    bool m_userScriptsNotified { false };
    bool m_hasExecutedAppBoundBehaviorBeforeNavigation { false };
    bool m_canUseCredentialStorage { true };

    size_t m_suspendMediaPlaybackCounter { 0 };

    bool m_lastNavigationWasAppInitiated { true };
    bool m_isRunningModalJavaScriptDialog { false };
    bool m_isSuspended { false };
    bool m_isLockdownModeExplicitlySet { false };

#if ENABLE(ADVANCED_PRIVACY_PROTECTIONS)
    RefPtr<LinkDecorationFilteringDataObserver> m_linkDecorationFilteringDataUpdateObserver;
    bool m_needsInitialLinkDecorationFilteringData { true };
    bool m_shouldUpdateAllowedQueryParametersForAdvancedPrivacyProtections { false };
    OptionSet<WebCore::AdvancedPrivacyProtections> m_advancedPrivacyProtectionsPolicies;
#endif

#if ENABLE(APP_HIGHLIGHTS)
    RetainPtr<SYNotesActivationObserver> m_appHighlightsObserver;
#endif

#if ENABLE(IMAGE_ANALYSIS) && PLATFORM(MAC)
    RetainPtr<WKQuickLookPreviewController> m_quickLookPreviewController;
#endif

    bool m_isPerformingTextRecognitionInElementFullScreen { false };

#if ENABLE(NETWORK_ISSUE_REPORTING)
    std::unique_ptr<NetworkIssueReporter> m_networkIssueReporter;
#endif

    RefPtr<WebPageProxy> m_pageToCloneSessionStorageFrom;
    Ref<BrowsingContextGroup> m_browsingContextGroup;
};

} // namespace WebKit
