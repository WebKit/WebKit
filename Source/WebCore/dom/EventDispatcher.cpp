/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

#include "CompositionEvent.h"
#include "EventContext.h"
#include "EventNames.h"
#include "EventPath.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameLoader.h"
#include "HTMLInputElement.h"
#include "InputEvent.h"
#include "KeyboardEvent.h"
#include "LocalFrame.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "MouseEvent.h"
#include "ScopedEventQueue.h"
#include "ScriptDisallowedScope.h"
#include "ShadowRoot.h"
#include "TextEvent.h"
#include "TouchEvent.h"
#include <wtf/text/TextStream.h>

namespace WebCore {

void EventDispatcher::dispatchScopedEvent(Node& node, Event& event)
{
    // Need to set the target here so the scoped event queue knows which node to dispatch to.
    event.setTarget(RefPtr { EventPath::eventTargetRespectingTargetRules(node) });
    ScopedEventQueue::singleton().enqueueEvent(event);
}

static void callDefaultEventHandlersInBubblingOrder(Event& event, const EventPath& path)
{
    if (path.isEmpty())
        return;

    // Non-bubbling events call only one default event handler, the one for the target.
    Ref rootNode { *path.contextAt(0).node() };
    rootNode->defaultEventHandler(event);

    if (event.defaultHandled() || !event.bubbles() || event.defaultPrevented())
        return;

    size_t size = path.size();
    for (size_t i = 1; i < size; ++i) {
        Ref currentNode { *path.contextAt(i).node() };
        currentNode->defaultEventHandler(event);
        if (event.defaultPrevented() || event.defaultHandled())
            return;
    }
}

static bool isInShadowTree(EventTarget* target)
{
    auto* node = dynamicDowncast<Node>(target);
    return node && node->isInShadowTree();
}

static void dispatchEventInDOM(Event& event, const EventPath& path)
{
    // Invoke capturing event listeners in the reverse order.
    for (size_t i = path.size(); i > 0; --i) {
        const EventContext& eventContext = path.contextAt(i - 1);
        if (eventContext.currentTarget() == eventContext.target())
            event.setEventPhase(Event::AT_TARGET);
        else
            event.setEventPhase(Event::CAPTURING_PHASE);
        eventContext.handleLocalEvents(event, EventTarget::EventInvokePhase::Capturing);
        if (event.propagationStopped())
            return;
    }

    // Invoke bubbling event listeners.
    size_t size = path.size();
    for (size_t i = 0; i < size; ++i) {
        const EventContext& eventContext = path.contextAt(i);
        if (eventContext.currentTarget() == eventContext.target())
            event.setEventPhase(Event::AT_TARGET);
        else if (event.bubbles())
            event.setEventPhase(Event::BUBBLING_PHASE);
        else
            continue;
        eventContext.handleLocalEvents(event, EventTarget::EventInvokePhase::Bubbling);
        if (event.propagationStopped())
            return;
    }
}

static bool shouldSuppressEventDispatchInDOM(Node& node, Event& event)
{
    if (!event.isTrusted())
        return false;

    RefPtr frame = node.document().frame();
    if (!frame)
        return false;

    RefPtr localFrame = dynamicDowncast<LocalFrame>(frame->mainFrame());
    if (!localFrame)
        return false;

    if (!localFrame->checkedLoader()->shouldSuppressTextInputFromEditing())
        return false;

    if (auto* textEvent = dynamicDowncast<TextEvent>(event))
        return textEvent->isKeyboard() || textEvent->isComposition();

    return is<CompositionEvent>(event) || is<InputEvent>(event) || is<KeyboardEvent>(event);
}

static HTMLInputElement* findInputElementInEventPath(const EventPath& path)
{
    size_t size = path.size();
    for (size_t i = 0; i < size; ++i) {
        auto& eventContext = path.contextAt(i);
        if (auto* inputElement = dynamicDowncast<HTMLInputElement>(eventContext.currentTarget()))
            return inputElement;
    }
    return nullptr;
}

static bool hasRelevantEventListener(Document& document, const Event& event)
{
    if (document.hasEventListenersOfType(event.type()))
        return true;

    auto legacyType = EventTarget::legacyTypeForEvent(event);
    if (!legacyType.isNull() && document.hasEventListenersOfType(legacyType))
        return true;

    return false;
}

static void resetAfterDispatchInShadowTree(Event& event)
{
    event.setTarget(nullptr);
    event.setRelatedTarget(nullptr);
    // FIXME: We should also clear the event's touch target list.
}

void EventDispatcher::dispatchEvent(Node& node, Event& event)
{
    ASSERT_WITH_SECURITY_IMPLICATION(ScriptDisallowedScope::InMainThread::isEventDispatchAllowedInSubtree(node));
    
    LOG_WITH_STREAM(Events, stream << "EventDispatcher::dispatchEvent " << event << " on node " << node);

    Ref protectedNode { node };
    RefPtr protectedView { node.document().view() };

    auto typeInfo = eventNames().typeInfoForEvent(event.type());
    bool shouldDispatchEventToScripts = hasRelevantEventListener(node.document(), event);

    bool targetOrRelatedTargetIsInShadowTree = node.isInShadowTree() || isInShadowTree(event.relatedTarget());
    // FIXME: We should also check touch target list.
    bool hasNoEventListnerOrDefaultEventHandler = !shouldDispatchEventToScripts && !typeInfo.hasDefaultEventHandler() && !node.document().hasConnectedPluginElements();
    if (hasNoEventListnerOrDefaultEventHandler && !targetOrRelatedTargetIsInShadowTree)
        return;

    EventPath eventPath { node, event };

    if (node.document().settings().sendMouseEventsToDisabledFormControlsEnabled() && event.isTrusted() && event.isMouseEvent()
        && (typeInfo.type() == EventType::mousedown || typeInfo.type() == EventType::mouseup || typeInfo.type() == EventType::click || typeInfo.type() == EventType::dblclick)) {
        eventPath.adjustForDisabledFormControl();
    }

    bool shouldClearTargetsAfterDispatch = false;
    for (size_t i = eventPath.size(); i > 0; --i) {
        const EventContext& eventContext = eventPath.contextAt(i - 1);
        // FIXME: We should also set shouldClearTargetsAfterDispatch to true if an EventTarget object in eventContext's touch target list
        // is a node and its root is a shadow root.
        if (eventContext.target()) {
            shouldClearTargetsAfterDispatch = isInShadowTree(eventContext.target()) || isInShadowTree(eventContext.relatedTarget());
            break;
        }
    }

    if (hasNoEventListnerOrDefaultEventHandler) {
        if (shouldClearTargetsAfterDispatch)
            resetAfterDispatchInShadowTree(event);
        return;
    }

    event.resetBeforeDispatch();

    event.setTarget(RefPtr { EventPath::eventTargetRespectingTargetRules(node) });
    if (!event.target())
        return;

    InputElementClickState clickHandlingState;
    clickHandlingState.trusted = event.isTrusted();

    RefPtr inputForLegacyPreActivationBehavior = dynamicDowncast<HTMLInputElement>(node);
    if (!inputForLegacyPreActivationBehavior && event.bubbles() && event.type() == eventNames().clickEvent)
        inputForLegacyPreActivationBehavior = findInputElementInEventPath(eventPath);
    if (inputForLegacyPreActivationBehavior
        && (!event.isTrusted() || !inputForLegacyPreActivationBehavior->isDisabledFormControl())) {
        inputForLegacyPreActivationBehavior->willDispatchEvent(event, clickHandlingState);
    }

    if (!event.propagationStopped() && !eventPath.isEmpty() && !shouldSuppressEventDispatchInDOM(node, event) && shouldDispatchEventToScripts) {
        event.setEventPath(eventPath);
        dispatchEventInDOM(event, eventPath);
    }

    event.resetAfterDispatch();

    if (clickHandlingState.stateful)
        inputForLegacyPreActivationBehavior->didDispatchClickEvent(event, clickHandlingState);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (typeInfo.hasDefaultEventHandler() && !event.defaultPrevented() && !event.defaultHandled() && !event.isDefaultEventHandlerIgnored()) {
        // FIXME: Not clear why we need to reset the target for the default event handlers.
        // We should research this, and remove this code if possible.
        RefPtr finalTarget = event.target();
        event.setTarget(RefPtr { EventPath::eventTargetRespectingTargetRules(node) });
        callDefaultEventHandlersInBubblingOrder(event, eventPath);
        event.setTarget(WTFMove(finalTarget));
    }

    if (shouldClearTargetsAfterDispatch)
        resetAfterDispatchInShadowTree(event);
}

template<typename T>
static void dispatchEventWithType(const Vector<T*>& targets, Event& event)
{
    ASSERT(targets.size() >= 1);
    ASSERT(*targets.begin());

    EventPath eventPath { targets };
    event.setTarget(RefPtr { *targets.begin() });
    event.setEventPath(eventPath);
    event.resetBeforeDispatch();
    dispatchEventInDOM(event, eventPath);
    event.resetAfterDispatch();
}

void EventDispatcher::dispatchEvent(const Vector<EventTarget*>& targets, Event& event)
{
    dispatchEventWithType<EventTarget>(targets, event);
}

}
