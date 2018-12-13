/*
 * Copyright (C) 2009-2017 Apple Inc. All rights reserved.
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
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
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

#include "EventTarget.h"
#include "InspectorWebAgentBase.h"
#include "Timer.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace JSC {
class ExecState;
class JSValue;
}

namespace WebCore {
    
class AccessibilityObject;
class CharacterData;
class DOMEditor;
class Document;
class Element;
class Event;
class Exception;
class FloatQuad;
class Frame;
class InspectorHistory;
class InspectorOverlay;
class InspectorPageAgent;
#if ENABLE(VIDEO)
class HTMLMediaElement;
#endif
class HitTestResult;
class Node;
class PseudoElement;
class RevalidateStyleAttributeTask;
class ShadowRoot;

struct HighlightConfig;

typedef String ErrorString;

struct EventListenerInfo {
    EventListenerInfo(Node* node, const AtomicString& eventType, EventListenerVector&& eventListenerVector)
        : node(node)
        , eventType(eventType)
        , eventListenerVector(WTFMove(eventListenerVector))
    {
    }

    Node* node;
    const AtomicString eventType;
    const EventListenerVector eventListenerVector;
};

class InspectorDOMAgent final : public InspectorAgentBase, public Inspector::DOMBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    struct DOMListener {
        virtual ~DOMListener() = default;
        virtual void didRemoveDOMNode(Node&, int nodeId) = 0;
        virtual void didModifyDOMAttr(Element&) = 0;
    };

    InspectorDOMAgent(WebAgentContext&, InspectorPageAgent*, InspectorOverlay*);
    virtual ~InspectorDOMAgent();

    static String toErrorString(ExceptionCode);
    static String toErrorString(Exception&&);

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    Vector<Document*> documents();
    void reset();

    // Methods called from the frontend for DOM nodes inspection.
    void querySelector(ErrorString&, int nodeId, const String& selectors, int* elementId) override;
    void querySelectorAll(ErrorString&, int nodeId, const String& selectors, RefPtr<JSON::ArrayOf<int>>& result) override;
    void getDocument(ErrorString&, RefPtr<Inspector::Protocol::DOM::Node>& root) override;
    void requestChildNodes(ErrorString&, int nodeId, const int* depth) override;
    void setAttributeValue(ErrorString&, int elementId, const String& name, const String& value) override;
    void setAttributesAsText(ErrorString&, int elementId, const String& text, const String* name) override;
    void removeAttribute(ErrorString&, int elementId, const String& name) override;
    void removeNode(ErrorString&, int nodeId) override;
    void setNodeName(ErrorString&, int nodeId, const String& name, int* newId) override;
    void getOuterHTML(ErrorString&, int nodeId, WTF::String* outerHTML) override;
    void setOuterHTML(ErrorString&, int nodeId, const String& outerHTML) override;
    void insertAdjacentHTML(ErrorString&, int nodeId, const String& position, const String& html) override;
    void setNodeValue(ErrorString&, int nodeId, const String& value) override;
    void getSupportedEventNames(ErrorString&, RefPtr<JSON::ArrayOf<String>>& eventNames) override;
    void getEventListenersForNode(ErrorString&, int nodeId, const WTF::String* objectGroup, RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::EventListener>>& listenersArray) override;
    void setEventListenerDisabled(ErrorString&, int eventListenerId, bool disabled) override;
    void setBreakpointForEventListener(ErrorString&, int eventListenerId) override;
    void removeBreakpointForEventListener(ErrorString&, int eventListenerId) override;
    void getAccessibilityPropertiesForNode(ErrorString&, int nodeId, RefPtr<Inspector::Protocol::DOM::AccessibilityProperties>& axProperties) override;
    void performSearch(ErrorString&, const String& whitespaceTrimmedQuery, const JSON::Array* nodeIds, String* searchId, int* resultCount) override;
    void getSearchResults(ErrorString&, const String& searchId, int fromIndex, int toIndex, RefPtr<JSON::ArrayOf<int>>&) override;
    void discardSearchResults(ErrorString&, const String& searchId) override;
    void resolveNode(ErrorString&, int nodeId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result) override;
    void getAttributes(ErrorString&, int nodeId, RefPtr<JSON::ArrayOf<String>>& result) override;
    void setInspectModeEnabled(ErrorString&, bool enabled, const JSON::Object* highlightConfig) override;
    void requestNode(ErrorString&, const String& objectId, int* nodeId) override;
    void pushNodeByPathToFrontend(ErrorString&, const String& path, int* nodeId) override;
    void hideHighlight(ErrorString&) override;
    void highlightRect(ErrorString&, int x, int y, int width, int height, const JSON::Object* color, const JSON::Object* outlineColor, const bool* usePageCoordinates) override;
    void highlightQuad(ErrorString&, const JSON::Array& quad, const JSON::Object* color, const JSON::Object* outlineColor, const bool* usePageCoordinates) override;
    void highlightSelector(ErrorString&, const JSON::Object& highlightConfig, const String& selectorString, const String* frameId) override;
    void highlightNode(ErrorString&, const JSON::Object& highlightConfig, const int* nodeId, const String* objectId) override;
    void highlightNodeList(ErrorString&, const JSON::Array& nodeIds, const JSON::Object& highlightConfig) override;
    void highlightFrame(ErrorString&, const String& frameId, const JSON::Object* color, const JSON::Object* outlineColor) override;
    void moveTo(ErrorString&, int nodeId, int targetNodeId, const int* anchorNodeId, int* newNodeId) override;
    void undo(ErrorString&) override;
    void redo(ErrorString&) override;
    void markUndoableState(ErrorString&) override;
    void focus(ErrorString&, int nodeId) override;
    void setInspectedNode(ErrorString&, int nodeId) override;

    void getEventListeners(Node*, Vector<EventListenerInfo>& listenersArray, bool includeAncestors);


    // InspectorInstrumentation
    int identifierForNode(Node&);
    void addEventListenersToNode(Node&);
    void didInsertDOMNode(Node&);
    void didRemoveDOMNode(Node&);
    void willModifyDOMAttr(Element&, const AtomicString& oldValue, const AtomicString& newValue);
    void didModifyDOMAttr(Element&, const AtomicString& name, const AtomicString& value);
    void didRemoveDOMAttr(Element&, const AtomicString& name);
    void characterDataModified(CharacterData&);
    void didInvalidateStyleAttr(Node&);
    void didPushShadowRoot(Element& host, ShadowRoot&);
    void willPopShadowRoot(Element& host, ShadowRoot&);
    void didChangeCustomElementState(Element&);
    bool handleTouchEvent(Node&);
    void didCommitLoad(Document*);
    void frameDocumentUpdated(Frame&);
    void pseudoElementCreated(PseudoElement&);
    void pseudoElementDestroyed(PseudoElement&);
    void didAddEventListener(EventTarget&);
    void willRemoveEventListener(EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    bool isEventListenerDisabled(EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    void eventDidResetAfterDispatch(const Event&);

    // Callbacks that don't directly correspond to an instrumentation entry point.
    void setDocument(Document*);
    void releaseDanglingNodes();

    void styleAttributeInvalidated(const Vector<Element*>& elements);

    int pushNodeToFrontend(ErrorString&, int documentNodeId, Node*);
    Node* nodeForId(int nodeId);
    int boundNodeId(const Node*);
    void setDOMListener(DOMListener*);

    static String documentURLString(Document*);

    RefPtr<Inspector::Protocol::Runtime::RemoteObject> resolveNode(Node*, const String& objectGroup);
    bool handleMousePress();
    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);
    void inspect(Node*);
    void focusNode();

    InspectorHistory* history() { return m_history.get(); }

    // We represent embedded doms as a part of the same hierarchy. Hence we treat children of frame owners differently.
    // We also skip whitespace text nodes conditionally. Following methods encapsulate these specifics.
    static Node* innerFirstChild(Node*);
    static Node* innerNextSibling(Node*);
    static Node* innerPreviousSibling(Node*);
    static unsigned innerChildNodeCount(Node*);
    static Node* innerParentNode(Node*);

    Node* assertNode(ErrorString&, int nodeId);
    Element* assertElement(ErrorString&, int nodeId);
    Document* assertDocument(ErrorString&, int nodeId);

    static Node* scriptValueAsNode(JSC::JSValue);
    static JSC::JSValue nodeAsScriptValue(JSC::ExecState&, Node*);

    // Methods called from other agents.
    InspectorPageAgent* pageAgent() { return m_pageAgent; }

    bool hasBreakpointForEventListener(EventTarget&, const AtomicString& eventType, EventListener&, bool capture);
    int idForEventListener(EventTarget&, const AtomicString& eventType, EventListener&, bool capture);

private:
#if ENABLE(VIDEO)
    void mediaMetricsTimerFired();
#endif

    void highlightMousedOverNode();
    void setSearchingForNode(ErrorString&, bool enabled, const JSON::Object* highlightConfig);
    std::unique_ptr<HighlightConfig> highlightConfigFromInspectorObject(ErrorString&, const JSON::Object* highlightInspectorObject);

    // Node-related methods.
    typedef HashMap<RefPtr<Node>, int> NodeToIdMap;
    int bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    Node* assertEditableNode(ErrorString&, int nodeId);
    Element* assertEditableElement(ErrorString&, int nodeId);

    int pushNodePathToFrontend(Node*);
    void pushChildNodesToFrontend(int nodeId, int depth = 1);

    Ref<Inspector::Protocol::DOM::Node> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    Ref<JSON::ArrayOf<String>> buildArrayForElementAttributes(Element*);
    Ref<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> buildArrayForPseudoElements(const Element&, NodeToIdMap* nodesMap);
    Ref<Inspector::Protocol::DOM::EventListener> buildObjectForEventListener(const RegisteredEventListener&, int identifier, const AtomicString& eventType, Node*, const String* objectGroupId, bool disabled, bool hasBreakpoint);
    RefPtr<Inspector::Protocol::DOM::AccessibilityProperties> buildObjectForAccessibilityProperties(Node*);
    void processAccessibilityChildren(RefPtr<AccessibilityObject>&&, RefPtr<JSON::ArrayOf<int>>&&);
    
    Node* nodeForPath(const String& path);
    Node* nodeForObjectId(const String& objectId);

    void discardBindings();

    void innerHighlightQuad(std::unique_ptr<FloatQuad>, const JSON::Object* color, const JSON::Object* outlineColor, const bool* usePageCoordinates);

    Inspector::InjectedScriptManager& m_injectedScriptManager;
    std::unique_ptr<Inspector::DOMFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::DOMBackendDispatcher> m_backendDispatcher;
    InspectorPageAgent* m_pageAgent { nullptr };

    InspectorOverlay* m_overlay { nullptr };
    DOMListener* m_domListener { nullptr };
    NodeToIdMap m_documentNodeToIdMap;
    // Owns node mappings for dangling nodes.
    Vector<std::unique_ptr<NodeToIdMap>> m_danglingNodeToIdMaps;
    HashMap<int, Node*> m_idToNode;
    HashMap<int, NodeToIdMap*> m_idToNodesMap;
    HashSet<int> m_childrenRequested;
    int m_lastNodeId { 1 };
    RefPtr<Document> m_document;
    typedef HashMap<String, Vector<RefPtr<Node>>> SearchResults;
    SearchResults m_searchResults;
    std::unique_ptr<RevalidateStyleAttributeTask> m_revalidateStyleAttrTask;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<Node> m_mousedOverNode;
    std::unique_ptr<HighlightConfig> m_inspectModeHighlightConfig;
    std::unique_ptr<InspectorHistory> m_history;
    std::unique_ptr<DOMEditor> m_domEditor;
    bool m_searchingForNode { false };
    bool m_suppressAttributeModifiedEvent { false };
    bool m_documentRequested { false };

#if ENABLE(VIDEO)
    Timer m_mediaMetricsTimer;
    struct MediaMetrics {
        unsigned displayCompositedFrames { 0 };
        bool isLowPower { false };

        MediaMetrics() { }

        MediaMetrics(unsigned displayCompositedFrames)
            : displayCompositedFrames(displayCompositedFrames)
        {
        }
    };

    // The pointer key for this map should not be used for anything other than matching.
    HashMap<HTMLMediaElement*, MediaMetrics> m_mediaMetrics;
#endif

    struct InspectorEventListener {
        int identifier { 1 };
        RefPtr<EventTarget> eventTarget;
        RefPtr<EventListener> eventListener;
        AtomicString eventType;
        bool useCapture { false };
        bool disabled { false };
        bool hasBreakpoint { false };

        InspectorEventListener() { }

        InspectorEventListener(int identifier, EventTarget& target, const AtomicString& type, EventListener& listener, bool capture)
            : identifier(identifier)
            , eventTarget(&target)
            , eventListener(&listener)
            , eventType(type)
            , useCapture(capture)
        {
        }

        bool matches(EventTarget& target, const AtomicString& type, EventListener& listener, bool capture)
        {
            if (eventTarget.get() != &target)
                return false;
            if (eventListener.get() != &listener)
                return false;
            if (eventType != type)
                return false;
            if (useCapture != capture)
                return false;
            return true;
        }
    };

    friend class EventFiredCallback;

    HashSet<const Event*> m_dispatchedEvents;
    HashMap<int, InspectorEventListener> m_eventListenerEntries;
    int m_lastEventListenerId { 1 };
};

} // namespace WebCore
