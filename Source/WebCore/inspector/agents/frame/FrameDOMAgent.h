/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "EventTarget.h"
#include "InspectorWebAgentBase.h"
#include "Timer.h"
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/CheckedPtr.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashMap.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class CharacterData;
class DOMEditor;
class Document;
class Element;
class EventListener;
class EventTarget;
class InspectorHistory;
class LocalFrame;
class Node;
class PseudoElement;
class RegisteredEventListener;
class ShadowRoot;

// FrameDOMAgent is the per-frame DOM agent for Site Isolation.
class FrameDOMAgent final : public InspectorAgentBase, public Inspector::DOMBackendDispatcherHandler, public CanMakeCheckedPtr<FrameDOMAgent> {
    WTF_MAKE_NONCOPYABLE(FrameDOMAgent);
    WTF_MAKE_TZONE_ALLOCATED(FrameDOMAgent);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FrameDOMAgent);
public:
    FrameDOMAgent(FrameAgentContext&);
    ~FrameDOMAgent();

    // InspectorAgentBase
    void didCreateFrontendAndBackend() override;
    void willDestroyFrontendAndBackend(Inspector::DisconnectReason) override;

    // DOMBackendDispatcherHandler
    Inspector::CommandResult<Ref<Inspector::Protocol::DOM::Node>> getDocument() override;
    Inspector::CommandResult<void> requestChildNodes(int nodeId, std::optional<int>&& depth) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<String>>> getAttributes(int nodeId) override;
    Inspector::CommandResult<std::optional<int>> requestAssignedSlot(int nodeId) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<int>>> requestAssignedNodes(int slotElementId) override;

    Inspector::CommandResult<std::optional<int>> querySelector(int nodeId, const String& selector) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<int>>> querySelectorAll(int nodeId, const String& selector) override;
    Inspector::CommandResult<int> setNodeName(int nodeId, const String& name) override;
    Inspector::CommandResult<void> setNodeValue(int nodeId, const String& value) override;
    Inspector::CommandResult<void> removeNode(int nodeId) override;
    Inspector::CommandResult<void> setAttributeValue(int nodeId, const String& name, const String& value) override;
    Inspector::CommandResult<void> setAttributesAsText(int nodeId, const String& text, const String& name) override;
    Inspector::CommandResult<void> removeAttribute(int nodeId, const String& name) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<String>>> getSupportedEventNames() override;
#if ENABLE(INSPECTOR_ALTERNATE_DISPATCHERS)
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::DataBinding>>> getDataBindingsForNode(int nodeId) override;
    Inspector::CommandResult<String> getAssociatedDataForNode(int nodeId) override;
#endif
    Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::DOM::EventListener>>> getEventListenersForNode(int nodeId, std::optional<bool>&& includeAncestors) override;
    Inspector::CommandResult<void> setEventListenerDisabled(int eventListenerId, bool disabled) override;
    Inspector::CommandResult<void> setBreakpointForEventListener(int eventListenerId, RefPtr<JSON::Object>&& options) override;
    Inspector::CommandResult<void> removeBreakpointForEventListener(int eventListenerId) override;
    Inspector::CommandResult<Ref<Inspector::Protocol::DOM::AccessibilityProperties>> getAccessibilityPropertiesForNode(int nodeId) override;
    Inspector::CommandResult<String> getOuterHTML(int nodeId) override;
    Inspector::CommandResult<void> setOuterHTML(int nodeId, const String& outerHTML) override;
    Inspector::CommandResult<void> insertAdjacentHTML(int nodeId, const String& position, const String& html) override;
    Inspector::CommandResultOf<String, int> performSearch(const String& query, RefPtr<JSON::Array>&& nodeIds, std::optional<bool>&& caseSensitive) override;
    Inspector::CommandResult<Ref<JSON::ArrayOf<int>>> getSearchResults(const String& searchId, int fromIndex, int toIndex) override;
    Inspector::CommandResult<void> discardSearchResults(const String& searchId) override;
    Inspector::CommandResult<int> requestNode(const String& objectId) override;
#if PLATFORM(IOS_FAMILY)
    Inspector::CommandResult<void> setInspectModeEnabled(bool enabled, RefPtr<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig) override;
#else
    Inspector::CommandResult<void> setInspectModeEnabled(bool enabled, RefPtr<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig, std::optional<bool>&& showRulers) override;
#endif
    Inspector::CommandResult<void> highlightRect(int x, int y, int width, int height, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, std::optional<bool>&& usePageCoordinates) override;
    Inspector::CommandResult<void> highlightQuad(Ref<JSON::Array>&& quad, RefPtr<JSON::Object>&& color, RefPtr<JSON::Object>&& outlineColor, std::optional<bool>&& usePageCoordinates) override;
#if PLATFORM(IOS_FAMILY)
    Inspector::CommandResult<void> highlightSelector(const String& selectorString, const String& frameId, Ref<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig) override;
    Inspector::CommandResult<void> highlightNode(std::optional<int>&& nodeId, const String& objectId, Ref<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig) override;
    Inspector::CommandResult<void> highlightNodeList(Ref<JSON::Array>&& nodeIds, Ref<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig) override;
#else
    Inspector::CommandResult<void> highlightSelector(const String& selectorString, const String& frameId, Ref<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig, std::optional<bool>&& showRulers) override;
    Inspector::CommandResult<void> highlightNode(std::optional<int>&& nodeId, const String& objectId, Ref<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig, std::optional<bool>&& showRulers) override;
    Inspector::CommandResult<void> highlightNodeList(Ref<JSON::Array>&& nodeIds, Ref<JSON::Object>&& highlightConfig, RefPtr<JSON::Object>&& gridOverlayConfig, RefPtr<JSON::Object>&& flexOverlayConfig, std::optional<bool>&& showRulers) override;
#endif
    Inspector::CommandResult<void> hideHighlight() override;
    Inspector::CommandResult<void> highlightFrame(const String& frameId, RefPtr<JSON::Object>&& contentColor, RefPtr<JSON::Object>&& contentOutlineColor) override;
    Inspector::CommandResult<void> showGridOverlay(int nodeId, Ref<JSON::Object>&& gridOverlayConfig) override;
    Inspector::CommandResult<void> hideGridOverlay(std::optional<int>&& nodeId) override;
    Inspector::CommandResult<void> showFlexOverlay(int nodeId, Ref<JSON::Object>&& flexOverlayConfig) override;
    Inspector::CommandResult<void> hideFlexOverlay(std::optional<int>&& nodeId) override;
    Inspector::CommandResult<int> pushNodeByPathToFrontend(const String& path) override;
    Inspector::CommandResult<Ref<Inspector::Protocol::Runtime::RemoteObject>> resolveNode(int nodeId, const String& objectGroup) override;
    Inspector::CommandResult<int> moveTo(int nodeId, int targetNodeId, std::optional<int>&& insertBeforeNodeId) override;
    Inspector::CommandResult<void> undo() override;
    Inspector::CommandResult<void> redo() override;
    Inspector::CommandResult<void> markUndoableState() override;
    Inspector::CommandResult<void> focus(int nodeId) override;
    Inspector::CommandResult<void> setInspectedNode(int nodeId) override;
    Inspector::CommandResult<void> setAllowEditingUserAgentShadowTrees(bool) override;
    Inspector::CommandResult<Ref<Inspector::Protocol::DOM::MediaStats>> getMediaStats(int nodeId) override;

    // InspectorInstrumentation hooks
    void didInsertDOMNode(Node&);
    void didRemoveDOMNode(Node&);
    void willDestroyDOMNode(Node&);
    void willModifyDOMAttr(Element&, const AtomString& oldValue, const AtomString& newValue);
    void didModifyDOMAttr(Element&, const AtomString& name, const AtomString& value);
    void didRemoveDOMAttr(Element&, const AtomString& name);
    void characterDataModified(CharacterData&);
    void didInvalidateStyleAttr(Element&);
    void didPushShadowRoot(Element& host, ShadowRoot&);
    void willPopShadowRoot(Element& host, ShadowRoot&);
    void didChangeCustomElementState(Element&);
    void pseudoElementCreated(PseudoElement&);
    void pseudoElementDestroyed(PseudoElement&);
    void frameDocumentUpdated(LocalFrame&);
    bool isEventListenerDisabled(EventTarget&, const AtomString& eventType, EventListener&, bool capture);

    // Public accessors
    Node* nodeForId(Inspector::Protocol::DOM::NodeId);
    Inspector::Protocol::DOM::NodeId boundNodeId(const Node*);
    Inspector::Protocol::DOM::NodeId pushNodePathToFrontend(Node*);
    InspectorHistory* history() LIFETIME_BOUND { return m_history.get(); }

private:
    Inspector::Protocol::DOM::NodeId bind(Node&);
    void unbind(Node&);
    void discardBindings();

    RefPtr<Node> assertNode(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);
    RefPtr<Element> assertElement(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);
    RefPtr<Node> assertEditableNode(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);
    RefPtr<Element> assertEditableElement(Inspector::Protocol::ErrorString&, Inspector::Protocol::DOM::NodeId);

    Ref<Inspector::Protocol::DOM::Node> buildObjectForNode(Node*, int depth);
    Ref<JSON::ArrayOf<String>> buildArrayForElementAttributes(Element*);
    Ref<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> buildArrayForContainerChildren(Node* container, int depth);
    RefPtr<JSON::ArrayOf<Inspector::Protocol::DOM::Node>> buildArrayForPseudoElements(const Element&);
    void pushChildNodesToFrontend(Inspector::Protocol::DOM::NodeId, int depth = 1);
    Inspector::Protocol::DOM::NodeId pushNodePathToFrontend(Inspector::Protocol::ErrorString&, Node*);

    void setDocument(Document*);
    void reset();
    void destroyedNodesTimerFired();

    RefPtr<Node> nodeForPath(const String& path);

    struct InspectorEventListener {
        Inspector::Protocol::DOM::EventListenerId identifier { 1 };
        RefPtr<EventTarget> eventTarget;
        RefPtr<EventListener> eventListener;
        AtomString eventType;
        bool useCapture { false };
        bool disabled { false };

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

    Ref<Inspector::Protocol::DOM::EventListener> buildObjectForEventListener(const Ref<RegisteredEventListener>&, Inspector::Protocol::DOM::EventListenerId, EventTarget&, const AtomString& eventType, bool disabled);

    const UniqueRef<Inspector::DOMFrontendDispatcher> m_frontendDispatcher;
    const Ref<Inspector::DOMBackendDispatcher> m_backendDispatcher;
    WeakRef<InstrumentingAgents> m_instrumentingAgents;
    WeakRef<LocalFrame> m_inspectedFrame;

    WeakHashMap<Node, Inspector::Protocol::DOM::NodeId, WeakPtrImplWithEventTargetData> m_nodeToId;
    HashMap<Inspector::Protocol::DOM::NodeId, WeakPtr<Node, WeakPtrImplWithEventTargetData>> m_idToNode;
    HashSet<Inspector::Protocol::DOM::NodeId> m_childrenRequested;
    Inspector::Protocol::DOM::NodeId m_lastNodeId { 1 };
    RefPtr<Document> m_document;

    Vector<Inspector::Protocol::DOM::NodeId> m_destroyedDetachedNodeIdentifiers;
    Vector<std::pair<Inspector::Protocol::DOM::NodeId, Inspector::Protocol::DOM::NodeId>> m_destroyedAttachedNodeIdentifiers;
    Timer m_destroyedNodesTimer;

    using SearchResults = HashMap<String, Vector<RefPtr<Node>>>;
    SearchResults m_searchResults;

    HashMap<Inspector::Protocol::DOM::EventListenerId, InspectorEventListener> m_eventListenerEntries;
    Inspector::Protocol::DOM::EventListenerId m_lastEventListenerId { 1 };

    std::unique_ptr<InspectorHistory> m_history;
    std::unique_ptr<DOMEditor> m_domEditor;

    bool m_suppressAttributeModifiedEvent { false };
    bool m_documentRequested { false };
    bool m_allowEditingUserAgentShadowTrees { false };
};

} // namespace WebCore
