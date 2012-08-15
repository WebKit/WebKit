/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2011 Google Inc. All rights reserved.
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

#include "ComposedShadowTreeWalker.h"
#include "ContainerNode.h"
#include "ElementShadow.h"
#include "EventContext.h"
#include "EventDispatchMediator.h"
#include "FrameView.h"
#include "HTMLMediaElement.h"
#include "InsertionPoint.h"
#include "InspectorInstrumentation.h"
#include "MouseEvent.h"
#include "ScopedEventQueue.h"
#include "ShadowRoot.h"
#include "WindowEventContext.h"
#include <wtf/RefPtr.h>
#include <wtf/UnusedParam.h>

#if ENABLE(SVG)
#include "SVGElementInstance.h"
#include "SVGNames.h"
#include "SVGUseElement.h"
#endif

namespace WebCore {

static HashSet<Node*>* gNodesDispatchingSimulatedClicks = 0;

EventRelatedTargetAdjuster::EventRelatedTargetAdjuster(PassRefPtr<Node> node, PassRefPtr<Node> relatedTarget)
    : m_node(node)
    , m_relatedTarget(relatedTarget)
{
    ASSERT(m_node);
    ASSERT(m_relatedTarget);
}

void EventRelatedTargetAdjuster::adjust(Vector<EventContext>& ancestors)
{
    Vector<EventTarget*> relatedTargetStack;
    TreeScope* lastTreeScope = 0;
    Node* lastNode = 0;
    for (ComposedShadowTreeParentWalker walker(m_relatedTarget.get()); walker.get(); walker.parentIncludingInsertionPointAndShadowRoot()) {
        Node* node = walker.get();
        if (relatedTargetStack.isEmpty())
            relatedTargetStack.append(node);
        else if (isInsertionPoint(node) && toInsertionPoint(node)->contains(lastNode))
            relatedTargetStack.append(relatedTargetStack.last());
        TreeScope* scope = node->treeScope();
        // Skips adding a node to the map if treeScope does not change. Just for the performance optimization.
        if (scope != lastTreeScope)
            m_relatedTargetMap.add(scope, relatedTargetStack.last());
        lastTreeScope = scope;
        lastNode = node;
        if (node->isShadowRoot()) {
            ASSERT(!relatedTargetStack.isEmpty());
            relatedTargetStack.removeLast();
        }
    }

    lastTreeScope = 0;
    EventTarget* adjustedRelatedTarget = 0;
    for (Vector<EventContext>::iterator iter = ancestors.begin(); iter < ancestors.end(); ++iter) {
        TreeScope* scope = iter->node()->treeScope();
        if (scope == lastTreeScope) {
            // Re-use the previous adjustedRelatedTarget if treeScope does not change. Just for the performance optimization.
            iter->setRelatedTarget(adjustedRelatedTarget);
        } else {
            adjustedRelatedTarget = findRelatedTarget(scope);
            iter->setRelatedTarget(adjustedRelatedTarget);
        }
        lastTreeScope = scope;
        if (iter->target() == adjustedRelatedTarget) {
            // Event dispatching should be stopped here.
            ancestors.shrink(iter - ancestors.begin());
            break;
        }
    }
}

EventTarget* EventRelatedTargetAdjuster::findRelatedTarget(TreeScope* scope)
{
    Vector<TreeScope*> parentTreeScopes;
    EventTarget* relatedTarget = 0;
    while (scope) {
        parentTreeScopes.append(scope);
        RelatedTargetMap::const_iterator found = m_relatedTargetMap.find(scope);
        if (found != m_relatedTargetMap.end()) {
            relatedTarget = found->second;
            break;
        }
        scope = scope->parentTreeScope();
    }
    for (Vector<TreeScope*>::iterator iter = parentTreeScopes.begin(); iter < parentTreeScopes.end(); ++iter)
      m_relatedTargetMap.add(*iter, relatedTarget);
    return relatedTarget;
}

bool EventDispatcher::dispatchEvent(Node* node, PassRefPtr<EventDispatchMediator> mediator)
{
    ASSERT(!eventDispatchForbidden());

    EventDispatcher dispatcher(node);
    return mediator->dispatchEvent(&dispatcher);
}

inline static EventTarget* eventTargetRespectingSVGTargetRules(Node* referenceNode)
{
    ASSERT(referenceNode);

#if ENABLE(SVG)
    if (!referenceNode->isSVGElement() || !referenceNode->isInShadowTree())
        return referenceNode;

    // Spec: The event handling for the non-exposed tree works as if the referenced element had been textually included
    // as a deeply cloned child of the 'use' element, except that events are dispatched to the SVGElementInstance objects
    Element* shadowHostElement = toShadowRoot(referenceNode->treeScope()->rootNode())->host();
    // At this time, SVG nodes are not supported in non-<use> shadow trees.
    if (!shadowHostElement || !shadowHostElement->hasTagName(SVGNames::useTag))
        return referenceNode;
    SVGUseElement* useElement = static_cast<SVGUseElement*>(shadowHostElement);
    if (SVGElementInstance* instance = useElement->instanceForShadowTreeElement(referenceNode))
        return instance;
#endif

    return referenceNode;
}

void EventDispatcher::adjustRelatedTarget(Event* event, PassRefPtr<EventTarget> prpRelatedTarget)
{
    if (!prpRelatedTarget)
        return;
    RefPtr<Node> relatedTarget = prpRelatedTarget->toNode();
    if (!relatedTarget)
        return;
    if (!m_node.get())
        return;
    ensureEventAncestors(event);
    EventRelatedTargetAdjuster(m_node, relatedTarget.release()).adjust(m_ancestors);
}

EventDispatcher::EventDispatcher(Node* node)
    : m_node(node)
    , m_ancestorsInitialized(false)
#ifndef NDEBUG
    , m_eventDispatched(false)
#endif
{
    ASSERT(node);
    m_view = node->document()->view();
}

void EventDispatcher::ensureEventAncestors(Event* event)
{
    if (m_ancestorsInitialized)
        return;
    m_ancestorsInitialized = true;
    bool inDocument = m_node->inDocument();
    bool isSVGElement = m_node->isSVGElement();
    Vector<EventTarget*> targetStack;
    Node* last = 0;
    for (ComposedShadowTreeParentWalker walker(m_node.get()); walker.get(); walker.parentIncludingInsertionPointAndShadowRoot()) {
        Node* node = walker.get();
        if (targetStack.isEmpty())
            targetStack.append(eventTargetRespectingSVGTargetRules(node));
        else if (isInsertionPoint(node) && toInsertionPoint(node)->contains(last))
            targetStack.append(targetStack.last());
        m_ancestors.append(EventContext(node, eventTargetRespectingSVGTargetRules(node), targetStack.last()));
        if (!inDocument)
            return;
        last = node;
        if (!node->isShadowRoot())
            continue;
        if (determineDispatchBehavior(event, toShadowRoot(node), targetStack.last()) == StayInsideShadowDOM)
            return;
        if (!isSVGElement) {
            ASSERT(!targetStack.isEmpty());
            targetStack.removeLast();
        }
    }
}

void EventDispatcher::dispatchScopedEvent(Node* node, PassRefPtr<EventDispatchMediator> mediator)
{
    // We need to set the target here because it can go away by the time we actually fire the event.
    mediator->event()->setTarget(eventTargetRespectingSVGTargetRules(node));
    ScopedEventQueue::instance()->enqueueEventDispatchMediator(mediator);
}

void EventDispatcher::dispatchSimulatedClick(Node* node, PassRefPtr<Event> underlyingEvent, bool sendMouseEvents, bool showPressedLook)
{
    if (node->disabled())
        return;

    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<Node*>;
    else if (gNodesDispatchingSimulatedClicks->contains(node))
        return;

    gNodesDispatchingSimulatedClicks->add(node);

    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        EventDispatcher(node).dispatchEvent(SimulatedMouseEvent::create(eventNames().mousedownEvent, node->document()->defaultView(), underlyingEvent));
    node->setActive(true, showPressedLook);
    if (sendMouseEvents)
        EventDispatcher(node).dispatchEvent(SimulatedMouseEvent::create(eventNames().mouseupEvent, node->document()->defaultView(), underlyingEvent));
    node->setActive(false);

    // always send click
    EventDispatcher(node).dispatchEvent(SimulatedMouseEvent::create(eventNames().clickEvent, node->document()->defaultView(), underlyingEvent));

    gNodesDispatchingSimulatedClicks->remove(node);
}

bool EventDispatcher::dispatchEvent(PassRefPtr<Event> prpEvent)
{
#ifndef NDEBUG
    ASSERT(!m_eventDispatched);
    m_eventDispatched = true;
#endif
    RefPtr<Event> event = prpEvent;
    ChildNodesLazySnapshot::takeChildNodesLazySnapshot();

    event->setTarget(eventTargetRespectingSVGTargetRules(m_node.get()));
    ASSERT(!eventDispatchForbidden());
    ASSERT(event->target());
    ASSERT(!event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.
    ensureEventAncestors(event.get());
    WindowEventContext windowEventContext(event.get(), m_node.get(), topEventContext());
    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEvent(m_node->document(), *event, windowEventContext.window(), m_node.get(), m_ancestors);

    void* preDispatchEventHandlerResult;
    if (dispatchEventPreProcess(event, preDispatchEventHandlerResult) == ContinueDispatching)
        if (dispatchEventAtCapturing(event, windowEventContext) == ContinueDispatching)
            if (dispatchEventAtTarget(event) == ContinueDispatching)
                dispatchEventAtBubbling(event, windowEventContext);
    dispatchEventPostProcess(event, preDispatchEventHandlerResult);

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    event->setTarget(windowEventContext.target());
    event->setCurrentTarget(0);
    InspectorInstrumentation::didDispatchEvent(cookie);

    return !event->defaultPrevented();
}

inline EventDispatchContinuation EventDispatcher::dispatchEventPreProcess(PassRefPtr<Event> event, void*& preDispatchEventHandlerResult)
{
    // Give the target node a chance to do some work before DOM event handlers get a crack.
    preDispatchEventHandlerResult = m_node->preDispatchEventHandler(event.get());
    return (m_ancestors.isEmpty() || event->propagationStopped()) ? DoneDispatching : ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtCapturing(PassRefPtr<Event> event, WindowEventContext& windowEventContext)
{
    // Trigger capturing event handlers, starting at the top and working our way down.
    event->setEventPhase(Event::CAPTURING_PHASE);

    if (windowEventContext.handleLocalEvents(event.get()) && event->propagationStopped())
        return DoneDispatching;

    for (size_t i = m_ancestors.size() - 1; i > 0; --i) {
        const EventContext& eventContext = m_ancestors[i];
        if (eventContext.currentTargetSameAsTarget()) {
            if (event->bubbles())
                continue;
            event->setEventPhase(Event::AT_TARGET);
        } else
            event->setEventPhase(Event::CAPTURING_PHASE);
        eventContext.handleLocalEvents(event.get());
        if (event->propagationStopped())
            return DoneDispatching;
    }

    return ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtTarget(PassRefPtr<Event> event)
{
    event->setEventPhase(Event::AT_TARGET);
    m_ancestors[0].handleLocalEvents(event.get());
    return event->propagationStopped() ? DoneDispatching : ContinueDispatching;
}

inline EventDispatchContinuation EventDispatcher::dispatchEventAtBubbling(PassRefPtr<Event> event, WindowEventContext& windowContext)
{
    if (event->bubbles() && !event->cancelBubble()) {
        // Trigger bubbling event handlers, starting at the bottom and working our way up.
        event->setEventPhase(Event::BUBBLING_PHASE);

        size_t size = m_ancestors.size();
        for (size_t i = 1; i < size; ++i) {
            const EventContext& eventContext = m_ancestors[i];
            if (eventContext.currentTargetSameAsTarget())
                event->setEventPhase(Event::AT_TARGET);
            else
                event->setEventPhase(Event::BUBBLING_PHASE);
            eventContext.handleLocalEvents(event.get());
            if (event->propagationStopped() || event->cancelBubble())
                return DoneDispatching;
        }
        windowContext.handleLocalEvents(event.get());
    }
    return ContinueDispatching;
}

inline void EventDispatcher::dispatchEventPostProcess(PassRefPtr<Event> event, void* preDispatchEventHandlerResult)
{
    event->setTarget(eventTargetRespectingSVGTargetRules(m_node.get()));
    event->setCurrentTarget(0);
    event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    m_node->postDispatchEventHandler(event.get(), preDispatchEventHandlerResult);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event->defaultPrevented() && !event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        m_node->defaultEventHandler(event.get());
        ASSERT(!event->defaultPrevented());
        if (event->defaultHandled())
            return;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (event->bubbles()) {
            size_t size = m_ancestors.size();
            for (size_t i = 1; i < size; ++i) {
                m_ancestors[i].node()->defaultEventHandler(event.get());
                ASSERT(!event->defaultPrevented());
                if (event->defaultHandled())
                    return;
            }
        }
    }
}

const EventContext* EventDispatcher::topEventContext()
{
    return m_ancestors.isEmpty() ? 0 : &m_ancestors.last();
}

static inline bool inTheSameScope(ShadowRoot* shadowRoot, EventTarget* target)
{
    return target->toNode() && target->toNode()->treeScope()->rootNode() == shadowRoot;
}

EventDispatchBehavior EventDispatcher::determineDispatchBehavior(Event* event, ShadowRoot* shadowRoot, EventTarget* target)
{
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO)
    // Video-only full screen is a mode where we use the shadow DOM as an implementation
    // detail that should not be detectable by the web content.
    if (Element* element = m_node->document()->webkitCurrentFullScreenElement()) {
        // FIXME: We assume that if the full screen element is a media element that it's
        // the video-only full screen. Both here and elsewhere. But that is probably wrong.
        if (element->isMediaElement() && shadowRoot && shadowRoot->host() == element)
            return StayInsideShadowDOM;
    }
#else
    UNUSED_PARAM(shadowRoot);
#endif

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    const AtomicString eventType = event->type();
    if (inTheSameScope(shadowRoot, target)
        && (eventType == eventNames().abortEvent
            || eventType == eventNames().changeEvent
            || eventType == eventNames().resetEvent
            || eventType == eventNames().resizeEvent
            || eventType == eventNames().scrollEvent
            || eventType == eventNames().selectEvent
            || eventType == eventNames().selectstartEvent))
        return StayInsideShadowDOM;

    return RetargetEvent;
}

}
