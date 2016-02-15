/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011, 2013, 2015 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010, 2011, 2012, 2013 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "EventDispatcher.h"

#include "EventContext.h"
#include "FocusEvent.h"
#include "FrameView.h"
#include "HTMLInputElement.h"
#include "HTMLMediaElement.h"
#include "HTMLSlotElement.h"
#include "MouseEvent.h"
#include "PseudoElement.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#include "TouchEvent.h"
#include <wtf/NeverDestroyed.h>

namespace WebCore {

class WindowEventContext {
public:
    WindowEventContext(PassRefPtr<Node>, const EventContext*);

    DOMWindow* window() const { return m_window.get(); }
    EventTarget* target() const { return m_target.get(); }
    bool handleLocalEvents(Event&);

private:
    RefPtr<DOMWindow> m_window;
    RefPtr<EventTarget> m_target;
};

WindowEventContext::WindowEventContext(PassRefPtr<Node> node, const EventContext* topEventContext)
{
    Node* topLevelContainer = topEventContext ? topEventContext->node() : node.get();
    if (!is<Document>(*topLevelContainer))
        return;

    m_window = downcast<Document>(*topLevelContainer).domWindow();
    m_target = topEventContext ? topEventContext->target() : node.get();
}

bool WindowEventContext::handleLocalEvents(Event& event)
{
    if (!m_window)
        return false;

    event.setTarget(m_target.copyRef());
    event.setCurrentTarget(m_window.get());
    m_window->fireEventListeners(event);
    return true;
}

class EventPath {
public:
    EventPath(Node& origin, Event&);

    bool isEmpty() const { return m_path.isEmpty(); }
    size_t size() const { return m_path.size(); }
    const EventContext& contextAt(size_t i) const { return *m_path[i]; }
    EventContext& contextAt(size_t i) { return *m_path[i]; }

#if ENABLE(TOUCH_EVENTS)
    void retargetTouchLists(const TouchEvent&);
#endif
    void setRelatedTarget(Node& origin, EventTarget&);

    bool hasEventListeners(const AtomicString& eventType) const;

    EventContext* lastContextIfExists() { return m_path.isEmpty() ? nullptr : m_path.last().get(); }

private:
#if ENABLE(TOUCH_EVENTS)
    void retargetTouch(TouchEventContext::TouchListType, const Touch&);
#endif

    Event& m_event;
    Vector<std::unique_ptr<EventContext>, 32> m_path;
};

inline EventTarget* eventTargetRespectingTargetRules(Node& referenceNode)
{
    if (is<PseudoElement>(referenceNode))
        return downcast<PseudoElement>(referenceNode).hostElement();

    // Events sent to elements inside an SVG use element's shadow tree go to the use element.
    if (is<SVGElement>(referenceNode)) {
        if (auto* useElement = downcast<SVGElement>(referenceNode).correspondingUseElement())
            return useElement;
    }

    return &referenceNode;
}

void EventDispatcher::dispatchScopedEvent(Node& node, Event& event)
{
    // We need to set the target here because it can go away by the time we actually fire the event.
    event.setTarget(eventTargetRespectingTargetRules(node));
    ScopedEventQueue::singleton().enqueueEvent(event);
}

static void callDefaultEventHandlersInTheBubblingOrder(Event& event, const EventPath& path)
{
    if (path.isEmpty())
        return;

    // Non-bubbling events call only one default event handler, the one for the target.
    path.contextAt(0).node()->defaultEventHandler(&event);
    ASSERT(!event.defaultPrevented());

    if (event.defaultHandled() || !event.bubbles())
        return;

    size_t size = path.size();
    for (size_t i = 1; i < size; ++i) {
        path.contextAt(i).node()->defaultEventHandler(&event);
        ASSERT(!event.defaultPrevented());
        if (event.defaultHandled())
            return;
    }
}

static void dispatchEventInDOM(Event& event, const EventPath& path, WindowEventContext& windowEventContext)
{
    // Trigger capturing event handlers, starting at the top and working our way down.
    event.setEventPhase(Event::CAPTURING_PHASE);

    // We don't dispatch load events to the window. This quirk was originally
    // added because Mozilla doesn't propagate load events to the window object.
    bool shouldFireEventAtWindow = event.type() != eventNames().loadEvent;
    if (shouldFireEventAtWindow && windowEventContext.handleLocalEvents(event) && event.propagationStopped())
        return;

    for (size_t i = path.size() - 1; i > 0; --i) {
        const EventContext& eventContext = path.contextAt(i);
        if (eventContext.currentTargetSameAsTarget())
            continue;
        eventContext.handleLocalEvents(event);
        if (event.propagationStopped())
            return;
    }

    event.setEventPhase(Event::AT_TARGET);
    path.contextAt(0).handleLocalEvents(event);
    if (event.propagationStopped())
        return;

    // Trigger bubbling event handlers, starting at the bottom and working our way up.
    size_t size = path.size();
    for (size_t i = 1; i < size; ++i) {
        const EventContext& eventContext = path.contextAt(i);
        if (eventContext.currentTargetSameAsTarget())
            event.setEventPhase(Event::AT_TARGET);
        else if (event.bubbles() && !event.cancelBubble())
            event.setEventPhase(Event::BUBBLING_PHASE);
        else
            continue;
        eventContext.handleLocalEvents(event);
        if (event.propagationStopped())
            return;
    }
    if (event.bubbles() && !event.cancelBubble()) {
        event.setEventPhase(Event::BUBBLING_PHASE);
        if (shouldFireEventAtWindow)
            windowEventContext.handleLocalEvents(event);
    }
}

bool EventDispatcher::dispatchEvent(Node* origin, Event& event)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());
    ASSERT(origin);
    RefPtr<Node> node(origin);
    RefPtr<FrameView> view = node->document().view();
    EventPath eventPath(*node, event);

    if (EventTarget* relatedTarget = event.relatedTarget())
        eventPath.setRelatedTarget(*node, *relatedTarget);
#if ENABLE(TOUCH_EVENTS)
    if (is<TouchEvent>(event))
        eventPath.retargetTouchLists(downcast<TouchEvent>(event));
#endif

    ChildNodesLazySnapshot::takeChildNodesLazySnapshot();

    EventTarget* target = eventTargetRespectingTargetRules(*node);
    event.setTarget(target);
    if (!event.target())
        return true;

    ASSERT_WITH_SECURITY_IMPLICATION(!NoEventDispatchAssertion::isEventDispatchForbidden());

    WindowEventContext windowEventContext(node.get(), eventPath.lastContextIfExists());

    InputElementClickState clickHandlingState;
    if (is<HTMLInputElement>(*node))
        downcast<HTMLInputElement>(*node).willDispatchEvent(event, clickHandlingState);

    if (!event.propagationStopped() && !eventPath.isEmpty())
        dispatchEventInDOM(event, eventPath, windowEventContext);

    event.setTarget(eventTargetRespectingTargetRules(*node));
    event.setCurrentTarget(nullptr);
    event.setEventPhase(0);

    if (clickHandlingState.stateful)
        downcast<HTMLInputElement>(*node).didDispatchClickEvent(event, clickHandlingState);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event.defaultPrevented() && !event.defaultHandled())
        callDefaultEventHandlersInTheBubblingOrder(event, eventPath);

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    event.setTarget(windowEventContext.target());
    event.setCurrentTarget(nullptr);

    return !event.defaultPrevented();
}

static inline bool shouldEventCrossShadowBoundary(Event& event, ShadowRoot& shadowRoot, EventTarget& target)
{
    Node* targetNode = target.toNode();
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO)
    // Video-only full screen is a mode where we use the shadow DOM as an implementation
    // detail that should not be detectable by the web content.
    if (targetNode) {
        if (Element* element = targetNode->document().webkitCurrentFullScreenElement()) {
            // FIXME: We assume that if the full screen element is a media element that it's
            // the video-only full screen. Both here and elsewhere. But that is probably wrong.
            if (element->isMediaElement() && shadowRoot.host() == element)
                return false;
        }
    }
#endif

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    const AtomicString& eventType = event.type();
    bool targetIsInShadowRoot = targetNode && &targetNode->treeScope().rootNode() == &shadowRoot;
    return !targetIsInShadowRoot
        || !(eventType == eventNames().abortEvent
            || eventType == eventNames().changeEvent
            || eventType == eventNames().errorEvent
            || eventType == eventNames().loadEvent
            || eventType == eventNames().resetEvent
            || eventType == eventNames().resizeEvent
            || eventType == eventNames().scrollEvent
            || eventType == eventNames().selectEvent
            || eventType == eventNames().selectstartEvent);
}

static Node* nodeOrHostIfPseudoElement(Node* node)
{
    return is<PseudoElement>(*node) ? downcast<PseudoElement>(*node).hostElement() : node;
}

EventPath::EventPath(Node& originalTarget, Event& event)
    : m_event(event)
{
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
    Vector<EventTarget*, 16> targetStack;
#endif

    bool isMouseOrFocusEvent = event.isMouseEvent() || event.isFocusEvent();
#if ENABLE(TOUCH_EVENTS)
    bool isTouchEvent = event.isTouchEvent();
#endif
    EventTarget* target = nullptr;

    Node* node = nodeOrHostIfPseudoElement(&originalTarget);
    while (node) {
        if (!target)
            target = eventTargetRespectingTargetRules(*node);
        ContainerNode* parent;
        for (; node; node = parent) {
            EventTarget* currentTarget = eventTargetRespectingTargetRules(*node);
            if (isMouseOrFocusEvent)
                m_path.append(std::make_unique<MouseOrFocusEventContext>(node, currentTarget, target));
#if ENABLE(TOUCH_EVENTS)
            else if (isTouchEvent)
                m_path.append(std::make_unique<TouchEventContext>(node, currentTarget, target));
#endif
            else
                m_path.append(std::make_unique<EventContext>(node, currentTarget, target));
            if (is<ShadowRoot>(*node))
                break;
            parent = node->parentNode();
            if (!parent)
                return;
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
            if (ShadowRoot* shadowRootOfParent = parent->shadowRoot()) {
                if (auto* assignedSlot = shadowRootOfParent->findAssignedSlot(*node)) {
                    // node is assigned to a slot. Continue dispatching the event at this slot.
                    targetStack.append(target);
                    parent = assignedSlot;
                    target = assignedSlot;
                }
            }
#endif
            node = parent;
        }

        ShadowRoot& shadowRoot = downcast<ShadowRoot>(*node);
        // At a shadow root. Continue dispatching the event at the shadow host.
#if ENABLE(SHADOW_DOM) || ENABLE(DETAILS_ELEMENT)
        if (!targetStack.isEmpty()) {
            // Move target back to a descendant of the shadow host if the event did not originate in this shadow tree or its inner shadow trees.
            target = targetStack.last();
            targetStack.removeLast();
            ASSERT(shadowRoot.host()->contains(target->toNode()));
        } else
#endif
            target = nullptr;

        if (!shouldEventCrossShadowBoundary(event, shadowRoot, originalTarget))
            return;
        node = shadowRoot.host();
    }
}

class RelatedNodeRetargeter {
public:
    RelatedNodeRetargeter(Node& relatedNode, TreeScope& targetTreeScope)
        : m_relatedNode(relatedNode)
        , m_retargetedRelatedNode(&relatedNode)
    {
        TreeScope* currentTreeScope = &m_relatedNode.treeScope();
        if (LIKELY(currentTreeScope == &targetTreeScope))
            return;

        if (&currentTreeScope->documentScope() != &targetTreeScope.documentScope()) {
            m_hasDifferentTreeRoot = true;
            m_retargetedRelatedNode = nullptr;
            return;
        }
        if (relatedNode.inDocument() != targetTreeScope.rootNode().inDocument()) {
            m_hasDifferentTreeRoot = true;
            while (m_retargetedRelatedNode->isInShadowTree())
                m_retargetedRelatedNode = downcast<ShadowRoot>(m_retargetedRelatedNode->treeScope().rootNode()).host();
            return;
        }

        collectTreeScopes();

        // FIXME: We should collect this while constructing the event path.
        Vector<TreeScope*, 8> targetTreeScopeAncestors;
        for (TreeScope* currentTreeScope = &targetTreeScope; currentTreeScope; currentTreeScope = currentTreeScope->parentTreeScope())
            targetTreeScopeAncestors.append(currentTreeScope);
        ASSERT_WITH_SECURITY_IMPLICATION(!targetTreeScopeAncestors.isEmpty());

        unsigned i = m_ancestorTreeScopes.size();
        unsigned j = targetTreeScopeAncestors.size();
        ASSERT_WITH_SECURITY_IMPLICATION(m_ancestorTreeScopes.last() == targetTreeScopeAncestors.last());
        while (m_ancestorTreeScopes[i - 1] == targetTreeScopeAncestors[j - 1]) {
            i--;
            j--;
            if (!i || !j)
                break;
        }

        m_lowestCommonAncestorIndex = i;
        m_retargetedRelatedNode = nodeInLowestCommonAncestor();
    }

    Node* currentNode(TreeScope& currentTreeScope)
    {
        checkConsistency(currentTreeScope);
        return m_retargetedRelatedNode;
    }

    void moveToNewTreeScope(TreeScope* previousTreeScope, TreeScope& newTreeScope)
    {
        if (m_hasDifferentTreeRoot)
            return;

        auto& currentRelatedNodeScope = m_retargetedRelatedNode->treeScope();
        if (previousTreeScope != &currentRelatedNodeScope) {
            // currentRelatedNode is still outside our shadow tree. New tree scope may contain currentRelatedNode
            // but there is no need to re-target it. Moving into a slot (thereby a deeper shadow tree) doesn't matter.
            return;
        }

        bool enteredSlot = newTreeScope.parentTreeScope() == previousTreeScope;
        if (enteredSlot) {
            if (m_lowestCommonAncestorIndex) {
                if (m_ancestorTreeScopes.isEmpty())
                    collectTreeScopes();
                bool relatedNodeIsInSlot = m_ancestorTreeScopes[m_lowestCommonAncestorIndex - 1] == &newTreeScope;
                if (relatedNodeIsInSlot) {
                    m_lowestCommonAncestorIndex--;
                    m_retargetedRelatedNode = nodeInLowestCommonAncestor();
                    ASSERT(&newTreeScope == &m_retargetedRelatedNode->treeScope());
                }
            } else
                ASSERT(m_retargetedRelatedNode == &m_relatedNode);
        } else {
            ASSERT(previousTreeScope->parentTreeScope() == &newTreeScope);
            m_lowestCommonAncestorIndex++;
            ASSERT_WITH_SECURITY_IMPLICATION(m_ancestorTreeScopes.isEmpty() || m_lowestCommonAncestorIndex < m_ancestorTreeScopes.size());
            m_retargetedRelatedNode = downcast<ShadowRoot>(currentRelatedNodeScope.rootNode()).host();
            ASSERT(&newTreeScope == &m_retargetedRelatedNode->treeScope());
        }
    }

    void checkConsistency(TreeScope& currentTreeScope)
    {
#if !ASSERT_DISABLED
        for (auto* relatedNodeScope = &m_relatedNode.treeScope(); relatedNodeScope; relatedNodeScope = relatedNodeScope->parentTreeScope()) {
            for (auto* targetScope = &currentTreeScope; targetScope; targetScope = targetScope->parentTreeScope()) {
                if (targetScope == relatedNodeScope) {
                    ASSERT(&m_retargetedRelatedNode->treeScope() == relatedNodeScope);
                    return;
                }
            }
        }
        ASSERT(!m_retargetedRelatedNode);
#else
        UNUSED_PARAM(currentTreeScope);
#endif
    }

private:
    Node* nodeInLowestCommonAncestor()
    {
        if (!m_lowestCommonAncestorIndex)
            return &m_relatedNode;
        auto& rootNode = m_ancestorTreeScopes[m_lowestCommonAncestorIndex - 1]->rootNode();
        return downcast<ShadowRoot>(rootNode).host();
    }

    void collectTreeScopes()
    {
        ASSERT(m_ancestorTreeScopes.isEmpty());
        for (TreeScope* currentTreeScope = &m_relatedNode.treeScope(); currentTreeScope; currentTreeScope = currentTreeScope->parentTreeScope())
            m_ancestorTreeScopes.append(currentTreeScope);
        ASSERT_WITH_SECURITY_IMPLICATION(!m_ancestorTreeScopes.isEmpty());
    }

    Node& m_relatedNode;
    Node* m_retargetedRelatedNode;
    Vector<TreeScope*, 8> m_ancestorTreeScopes;
    unsigned m_lowestCommonAncestorIndex { 0 };
    bool m_hasDifferentTreeRoot { false };
};

void EventPath::setRelatedTarget(Node& origin, EventTarget& relatedTarget)
{
    Node* relatedNode = relatedTarget.toNode();
    if (!relatedNode || m_path.isEmpty())
        return;

    RelatedNodeRetargeter retargeter(*relatedNode, downcast<MouseOrFocusEventContext>(*m_path[0]).node()->treeScope());

    bool originIsRelatedTarget = &origin == relatedNode;
    // FIXME: We should add a new flag on Event instead.
    bool shouldTrimEventPath = m_event.type() == eventNames().mouseoverEvent
        || m_event.type() == eventNames().mousemoveEvent
        || m_event.type() == eventNames().mouseoutEvent;
    Node& rootNodeInOriginTreeScope = origin.treeScope().rootNode();
    TreeScope* previousTreeScope = nullptr;
    size_t originalEventPathSize = m_path.size();
    for (unsigned contextIndex = 0; contextIndex < originalEventPathSize; contextIndex++) {
        auto& context = downcast<MouseOrFocusEventContext>(*m_path[contextIndex]);

        TreeScope& currentTreeScope = context.node()->treeScope();
        if (UNLIKELY(previousTreeScope && &currentTreeScope != previousTreeScope))
            retargeter.moveToNewTreeScope(previousTreeScope, currentTreeScope);

        Node* currentRelatedNode = retargeter.currentNode(currentTreeScope);
        if (UNLIKELY(shouldTrimEventPath && !originIsRelatedTarget && context.target() == currentRelatedNode)) {
            m_path.shrink(contextIndex);
            break;
        }

        context.setRelatedTarget(currentRelatedNode);

        if (UNLIKELY(shouldTrimEventPath && originIsRelatedTarget && context.node() == &rootNodeInOriginTreeScope)) {
            m_path.shrink(contextIndex + 1);
            break;
        }

        previousTreeScope = &currentTreeScope;
    }
}

#if ENABLE(TOUCH_EVENTS)
void EventPath::retargetTouch(TouchEventContext::TouchListType touchListType, const Touch& touch)
{
    EventTarget* eventTarget = touch.target();
    if (!eventTarget)
        return;

    Node* targetNode = eventTarget->toNode();
    if (!targetNode)
        return;

    RelatedNodeRetargeter retargeter(*targetNode, m_path[0]->node()->treeScope());
    TreeScope* previousTreeScope = nullptr;
    for (auto& context : m_path) {
        TreeScope& currentTreeScope = context->node()->treeScope();
        if (UNLIKELY(previousTreeScope && &currentTreeScope != previousTreeScope))
            retargeter.moveToNewTreeScope(previousTreeScope, currentTreeScope);

        Node* currentRelatedNode = retargeter.currentNode(currentTreeScope);
        downcast<TouchEventContext>(*context).touchList(touchListType)->append(touch.cloneWithNewTarget(currentRelatedNode));

        previousTreeScope = &currentTreeScope;
    }
}

void EventPath::retargetTouchLists(const TouchEvent& touchEvent)
{
    if (touchEvent.touches()) {
        for (size_t i = 0; i < touchEvent.touches()->length(); ++i)
            retargetTouch(TouchEventContext::Touches, *touchEvent.touches()->item(i));
    }

    if (touchEvent.targetTouches()) {
        for (size_t i = 0; i < touchEvent.targetTouches()->length(); ++i)
            retargetTouch(TouchEventContext::TargetTouches, *touchEvent.targetTouches()->item(i));
    }

    if (touchEvent.changedTouches()) {
        for (size_t i = 0; i < touchEvent.changedTouches()->length(); ++i)
            retargetTouch(TouchEventContext::ChangedTouches, *touchEvent.changedTouches()->item(i));
    }
}
#endif

bool EventPath::hasEventListeners(const AtomicString& eventType) const
{
    for (auto& eventPath : m_path) {
        if (eventPath->node()->hasEventListeners(eventType))
            return true;
    }

    return false;
}

}
