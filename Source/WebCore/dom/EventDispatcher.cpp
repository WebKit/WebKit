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
#include "ElementShadow.h"
#include "EventContext.h"
#include "EventDispatchMediator.h"
#include "FrameView.h"
#include "HTMLMediaElement.h"
#include "InsertionPoint.h"
#include "InspectorInstrumentation.h"
#include "MouseEvent.h"
#include "ScopedEventQueue.h"
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
    TreeScope* lastTreeScope = 0;
    for (ComposedShadowTreeWalker walker(m_relatedTarget.get()); walker.get(); walker.parentIncludingInsertionPointAndShadowRoot()) {
        TreeScope* scope = walker.get()->treeScope();
        // Skips adding a node to the map if treeScope does not change.
        if (scope != lastTreeScope)
            m_relatedTargetMap.add(scope, walker.get());
        lastTreeScope = scope;
    }

    lastTreeScope = 0;
    EventTarget* adjustedRelatedTarget = 0;
    for (Vector<EventContext>::iterator iter = ancestors.begin(); iter < ancestors.end(); ++iter) {
        TreeScope* scope = iter->node()->treeScope();
        if (scope == lastTreeScope) {
            // Re-use the previous adjustedRelatedTarget if treeScope does not change.
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
    Element* shadowHostElement = referenceNode->treeScope()->rootNode()->shadowHost();
    // At this time, SVG nodes are not allowed in non-<use> shadow trees, so any shadow root we do
    // have should be a use. The assert and following test is here to catch future shadow DOM changes
    // that do enable SVG in a shadow tree.
    ASSERT(!shadowHostElement || shadowHostElement->hasTagName(SVGNames::useTag));
    if (shadowHostElement && shadowHostElement->hasTagName(SVGNames::useTag)) {
        SVGUseElement* useElement = static_cast<SVGUseElement*>(shadowHostElement);

        if (SVGElementInstance* instance = useElement->instanceForShadowTreeElement(referenceNode))
            return instance;
    }
#endif

    return referenceNode;
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

    EventDispatcher dispatcher(node);

    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<Node*>;
    else if (gNodesDispatchingSimulatedClicks->contains(node))
        return;

    gNodesDispatchingSimulatedClicks->add(node);

    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        dispatcher.dispatchEvent(SimulatedMouseEvent::create(eventNames().mousedownEvent, node->document()->defaultView(), underlyingEvent));
    node->setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatcher.dispatchEvent(SimulatedMouseEvent::create(eventNames().mouseupEvent, node->document()->defaultView(), underlyingEvent));
    node->setActive(false);

    // always send click
    dispatcher.dispatchEvent(SimulatedMouseEvent::create(eventNames().clickEvent, node->document()->defaultView(), underlyingEvent));

    gNodesDispatchingSimulatedClicks->remove(node);
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
    , m_shouldPreventDispatch(false)
{
    ASSERT(node);
    m_view = node->document()->view();
}

void EventDispatcher::ensureEventAncestors(Event* event)
{
    if (m_ancestorsInitialized)
        return;

    ComposedShadowTreeWalker ancestorWalker(m_node.get());
    EventTarget* originalTarget = eventTargetRespectingSVGTargetRules(ancestorWalker.get());
    m_ancestors.append(EventContext(m_node.get(), originalTarget, originalTarget));
    m_ancestorsInitialized = true;

    if (!m_node->inDocument())
        return;

    Vector<EventTarget*> targetStack;
    targetStack.append(originalTarget);
    while (true) {
        if (ancestorWalker.get()->isShadowRoot()) {
            if (determineDispatchBehavior(event, ancestorWalker.get()) == StayInsideShadowDOM)
                return;
            ancestorWalker.parentIncludingInsertionPointAndShadowRoot();
            if (!ancestorWalker.get())
                return;
            if (!m_node->isSVGElement()) {
                targetStack.removeLast();
                if (targetStack.isEmpty())
                    targetStack.append(ancestorWalker.get());
            }
        } else {
            ancestorWalker.parentIncludingInsertionPointAndShadowRoot();
            if (!ancestorWalker.get())
                return;
            if (isInsertionPoint(ancestorWalker.get()) && toInsertionPoint(ancestorWalker.get())->isActive())
                targetStack.append(ancestorWalker.get());
        }
        m_ancestors.append(EventContext(ancestorWalker.get(), eventTargetRespectingSVGTargetRules(ancestorWalker.get()), targetStack.last()));
    }
}

bool EventDispatcher::dispatchEvent(PassRefPtr<Event> event)
{
    event->setTarget(eventTargetRespectingSVGTargetRules(m_node.get()));

    ASSERT(!eventDispatchForbidden());
    ASSERT(event->target());
    ASSERT(!event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.

    RefPtr<EventTarget> originalTarget = event->target();
    ensureEventAncestors(event.get());

    WindowEventContext windowContext(event.get(), m_node.get(), topEventContext());

    InspectorInstrumentationCookie cookie = InspectorInstrumentation::willDispatchEvent(m_node->document(), *event, windowContext.window(), m_node.get(), m_ancestors);

    // Give the target node a chance to do some work before DOM event handlers get a crack.
    void* data = m_node->preDispatchEventHandler(event.get());
    if (m_ancestors.isEmpty() || m_shouldPreventDispatch || event->propagationStopped())
        goto doneDispatching;

    // Trigger capturing event handlers, starting at the top and working our way down.
    event->setEventPhase(Event::CAPTURING_PHASE);

    if (windowContext.handleLocalEvents(event.get()) && event->propagationStopped())
        goto doneDispatching;

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
            goto doneDispatching;
    }

    event->setEventPhase(Event::AT_TARGET);
    event->setTarget(originalTarget.get());
    event->setCurrentTarget(eventTargetRespectingSVGTargetRules(m_node.get()));
    m_ancestors[0].handleLocalEvents(event.get());
    if (event->propagationStopped())
        goto doneDispatching;

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
                goto doneDispatching;
        }
        windowContext.handleLocalEvents(event.get());
    }

doneDispatching:
    event->setTarget(originalTarget.get());
    event->setCurrentTarget(0);
    event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    m_node->postDispatchEventHandler(event.get(), data);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event->defaultPrevented() && !event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        m_node->defaultEventHandler(event.get());
        ASSERT(!event->defaultPrevented());
        if (event->defaultHandled())
            goto doneWithDefault;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (event->bubbles()) {
            size_t size = m_ancestors.size();
            for (size_t i = 1; i < size; ++i) {
                m_ancestors[i].node()->defaultEventHandler(event.get());
                ASSERT(!event->defaultPrevented());
                if (event->defaultHandled())
                    goto doneWithDefault;
            }
        }
    }

doneWithDefault:

    // Ensure that after event dispatch, the event's target object is the
    // outermost shadow DOM boundary.
    event->setTarget(windowContext.target());
    event->setCurrentTarget(0);
    InspectorInstrumentation::didDispatchEvent(cookie);

    return !event->defaultPrevented();
}

const EventContext* EventDispatcher::topEventContext()
{
    return m_ancestors.isEmpty() ? 0 : &m_ancestors.last();
}

EventDispatchBehavior EventDispatcher::determineDispatchBehavior(Event* event, Node* shadowRoot)
{
#if ENABLE(FULLSCREEN_API) && ENABLE(VIDEO)
    // Video-only full screen is a mode where we use the shadow DOM as an implementation
    // detail that should not be detectable by the web content.
    if (Element* element = m_node->document()->webkitCurrentFullScreenElement()) {
        // FIXME: We assume that if the full screen element is a media element that it's
        // the video-only full screen. Both here and elsewhere. But that is probably wrong.
        if (element->isMediaElement() && shadowRoot && shadowRoot->shadowHost() == element)
            return StayInsideShadowDOM;
    }
#else
    UNUSED_PARAM(shadowRoot);
#endif

    // WebKit never allowed selectstart event to cross the the shadow DOM boundary.
    // Changing this breaks existing sites.
    // See https://bugs.webkit.org/show_bug.cgi?id=52195 for details.
    if (event->type() == eventNames().selectstartEvent)
        return StayInsideShadowDOM;

    return RetargetEvent;
}

}
