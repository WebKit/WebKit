/*
 * Copyright (C) 2009, 2015 Apple Inc. All rights reserved.
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

#ifndef InspectorDOMAgent_h
#define InspectorDOMAgent_h

#include "EventTarget.h"
#include "InspectorWebAgentBase.h"
#include "RenderLayer.h"
#include "Timer.h"
#include <inspector/InspectorBackendDispatchers.h>
#include <inspector/InspectorFrontendDispatchers.h>
#include <inspector/InspectorValues.h>
#include <wtf/Deque.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>
#include <wtf/text/AtomicString.h>

namespace Deprecated {
class ScriptValue;
}

namespace Inspector {
class InjectedScriptManager;
}

namespace WebCore {
    
class AccessibilityObject;
class ContainerNode;
class CharacterData;
class DOMEditor;
class Document;
class Element;
class Event;
class InspectorHistory;
class InspectorOverlay;
class InspectorPageAgent;
class HitTestResult;
class HTMLElement;
class NameNodeMap;
class Node;
class RevalidateStyleAttributeTask;
class ShadowRoot;

struct HighlightConfig;

typedef String ErrorString;
typedef int BackendNodeId;

struct EventListenerInfo {
    EventListenerInfo(Node* node, const AtomicString& eventType, const EventListenerVector& eventListenerVector)
        : node(node)
        , eventType(eventType)
        , eventListenerVector(eventListenerVector)
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
        virtual ~DOMListener()
        {
        }
        virtual void didRemoveDocument(Document*) = 0;
        virtual void didRemoveDOMNode(Node*) = 0;
        virtual void didModifyDOMAttr(Element*) = 0;
    };

    InspectorDOMAgent(WebAgentContext&, InspectorPageAgent*, InspectorOverlay*);
    virtual ~InspectorDOMAgent();

    static String toErrorString(const ExceptionCode&);

    void didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*) override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    Vector<Document*> documents();
    void reset();

    // Methods called from the frontend for DOM nodes inspection.
    void querySelector(ErrorString&, int nodeId, const String& selectors, int* elementId) override;
    void querySelectorAll(ErrorString&, int nodeId, const String& selectors, RefPtr<Inspector::Protocol::Array<int>>& result) override;
    void getDocument(ErrorString&, RefPtr<Inspector::Protocol::DOM::Node>& root) override;
    void requestChildNodes(ErrorString&, int nodeId, const int* depth) override;
    void setAttributeValue(ErrorString&, int elementId, const String& name, const String& value) override;
    void setAttributesAsText(ErrorString&, int elementId, const String& text, const String* name) override;
    void removeAttribute(ErrorString&, int elementId, const String& name) override;
    void removeNode(ErrorString&, int nodeId) override;
    void setNodeName(ErrorString&, int nodeId, const String& name, int* newId) override;
    void getOuterHTML(ErrorString&, int nodeId, WTF::String* outerHTML) override;
    void setOuterHTML(ErrorString&, int nodeId, const String& outerHTML) override;
    void setNodeValue(ErrorString&, int nodeId, const String& value) override;
    void getEventListenersForNode(ErrorString&, int nodeId, const WTF::String* objectGroup, RefPtr<Inspector::Protocol::Array<Inspector::Protocol::DOM::EventListener>>& listenersArray) override;
    void getAccessibilityPropertiesForNode(ErrorString&, int nodeId, RefPtr<Inspector::Protocol::DOM::AccessibilityProperties>& axProperties) override;
    void performSearch(ErrorString&, const String& whitespaceTrimmedQuery, const Inspector::InspectorArray* nodeIds, String* searchId, int* resultCount) override;
    void getSearchResults(ErrorString&, const String& searchId, int fromIndex, int toIndex, RefPtr<Inspector::Protocol::Array<int>>&) override;
    void discardSearchResults(ErrorString&, const String& searchId) override;
    void resolveNode(ErrorString&, int nodeId, const String* objectGroup, RefPtr<Inspector::Protocol::Runtime::RemoteObject>& result) override;
    void getAttributes(ErrorString&, int nodeId, RefPtr<Inspector::Protocol::Array<String>>& result) override;
    void setInspectModeEnabled(ErrorString&, bool enabled, const Inspector::InspectorObject* highlightConfig) override;
    void requestNode(ErrorString&, const String& objectId, int* nodeId) override;
    void pushNodeByPathToFrontend(ErrorString&, const String& path, int* nodeId) override;
    void pushNodeByBackendIdToFrontend(ErrorString&, BackendNodeId, int* nodeId) override;
    void releaseBackendNodeIds(ErrorString&, const String& nodeGroup) override;
    void hideHighlight(ErrorString&) override;
    void highlightRect(ErrorString&, int x, int y, int width, int height, const Inspector::InspectorObject* color, const Inspector::InspectorObject* outlineColor, const bool* usePageCoordinates) override;
    void highlightQuad(ErrorString&, const Inspector::InspectorArray& quad, const Inspector::InspectorObject* color, const Inspector::InspectorObject* outlineColor, const bool* usePageCoordinates) override;
    void highlightSelector(ErrorString&, const Inspector::InspectorObject& highlightConfig, const String& selectorString, const String* frameId) override;
    void highlightNode(ErrorString&, const Inspector::InspectorObject& highlightConfig, const int* nodeId, const String* objectId) override;
    void highlightFrame(ErrorString&, const String& frameId, const Inspector::InspectorObject* color, const Inspector::InspectorObject* outlineColor) override;

    void moveTo(ErrorString&, int nodeId, int targetNodeId, const int* anchorNodeId, int* newNodeId) override;
    void undo(ErrorString&) override;
    void redo(ErrorString&) override;
    void markUndoableState(ErrorString&) override;
    void focus(ErrorString&, int nodeId) override;

    void getEventListeners(Node*, Vector<EventListenerInfo>& listenersArray, bool includeAncestors);


    // InspectorInstrumentation callbacks.
    void didInsertDOMNode(Node&);
    void didRemoveDOMNode(Node&);
    void willModifyDOMAttr(Element&, const AtomicString& oldValue, const AtomicString& newValue);
    void didModifyDOMAttr(Element&, const AtomicString& name, const AtomicString& value);
    void didRemoveDOMAttr(Element&, const AtomicString& name);
    void characterDataModified(CharacterData&);
    void didInvalidateStyleAttr(Node&);
    void didPushShadowRoot(Element& host, ShadowRoot&);
    void willPopShadowRoot(Element& host, ShadowRoot&);
    bool handleTouchEvent(Node&);
    void didCommitLoad(Document*);
    void frameDocumentUpdated(Frame*);
    void pseudoElementCreated(PseudoElement&);
    void pseudoElementDestroyed(PseudoElement&);

    // Callbacks that don't directly correspond to an instrumentation entry point.
    void setDocument(Document*);
    void releaseDanglingNodes();
    void mainFrameDOMContentLoaded();

    void styleAttributeInvalidated(const Vector<Element*>& elements);

    int pushNodeToFrontend(ErrorString&, int documentNodeId, Node*);
    Node* nodeForId(int nodeId);
    int boundNodeId(Node*);
    void setDOMListener(DOMListener*);
    BackendNodeId backendNodeIdForNode(Node*, const String& nodeGroup);

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
    static bool isWhitespace(Node*);

    Node* assertNode(ErrorString&, int nodeId);
    Element* assertElement(ErrorString&, int nodeId);
    Document* assertDocument(ErrorString&, int nodeId);

    static Node* scriptValueAsNode(Deprecated::ScriptValue);
    static Deprecated::ScriptValue nodeAsScriptValue(JSC::ExecState*, Node*);

    // Methods called from other agents.
    InspectorPageAgent* pageAgent() { return m_pageAgent; }

private:
    void setSearchingForNode(ErrorString&, bool enabled, const Inspector::InspectorObject* highlightConfig);
    std::unique_ptr<HighlightConfig> highlightConfigFromInspectorObject(ErrorString&, const Inspector::InspectorObject* highlightInspectorObject);

    // Node-related methods.
    typedef HashMap<RefPtr<Node>, int> NodeToIdMap;
    int bind(Node*, NodeToIdMap*);
    void unbind(Node*, NodeToIdMap*);

    Node* assertEditableNode(ErrorString&, int nodeId);
    Element* assertEditableElement(ErrorString&, int nodeId);

    int pushNodePathToFrontend(Node*);
    void pushChildNodesToFrontend(int nodeId, int depth = 1);

    bool hasBreakpoint(Node*, int type);
    void updateSubtreeBreakpoints(Node* root, uint32_t rootMask, bool value);

    Ref<Inspector::Protocol::DOM::Node> buildObjectForNode(Node*, int depth, NodeToIdMap*);
    Ref<Inspector::Protocol::Array<String>> buildArrayForElementAttributes(Element*);
    Ref<Inspector::Protocol::Array<Inspector::Protocol::DOM::Node>> buildArrayForContainerChildren(Node* container, int depth, NodeToIdMap* nodesMap);
    RefPtr<Inspector::Protocol::Array<Inspector::Protocol::DOM::Node>> buildArrayForPseudoElements(const Element&, NodeToIdMap* nodesMap);
    Ref<Inspector::Protocol::DOM::EventListener> buildObjectForEventListener(const RegisteredEventListener&, const AtomicString& eventType, Node*, const String* objectGroupId);
    RefPtr<Inspector::Protocol::DOM::AccessibilityProperties> buildObjectForAccessibilityProperties(Node*);
    void processAccessibilityChildren(RefPtr<AccessibilityObject>&&, RefPtr<Inspector::Protocol::Array<int>>&&);
    
    Node* nodeForPath(const String& path);
    Node* nodeForObjectId(const String& objectId);

    void discardBindings();

    void innerHighlightQuad(std::unique_ptr<FloatQuad>, const Inspector::InspectorObject* color, const Inspector::InspectorObject* outlineColor, const bool* usePageCoordinates);

    Inspector::InjectedScriptManager& m_injectedScriptManager;
    std::unique_ptr<Inspector::DOMFrontendDispatcher> m_frontendDispatcher;
    RefPtr<Inspector::DOMBackendDispatcher> m_backendDispatcher;
    InspectorPageAgent* m_pageAgent { nullptr };

    InspectorOverlay* m_overlay { nullptr };
    DOMListener* m_domListener { nullptr };
    NodeToIdMap m_documentNodeToIdMap;
    typedef HashMap<RefPtr<Node>, BackendNodeId> NodeToBackendIdMap;
    HashMap<String, NodeToBackendIdMap> m_nodeGroupToBackendIdMap;
    // Owns node mappings for dangling nodes.
    Vector<std::unique_ptr<NodeToIdMap>> m_danglingNodeToIdMaps;
    HashMap<int, Node*> m_idToNode;
    HashMap<int, NodeToIdMap*> m_idToNodesMap;
    HashSet<int> m_childrenRequested;
    HashMap<BackendNodeId, std::pair<Node*, String>> m_backendIdToNode;
    int m_lastNodeId { 1 };
    BackendNodeId m_lastBackendNodeId { -1 };
    RefPtr<Document> m_document;
    typedef HashMap<String, Vector<RefPtr<Node>>> SearchResults;
    SearchResults m_searchResults;
    std::unique_ptr<RevalidateStyleAttributeTask> m_revalidateStyleAttrTask;
    RefPtr<Node> m_nodeToFocus;
    bool m_searchingForNode { false };
    std::unique_ptr<HighlightConfig> m_inspectModeHighlightConfig;
    std::unique_ptr<InspectorHistory> m_history;
    std::unique_ptr<DOMEditor> m_domEditor;
    bool m_suppressAttributeModifiedEvent { false };
    bool m_documentRequested { false };
};

} // namespace WebCore

#endif // !defined(InspectorDOMAgent_h)
