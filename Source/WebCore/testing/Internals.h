/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef Internals_h
#define Internals_h

#include "CSSComputedStyleDeclaration.h"
#include "ContextDestructionObserver.h"
#include "ExceptionCodePlaceholder.h"
#include "NodeList.h"
#include <bindings/ScriptValue.h>
#include <runtime/ArrayBuffer.h>
#include <runtime/Float32Array.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ClientRect;
class ClientRectList;
class DOMStringList;
class DOMWindow;
class Document;
class DocumentMarker;
class Element;
class Frame;
class InspectorFrontendChannelDummy;
class InternalSettings;
class MallocStatistics;
class MemoryInfo;
class Node;
class Page;
class Range;
class ScriptExecutionContext;
class ScriptProfile;
class SerializedScriptValue;
class TimeRanges;
class TypeConversions;

typedef int ExceptionCode;
typedef Vector<RefPtr<ScriptProfile>> ProfilesArray;

class Internals : public RefCounted<Internals>
                , public ContextDestructionObserver {
public:
    static PassRefPtr<Internals> create(Document*);
    virtual ~Internals();

    static void resetToConsistentState(Page*);

    String elementRenderTreeAsText(Element*, ExceptionCode&);

    String address(Node*);

    bool isPreloaded(const String& url);
    bool isLoadingFromMemoryCache(const String& url);

    PassRefPtr<CSSComputedStyleDeclaration> computedStyleIncludingVisitedInfo(Node*, ExceptionCode&) const;

    Node* ensureShadowRoot(Element* host, ExceptionCode&);
    Node* createShadowRoot(Element* host, ExceptionCode&);
    Node* shadowRoot(Element* host, ExceptionCode&);
    String shadowRootType(const Node*, ExceptionCode&) const;
    Element* includerFor(Node*, ExceptionCode&);
    String shadowPseudoId(Element*, ExceptionCode&);
    void setShadowPseudoId(Element*, const String&, ExceptionCode&);

    // Spatial Navigation testing.
    unsigned lastSpatialNavigationCandidateCount(ExceptionCode&) const;

    // CSS Animation testing.
    unsigned numberOfActiveAnimations() const;
    bool animationsAreSuspended(ExceptionCode&) const;
    void suspendAnimations(ExceptionCode&) const;
    void resumeAnimations(ExceptionCode&) const;
    bool pauseAnimationAtTimeOnElement(const String& animationName, double pauseTime, Element*, ExceptionCode&);
    bool pauseAnimationAtTimeOnPseudoElement(const String& animationName, double pauseTime, Element*, const String& pseudoId, ExceptionCode&);

    // CSS Transition testing.
    bool pauseTransitionAtTimeOnElement(const String& propertyName, double pauseTime, Element*, ExceptionCode&);
    bool pauseTransitionAtTimeOnPseudoElement(const String& property, double pauseTime, Element*, const String& pseudoId, ExceptionCode&);

    Node* treeScopeRootNode(Node*, ExceptionCode&);
    Node* parentTreeScope(Node*, ExceptionCode&);
    bool hasSelectorForIdInShadow(Element* host, const String& idValue, ExceptionCode&);
    bool hasSelectorForClassInShadow(Element* host, const String& className, ExceptionCode&);
    bool hasSelectorForAttributeInShadow(Element* host, const String& attributeName, ExceptionCode&);
    bool hasSelectorForPseudoClassInShadow(Element* host, const String& pseudoClass, ExceptionCode&);

    bool attached(Node*, ExceptionCode&);

    String visiblePlaceholder(Element*);
#if ENABLE(INPUT_TYPE_COLOR)
    void selectColorInColorChooser(Element*, const String& colorValue);
#endif
    Vector<String> formControlStateOfPreviousHistoryItem(ExceptionCode&);
    void setFormControlStateOfPreviousHistoryItem(const Vector<String>&, ExceptionCode&);

    PassRefPtr<ClientRect> absoluteCaretBounds(ExceptionCode&);

    PassRefPtr<ClientRect> boundingBox(Element*, ExceptionCode&);

    PassRefPtr<ClientRectList> inspectorHighlightRects(ExceptionCode&);
    String inspectorHighlightObject(ExceptionCode&);

    unsigned markerCountForNode(Node*, const String&, ExceptionCode&);
    PassRefPtr<Range> markerRangeForNode(Node*, const String& markerType, unsigned index, ExceptionCode&);
    String markerDescriptionForNode(Node*, const String& markerType, unsigned index, ExceptionCode&);
    void addTextMatchMarker(const Range*, bool isActive);

    void setScrollViewPosition(long x, long y, ExceptionCode&);
    void setPagination(const String& mode, int gap, ExceptionCode& ec) { setPagination(mode, gap, 0, ec); }
    void setPagination(const String& mode, int gap, int pageLength, ExceptionCode&);
    String configurationForViewport(float devicePixelRatio, int deviceWidth, int deviceHeight, int availableWidth, int availableHeight, ExceptionCode&);

    bool wasLastChangeUserEdit(Element* textField, ExceptionCode&);
    bool elementShouldAutoComplete(Element* inputElement, ExceptionCode&);
    String suggestedValue(Element* inputElement, ExceptionCode&);
    void setSuggestedValue(Element* inputElement, const String&, ExceptionCode&);
    void setEditingValue(Element* inputElement, const String&, ExceptionCode&);
    void setAutofilled(Element*, bool enabled, ExceptionCode&);
    void scrollElementToRect(Element*, long x, long y, long w, long h, ExceptionCode&);

    void paintControlTints(ExceptionCode&);

    PassRefPtr<Range> rangeFromLocationAndLength(Element* scope, int rangeLocation, int rangeLength, ExceptionCode&);
    unsigned locationFromRange(Element* scope, const Range*, ExceptionCode&);
    unsigned lengthFromRange(Element* scope, const Range*, ExceptionCode&);
    String rangeAsText(const Range*, ExceptionCode&);

    void setDelegatesScrolling(bool enabled, ExceptionCode&);

    int lastSpellCheckRequestSequence(ExceptionCode&);
    int lastSpellCheckProcessedSequence(ExceptionCode&);

    Vector<String> userPreferredLanguages() const;
    void setUserPreferredLanguages(const Vector<String>&);

    unsigned wheelEventHandlerCount(ExceptionCode&);
    unsigned touchEventHandlerCount(ExceptionCode&);

    PassRefPtr<NodeList> nodesFromRect(Document*, int x, int y, unsigned topPadding, unsigned rightPadding,
        unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, bool allowChildFrameContent, ExceptionCode&) const;

    String parserMetaData(Deprecated::ScriptValue = Deprecated::ScriptValue());

    Node* findEditingDeleteButton();
    void updateEditorUINowIfScheduled();

    bool hasSpellingMarker(int from, int length, ExceptionCode&);
    bool hasGrammarMarker(int from, int length, ExceptionCode&);
    bool hasAutocorrectedMarker(int from, int length, ExceptionCode&);
    void setContinuousSpellCheckingEnabled(bool enabled, ExceptionCode&);
    void setAutomaticQuoteSubstitutionEnabled(bool enabled, ExceptionCode&);
    void setAutomaticLinkDetectionEnabled(bool enabled, ExceptionCode&);
    void setAutomaticDashSubstitutionEnabled(bool enabled, ExceptionCode&);
    void setAutomaticTextReplacementEnabled(bool enabled, ExceptionCode&);
    void setAutomaticSpellingCorrectionEnabled(bool enabled, ExceptionCode&);

    bool isOverwriteModeEnabled(ExceptionCode&);
    void toggleOverwriteModeEnabled(ExceptionCode&);

    unsigned countMatchesForText(const String&, unsigned findOptions, const String& markMatches, ExceptionCode&);

    unsigned numberOfScrollableAreas(ExceptionCode&);

    bool isPageBoxVisible(int pageNumber, ExceptionCode&);

    static const char* internalsId;

    InternalSettings* settings() const;
    unsigned workerThreadCount() const;

    void setBatteryStatus(const String& eventType, bool charging, double chargingTime, double dischargingTime, double level, ExceptionCode&);

    void setDeviceProximity(const String& eventType, double value, double min, double max, ExceptionCode&);

    enum {
        // Values need to be kept in sync with Internals.idl.
        LAYER_TREE_INCLUDES_VISIBLE_RECTS = 1,
        LAYER_TREE_INCLUDES_TILE_CACHES = 2,
        LAYER_TREE_INCLUDES_REPAINT_RECTS = 4,
        LAYER_TREE_INCLUDES_PAINTING_PHASES = 8,
        LAYER_TREE_INCLUDES_CONTENT_LAYERS = 16
    };
    String layerTreeAsText(Document*, unsigned flags, ExceptionCode&) const;
    String layerTreeAsText(Document*, ExceptionCode&) const;
    String repaintRectsAsText(ExceptionCode&) const;
    String scrollingStateTreeAsText(ExceptionCode&) const;
    String mainThreadScrollingReasons(ExceptionCode&) const;
    PassRefPtr<ClientRectList> nonFastScrollableRects(ExceptionCode&) const;

    void garbageCollectDocumentResources(ExceptionCode&) const;

    void allowRoundingHacks() const;

    void insertAuthorCSS(const String&, ExceptionCode&) const;
    void insertUserCSS(const String&, ExceptionCode&) const;

    const ProfilesArray& consoleProfiles() const;

    unsigned numberOfLiveNodes() const;
    unsigned numberOfLiveDocuments() const;

#if ENABLE(INSPECTOR)
    Vector<String> consoleMessageArgumentCounts() const;
    PassRefPtr<DOMWindow> openDummyInspectorFrontend(const String& url);
    void closeDummyInspectorFrontend();
    void setJavaScriptProfilingEnabled(bool enabled, ExceptionCode&);
    void setInspectorIsUnderTest(bool isUnderTest, ExceptionCode&);
#endif

    String counterValue(Element*);

    int pageNumber(Element*, float pageWidth = 800, float pageHeight = 600);
    Vector<String> shortcutIconURLs() const;
    Vector<String> allIconURLs() const;

    int numberOfPages(float pageWidthInPixels = 800, float pageHeightInPixels = 600);
    String pageProperty(String, int, ExceptionCode& = ASSERT_NO_EXCEPTION) const;
    String pageSizeAndMarginsInPixels(int, int, int, int, int, int, int, ExceptionCode& = ASSERT_NO_EXCEPTION) const;

    void setPageScaleFactor(float scaleFactor, int x, int y, ExceptionCode&);

    void setHeaderHeight(float);
    void setFooterHeight(float);

    void setTopContentInset(float);

#if ENABLE(FULLSCREEN_API)
    void webkitWillEnterFullScreenForElement(Element*);
    void webkitDidEnterFullScreenForElement(Element*);
    void webkitWillExitFullScreenForElement(Element*);
    void webkitDidExitFullScreenForElement(Element*);
#endif

    void setApplicationCacheOriginQuota(unsigned long long);

    void registerURLSchemeAsBypassingContentSecurityPolicy(const String& scheme);
    void removeURLSchemeRegisteredAsBypassingContentSecurityPolicy(const String& scheme);

    PassRefPtr<MallocStatistics> mallocStatistics() const;
    PassRefPtr<TypeConversions> typeConversions() const;
    PassRefPtr<MemoryInfo> memoryInfo() const;

    Vector<String> getReferencedFilePaths() const;

    void startTrackingRepaints(ExceptionCode&);
    void stopTrackingRepaints(ExceptionCode&);

    PassRefPtr<ArrayBuffer> serializeObject(PassRefPtr<SerializedScriptValue>) const;
    PassRefPtr<SerializedScriptValue> deserializeBuffer(PassRefPtr<ArrayBuffer>) const;

    void setUsesOverlayScrollbars(bool enabled);

    String getCurrentCursorInfo(ExceptionCode&);

    String markerTextForListItem(Element*, ExceptionCode&);

    void forceReload(bool endToEnd);

#if ENABLE(ENCRYPTED_MEDIA_V2)
    void initializeMockCDM();
#endif

#if ENABLE(SPEECH_SYNTHESIS)
    void enableMockSpeechSynthesizer();
#endif

#if ENABLE(MEDIA_STREAM)
    void enableMockRTCPeerConnectionHandler();
#endif

    String getImageSourceURL(Element*, ExceptionCode&);

#if ENABLE(VIDEO)
    void simulateAudioInterruption(Node*);
#endif

    bool isSelectPopupVisible(Node*);

    String captionsStyleSheetOverride(ExceptionCode&);
    void setCaptionsStyleSheetOverride(const String&, ExceptionCode&);
    void setPrimaryAudioTrackLanguageOverride(const String&, ExceptionCode&);
    void setCaptionDisplayMode(const String&, ExceptionCode&);

#if ENABLE(VIDEO)
    PassRefPtr<TimeRanges> createTimeRanges(Float32Array* startTimes, Float32Array* endTimes);
    double closestTimeToTimeRanges(double time, TimeRanges*);
#endif

    PassRefPtr<ClientRect> selectionBounds(ExceptionCode&);

#if ENABLE(VIBRATION)
    bool isVibrating();
#endif

    bool isPluginUnavailabilityIndicatorObscured(Element*, ExceptionCode&);
    bool isPluginSnapshotted(Element*, ExceptionCode&);

#if ENABLE(MEDIA_SOURCE)
    void initializeMockMediaSource();
#endif

#if ENABLE(VIDEO)
    void beginMediaSessionInterruption();
    void endMediaSessionInterruption(const String&);
    void applicationWillEnterForeground() const;
    void applicationWillEnterBackground() const;
    void setMediaSessionRestrictions(const String& mediaType, const String& restrictions, ExceptionCode&);
    void postRemoteControlCommand(const String&, ExceptionCode&);
#endif

    void simulateSystemSleep() const;
    void simulateSystemWake() const;

private:
    explicit Internals(Document*);
    Document* contextDocument() const;
    Frame* frame() const;
    Vector<String> iconURLs(Document*, int iconTypesMask) const;

    DocumentMarker* markerAt(Node*, const String& markerType, unsigned index, ExceptionCode&);
#if ENABLE(INSPECTOR)
    RefPtr<DOMWindow> m_frontendWindow;
    OwnPtr<InspectorFrontendChannelDummy> m_frontendChannel;
#endif
};

} // namespace WebCore

#endif
