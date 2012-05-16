/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "FrameDestructionObserver.h"
#include "NodeList.h"
#include "PlatformString.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class ClientRect;
class ClientRectList;
class Document;
class DocumentMarker;
class Element;
class InternalSettings;
class Node;
class Range;
class ShadowRoot;
class WebKitPoint;

typedef int ExceptionCode;

class Internals : public RefCounted<Internals>,
                  public FrameDestructionObserver {
public:
    static PassRefPtr<Internals> create(Document*);
    virtual ~Internals();

    void reset(Document*);

    String elementRenderTreeAsText(Element*, ExceptionCode&);

    String address(Node*);

    bool isPreloaded(Document*, const String& url);

    size_t numberOfScopedHTMLStyleChildren(const Node*, ExceptionCode&) const;

#if ENABLE(SHADOW_DOM)
    typedef ShadowRoot ShadowRootIfShadowDOMEnabledOrNode;
#else
    typedef Node ShadowRootIfShadowDOMEnabledOrNode;
#endif
    ShadowRootIfShadowDOMEnabledOrNode* ensureShadowRoot(Element* host, ExceptionCode&);
    ShadowRootIfShadowDOMEnabledOrNode* shadowRoot(Element* host, ExceptionCode&);
    ShadowRootIfShadowDOMEnabledOrNode* youngestShadowRoot(Element* host, ExceptionCode&);
    ShadowRootIfShadowDOMEnabledOrNode* oldestShadowRoot(Element* host, ExceptionCode&);
    ShadowRootIfShadowDOMEnabledOrNode* youngerShadowRoot(Node* shadow, ExceptionCode&);
    ShadowRootIfShadowDOMEnabledOrNode* olderShadowRoot(Node* shadow, ExceptionCode&);
    Element* includerFor(Node*, ExceptionCode&);
    String shadowPseudoId(Element*, ExceptionCode&);
    PassRefPtr<Element> createContentElement(Document*, ExceptionCode&);
    Element* getElementByIdInShadowRoot(Node* shadowRoot, const String& id, ExceptionCode&);
    bool isValidContentSelect(Element* insertionPoint, ExceptionCode&);
    Node* treeScopeRootNode(Node*, ExceptionCode&);

    bool attached(Node*, ExceptionCode&);

    // FIXME: Rename these functions if walker is prefered.
    Node* nextSiblingByWalker(Node*, ExceptionCode&);
    Node* firstChildByWalker(Node*, ExceptionCode&);
    Node* lastChildByWalker(Node*, ExceptionCode&);
    Node* nextNodeByWalker(Node*, ExceptionCode&);
    Node* previousNodeByWalker(Node*, ExceptionCode&);

    String visiblePlaceholder(Element*);
#if ENABLE(INPUT_TYPE_COLOR)
    void selectColorInColorChooser(Element*, const String& colorValue);
#endif

    PassRefPtr<ClientRect> boundingBox(Element*, ExceptionCode&);

    PassRefPtr<ClientRectList> inspectorHighlightRects(Document*, ExceptionCode&);

    void setBackgroundBlurOnNode(Node*, int blurLength, ExceptionCode&);

    unsigned markerCountForNode(Node*, const String&, ExceptionCode&);
    PassRefPtr<Range> markerRangeForNode(Node*, const String& markerType, unsigned index, ExceptionCode&);
    String markerDescriptionForNode(Node*, const String& markerType, unsigned index, ExceptionCode&);

    void setScrollViewPosition(Document*, long x, long y, ExceptionCode&);

    void setPagination(Document*, const String& mode, int gap, ExceptionCode&);

    bool wasLastChangeUserEdit(Element* textField, ExceptionCode&);
    String suggestedValue(Element* inputElement, ExceptionCode&);
    void setSuggestedValue(Element* inputElement, const String&, ExceptionCode&);
    void setEditingValue(Element* inputElement, const String&, ExceptionCode&);
    void scrollElementToRect(Element*, long x, long y, long w, long h, ExceptionCode&);

    void paintControlTints(Document*, ExceptionCode&);

    PassRefPtr<Range> rangeFromLocationAndLength(Element* scope, int rangeLocation, int rangeLength, ExceptionCode&);
    unsigned locationFromRange(Element* scope, const Range*, ExceptionCode&);
    unsigned lengthFromRange(Element* scope, const Range*, ExceptionCode&);
    String rangeAsText(const Range*, ExceptionCode&);

    void setDelegatesScrolling(bool enabled, Document*, ExceptionCode&);
#if ENABLE(TOUCH_ADJUSTMENT)
    PassRefPtr<WebKitPoint> touchPositionAdjustedToBestClickableNode(long x, long y, long width, long height, Document*, ExceptionCode&);
    Node* touchNodeAdjustedToBestClickableNode(long x, long y, long width, long height, Document*, ExceptionCode&);
    PassRefPtr<ClientRect> bestZoomableAreaForTouchPoint(long x, long y, long width, long height, Document*, ExceptionCode&);
#endif

    int lastSpellCheckRequestSequence(Document*, ExceptionCode&);
    int lastSpellCheckProcessedSequence(Document*, ExceptionCode&);
    
    Vector<String> userPreferredLanguages() const;
    void setUserPreferredLanguages(const Vector<String>&);

    void setShouldDisplayTrackKind(Document*, const String& kind, bool, ExceptionCode&);
    bool shouldDisplayTrackKind(Document*, const String& kind, ExceptionCode&);

    unsigned wheelEventHandlerCount(Document*, ExceptionCode&);
    unsigned touchEventHandlerCount(Document*, ExceptionCode&);

    PassRefPtr<NodeList> nodesFromRect(Document*, int x, int y, unsigned topPadding, unsigned rightPadding,
        unsigned bottomPadding, unsigned leftPadding, bool ignoreClipping, bool allowShadowContent, ExceptionCode&) const;

    void emitInspectorDidBeginFrame();
    void emitInspectorDidCancelFrame();

    bool hasSpellingMarker(Document*, int from, int length, ExceptionCode&);
    bool hasGrammarMarker(Document*, int from, int length, ExceptionCode&);

    unsigned numberOfScrollableAreas(Document*, ExceptionCode&);

    bool isPageBoxVisible(Document*, int pageNumber, ExceptionCode&);

    static const char* internalsId;

    InternalSettings* settings() const { return m_settings.get(); }

    void setBatteryStatus(Document*, const String& eventType, bool charging, double chargingTime, double dischargingTime, double level, ExceptionCode&);

    void setNetworkInformation(Document*, const String& eventType, long bandwidth, bool metered, ExceptionCode&);

    void suspendAnimations(Document*, ExceptionCode&) const;
    void resumeAnimations(Document*, ExceptionCode&) const;

#if ENABLE(INSPECTOR)
    unsigned numberOfLiveNodes() const;
    unsigned numberOfLiveDocuments() const;
    Vector<String> consoleMessageArgumentCounts(Document*) const;
#endif

#if ENABLE(FULLSCREEN_API)
    void webkitWillEnterFullScreenForElement(Document*, Element*);
    void webkitDidEnterFullScreenForElement(Document*, Element*);
    void webkitWillExitFullScreenForElement(Document*, Element*);
    void webkitDidExitFullScreenForElement(Document*, Element*);
#endif

private:
    explicit Internals(Document*);
    DocumentMarker* markerAt(Node*, const String& markerType, unsigned index, ExceptionCode&);

    RefPtr<InternalSettings> m_settings;
};

} // namespace WebCore

#endif
