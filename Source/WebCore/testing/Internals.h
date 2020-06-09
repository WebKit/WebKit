/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013-2020 Apple Inc. All rights reserved.
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

#include "CSSComputedStyleDeclaration.h"
#include "ContextDestructionObserver.h"
#include "Cookie.h"
#include "ExceptionOr.h"
#include "HEVCUtilities.h"
#include "IDLTypes.h"
#include "OrientationNotifier.h"
#include "PageConsoleClient.h"
#include "RealtimeMediaSource.h"
#include "SleepDisabler.h"
#include "TextIndicator.h"
#include <JavaScriptCore/Float32Array.h>
#include <wtf/Optional.h>

#if ENABLE(MEDIA_SESSION)
#include "MediaSessionInterruptionProvider.h"
#endif

#if ENABLE(VIDEO)
#include "MediaElementSession.h"
#endif

namespace WebCore {

class AnimationTimeline;
class AudioContext;
class AudioTrack;
class CacheStorageConnection;
class DOMRect;
class DOMRectList;
class DOMRectReadOnly;
class DOMURL;
class DOMWindow;
class Document;
class Element;
class EventListener;
class ExtendableEvent;
class FetchResponse;
class File;
class Frame;
class GCObservation;
class HTMLAnchorElement;
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
class MediaSession;
class MediaStream;
class MediaStreamTrack;
class MemoryInfo;
class MockCDMFactory;
class MockContentFilterSettings;
class MockPageOverlay;
class MockPaymentCoordinator;
class NodeList;
class Page;
class RTCPeerConnection;
class Range;
class RenderedDocumentMarker;
class SVGSVGElement;
class ScrollableArea;
class SerializedScriptValue;
class SourceBuffer;
class StringCallback;
class StyleSheet;
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
#endif

#if ENABLE(SERVICE_WORKER)
class ServiceWorker;
#endif

#if ENABLE(WEBXR)
class WebXRTest;
#endif

template<typename IDLType> class DOMPromiseDeferred;

struct MockWebAuthenticationConfiguration;

class Internals final : public RefCounted<Internals>, private ContextDestructionObserver
#if ENABLE(MEDIA_STREAM)
    , private RealtimeMediaSource::Observer
    , private RealtimeMediaSource::AudioSampleObserver
    , private RealtimeMediaSource::VideoSampleObserver
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
    unsigned imagePendingDecodePromisesCountForTesting(HTMLImageElement&);
    void setClearDecoderAfterAsyncFrameRequestForTesting(HTMLImageElement&, bool enabled);
    unsigned imageDecodeCount(HTMLImageElement&);
    unsigned pdfDocumentCachingCount(HTMLImageElement&);
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
    String shadowPseudoId(Element&);
    void setShadowPseudoId(Element&, const String&);

    // CSS Deferred Parsing Testing
    unsigned deferredStyleRulesCount(StyleSheet&);
    unsigned deferredGroupRulesCount(StyleSheet&);
    unsigned deferredKeyframesRulesCount(StyleSheet&);

    // DOMTimers throttling testing.
    ExceptionOr<bool> isTimerThrottled(int timeoutId);
    String requestAnimationFrameThrottlingReasons() const;
    double requestAnimationFrameInterval() const;
    bool scriptedAnimationsAreSuspended() const;
    bool areTimersThrottled() const;

    enum EventThrottlingBehavior { Responsive, Unresponsive };
    void setEventThrottlingBehaviorOverride(Optional<EventThrottlingBehavior>);
    Optional<EventThrottlingBehavior> eventThrottlingBehaviorOverride() const;

    // Spatial Navigation testing.
    ExceptionOr<unsigned> lastSpatialNavigationCandidateCount() const;

    // CSS Animation testing.
    bool animationWithIdExists(const String&) const;
    unsigned numberOfActiveAnimations() const;
    ExceptionOr<bool> animationsAreSuspended() const;
    ExceptionOr<void> suspendAnimations() const;
    ExceptionOr<void> resumeAnimations() const;
    ExceptionOr<bool> pauseAnimationAtTimeOnElement(const String& animationName, double pauseTime, Element&);
    ExceptionOr<bool> pauseAnimationAtTimeOnPseudoElement(const String& animationName, double pauseTime, Element&, const String& pseudoId);
    double animationsInterval() const;

    // CSS Transition testing.
    ExceptionOr<bool> pauseTransitionAtTimeOnElement(const String& propertyName, double pauseTime, Element&);
    ExceptionOr<bool> pauseTransitionAtTimeOnPseudoElement(const String& property, double pauseTime, Element&, const String& pseudoId);

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
    ExceptionOr<Vector<String>> formControlStateOfPreviousHistoryItem();
    ExceptionOr<void> setFormControlStateOfPreviousHistoryItem(const Vector<String>&);

    ExceptionOr<Ref<DOMRect>> absoluteCaretBounds();
    ExceptionOr<bool> isCaretBlinkingSuspended();

    Ref<DOMRect> boundingBox(Element&);

    ExceptionOr<Ref<DOMRectList>> inspectorHighlightRects();

    ExceptionOr<unsigned> markerCountForNode(Node&, const String&);
    ExceptionOr<RefPtr<Range>> markerRangeForNode(Node&, const String& markerType, unsigned index);
    ExceptionOr<String> markerDescriptionForNode(Node&, const String& markerType, unsigned index);
    ExceptionOr<String> dumpMarkerRects(const String& markerType);
    ExceptionOr<void> setMarkedTextMatchesAreHighlighted(bool);

    void invalidateFontCache();
    void setFontSmoothingEnabled(bool);

    ExceptionOr<void> setLowPowerModeEnabled(bool);
    ExceptionOr<void> setOutsideViewportThrottlingEnabled(bool);

    ExceptionOr<void> setScrollViewPosition(int x, int y);
    ExceptionOr<void> unconstrainedScrollTo(Element&, double x, double y);

    ExceptionOr<Ref<DOMRect>> layoutViewportRect();
    ExceptionOr<Ref<DOMRect>> visualViewportRect();

    ExceptionOr<void> setViewIsTransparent(bool);

    ExceptionOr<String> viewBaseBackgroundColor();
    ExceptionOr<void> setViewBaseBackgroundColor(const String& colorValue);

    ExceptionOr<void> setPagination(const String& mode, int gap, int pageLength);
    ExceptionOr<void> setPaginationLineGridEnabled(bool);
    ExceptionOr<String> configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight);

    ExceptionOr<bool> wasLastChangeUserEdit(Element& textField);
    bool elementShouldAutoComplete(HTMLInputElement&);
    void setAutofilled(HTMLInputElement&, bool enabled);
    void setAutoFilledAndViewable(HTMLInputElement&, bool enabled);
    enum class AutoFillButtonType { None, Contacts, Credentials, StrongPassword, CreditCard };
    void setShowAutoFillButton(HTMLInputElement&, AutoFillButtonType);
    AutoFillButtonType autoFillButtonType(const HTMLInputElement&);
    AutoFillButtonType lastAutoFillButtonType(const HTMLInputElement&);
    ExceptionOr<void> scrollElementToRect(Element&, int x, int y, int w, int h);

    ExceptionOr<String> autofillFieldName(Element&);

    ExceptionOr<void> invalidateControlTints();

    RefPtr<Range> rangeFromLocationAndLength(Element& scope, unsigned rangeLocation, unsigned rangeLength);
    unsigned locationFromRange(Element& scope, const Range&);
    unsigned lengthFromRange(Element& scope, const Range&);
    String rangeAsText(const Range&);
    String rangeAsTextUsingBackwardsTextIterator(const Range&);
    Ref<Range> subrange(Range&, unsigned rangeLocation, unsigned rangeLength);
    ExceptionOr<RefPtr<Range>> rangeForDictionaryLookupAtLocation(int x, int y);
    RefPtr<Range> rangeOfStringNearLocation(const Range&, const String&, unsigned);

    ExceptionOr<void> setDelegatesScrolling(bool enabled);

    ExceptionOr<uint64_t> lastSpellCheckRequestSequence();
    ExceptionOr<uint64_t> lastSpellCheckProcessedSequence();

    Vector<String> userPreferredLanguages() const;
    void setUserPreferredLanguages(const Vector<String>&);

    Vector<String> userPreferredAudioCharacteristics() const;
    void setUserPreferredAudioCharacteristic(const String&);

    void setMaxCanvasPixelMemory(unsigned);

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

    void handleAcceptedCandidate(const String& candidate, unsigned location, unsigned length);
    void changeSelectionListType();

    bool isOverwriteModeEnabled();
    void toggleOverwriteModeEnabled();

    bool testProcessIncomingSyncMessagesWhenWaitingForSyncReply();

    ExceptionOr<RefPtr<Range>> rangeOfString(const String&, RefPtr<Range>&&, const Vector<String>& findOptions);
    ExceptionOr<unsigned> countMatchesForText(const String&, const Vector<String>& findOptions, const String& markMatches);
    ExceptionOr<unsigned> countFindMatches(const String&, const Vector<String>& findOptions);

    unsigned numberOfScrollableAreas();

    ExceptionOr<bool> isPageBoxVisible(int pageNumber);

    static const char* internalsId;

    InternalSettings* settings() const;
    unsigned workerThreadCount() const;
    ExceptionOr<bool> areSVGAnimationsPaused() const;
    ExceptionOr<double> svgAnimationsInterval(SVGSVGElement&) const;

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
    };
    ExceptionOr<String> layerTreeAsText(Document&, unsigned short flags) const;
    ExceptionOr<uint64_t> layerIDForElement(Element&);
    ExceptionOr<String> repaintRectsAsText() const;

    ExceptionOr<String> scrollbarOverlayStyle(Node*) const;
    ExceptionOr<bool> scrollbarUsingDarkAppearance(Node*) const;

    ExceptionOr<String> horizontalScrollbarState(Node*) const;
    ExceptionOr<String> verticalScrollbarState(Node*) const;

    ExceptionOr<String> scrollingStateTreeAsText() const;
    ExceptionOr<String> scrollingTreeAsText() const;
    ExceptionOr<String> mainThreadScrollingReasons() const;
    ExceptionOr<Ref<DOMRectList>> nonFastScrollableRects() const;

    ExceptionOr<void> setElementUsesDisplayListDrawing(Element&, bool usesDisplayListDrawing);
    ExceptionOr<void> setElementTracksDisplayListReplay(Element&, bool isTrackingReplay);

    enum {
        // Values need to be kept in sync with Internals.idl.
        DISPLAY_LIST_INCLUDES_PLATFORM_OPERATIONS = 1,
    };
    ExceptionOr<String> displayListForElement(Element&, unsigned short flags);
    ExceptionOr<String> replayDisplayListForElement(Element&, unsigned short flags);

    ExceptionOr<void> garbageCollectDocumentResources() const;

    void beginSimulatedMemoryPressure();
    void endSimulatedMemoryPressure();
    bool isUnderMemoryPressure();

    ExceptionOr<void> insertAuthorCSS(const String&) const;
    ExceptionOr<void> insertUserCSS(const String&) const;

#if ENABLE(INDEXED_DATABASE)
    unsigned numberOfIDBTransactions() const;
#endif

    unsigned numberOfLiveNodes() const;
    unsigned numberOfLiveDocuments() const;
    unsigned referencingNodeCount(const Document&) const;

#if ENABLE(INTERSECTION_OBSERVER)
    unsigned numberOfIntersectionObservers(const Document&) const;
#endif

    uint64_t documentIdentifier(const Document&) const;
    bool isDocumentAlive(uint64_t documentIdentifier) const;

    uint64_t storageAreaMapCount() const;

    uint64_t elementIdentifier(Element&) const;
    uint64_t frameIdentifier(const Document&) const;
    uint64_t pageIdentifier(const Document&) const;

    bool isAnyWorkletGlobalScopeAlive() const;

    String serviceWorkerClientIdentifier(const Document&) const;

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

    enum CompositingPolicy { Normal, Conservative };
    ExceptionOr<void> setCompositingPolicyOverride(Optional<CompositingPolicy>);
    ExceptionOr<Optional<CompositingPolicy>> compositingPolicyOverride() const;

    void updateLayoutAndStyleForAllFrames();
    ExceptionOr<void> updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks(Node*);
    unsigned layoutCount() const;

    Ref<ArrayBuffer> serializeObject(const RefPtr<SerializedScriptValue>&) const;
    Ref<SerializedScriptValue> deserializeBuffer(ArrayBuffer&) const;

    bool isFromCurrentWorld(JSC::JSValue) const;
    JSC::JSValue evaluateInWorldIgnoringException(const String& name, const String& source);

    void setUsesOverlayScrollbars(bool);
    void setUsesMockScrollAnimator(bool);

    ExceptionOr<String> getCurrentCursorInfo();

    String markerTextForListItem(Element&);

    String toolTipFromElement(Element&) const;

    void forceReload(bool endToEnd);
    void reloadExpiredOnly();

    void enableFixedWidthAutoSizeMode(bool enabled, int width, int height);

#if ENABLE(LEGACY_ENCRYPTED_MEDIA)
    void initializeMockCDM();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    Ref<MockCDMFactory> registerMockCDM();
#endif

    void enableMockMediaCapabilities();

#if ENABLE(SPEECH_SYNTHESIS)
    void enableMockSpeechSynthesizer();
#endif

#if ENABLE(MEDIA_STREAM)
    void setShouldInterruptAudioOnPageVisibilityChange(bool);
    void setMediaCaptureRequiresSecureConnection(bool);
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
    void setEnableWebRTCEncryption(bool);
    void setUseDTLS10(bool);
    void setUseGPUProcessForWebRTC(bool);
#endif

    String getImageSourceURL(Element&);

#if ENABLE(VIDEO)
    Vector<String> mediaResponseSources(HTMLMediaElement&);
    Vector<String> mediaResponseContentRanges(HTMLMediaElement&);
    void simulateAudioInterruption(HTMLMediaElement&);
    ExceptionOr<bool> mediaElementHasCharacteristic(HTMLMediaElement&, const String&);
    void beginSimulatedHDCPError(HTMLMediaElement&);
    void endSimulatedHDCPError(HTMLMediaElement&);

    bool elementShouldBufferData(HTMLMediaElement&);
    String elementBufferingPolicy(HTMLMediaElement&);
    double privatePlayerVolume(const HTMLMediaElement&);
#endif

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
    void setSelectionWithoutValidation(Ref<Node> baseNode, unsigned baseOffset, RefPtr<Node> extentNode, unsigned extentOffset);

    ExceptionOr<bool> isPluginUnavailabilityIndicatorObscured(Element&);
    ExceptionOr<String> unavailablePluginReplacementText(Element&);
    bool isPluginSnapshotted(Element&);
    bool pluginIsBelowSizeThreshold(Element&);

#if ENABLE(MEDIA_SOURCE)
    WEBCORE_TESTSUPPORT_EXPORT void initializeMockMediaSource();
    Vector<String> bufferedSamplesForTrackID(SourceBuffer&, const AtomString&);
    Vector<String> enqueuedSamplesForTrackID(SourceBuffer&, const AtomString&);
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
    bool elementIsBlockingDisplaySleep(HTMLMediaElement&) const;
#endif

#if ENABLE(MEDIA_SESSION)
    void sendMediaSessionStartOfInterruptionNotification(MediaSessionInterruptingCategory);
    void sendMediaSessionEndOfInterruptionNotification(MediaSessionInterruptingCategory);
    String mediaSessionCurrentState(MediaSession&) const;
    double mediaElementPlayerVolume(HTMLMediaElement&) const;
    enum class MediaControlEvent { PlayPause, NextTrack, PreviousTrack };
    void sendMediaControlEvent(MediaControlEvent);
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
    void queueMicroTask(int);
    bool testPreloaderSettingViewport();

#if ENABLE(CONTENT_FILTERING)
    MockContentFilterSettings& mockContentFilterSettings();
#endif

#if ENABLE(CSS_SCROLL_SNAP)
    ExceptionOr<String> scrollSnapOffsets(Element&);
    void setPlatformMomentumScrollingPredictionEnabled(bool);
#endif

    ExceptionOr<String> pathStringWithShrinkWrappedRects(const Vector<double>& rectComponents, double radius);

    String getCurrentMediaControlsStatusForElement(HTMLMediaElement&);

    String userVisibleString(const DOMURL&);
    void setShowAllPlugins(bool);

    String resourceLoadStatisticsForURL(const DOMURL&);
    void setResourceLoadStatisticsEnabled(bool);

#if ENABLE(STREAMS_API)
    bool isReadableStreamDisturbed(JSC::JSGlobalObject&, JSC::JSValue);
    JSC::JSValue cloneArrayBuffer(JSC::JSGlobalObject&, JSC::JSValue, JSC::JSValue, JSC::JSValue);
#endif

    String composedTreeAsText(Node&);

    bool isProcessingUserGesture();
    double lastHandledUserGestureTimestamp();

    void withUserGesture(RefPtr<VoidCallback>&&);

    bool userIsInteracting();

    RefPtr<GCObservation> observeGC(JSC::JSValue);

    enum class UserInterfaceLayoutDirection : uint8_t { LTR, RTL };
    void setUserInterfaceLayoutDirection(UserInterfaceLayoutDirection);

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
#if ENABLE(APPLE_PAY)
    void setApplePayIsActive(Document&);
#endif

#if ENABLE(WEBGL)
    void simulateWebGLContextChanged(WebGLRenderingContext&);
    void failNextGPUStatusCheck(WebGLRenderingContext&);
    bool hasLowAndHighPowerGPUs();
#endif

    void setPageVisibility(bool isVisible);
    void setPageIsFocusedAndActive(bool);


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
    void delayMediaStreamTrackSamples(MediaStreamTrack&, float);
    void setMediaStreamTrackMuted(MediaStreamTrack&, bool);
    void removeMediaStreamTrack(MediaStream&, MediaStreamTrack&);
    void simulateMediaStreamTrackCaptureSourceFailure(MediaStreamTrack&);
    void setMediaStreamTrackIdentifier(MediaStreamTrack&, String&& id);
    void setMediaStreamSourceInterrupted(MediaStreamTrack&, bool);
    bool isMockRealtimeMediaSourceCenterEnabled();
    bool shouldAudioTrackPlay(const AudioTrack&);
#endif

    bool supportsAudioSession() const;
    String audioSessionCategory() const;
    double preferredAudioBufferSize() const;
    bool audioSessionActive() const;

    void storeRegistrationsOnDisk(DOMPromiseDeferred<void>&&);

    void clearCacheStorageMemoryRepresentation(DOMPromiseDeferred<void>&&);
    void cacheStorageEngineRepresentation(DOMPromiseDeferred<IDLDOMString>&&);
    void setResponseSizeWithPadding(FetchResponse&, uint64_t size);
    uint64_t responseSizeWithPadding(FetchResponse&) const;

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

    bool isSystemPreviewLink(Element&) const;
    bool isSystemPreviewImage(Element&) const;

    void postTask(RefPtr<VoidCallback>&&);
    ExceptionOr<void> queueTask(ScriptExecutionContext&, const String& source, RefPtr<VoidCallback>&&);
    ExceptionOr<void> queueTaskToQueueMicrotask(Document&, const String& source, RefPtr<VoidCallback>&&);
    ExceptionOr<bool> hasSameEventLoopAs(WindowProxy&);

    void markContextAsInsecure();

    bool usingAppleInternalSDK() const;

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
    RefPtr<HTMLMediaElement> bestMediaElementForShowingPlaybackControlsManager(PlaybackControlsPurpose);

    using MediaSessionState = PlatformMediaSession::State;
    MediaSessionState mediaSessionState(HTMLMediaElement&);
#endif

    void setCaptureExtraNetworkLoadMetricsEnabled(bool);
    String ongoingLoadsDescriptions() const;

    void reloadWithoutContentExtensions();

    void setUseSystemAppearance(bool);

    size_t pluginCount();

    void notifyResourceLoadObserver();

    unsigned primaryScreenDisplayID();

    bool capsLockIsOn();
        
    bool supportsVCPEncoder();
        
    using HEVCParameterSet = WebCore::HEVCParameterSet;
    Optional<HEVCParameterSet> parseHEVCCodecParameters(const String& codecString);

    using DoViParameterSet = WebCore::DoViParameterSet;
    Optional<DoViParameterSet> parseDoViCodecParameters(const String& codecString);

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
            , expires(cookie.expires.valueOr(0))
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

    void setXHRMaximumIntervalForUserGestureForwarding(XMLHttpRequest&, double);
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

        OptionSet<WebCore::TextIndicatorOption> coreOptions()
        {
            OptionSet<WebCore::TextIndicatorOption> options;
            if (useBoundingRectAndPaintAllContentForComplexRanges)
                options.add(TextIndicatorOption::UseBoundingRectAndPaintAllContentForComplexRanges);
            if (computeEstimatedBackgroundColor)
                options.add(TextIndicatorOption::ComputeEstimatedBackgroundColor);
            if (respectTextColor)
                options.add(TextIndicatorOption::RespectTextColor);
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

    String highlightPseudoElementColor(const String& highlightName, Element&);

    String windowLocationHost(DOMWindow&);

    String systemColorForCSSValue(const String& cssValue, bool useDarkModeAppearance, bool useElevatedUserInterfaceLevel);

    bool systemHasBattery() const;

    int readPreferenceInteger(const String& domain, const String& key);
    String encodedPreferenceValue(const String& domain, const String& key);

    String getUTIFromTag(const String& tagClass, const String& tag, const String& conformingToUTI);

    bool supportsPictureInPicture();

    String focusRingColor();

    bool isRemoteUIAppForAccessibility();

    unsigned createSleepDisabler(const String& reason, bool display);
    bool destroySleepDisabler(unsigned identifier);

#if ENABLE(WEBXR)
    ExceptionOr<RefPtr<WebXRTest>> xrTest();
#endif

#if ENABLE(ENCRYPTED_MEDIA)
    unsigned mediaKeysInternalInstanceObjectRefCount(const MediaKeys&) const;
    unsigned mediaKeySessionInternalInstanceSessionObjectRefCount(const MediaKeySession&) const;
#endif

private:
    explicit Internals(Document&);
    Document* contextDocument() const;
    Frame* frame() const;

    ExceptionOr<RenderedDocumentMarker*> markerAt(Node&, const String& markerType, unsigned index);
    ExceptionOr<ScrollableArea*> scrollableAreaForNode(Node*) const;

#if ENABLE(MEDIA_STREAM)
    // RealtimeMediaSource::Observer API
    void videoSampleAvailable(MediaSample&) final;
    // RealtimeMediaSource::AudioSampleObserver API
    void audioSamplesAvailable(const MediaTime&, const PlatformAudioData&, const AudioStreamDescription&, size_t) final { m_trackAudioSampleCount++; }

    OrientationNotifier m_orientationNotifier;
    unsigned long m_trackVideoSampleCount { 0 };
    unsigned long m_trackAudioSampleCount { 0 };
    RefPtr<RealtimeMediaSource> m_trackSource;
    std::unique_ptr<TrackFramePromise> m_nextTrackFramePromise;
#endif

    std::unique_ptr<InspectorStubFrontend> m_inspectorFrontend;
    RefPtr<CacheStorageConnection> m_cacheStorageConnection;

    HashMap<unsigned, std::unique_ptr<WebCore::SleepDisabler>> m_sleepDisablers;

#if ENABLE(WEBXR)
    RefPtr<WebXRTest> m_xrTest;
#endif
};

} // namespace WebCore
