/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "ActivityState.h"
#include "CSSComputedStyleDeclaration.h"
#include "ContextDestructionObserver.h"
#include "Cookie.h"
#include "EpochTimeStamp.h"
#include "EventTrackingRegions.h"
#include "ExceptionOr.h"
#include "HEVCUtilities.h"
#include "IDLTypes.h"
#include "OrientationNotifier.h"
#include "PageConsoleClient.h"
#include "RealtimeMediaSource.h"
#include "SleepDisabler.h"
#include "TextIndicator.h"
#include "VP9Utilities.h"
#include <JavaScriptCore/Forward.h>

#if ENABLE(VIDEO)
#include "MediaElementSession.h"
#include "MediaUniqueIdentifier.h"
#endif

#if USE(AUDIO_SESSION)
#include "AudioSession.h"
#endif

#if USE(APPLE_INTERNAL_SDK)
#include "InternalsAdditions.h"
#endif

OBJC_CLASS DDScannerResult;
OBJC_CLASS VKCImageAnalysis;

namespace WebCore {

class AbstractRange;
class AnimationTimeline;
class ArtworkImageLoader;
class AudioContext;
class AudioTrack;
class BaseAudioContext;
class Blob;
class CacheStorageConnection;
class CaptionUserPreferencesTestingModeToken;
class DOMPointReadOnly;
class DOMRect;
class DOMRectList;
class DOMRectReadOnly;
class DOMURL;
class DOMWindow;
class Document;
class Element;
class EventListener;
class ExtendableEvent;
class FetchRequest;
class FetchResponse;
class File;
class Frame;
class GCObservation;
class HTMLAnchorElement;
class HTMLAttachmentElement;
class HTMLCanvasElement;
class HTMLImageElement;
class HTMLInputElement;
class HTMLLinkElement;
class HTMLMediaElement;
class HTMLPictureElement;
class HTMLSelectElement;
class HTMLVideoElement;
class ImageData;
class InspectorStubFrontend;
class InternalsMapLike;
class InternalSettings;
class InternalsSetLike;
class Location;
class MallocStatistics;
class MediaStream;
class MediaStreamTrack;
class MemoryInfo;
class MessagePort;
class MockCDMFactory;
class MockContentFilterSettings;
class MockPageOverlay;
class MockPaymentCoordinator;
class NodeList;
class Page;
class RTCPeerConnection;
class ReadableStream;
class Range;
class RenderedDocumentMarker;
class SVGSVGElement;
class ScrollableArea;
class SerializedScriptValue;
class SharedBuffer;
class SourceBuffer;
class SpeechSynthesisUtterance;
class StaticRange;
class StringCallback;
class StyleSheet;
class TextIterator;
class TextTrack;
class TimeRanges;
class TypeConversions;
class UnsuspendableActiveDOMObject;
class VoidCallback;
class WebAnimation;
class WebGLRenderingContext;
class WindowProxy;
class XMLHttpRequest;

#if ENABLE(ENCRYPTED_MEDIA)
class MediaKeys;
class MediaKeySession;
#endif

#if ENABLE(VIDEO)
class TextTrackCueGeneric;
class VTTCue;
#endif

#if ENABLE(SERVICE_WORKER)
class PushSubscription;
class ServiceWorker;
#endif

#if ENABLE(WEB_RTC)
class RTCRtpSFrameTransform;
#endif

#if ENABLE(WEBXR)
class WebXRTest;
#endif

#if ENABLE(MEDIA_SESSION)
class MediaSession;
struct MediaSessionActionDetails;
#if ENABLE(MEDIA_SESSION_COORDINATOR)
class MediaSessionCoordinator;
class MockMediaSessionCoordinator;
#endif
#endif

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
class HTMLModelElement;
#endif

#if ENABLE(SPEECH_SYNTHESIS)
class PlatformSpeechSynthesizerMock;
#endif

template<typename IDLType> class DOMPromiseDeferred;

struct MockWebAuthenticationConfiguration;

class Internals final : public RefCounted<Internals>, private ContextDestructionObserver
#if ENABLE(MEDIA_STREAM)
    , public RealtimeMediaSource::Observer
    , private RealtimeMediaSource::AudioSampleObserver
    , private RealtimeMediaSource::VideoFrameObserver
#endif
    {
public:
    static Ref<Internals> create(Document&);
    virtual ~Internals();

    static void resetToConsistentState(Page&);

    ExceptionOr<String> elementRenderTreeAsText(Element&);
    bool hasPausedImageAnimations(Element&);

    bool isPaintingFrequently(Element&);
    void incrementFrequentPaintCounter(Element&);

    String address(Node&);
    bool nodeNeedsStyleRecalc(Node&);
    String styleChangeType(Node&);
    String description(JSC::JSValue);
    void log(const String&);

    bool isPreloaded(const String& url);
    bool isLoadingFromMemoryCache(const String& url);
    String fetchResponseSource(FetchResponse&);
    String xhrResponseSource(XMLHttpRequest&);
    bool isSharingStyleSheetContents(HTMLLinkElement&, HTMLLinkElement&);
    bool isStyleSheetLoadingSubresources(HTMLLinkElement&);
    enum class CachePolicy { UseProtocolCachePolicy, ReloadIgnoringCacheData, ReturnCacheDataElseLoad, ReturnCacheDataDontLoad };
    void setOverrideCachePolicy(CachePolicy);
    ExceptionOr<void> setCanShowModalDialogOverride(bool allow);
    enum class ResourceLoadPriority { ResourceLoadPriorityVeryLow, ResourceLoadPriorityLow, ResourceLoadPriorityMedium, ResourceLoadPriorityHigh, ResourceLoadPriorityVeryHigh };
    void setOverrideResourceLoadPriority(ResourceLoadPriority);
    void setStrictRawResourceValidationPolicyDisabled(bool);

    using FetchObject = std::variant<RefPtr<FetchRequest>, RefPtr<FetchResponse>>;
    bool isFetchObjectContextStopped(const FetchObject&);

    void clearMemoryCache();
    void pruneMemoryCacheToSize(unsigned size);
    void destroyDecodedDataForAllImages();
    unsigned memoryCacheSize() const;

    unsigned imageFrameIndex(HTMLImageElement&);
    unsigned imageFrameCount(HTMLImageElement&);
    float imageFrameDurationAtIndex(HTMLImageElement&, unsigned index);
    void setImageFrameDecodingDuration(HTMLImageElement&, float duration);
    void resetImageAnimation(HTMLImageElement&);
    bool isImageAnimating(HTMLImageElement&);
    void setImageAnimationEnabled(bool);
    void resumeImageAnimation(HTMLImageElement&);
    void pauseImageAnimation(HTMLImageElement&);
    unsigned imagePendingDecodePromisesCountForTesting(HTMLImageElement&);
    void setClearDecoderAfterAsyncFrameRequestForTesting(HTMLImageElement&, bool enabled);
    unsigned imageDecodeCount(HTMLImageElement&);
    unsigned pdfDocumentCachingCount(HTMLImageElement&);
    unsigned remoteImagesCountForTesting() const;
    void setLargeImageAsyncDecodingEnabledForTesting(HTMLImageElement&, bool enabled);
    void setForceUpdateImageDataEnabledForTesting(HTMLImageElement&, bool enabled);

    void setGridMaxTracksLimit(unsigned);

    void clearBackForwardCache();
    unsigned backForwardCacheSize() const;
    void preventDocumentFromEnteringBackForwardCache();

    void disableTileSizeUpdateDelay();

    void setSpeculativeTilingDelayDisabledForTesting(bool);

    Ref<CSSComputedStyleDeclaration> computedStyleIncludingVisitedInfo(Element&) const;

    Node* ensureUserAgentShadowRoot(Element& host);
    Node* shadowRoot(Element& host);
    ExceptionOr<String> shadowRootType(const Node&) const;
    const AtomString& shadowPseudoId(Element&);
    void setShadowPseudoId(Element&, const AtomString&);

    // DOMTimers throttling testing.
    ExceptionOr<bool> isTimerThrottled(int timeoutId);
    String requestAnimationFrameThrottlingReasons() const;
    double requestAnimationFrameInterval() const;
    bool scriptedAnimationsAreSuspended() const;
    bool areTimersThrottled() const;

    enum EventThrottlingBehavior { Responsive, Unresponsive };
    void setEventThrottlingBehaviorOverride(std::optional<EventThrottlingBehavior>);
    std::optional<EventThrottlingBehavior> eventThrottlingBehaviorOverride() const;

    // Spatial Navigation testing.
    ExceptionOr<unsigned> lastSpatialNavigationCandidateCount() const;

    // CSS Animation testing.
    bool animationWithIdExists(const String&) const;
    unsigned numberOfActiveAnimations() const;
    ExceptionOr<bool> animationsAreSuspended() const;
    ExceptionOr<void> suspendAnimations() const;
    ExceptionOr<void> resumeAnimations() const;
    double animationsInterval() const;

    // Web Animations testing.
    struct AcceleratedAnimation {
        String property;
        double speed;
    };
    Vector<AcceleratedAnimation> acceleratedAnimationsForElement(Element&);
    unsigned numberOfAnimationTimelineInvalidations() const;
    double timeToNextAnimationTick(WebAnimation&) const;

    // For animations testing, we need a way to get at pseudo elements.
    ExceptionOr<RefPtr<Element>> pseudoElement(Element&, const String&);

    Node* treeScopeRootNode(Node&);
    Node* parentTreeScope(Node&);

    String visiblePlaceholder(Element&);
    void setCanShowPlaceholder(Element&, bool);

    Element* insertTextPlaceholder(int width, int height);
    void removeTextPlaceholder(Element&);

    void selectColorInColorChooser(HTMLInputElement&, const String& colorValue);
    ExceptionOr<Vector<AtomString>> formControlStateOfPreviousHistoryItem();
    ExceptionOr<void> setFormControlStateOfPreviousHistoryItem(const Vector<AtomString>&);

    ExceptionOr<Ref<DOMRect>> absoluteLineRectFromPoint(int x, int y);

    ExceptionOr<Ref<DOMRect>> absoluteCaretBounds();
    ExceptionOr<bool> isCaretBlinkingSuspended();

    Ref<DOMRect> boundingBox(Element&);

    ExceptionOr<Ref<DOMRectList>> inspectorHighlightRects();
    ExceptionOr<unsigned> inspectorGridOverlayCount();
    ExceptionOr<unsigned> inspectorFlexOverlayCount();
    ExceptionOr<unsigned> inspectorPaintRectCount();

    ExceptionOr<unsigned> markerCountForNode(Node&, const String&);
    ExceptionOr<RefPtr<Range>> markerRangeForNode(Node&, const String& markerType, unsigned index);
    ExceptionOr<String> markerDescriptionForNode(Node&, const String& markerType, unsigned index);
    ExceptionOr<String> dumpMarkerRects(const String& markerType);
    ExceptionOr<void> setMarkedTextMatchesAreHighlighted(bool);

    void invalidateFontCache();

    ExceptionOr<void> setLowPowerModeEnabled(bool);
    ExceptionOr<void> setOutsideViewportThrottlingEnabled(bool);

    ExceptionOr<void> setScrollViewPosition(int x, int y);
    ExceptionOr<void> unconstrainedScrollTo(Element&, double x, double y);
    ExceptionOr<void> scrollBySimulatingWheelEvent(Element&, double deltaX, double deltaY);

    ExceptionOr<Ref<DOMRect>> layoutViewportRect();
    ExceptionOr<Ref<DOMRect>> visualViewportRect();

    ExceptionOr<void> setViewIsTransparent(bool);

    ExceptionOr<String> viewBaseBackgroundColor();
    ExceptionOr<void> setViewBaseBackgroundColor(const String& colorValue);

    ExceptionOr<void> setPagination(const String& mode, int gap, int pageLength);
    ExceptionOr<void> setPaginationLineGridEnabled(bool);
    ExceptionOr<uint64_t> lineIndexAfterPageBreak(Element&);
    ExceptionOr<String> configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight);

    ExceptionOr<bool> wasLastChangeUserEdit(Element& textField);
    bool elementShouldAutoComplete(HTMLInputElement&);
    void setAutofilled(HTMLInputElement&, bool enabled);
    void setAutoFilledAndViewable(HTMLInputElement&, bool enabled);
    void setAutoFilledAndObscured(HTMLInputElement&, bool enabled);
    enum class AutoFillButtonType { None, Contacts, Credentials, StrongPassword, CreditCard, Loading };
    void setShowAutoFillButton(HTMLInputElement&, AutoFillButtonType);
    AutoFillButtonType autoFillButtonType(const HTMLInputElement&);
    AutoFillButtonType lastAutoFillButtonType(const HTMLInputElement&);
    ExceptionOr<void> scrollElementToRect(Element&, int x, int y, int w, int h);

    ExceptionOr<String> autofillFieldName(Element&);

    ExceptionOr<void> invalidateControlTints();

    RefPtr<Range> rangeFromLocationAndLength(Element& scope, unsigned rangeLocation, unsigned rangeLength);
    unsigned locationFromRange(Element& scope, const Range&, const Vector<String>& behaviors = { });
    unsigned lengthFromRange(Element& scope, const Range&, const Vector<String>& behaviors = { });
    String rangeAsText(const Range&);
    String rangeAsTextUsingBackwardsTextIterator(const Range&);
    Ref<Range> subrange(Range&, unsigned rangeLocation, unsigned rangeLength);
    ExceptionOr<RefPtr<Range>> rangeForDictionaryLookupAtLocation(int x, int y);
    RefPtr<Range> rangeOfStringNearLocation(const Range&, const String&, unsigned);

    struct TextIteratorState {
        String text;
        RefPtr<Range> range;
    };
    Vector<TextIteratorState> statesOfTextIterator(const Range&, const Vector<String>& behaviors = { });

    ExceptionOr<void> setDelegatesScrolling(bool enabled);

    ExceptionOr<uint64_t> lastSpellCheckRequestSequence();
    ExceptionOr<uint64_t> lastSpellCheckProcessedSequence();
    void advanceToNextMisspelling();

    Vector<String> userPreferredLanguages() const;
    void setUserPreferredLanguages(const Vector<String>&);

    Vector<String> userPreferredAudioCharacteristics() const;
    void setUserPreferredAudioCharacteristic(const String&);

    void setMaxCanvasPixelMemory(unsigned);
    void setMaxCanvasArea(unsigned);

    ExceptionOr<unsigned> wheelEventHandlerCount();
    ExceptionOr<unsigned> touchEventHandlerCount();

    ExceptionOr<Ref<DOMRectList>> touchEventRectsForEvent(const String&);
    ExceptionOr<Ref<DOMRectList>> passiveTouchEventListenerRects();

    ExceptionOr<RefPtr<NodeList>> nodesFromRect(Document&, int x, int y, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowUserAgentShadowContent, bool allowChildFrameContent) const;

    String parserMetaData(JSC::JSValue = JSC::JSValue::JSUndefined);

    void updateEditorUINowIfScheduled();

    static bool sentenceRetroCorrectionEnabled()
    {
#if PLATFORM(MAC)
        return true;
#else
        return false;
#endif
    }
    bool hasSpellingMarker(int from, int length);
    bool hasGrammarMarker(int from, int length);
    bool hasAutocorrectedMarker(int from, int length);
    bool hasDictationAlternativesMarker(int from, int length);
    void setContinuousSpellCheckingEnabled(bool);
    void setAutomaticQuoteSubstitutionEnabled(bool);
    void setAutomaticLinkDetectionEnabled(bool);
    void setAutomaticDashSubstitutionEnabled(bool);
    void setAutomaticTextReplacementEnabled(bool);
    void setAutomaticSpellingCorrectionEnabled(bool);

    bool isSpellcheckDisabledExceptTextReplacement(const HTMLInputElement&) const;

    void handleAcceptedCandidate(const String& candidate, unsigned location, unsigned length);
    void changeSelectionListType();
    void changeBackToReplacedString(const String& replacedString);

    bool isOverwriteModeEnabled();
    void toggleOverwriteModeEnabled();

    bool testProcessIncomingSyncMessagesWhenWaitingForSyncReply();

    ExceptionOr<RefPtr<Range>> rangeOfString(const String&, RefPtr<Range>&&, const Vector<String>& findOptions);
    ExceptionOr<unsigned> countMatchesForText(const String&, const Vector<String>& findOptions, const String& markMatches);
    ExceptionOr<unsigned> countFindMatches(const String&, const Vector<String>& findOptions);

    unsigned numberOfScrollableAreas();

    ExceptionOr<bool> isPageBoxVisible(int pageNumber);

    static constexpr ASCIILiteral internalsId = "internals"_s;

    InternalSettings* settings() const;
    unsigned workerThreadCount() const;
    ExceptionOr<bool> areSVGAnimationsPaused() const;
    ExceptionOr<double> svgAnimationsInterval(SVGSVGElement&) const;
    // Some SVGSVGElements are not accessible via JavaScript (e.g. those in CSS `background: url(data:image/svg+xml;utf8,<svg>...)`, but we need access to them for testing.
    Vector<Ref<SVGSVGElement>> allSVGSVGElements() const;

    enum {
        // Values need to be kept in sync with Internals.idl.
        LAYER_TREE_INCLUDES_VISIBLE_RECTS = 1,
        LAYER_TREE_INCLUDES_TILE_CACHES = 2,
        LAYER_TREE_INCLUDES_REPAINT_RECTS = 4,
        LAYER_TREE_INCLUDES_PAINTING_PHASES = 8,
        LAYER_TREE_INCLUDES_CONTENT_LAYERS = 16,
        LAYER_TREE_INCLUDES_ACCELERATES_DRAWING = 32,
        LAYER_TREE_INCLUDES_CLIPPING = 64,
        LAYER_TREE_INCLUDES_BACKING_STORE_ATTACHED = 128,
        LAYER_TREE_INCLUDES_ROOT_LAYER_PROPERTIES = 256,
        LAYER_TREE_INCLUDES_EVENT_REGION = 512,
        LAYER_TREE_INCLUDES_DEEP_COLOR = 1024,
        LAYER_TREE_INCLUDES_DEVICE_SCALE = 2048,
    };
    ExceptionOr<String> layerTreeAsText(Document&, unsigned short flags) const;
    ExceptionOr<uint64_t> layerIDForElement(Element&);
    ExceptionOr<String> repaintRectsAsText() const;

    enum {
        // Values need to be kept in sync with Internals.idl.
        PLATFORM_LAYER_TREE_DEBUG = 1,
        PLATFORM_LAYER_TREE_IGNORES_CHILDREN = 2,
        PLATFORM_LAYER_TREE_INCLUDE_MODELS = 4,
    };
    ExceptionOr<String> platformLayerTreeAsText(Element&, unsigned short flags) const;

    ExceptionOr<String> scrollbarOverlayStyle(Node*) const;
    ExceptionOr<bool> scrollbarUsingDarkAppearance(Node*) const;

    ExceptionOr<String> horizontalScrollbarState(Node*) const;
    ExceptionOr<String> verticalScrollbarState(Node*) const;

    ExceptionOr<String> scrollingStateTreeAsText() const;
    ExceptionOr<String> scrollingTreeAsText() const;
    ExceptionOr<String> synchronousScrollingReasons() const;
    ExceptionOr<Ref<DOMRectList>> nonFastScrollableRects() const;

    ExceptionOr<void> setElementUsesDisplayListDrawing(Element&, bool usesDisplayListDrawing);
    ExceptionOr<void> setElementTracksDisplayListReplay(Element&, bool isTrackingReplay);

    enum {
        // Values need to be kept in sync with Internals.idl.
        DISPLAY_LIST_INCLUDE_PLATFORM_OPERATIONS = 1,
        DISPLAY_LIST_INCLUDE_RESOURCE_IDENTIFIERS = 2,
    };
    ExceptionOr<String> displayListForElement(Element&, unsigned short flags);
    ExceptionOr<String> replayDisplayListForElement(Element&, unsigned short flags);

    void setForceUseGlyphDisplayListForTesting(bool enabled);
    ExceptionOr<String> cachedGlyphDisplayListsForTextNode(Node&, unsigned short flags);
    void clearGlyphDisplayListCacheForTesting();

    ExceptionOr<void> garbageCollectDocumentResources() const;

    void beginSimulatedMemoryPressure();
    void endSimulatedMemoryPressure();
    bool isUnderMemoryPressure();

    ExceptionOr<void> insertAuthorCSS(const String&) const;
    ExceptionOr<void> insertUserCSS(const String&) const;

    unsigned numberOfIDBTransactions() const;

    unsigned numberOfLiveNodes() const;
    unsigned numberOfLiveDocuments() const;
    unsigned referencingNodeCount(const Document&) const;

#if ENABLE(WEB_AUDIO)
    // BaseAudioContext lifetime testing.
    static uint64_t baseAudioContextIdentifier(const BaseAudioContext&);
    static bool isBaseAudioContextAlive(uint64_t contextID);
#endif

    unsigned numberOfIntersectionObservers(const Document&) const;

    unsigned numberOfResizeObservers(const Document&) const;

    String documentIdentifier(const Document&) const;
    bool isDocumentAlive(const String& documentIdentifier) const;

    uint64_t messagePortIdentifier(const MessagePort&) const;
    bool isMessagePortAlive(uint64_t messagePortIdentifier) const;

    uint64_t storageAreaMapCount() const;

    uint64_t elementIdentifier(Element&) const;
    bool isElementAlive(uint64_t elementIdentifier) const;

    uint64_t frameIdentifier(const Document&) const;
    uint64_t pageIdentifier(const Document&) const;

    bool isAnyWorkletGlobalScopeAlive() const;

    String serviceWorkerClientInternalIdentifier(const Document&) const;

    RefPtr<WindowProxy> openDummyInspectorFrontend(const String& url);
    void closeDummyInspectorFrontend();
    ExceptionOr<void> setInspectorIsUnderTest(bool);

    String counterValue(Element&);

    int pageNumber(Element&, float pageWidth = 800, float pageHeight = 600);
    Vector<String> shortcutIconURLs() const;

    int numberOfPages(float pageWidthInPixels = 800, float pageHeightInPixels = 600);
    ExceptionOr<String> pageProperty(const String& propertyName, int pageNumber) const;
    ExceptionOr<String> pageSizeAndMarginsInPixels(int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft) const;

    ExceptionOr<float> pageScaleFactor() const;

    ExceptionOr<void> setPageScaleFactor(float scaleFactor, int x, int y);
    ExceptionOr<void> setPageZoomFactor(float);
    ExceptionOr<void> setTextZoomFactor(float);

    ExceptionOr<void> setUseFixedLayout(bool);
    ExceptionOr<void> setFixedLayoutSize(int width, int height);
    ExceptionOr<void> setViewExposedRect(float left, float top, float width, float height);
    void setPrinting(int width, int height);

    void setHeaderHeight(float);
    void setFooterHeight(float);

    void setTopContentInset(float);

#if ENABLE(FULLSCREEN_API)
    void webkitWillEnterFullScreenForElement(Element&);
    void webkitDidEnterFullScreenForElement(Element&);
    void webkitWillExitFullScreenForElement(Element&);
    void webkitDidExitFullScreenForElement(Element&);
    bool isAnimatingFullScreen() const;
#endif

    struct FullscreenInsets {
        float top { 0 };
        float left { 0 };
        float bottom { 0 };
        float right { 0 };
    };
    void setFullscreenInsets(FullscreenInsets);
    void setFullscreenAutoHideDuration(double);
    void setFullscreenControlsHidden(bool);

#if ENABLE(VIDEO)
    bool isChangingPresentationMode(HTMLVideoElement&) const;
#endif

#if ENABLE(VIDEO_PRESENTATION_MODE)
    void setMockVideoPresentationModeEnabled(bool);
#endif

    WEBCORE_TESTSUPPORT_EXPORT void setApplicationCacheOriginQuota(unsigned long long);

    void registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme);
    void removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme);

    void registerDefaultPortForProtocol(unsigned short port, const String& protocol);

    Ref<MallocStatistics> mallocStatistics() const;
    Ref<TypeConversions> typeConversions() const;
    Ref<MemoryInfo> memoryInfo() const;

    Vector<String> getReferencedFilePaths() const;

    ExceptionOr<void> startTrackingRepaints();
    ExceptionOr<void> stopTrackingRepaints();

    ExceptionOr<void> startTrackingLayerFlushes();
    ExceptionOr<unsigned> layerFlushCount();

    ExceptionOr<void> startTrackingStyleRecalcs();
    ExceptionOr<unsigned> styleRecalcCount();
    unsigned lastStyleUpdateSize() const;

    ExceptionOr<void> startTrackingCompositingUpdates();
    ExceptionOr<unsigned> compositingUpdateCount();

    ExceptionOr<void> startTrackingRenderingUpdates();
    ExceptionOr<unsigned> renderingUpdateCount();

    enum CompositingPolicy { Normal, Conservative };
    ExceptionOr<void> setCompositingPolicyOverride(std::optional<CompositingPolicy>);
    ExceptionOr<std::optional<CompositingPolicy>> compositingPolicyOverride() const;

    void updateLayoutAndStyleForAllFrames();
    ExceptionOr<void> updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(Node*);
    unsigned layoutCount() const;

    Ref<ArrayBuffer> serializeObject(const RefPtr<SerializedScriptValue>&) const;
    Ref<SerializedScriptValue> deserializeBuffer(ArrayBuffer&) const;

    bool isFromCurrentWorld(JSC::JSValue) const;
    JSC::JSValue evaluateInWorldIgnoringException(const String& name, const String& source);

    void setUsesOverlayScrollbars(bool);

    ExceptionOr<String> getCurrentCursorInfo();

    String markerTextForListItem(Element&);

    String toolTipFromElement(Element&) const;

    void forceReload(bool endToEnd);
    void reloadExpiredOnly();

    void enableFixedWidthAutoSizeMode(bool enabled, int width, int height);
    void enableSizeToContentAutoSizeMode(bool enabled, int width, int height);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void initializeMockCDM();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    Ref<MockCDMFactory> registerMockCDM();
#endif

    void enableMockMediaCapabilities();

#if ENABLE(SPEECH_SYNTHESIS)
    void enableMockSpeechSynthesizer();
    void enableMockSpeechSynthesizerForMediaElement(HTMLMediaElement&);
    ExceptionOr<void> setSpeechUtteranceDuration(double);
#endif

#if ENABLE(MEDIA_STREAM)
    void setShouldInterruptAudioOnPageVisibilityChange(bool);
#endif
#if ENABLE(MEDIA_RECORDER)
    void setCustomPrivateRecorderCreator();
#endif

#if ENABLE(WEB_RTC)
    void emulateRTCPeerConnectionPlatformEvent(RTCPeerConnection&, const String& action);
    void useMockRTCPeerConnectionFactory(const String&);
    void setICECandidateFiltering(bool);
    void setEnumeratingAllNetworkInterfacesEnabled(bool);
    void stopPeerConnection(RTCPeerConnection&);
    void clearPeerConnectionFactory();
    void applyRotationForOutgoingVideoSources(RTCPeerConnection&);
    void setWebRTCH265Support(bool);
    void setWebRTCVP9Support(bool supportVP9Profile0, bool supportVP9Profile2);
    void setWebRTCVP9VTBSupport(bool);
    bool isSupportingVP9VTB() const;
    void isVP9VTBDeccoderUsed(RTCPeerConnection&, DOMPromiseDeferred<IDLBoolean>&&);

    void setSFrameCounter(RTCRtpSFrameTransform&, const String&);
    uint64_t sframeCounter(const RTCRtpSFrameTransform&);
    uint64_t sframeKeyId(const RTCRtpSFrameTransform&);
    void setEnableWebRTCEncryption(bool);
    void setUseDTLS10(bool);
#endif

    String getImageSourceURL(Element&);

    String blobInternalURL(const Blob&);
    void isBlobInternalURLRegistered(const String&, DOMPromiseDeferred<IDLBoolean>&&);

#if ENABLE(VIDEO)
    unsigned mediaElementCount();
    Vector<String> mediaResponseSources(HTMLMediaElement&);
    Vector<String> mediaResponseContentRanges(HTMLMediaElement&);
    void simulateAudioInterruption(HTMLMediaElement&);
    ExceptionOr<bool> mediaElementHasCharacteristic(HTMLMediaElement&, const String&);
    void beginSimulatedHDCPError(HTMLMediaElement&);
    void endSimulatedHDCPError(HTMLMediaElement&);
    ExceptionOr<bool> mediaPlayerRenderingCanBeAccelerated(HTMLMediaElement&);

    bool elementShouldBufferData(HTMLMediaElement&);
    String elementBufferingPolicy(HTMLMediaElement&);
    double privatePlayerVolume(const HTMLMediaElement&);
    bool privatePlayerMuted(const HTMLMediaElement&);
    bool isMediaElementHidden(const HTMLMediaElement&);
    double elementEffectivePlaybackRate(const HTMLMediaElement&);

    ExceptionOr<void> setOverridePreferredDynamicRangeMode(HTMLMediaElement&, const String&);
#endif

    ExceptionOr<void> setIsPlayingToBluetoothOverride(std::optional<bool>);

    bool isSelectPopupVisible(HTMLSelectElement&);

    ExceptionOr<String> captionsStyleSheetOverride();
    ExceptionOr<void> setCaptionsStyleSheetOverride(const String&);
    ExceptionOr<void> setPrimaryAudioTrackLanguageOverride(const String&);
    ExceptionOr<void> setCaptionDisplayMode(const String&);
#if ENABLE(VIDEO)
    RefPtr<TextTrackCueGeneric> createGenericCue(double startTime, double endTime, String text);
    ExceptionOr<String> textTrackBCP47Language(TextTrack&);
    Ref<TimeRanges> createTimeRanges(Float32Array& startTimes, Float32Array& endTimes);
    double closestTimeToTimeRanges(double time, TimeRanges&);
#endif

    ExceptionOr<Ref<DOMRect>> selectionBounds();
    ExceptionOr<RefPtr<StaticRange>> selectedRange();
    void setSelectionWithoutValidation(Ref<Node> baseNode, unsigned baseOffset, RefPtr<Node> extentNode, unsigned extentOffset);

    ExceptionOr<bool> isPluginUnavailabilityIndicatorObscured(Element&);
    ExceptionOr<String> unavailablePluginReplacementText(Element&);
    bool isPluginSnapshotted(Element&);

#if ENABLE(MEDIA_SOURCE)
    WEBCORE_TESTSUPPORT_EXPORT void initializeMockMediaSource();
    using BufferedSamplesPromise = DOMPromiseDeferred<IDLSequence<IDLDOMString>>;
    void bufferedSamplesForTrackId(SourceBuffer&, const AtomString&, BufferedSamplesPromise&&);
    void enqueuedSamplesForTrackID(SourceBuffer&, const AtomString&, BufferedSamplesPromise&&);
    double minimumUpcomingPresentationTimeForTrackID(SourceBuffer&, const AtomString&);
    void setShouldGenerateTimestamps(SourceBuffer&, bool);
    void setMaximumQueueDepthForTrackID(SourceBuffer&, const AtomString&, size_t);
#endif

#if ENABLE(VIDEO)
    ExceptionOr<void> beginMediaSessionInterruption(const String&);
    void endMediaSessionInterruption(const String&);
    void applicationWillBecomeInactive();
    void applicationDidBecomeActive();
    void applicationWillEnterForeground(bool suspendedUnderLock) const;
    void applicationDidEnterBackground(bool suspendedUnderLock) const;
    ExceptionOr<void> setMediaSessionRestrictions(const String& mediaType, StringView restrictionsString);
    ExceptionOr<String> mediaSessionRestrictions(const String& mediaType) const;
    void setMediaElementRestrictions(HTMLMediaElement&, StringView restrictionsString);
    ExceptionOr<void> postRemoteControlCommand(const String&, float argument);
    void activeAudioRouteDidChange(bool shouldPause);
    bool elementIsBlockingDisplaySleep(const HTMLMediaElement&) const;
    bool isPlayerVisibleInViewport(const HTMLMediaElement&) const;
    bool isPlayerMuted(const HTMLMediaElement&) const;
    void beginAudioSessionInterruption();
    void endAudioSessionInterruption();
#endif

#if ENABLE(WIRELESS_PLAYBACK_TARGET)
    void setMockMediaPlaybackTargetPickerEnabled(bool);
    ExceptionOr<void> setMockMediaPlaybackTargetPickerState(const String& deviceName, const String& deviceState);
    void mockMediaPlaybackTargetPickerDismissPopup();
#endif

#if ENABLE(WEB_AUDIO)
    void setAudioContextRestrictions(AudioContext&, StringView restrictionsString);
    void useMockAudioDestinationCocoa();
#endif

    void simulateSystemSleep() const;
    void simulateSystemWake() const;

    unsigned inflightBeaconsCount() const;

    enum class PageOverlayType { View, Document };
    ExceptionOr<Ref<MockPageOverlay>> installMockPageOverlay(PageOverlayType);
    ExceptionOr<String> pageOverlayLayerTreeAsText(unsigned short flags) const;

    void setPageMuted(StringView);
    String pageMediaState();

    void setPageDefersLoading(bool);
    ExceptionOr<bool> pageDefersLoading();

    RefPtr<File> createFile(const String&);
    String createTemporaryFile(const String& name, const String& contents);

    void queueMicroTask(int);
    bool testPreloaderSettingViewport();

#if ENABLE(CONTENT_FILTERING)
    MockContentFilterSettings& mockContentFilterSettings();
#endif

    ExceptionOr<String> scrollSnapOffsets(Element&);
    ExceptionOr<bool> isScrollSnapInProgress(Element&);
    void setPlatformMomentumScrollingPredictionEnabled(bool);

    ExceptionOr<String> pathStringWithShrinkWrappedRects(const Vector<double>& rectComponents, double radius);

#if ENABLE(VIDEO)
    String getCurrentMediaControlsStatusForElement(HTMLMediaElement&);
    void setMediaControlsMaximumRightContainerButtonCountOverride(HTMLMediaElement&, size_t);
    void setMediaControlsHidePlaybackRates(HTMLMediaElement&, bool);
#endif // ENABLE(VIDEO)

    void setPageMediaVolume(float);

    String userVisibleString(const DOMURL&);
    void setShowAllPlugins(bool);

    String resourceLoadStatisticsForURL(const DOMURL&);
    void setTrackingPreventionEnabled(bool);

    bool isReadableStreamDisturbed(ReadableStream&);
    JSC::JSValue cloneArrayBuffer(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue, JSC::JSValue);

    String composedTreeAsText(Node&);

    bool isProcessingUserGesture();
    double lastHandledUserGestureTimestamp();

    void withUserGesture(RefPtr<VoidCallback>&&);
    void withoutUserGesture(RefPtr<VoidCallback>&&);

    bool userIsInteracting();

    bool hasTransientActivation();

    bool consumeTransientActivation();

    RefPtr<GCObservation> observeGC(JSC::JSValue);

    enum class UserInterfaceLayoutDirection : uint8_t { LTR, RTL };
    void setUserInterfaceLayoutDirection(UserInterfaceLayoutDirection);

    bool userPrefersContrast() const;
    bool userPrefersReducedMotion() const;

    void reportBacktrace();

    enum class BaseWritingDirection { Natural, Ltr, Rtl };
    void setBaseWritingDirection(BaseWritingDirection);

#if ENABLE(POINTER_LOCK)
    bool pageHasPendingPointerLock() const;
    bool pageHasPointerLock() const;
#endif

    Vector<String> accessKeyModifiers() const;

    void setQuickLookPassword(const String&);

    void setAsRunningUserScripts(Document&);

#if ENABLE(WEBGL)
    enum class SimulatedWebGLContextEvent {
        ContextChange,
        GPUStatusFailure,
        Timeout
    };
    void simulateEventForWebGLContext(SimulatedWebGLContextEvent, WebGLRenderingContext&);
    bool hasLowAndHighPowerGPUs();

    enum class RequestedGPU {
        Default,
        LowPower,
        HighPerformance
    };
    RequestedGPU requestedGPU(WebGLRenderingContext&);
    bool requestedMetal(WebGLRenderingContext&);
    bool platformSupportsMetal(bool isWebGL2);
#endif

    void setPageVisibility(bool isVisible);
    void setPageIsFocused(bool);
    void setPageIsFocusedAndActive(bool);
    void setPageIsInWindow(bool);
    bool isPageActive() const;

#if ENABLE(WEB_RTC)
    void setH264HardwareEncoderAllowed(bool allowed);
#endif

#if ENABLE(MEDIA_STREAM)
    void stopObservingRealtimeMediaSource();

    void setMockAudioTrackChannelNumber(MediaStreamTrack&, unsigned short);
    void setCameraMediaStreamTrackOrientation(MediaStreamTrack&, int orientation);
    unsigned long trackAudioSampleCount() const { return m_trackAudioSampleCount; }
    unsigned long trackVideoSampleCount() const { return m_trackVideoSampleCount; }
    void observeMediaStreamTrack(MediaStreamTrack&);
    using TrackFramePromise = DOMPromiseDeferred<IDLInterface<ImageData>>;
    void grabNextMediaStreamTrackFrame(TrackFramePromise&&);
    void mediaStreamTrackVideoFrameRotation(DOMPromiseDeferred<IDLShort>&&);
    void delayMediaStreamTrackSamples(MediaStreamTrack&, float);
    void setMediaStreamTrackMuted(MediaStreamTrack&, bool);
    void removeMediaStreamTrack(MediaStream&, MediaStreamTrack&);
    void simulateMediaStreamTrackCaptureSourceFailure(MediaStreamTrack&);
    void setMediaStreamTrackIdentifier(MediaStreamTrack&, String&& id);
    void setMediaStreamSourceInterrupted(MediaStreamTrack&, bool);
    bool isMediaStreamSourceInterrupted(MediaStreamTrack&) const;
    bool isMediaStreamSourceEnded(MediaStreamTrack&) const;
    bool isMockRealtimeMediaSourceCenterEnabled();
    bool shouldAudioTrackPlay(const AudioTrack&);
#endif


#if USE(AUDIO_SESSION)
    using AudioSessionCategory = WebCore::AudioSessionCategory;
    using RouteSharingPolicy = WebCore::RouteSharingPolicy;
#else
    enum class AudioSessionCategory : uint8_t {
        None,
        AmbientSound,
        SoloAmbientSound,
        MediaPlayback,
        RecordAudio,
        PlayAndRecord,
        AudioProcessing,
    };

    enum class RouteSharingPolicy : uint8_t {
        Default,
        LongFormAudio,
        Independent,
        LongFormVideo
    };
#endif

    bool supportsAudioSession() const;
    AudioSessionCategory audioSessionCategory() const;
    RouteSharingPolicy routeSharingPolicy() const;
#if ENABLE(VIDEO)
    AudioSessionCategory categoryAtMostRecentPlayback(HTMLMediaElement&) const;
#endif
    double preferredAudioBufferSize() const;
    double currentAudioBufferSize() const;
    bool audioSessionActive() const;

    void storeRegistrationsOnDisk(DOMPromiseDeferred<void>&&);
    void sendH2Ping(String url, DOMPromiseDeferred<IDLDouble>&&);

    void clearCacheStorageMemoryRepresentation(DOMPromiseDeferred<void>&&);
    void cacheStorageEngineRepresentation(DOMPromiseDeferred<IDLDOMString>&&);
    void setResponseSizeWithPadding(FetchResponse&, uint64_t size);
    uint64_t responseSizeWithPadding(FetchResponse&) const;
    const String& responseNetworkLoadMetricsProtocol(const FetchResponse&);

    void updateQuotaBasedOnSpaceUsage();

    void setConsoleMessageListener(RefPtr<StringCallback>&&);

#if ENABLE(SERVICE_WORKER)
    using HasRegistrationPromise = DOMPromiseDeferred<IDLBoolean>;
    void hasServiceWorkerRegistration(const String& clientURL, HasRegistrationPromise&&);
    void terminateServiceWorker(ServiceWorker&, DOMPromiseDeferred<void>&&);
    void whenServiceWorkerIsTerminated(ServiceWorker&, DOMPromiseDeferred<void>&&);
#endif

#if ENABLE(APPLE_PAY)
    MockPaymentCoordinator& mockPaymentCoordinator(Document&);
#endif

    struct ImageOverlayText {
        String text;
        RefPtr<DOMPointReadOnly> topLeft;
        RefPtr<DOMPointReadOnly> topRight;
        RefPtr<DOMPointReadOnly> bottomRight;
        RefPtr<DOMPointReadOnly> bottomLeft;
        bool hasLeadingWhitespace { true };

        ~ImageOverlayText();
    };

    struct ImageOverlayLine {
        RefPtr<DOMPointReadOnly> topLeft;
        RefPtr<DOMPointReadOnly> topRight;
        RefPtr<DOMPointReadOnly> bottomRight;
        RefPtr<DOMPointReadOnly> bottomLeft;
        Vector<ImageOverlayText> children;
        bool hasTrailingNewline { true };

        ~ImageOverlayLine();
    };

    struct ImageOverlayBlock {
        String text;
        RefPtr<DOMPointReadOnly> topLeft;
        RefPtr<DOMPointReadOnly> topRight;
        RefPtr<DOMPointReadOnly> bottomRight;
        RefPtr<DOMPointReadOnly> bottomLeft;

        ~ImageOverlayBlock();
    };

    struct ImageOverlayDataDetector {
        RefPtr<DOMPointReadOnly> topLeft;
        RefPtr<DOMPointReadOnly> topRight;
        RefPtr<DOMPointReadOnly> bottomRight;
        RefPtr<DOMPointReadOnly> bottomLeft;

        ~ImageOverlayDataDetector();
    };

    void installImageOverlay(Element&, Vector<ImageOverlayLine>&&, Vector<ImageOverlayBlock>&& = { }, Vector<ImageOverlayDataDetector>&& = { });
    bool hasActiveDataDetectorHighlight() const;

#if ENABLE(IMAGE_ANALYSIS)
    void requestTextRecognition(Element&, RefPtr<VoidCallback>&&);
    RefPtr<Element> textRecognitionCandidate() const;
#endif

    bool isSystemPreviewLink(Element&) const;
    bool isSystemPreviewImage(Element&) const;

    void postTask(RefPtr<VoidCallback>&&);
    ExceptionOr<void> queueTask(ScriptExecutionContext&, const String& source, RefPtr<VoidCallback>&&);
    ExceptionOr<void> queueTaskToQueueMicrotask(Document&, const String& source, RefPtr<VoidCallback>&&);
    ExceptionOr<bool> hasSameEventLoopAs(WindowProxy&);

    void markContextAsInsecure();

    bool usingAppleInternalSDK() const;
    bool usingGStreamer() const;

    struct NowPlayingState {
        String title;
        double duration;
        double elapsedTime;
        uint64_t uniqueIdentifier;
        bool hasActiveSession;
        bool registeredAsNowPlayingApplication;
        bool haveEverRegisteredAsNowPlayingApplication;
    };
    ExceptionOr<NowPlayingState> nowPlayingState() const;

    struct MediaUsageState {
        String mediaURL;
        bool isPlaying;
        bool canShowControlsManager;
        bool canShowNowPlayingControls;
        bool isSuspended;
        bool isInActiveDocument;
        bool isFullscreen;
        bool isMuted;
        bool isMediaDocumentInMainFrame;
        bool isVideo;
        bool isAudio;
        bool hasVideo;
        bool hasAudio;
        bool hasRenderer;
        bool audioElementWithUserGesture;
        bool userHasPlayedAudioBefore;
        bool isElementRectMostlyInMainFrame;
        bool playbackPermitted;
        bool pageMediaPlaybackSuspended;
        bool isMediaDocumentAndNotOwnerElement;
        bool pageExplicitlyAllowsElementToAutoplayInline;
        bool requiresFullscreenForVideoPlaybackAndFullscreenNotPermitted;
        bool hasHadUserInteractionAndQuirksContainsShouldAutoplayForArbitraryUserGesture;
        bool isVideoAndRequiresUserGestureForVideoRateChange;
        bool isAudioAndRequiresUserGestureForAudioRateChange;
        bool isVideoAndRequiresUserGestureForVideoDueToLowPowerMode;
        bool noUserGestureRequired;
        bool requiresPlaybackAndIsNotPlaying;
        bool hasEverNotifiedAboutPlaying;
        bool outsideOfFullscreen;
        bool isLargeEnoughForMainContent;
    };
    ExceptionOr<MediaUsageState> mediaUsageState(HTMLMediaElement&) const;

    ExceptionOr<bool> elementShouldDisplayPosterImage(HTMLVideoElement&) const;

#if ENABLE(VIDEO)
    using PlaybackControlsPurpose = MediaElementSession::PlaybackControlsPurpose;
    RefPtr<HTMLMediaElement> bestMediaElementForRemoteControls(PlaybackControlsPurpose);

    // Same values as PlatformMediaSession::State, but re-declared to avoid redefinitions when linking
    // directly with libWebCore (e.g. with non-unified builds)
    enum MediaSessionState {
        Idle,
        Autoplaying,
        Playing,
        Paused,
        Interrupted,
    };
    MediaSessionState mediaSessionState(HTMLMediaElement&);

    size_t mediaElementCount() const;

    void setMediaElementVolumeLocked(HTMLMediaElement&, bool);

#if ENABLE(SPEECH_SYNTHESIS)
    ExceptionOr<RefPtr<SpeechSynthesisUtterance>> speechSynthesisUtteranceForCue(const VTTCue&);
    ExceptionOr<RefPtr<VTTCue>> mediaElementCurrentlySpokenCue(HTMLMediaElement&);
#endif

#endif // ENABLE(VIDEO)

    void setCaptureExtraNetworkLoadMetricsEnabled(bool);
    String ongoingLoadsDescriptions() const;

    void reloadWithoutContentExtensions();

    void setUseSystemAppearance(bool);

    size_t pluginCount();

    void notifyResourceLoadObserver();

    unsigned primaryScreenDisplayID();

    bool capsLockIsOn();
        
    using HEVCParameterSet = WebCore::HEVCParameters;
    using HEVCParameterCodec = WebCore::HEVCParameters::Codec;
    std::optional<HEVCParameterSet> parseHEVCCodecParameters(StringView);
    String createHEVCCodecParametersString(const HEVCParameterSet& parameters);

    struct DoViParameterSet {
        String codecName;
        uint16_t bitstreamProfileID;
        uint16_t bitstreamLevelID;
    };
    std::optional<DoViParameterSet> parseDoViCodecParameters(StringView);
    String createDoViCodecParametersString(const DoViParameterSet& parameters);

    using VPCodecConfigurationRecord = WebCore::VPCodecConfigurationRecord;
    std::optional<VPCodecConfigurationRecord> parseVPCodecParameters(StringView);

    struct CookieData {
        String name;
        String value;
        String domain;
        String path;
        // Expiration dates are expressed as milliseconds since the UNIX epoch.
        double expires { 0 };
        bool isHttpOnly { false };
        bool isSecure { false };
        bool isSession { false };
        bool isSameSiteNone { false };
        bool isSameSiteLax { false };
        bool isSameSiteStrict { false };

        CookieData(Cookie cookie)
            : name(cookie.name)
            , value(cookie.value)
            , domain(cookie.domain)
            , path(cookie.path)
            , expires(cookie.expires.value_or(0))
            , isHttpOnly(cookie.httpOnly)
            , isSecure(cookie.secure)
            , isSession(cookie.session)
            , isSameSiteNone(cookie.sameSite == Cookie::SameSitePolicy::None)
            , isSameSiteLax(cookie.sameSite == Cookie::SameSitePolicy::Lax)
            , isSameSiteStrict(cookie.sameSite == Cookie::SameSitePolicy::Strict)
        {
            ASSERT(!(isSameSiteLax && isSameSiteStrict) && !(isSameSiteLax && isSameSiteNone) && !(isSameSiteStrict && isSameSiteNone));
        }

        CookieData()
        {
        }
    };
    Vector<CookieData> getCookies() const;

    void setAlwaysAllowLocalWebarchive(bool);
    void processWillSuspend();
    void processDidResume();

    void testDictionaryLogging();

    void setMaximumIntervalForUserGestureForwardingForFetch(double);
    void setTransientActivationDuration(double seconds);

    void setIsPlayingToAutomotiveHeadUnit(bool);
    
    struct TextIndicatorInfo {
        RefPtr<DOMRectReadOnly> textBoundingRectInRootViewCoordinates;
        RefPtr<DOMRectList> textRectsInBoundingRectCoordinates;
        
        TextIndicatorInfo();
        TextIndicatorInfo(const WebCore::TextIndicatorData&);
        ~TextIndicatorInfo();
    };
        
    struct TextIndicatorOptions {
        bool useBoundingRectAndPaintAllContentForComplexRanges { false };
        bool computeEstimatedBackgroundColor { false };
        bool respectTextColor { false };
        bool useUserSelectAllCommonAncestor { false };

        OptionSet<WebCore::TextIndicatorOption> coreOptions()
        {
            OptionSet<WebCore::TextIndicatorOption> options;
            if (useBoundingRectAndPaintAllContentForComplexRanges)
                options.add(TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges);
            if (computeEstimatedBackgroundColor)
                options.add(TextIndicatorOption::ComputeEstimatedBackgroundColor);
            if (respectTextColor)
                options.add(TextIndicatorOption::RespectTextColor);
            if (useUserSelectAllCommonAncestor)
                options.add(TextIndicatorOption::UseUserSelectAllCommonAncestor);
            return options;
        }
    };

    TextIndicatorInfo textIndicatorForRange(const Range&, TextIndicatorOptions);

    void addPrefetchLoadEventListener(HTMLLinkElement&, RefPtr<EventListener>&&);

#if ENABLE(WEB_AUTHN)
    void setMockWebAuthenticationConfiguration(const MockWebAuthenticationConfiguration&);
#endif

    int processIdentifier() const;

    Ref<InternalsSetLike> createInternalsSetLike();
    Ref<InternalsMapLike> createInternalsMapLike();

    bool hasSandboxMachLookupAccessToGlobalName(const String& process, const String& service);
    bool hasSandboxMachLookupAccessToXPCServiceName(const String& process, const String& service);
    bool hasSandboxIOKitOpenAccessToClass(const String& process, const String& ioKitClass);
    bool hasSandboxUnixSyscallAccess(const String& process, unsigned syscall) const;
        
    String highlightPseudoElementColor(const AtomString& highlightName, Element&);

    String windowLocationHost(DOMWindow&);

    String systemColorForCSSValue(const String& cssValue, bool useDarkModeAppearance, bool useElevatedUserInterfaceLevel);

    bool systemHasBattery() const;

    void setSystemHasBatteryForTesting(bool);
    void setSystemHasACForTesting(bool);

    void setHardwareVP9DecoderDisabledForTesting(bool);
    void setVP9DecoderDisabledForTesting(bool);
    void setVP9ScreenSizeAndScaleForTesting(double, double, double);

    int readPreferenceInteger(const String& domain, const String& key);
    String encodedPreferenceValue(const String& domain, const String& key);

    String getUTIFromTag(const String& tagClass, const String& tag, const String& conformingToUTI);

    bool supportsPictureInPicture();

    String focusRingColor();

    bool isRemoteUIAppForAccessibility();

    unsigned createSleepDisabler(const String& reason, bool display);
    bool destroySleepDisabler(unsigned identifier);
        
#if ENABLE(APP_HIGHLIGHTS)
    Vector<String> appHighlightContextMenuItemTitles() const;
    unsigned numberOfAppHighlights();
#endif

#if ENABLE(WEBXR)
    ExceptionOr<RefPtr<WebXRTest>> xrTest();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    unsigned mediaKeysInternalInstanceObjectRefCount(const MediaKeys&) const;
    unsigned mediaKeySessionInternalInstanceSessionObjectRefCount(const MediaKeySession&) const;
#endif

    enum class ContentSizeCategory { L, XXXL };
    void setContentSizeCategory(ContentSizeCategory);

#if ENABLE(ATTACHMENT_ELEMENT)
    struct AttachmentThumbnailInfo {
        unsigned width { 0 };
        unsigned height { 0 };
    };

    ExceptionOr<AttachmentThumbnailInfo> attachmentThumbnailInfo(const HTMLAttachmentElement&);
#if ENABLE(SERVICE_CONTROLS)
    bool hasImageControls(const HTMLImageElement&) const;
#endif
#endif // ENABLE(ATTACHMENT_ELEMENT)

#if ENABLE(MEDIA_SESSION)
    ExceptionOr<double> currentMediaSessionPosition(const MediaSession&);
    ExceptionOr<void> sendMediaSessionAction(MediaSession&, const MediaSessionActionDetails&);

        using ArtworkImagePromise = DOMPromiseDeferred<IDLInterface<ImageData>>;
    void loadArtworkImage(String&&, ArtworkImagePromise&&);
    ExceptionOr<Vector<String>> platformSupportedCommands() const;

#if ENABLE(MEDIA_SESSION_COORDINATOR)
    ExceptionOr<void> registerMockMediaSessionCoordinator(ScriptExecutionContext&, RefPtr<StringCallback>&&);
    ExceptionOr<void> setMockMediaSessionCoordinatorCommandsShouldFail(bool);
#endif

#endif // ENABLE(MEDIA_SESSION)

    enum TreeType : uint8_t { Tree, ShadowIncludingTree, ComposedTree };
    String treeOrder(Node&, Node&, TreeType);
    String treeOrderBoundaryPoints(Node& containerA, unsigned offsetA, Node& containerB, unsigned offsetB, TreeType);
    bool rangeContainsNode(const AbstractRange&, Node&, TreeType);
    bool rangeContainsRange(const AbstractRange&, const AbstractRange&, TreeType);
    bool rangeContainsBoundaryPoint(const AbstractRange&, Node&, unsigned offset, TreeType);
    bool rangeIntersectsNode(const AbstractRange&, Node&, TreeType);
    bool rangeIntersectsRange(const AbstractRange&, const AbstractRange&, TreeType);

    void systemBeep();

    String dumpStyleResolvers();

    enum class AutoplayPolicy : uint8_t {
        Default,
        Allow,
        AllowWithoutSound,
        Deny,
    };
    ExceptionOr<void> setDocumentAutoplayPolicy(Document&, AutoplayPolicy);

    void retainTextIteratorForDocumentContent();

#if ENABLE(SERVICE_WORKER)
    RefPtr<PushSubscription> createPushSubscription(const String& endpoint, std::optional<EpochTimeStamp> expirationTime, const ArrayBuffer& serverVAPIDPublicKey, const ArrayBuffer& clientECDHPublicKey, const ArrayBuffer& auth);
#endif

    void overrideModalContainerSearchTermForTesting(AtomString&& term);

#if ENABLE(ARKIT_INLINE_PREVIEW_MAC)
    using ModelInlinePreviewUUIDsPromise = DOMPromiseDeferred<IDLSequence<IDLDOMString>>;
    void modelInlinePreviewUUIDs(ModelInlinePreviewUUIDsPromise&&) const;
    String modelInlinePreviewUUIDForModelElement(const HTMLModelElement&) const;
#endif

    void avoidIOSurfaceSizeCheckInWebProcess(HTMLCanvasElement&);
    bool hasSleepDisabler() const;

    void acceptTypedArrays(Int32Array&);

private:
    explicit Internals(Document&);
    Document* contextDocument() const;
    Frame* frame() const;

    void updatePageActivityState(OptionSet<ActivityState::Flag> statesToChange, bool newValue);

    ExceptionOr<RenderedDocumentMarker*> markerAt(Node&, const String& markerType, unsigned index);
    ExceptionOr<ScrollableArea*> scrollableAreaForNode(Node*) const;

#if ENABLE(IMAGE_ANALYSIS_ENHANCEMENTS)
    static RetainPtr<VKCImageAnalysis> fakeImageAnalysisResultForTesting(const Vector<ImageOverlayLine>&);
#endif

#if ENABLE(DATA_DETECTION)
    static DDScannerResult *fakeDataDetectorResultForTesting();
#endif

    static RefPtr<SharedBuffer> pngDataForTesting();

#if ENABLE(MEDIA_STREAM)
    // RealtimeMediaSource::Observer API
    void videoFrameAvailable(VideoFrame&, VideoFrameTimeMetadata) final;
    // RealtimeMediaSource::AudioSampleObserver API
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final { m_trackAudioSampleCount++; }

    OrientationNotifier m_orientationNotifier;
    unsigned long m_trackVideoSampleCount { 0 };
    unsigned long m_trackAudioSampleCount { 0 };
    RefPtr<RealtimeMediaSource> m_trackSource;
    std::unique_ptr<TrackFramePromise> m_nextTrackFramePromise;
    int m_trackVideoRotation { 0 };
#endif
#if ENABLE(MEDIA_SESSION)
    std::unique_ptr<ArtworkImageLoader> m_artworkLoader;
    std::unique_ptr<ArtworkImagePromise> m_artworkImagePromise;
#endif
    std::unique_ptr<InspectorStubFrontend> m_inspectorFrontend;
    RefPtr<CacheStorageConnection> m_cacheStorageConnection;

    HashMap<unsigned, std::unique_ptr<WebCore::SleepDisabler>> m_sleepDisablers;

    std::unique_ptr<TextIterator> m_textIterator;

#if ENABLE(WEBXR)
    RefPtr<WebXRTest> m_xrTest;
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    RefPtr<PlatformSpeechSynthesizerMock> m_platformSpeechSynthesizer;
#endif
#if ENABLE(MEDIA_SESSION_COORDINATOR)
    RefPtr<MockMediaSessionCoordinator> m_mockMediaSessionCoordinator;
#endif
#if ENABLE(VIDEO)
    std::unique_ptr<CaptionUserPreferencesTestingModeToken> m_testingModeToken;
#endif
};

} // namespace WebCore
