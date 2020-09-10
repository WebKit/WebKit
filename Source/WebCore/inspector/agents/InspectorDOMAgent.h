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
#include <JavaScriptCore/Breakpoint.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/JSONValues.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomString.h>

namespace Inspector {
class InjectedScriptManager;
}

namespace JSC {
class CallFrame;
class JSValue;
}

namespace WebCore {

class AXCoreObject;
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
#if ENABLE(VIDEO)
class HTMLMediaElement;
#endif
class HitTestResult;
class Node;
class Page;
class PseudoElement;
class RevalidateStyleAttributeTask;
class ShadowRoot;

struct HighlightConfig;

class InspectorDOMAgent final : public InspectorAgentBase, public Inspector::DOMBackendDispatcherHandler {
    WTF_MAKE_NONCOPYABLE(InspectorDOMAgent);
    WTF_MAKE_FAST_ALLOCATED;
public:
    InspectorDOMAgent(PageAgentContext&, InspectorOverlay*);
    ~InspectorDOMAgent();

    static String toErrorString(ExceptionCode);
    static String toErrorString(Exception&&);

    static String documentURLString(Document*);

    // We represent embedded doms as a part of the same hierarchy. Hence we treat children of frame owners differently.
    // We also skip whitespace text nodes conditionally. Following methods encapsulate these specifics.
    static Node* innerFirstChild(Node*);
    static Node* innerNextSibling(Node*);
    static Node* innerPreviousSibling(Node*);
    static unsigned innerChildNodeCount(Node*);
    static Node* innerParentNode(Node*);

    static Node* scriptValueAsNode(JSC::JSValue);
    static JSC::JSValue nodeAsScriptValue(JSC::JSGlobalObject&, Node*);

    // InspectorAgentBase
    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*);
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason);

    // DOMBackendDispatcherHandler
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> querySelector(Inspector::Protocol::DOM::NodeId, const String& selector);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> querySelectorAll(Inspector::Protocol::DOM::NodeId, const String& selector);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::Node>> getDocument();
    Inspector::Protocol::ErrorStringOr<void> requestChildNodes(Inspector::Protocol::DOM::NodeId, Optional<int>&& depth);
    Inspector::Protocol::ErrorStringOr<void> setAttributeValue(Inspector::Protocol::DOM::NodeId, const String& name, const String& value);
    Inspector::Protocol::ErrorStringOr<void> setAttributesAsText(Inspector::Protocol::DOM::NodeId, const String& text, const String& name);
    Inspector::Protocol::ErrorStringOr<void> removeAttribute(Inspector::Protocol::DOM::NodeId, const String& name);
    Inspector::Protocol::ErrorStringOr<void> removeNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> setNodeName(Inspector::Protocol::DOM::NodeId, const String&);
    Inspector::Protocol::ErrorStringOr<String> getOuterHTML(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<void> setOuterHTML(Inspector::Protocol::DOM::NodeId, const String&);
    Inspector::Protocol::ErrorStringOr<void> insertAdjacentHTML(Inspector::Protocol::DOM::NodeId, const String& position, const String& html);
    Inspector::Protocol::ErrorStringOr<void> setNodeValue(Inspector::Protocol::DOM::NodeId, const String&);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> getSupportedEventNames();
#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::DataBinding>>> getDataBindingsForNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<String> getAssociatedDataForNode(Inspector::Protocol::DOM::NodeId);
#endif
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::EventListener>>> getEventListenersForNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<void> setEventListenerDisabled(Inspector::Protocol::DOM::EventListenerId, bool);
    Inspector::Protocol::ErrorStringOr<void> setBreakpointForEventListener(Inspector::Protocol::DOM::EventListenerId, RefPtr<JSON::Object>&& options);
    Inspector::Protocol::ErrorStringOr<void> removeBreakpointForEventListener(Inspector::Protocol::DOM::EventListenerId);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::AccessibilityProperties>> getAccessibilityPropertiesForNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<std::tuple<String /* searchId */, int /* resultCount */>> performSearch(const String& query, RefPtr<JSON::Array>&& nodeIds, Optional<bool>&& caseSensitive);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>>> getSearchResults(const String& searchId, int fromIndex, int toIndex);
    Inspector::Protocol::ErrorStringOr<void> discardSearchResults(const String& searchId);
    Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::Runtime::RemoteObject>> resolveNode(Inspector::Protocol::DOM::NodeId, const String& objectGroup);
    Inspector::Protocol::ErrorStringOr<Ref<JSON::ArrayOf<String>>> getAttributes(Inspector::Protocol::DOM::NodeId);
#if PLATFORM(IOS_FAMILY)
    Inspector::Protocol::ErrorStringOr<void> setInspectModeEnabled(bool, RefPtr<JSON::Object>&& highlightConfig);
#else
    Inspector::Protocol::ErrorStringOr<void> setInspectModeEnabled(bool, RefPtr<JSON::Object>&& highlightConfig, Optional<bool>&& showRulers);
#endif
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> requestNode(const Inspector::Protocol::Runtime::RemoteObjectId&);
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> pushNodeByPathToFrontend(const String& path);
    Inspector::Protocol::ErrorStringOr<void> hideHighlight();
    Inspector::Protocol::ErrorStringOr<void> highlightRect(int x, int y, int width, int height, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, Optional<bool>&& usePageCoordinates);
    Inspector::Protocol::ErrorStringOr<void> highlightQuad(Ref<JSON::Array>&& quad, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, Optional<bool>&& usePageCoordinates);
    Inspector::Protocol::ErrorStringOr<void> highlightSelector(Ref<JSON::Object>&& highlightConfig, const String& selectorString, const Inspector::Protocol::Network::FrameId&);
    Inspector::Protocol::ErrorStringOr<void> highlightNode(Ref<JSON::Object>&& highlightConfig, Optional<Inspector::Protocol::DOM::NodeId>&&, const Inspector::Protocol::Runtime::RemoteObjectId&);
    Inspector::Protocol::ErrorStringOr<void> highlightNodeList(Ref<JSON::Array>&& nodeIds, Ref<JSON::Object>&& highlightConfig);
    Inspector::Protocol::ErrorStringOr<void> highlightFrame(const Inspector::Protocol::Network::FrameId&, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor);
    Inspector::Protocol::ErrorStringOr<Inspector::Protocol::DOM::NodeId> moveTo(Inspector::Protocol::DOM::NodeId nodeId, Inspector::Protocol::DOM::NodeId targetNodeId, Optional<Inspector::Protocol::DOM::NodeId>&& insertBeforeNodeId);
    Inspector::Protocol::ErrorStringOr<void> undo();
    Inspector::Protocol::ErrorStringOr<void> redo();
    Inspector::Protocol::ErrorStringOr<void> markUndoableState();
    Inspector::Protocol::ErrorStringOr<void> focus(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<void> setInspectedNode(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::ErrorStringOr<void> setAllowEditingUserAgentShadowTrees(bool);

    // InspectorInstrumentation
    Inspector::Protocol::DOM::NodeId identifierForNode(Node&);
    void addEventListenersToNode(Node&);
    void didInsertDOMNode(Node&);
    void didRemoveDOMNode(Node&);
    void willModifyDOMAttr(Element&, const AtomString& oldValue, const AtomString& newValue);
    void didModifyDOMAttr(Element&, const AtomString& name, const AtomString& value);
    void didRemoveDOMAttr(Element&, const AtomString& name);
    void characterDataModified(CharacterData&);
    void didInvalidateStyleAttr(Element&);
    void didPushShadowRoot(Element& host, ShadowRoot&);
    void willPopShadowRoot(Element& host, ShadowRoot&);
    void didChangeCustomElementState(Element&);
    bool handleTouchEvent(Node&);
    void didCommitLoad(Document*);
    void frameDocumentUpdated(Frame&);
    void pseudoElementCreated(PseudoElement&);
    void pseudoElementDestroyed(PseudoElement&);
    void didAddEventListener(EventTarget&);
    void willRemoveEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    bool isEventListenerDisabled(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    void eventDidResetAfterDispatch(const Event&);

    // Callbacks that don't directly correspond to an instrumentation entry point.
    void setDocument(Document*);
    void releaseDanglingNodes();

    void styleAttributeInvalidated(const Vector<Element*>& elements);

    Inspector::Protocol::DOM::NodeId pushNodeToFrontend(Node*);
    Inspector::Protocol::DOM::NodeId pushNodeToFrontend(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId documentNodeId, Node*);
    Inspector::Protocol::DOM::NodeId pushNodePathToFrontend(Node*);
    Inspector::Protocol::DOM::NodeId pushNodePathToFrontend(Inspector::Protocol::ErrorString, Node*);
    Node* nodeForId(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::DOM::NodeId boundNodeId(const Node*);

    RefPtr<Inspector::Protocol::Runtime::RemoteObject> resolveNode(Node*, const String& objectGroup);
    bool handleMousePress();
    void mouseDidMoveOverElement(const HitTestResult&, unsigned modifierFlags);
    void inspect(Node*);
    void focusNode();

    InspectorHistory* history() { return m_history.get(); }
    Vector<Document*> documents();
    void reset();

    Node* assertNode(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);
    Element* assertElement(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);
    Document* assertDocument(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);

    RefPtr<JSC::Breakpoint> breakpointForEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);
    Inspector::Protocol::DOM::EventListenerId idForEventListener(EventTarget&, const AtomString& eventType, EventListener&, bool capture);

private:
#if ENABLE(VIDEO)
    void mediaMetricsTimerFired();
#endif

    void highlightMousedOverNode();
    void setSearchingForNode(Inspector::Protocol::ErrorString&, bool enabled, RefPtr<JSON::Object>&& highlightConfig, bool showRulers);
    std::unique_ptr<HighlightConfig> highlightConfigFromInspectorObject(Inspector::Protocol::ErrorString&, RefPtr<JSON::Object>&& highlightInspectorObject);

    // Node-related methods.
    typedef HashMap<RefPtr<Node>, Inspector::Protocol::DOM::NodeId> NodeToIdMap;
    Inspector::Protocol::DOM::NodeId bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    Node* assertEditableNode(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);
    Element* assertEditableElement(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);

    void pushChildNodesToFrontend(Inspector::Protocol::DOM::NodeId, int depth = 1);

    Ref<Inspector::Protocol::DOM::Node> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    Ref<JSON::ArrayOf<String>> buildArrayForElementAttributes(Element*);
    Ref<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> buildArrayForPseudoElements(const Element&, NodeToIdMap* nodesMap);
    Ref<Inspector::Protocol::DOM::EventListener> buildObjectForEventListener(const RegisteredEventListener&, Inspector::Protocol::DOM::EventListenerId identifier, EventTarget&, const AtomString& eventType, bool disabled, const RefPtr<JSC::Breakpoint>&);
    Ref<Inspector::Protocol::DOM::AccessibilityProperties> buildObjectForAccessibilityProperties(Node&);
    void processAccessibilityChildren(AXCoreObject&, JSON::ArrayOf<Inspector::Protocol::DOM::NodeId>&);
    
    Node* nodeForPath(const String& path);
    Node* nodeForObjectId(const Inspector::Protocol::Runtime::RemoteObjectId&);

    void discardBindings();

    void innerHighlightQuad(std::unique_ptr<FloatQuad>, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, Optional<bool>&& usePageCoordinates);

    Inspector::InjectedScriptManager& m_injectedScriptManager;
    std::unique_ptr<Inspector::DOMFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::DOMBackendDispatcher> m_backendDispatcher;
    Page& m_inspectedPage;
    InspectorOverlay* m_overlay { nullptr };
    NodeToIdMap m_documentNodeToIdMap;
    // Owns node mappings for dangling nodes.
    Vector<std::unique_ptr<NodeToIdMap>> m_danglingNodeToIdMaps;
    HashMap<Inspector::Protocol::DOM::NodeId, Node*> m_idToNode;
    HashMap<Inspector::Protocol::DOM::NodeId, NodeToIdMap*> m_idToNodesMap;
    HashSet<Inspector::Protocol::DOM::NodeId> m_childrenRequested;
    Inspector::Protocol::DOM::NodeId m_lastNodeId { 1 };
    RefPtr<Document> m_document;
    typedef HashMap<String, Vector<RefPtr<Node>>> SearchResults;
    SearchResults m_searchResults;
    std::unique_ptr<RevalidateStyleAttributeTask> m_revalidateStyleAttrTask;
    RefPtr<Node> m_nodeToFocus;
    RefPtr<Node> m_mousedOverNode;
    RefPtr<Node> m_inspectedNode;
    std::unique_ptr<HighlightConfig> m_inspectModeHighlightConfig;
    std::unique_ptr<InspectorHistory> m_history;
    std::unique_ptr<DOMEditor> m_domEditor;

#if ENABLE(VIDEO)
    Timer m_mediaMetricsTimer;
    struct MediaMetrics {
        unsigned displayCompositedFrames { 0 };
        bool isPowerEfficient { false };

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
        Inspector::Protocol::DOM::EventListenerId identifier { 1 };
        RefPtr<EventTarget> eventTarget;
        RefPtr<EventListener> eventListener;
        AtomString eventType;
        bool useCapture { false };
        bool disabled { false };
        RefPtr<JSC::Breakpoint> breakpoint;

        InspectorEventListener() { }

        InspectorEventListener(Inspector::Protocol::DOM::EventListenerId identifier, EventTarget& target, const AtomString& type, EventListener& listener, bool capture)
            : identifier(identifier)
            , eventTarget(&target)
            , eventListener(&listener)
            , eventType(type)
            , useCapture(capture)
        {
        }

        bool matches(EventTarget& target, const AtomString& type, EventListener& listener, bool capture)
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
    HashMap<Inspector::Protocol::DOM::EventListenerId, InspectorEventListener> m_eventListenerEntries;
    Inspector::Protocol::DOM::EventListenerId m_lastEventListenerId { 1 };

    bool m_searchingForNode { false };
    bool m_suppressAttributeModifiedEvent { false };
    bool m_suppressEventListenerChangedEvent { false };
    bool m_documentRequested { false };
    bool m_allowEditingUserAgentShadowTrees { false };
};

} // namespace WebCore
