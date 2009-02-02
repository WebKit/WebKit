/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
 *
 */

#include "config.h"
#include "EventTargetNode.h"

#include "Document.h"
#include "EventException.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "NodeRareData.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "ProgressEvent.h"
#include "RegisteredEventListener.h"
#include "ScriptController.h"
#include "TextEvent.h"
#include "WebKitAnimationEvent.h"
#include "WebKitTransitionEvent.h"
#include "WheelEvent.h"
#include <wtf/HashSet.h>

#if ENABLE(DOM_STORAGE)
#include "StorageEvent.h"
#endif

#if ENABLE(SVG)
#include "SVGElementInstance.h"
#include "SVGUseElement.h"
#endif

namespace WebCore {
    
static HashSet<EventTargetNode*>* gNodesDispatchingSimulatedClicks = 0; 

EventTargetNode::~EventTargetNode()
{
    if (!eventListeners().isEmpty() && !inDocument())
        document()->unregisterDisconnectedNodeWithEventListeners(this);
}

ScriptExecutionContext* EventTargetNode::scriptExecutionContext() const
{
    return document();
}

const RegisteredEventListenerVector& EventTargetNode::eventListeners() const
{
    if (hasRareData()) {
        if (RegisteredEventListenerVector* listeners = rareData()->listeners())
            return *listeners;
    }
    static const RegisteredEventListenerVector* emptyListenersVector = new RegisteredEventListenerVector;
    return *emptyListenersVector;
}

void EventTargetNode::insertedIntoDocument()
{
    if (!eventListeners().isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    Node::insertedIntoDocument();
}

void EventTargetNode::removedFromDocument()
{
    if (!eventListeners().isEmpty())
        document()->registerDisconnectedNodeWithEventListeners(this);

    Node::removedFromDocument();
}

void EventTargetNode::willMoveToNewOwnerDocument()
{
    if (!eventListeners().isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    Node::willMoveToNewOwnerDocument();
}

void EventTargetNode::didMoveToNewOwnerDocument()
{
    if (!eventListeners().isEmpty())
        document()->registerDisconnectedNodeWithEventListeners(this);

    Node::didMoveToNewOwnerDocument();
}

static inline void updateSVGElementInstancesAfterEventListenerChange(EventTargetNode* referenceNode)
{
    ASSERT(referenceNode);

#if ENABLE(SVG)
    if (!referenceNode->isSVGElement())
        return;

    // Elements living inside a <use> shadow tree, never cause any updates!
    if (referenceNode->shadowTreeRootNode())
        return;

    // We're possibly (a child of) an element that is referenced by a <use> client
    // If an event listeners changes on a referenced element, update all instances.
    for (Node* node = referenceNode; node; node = node->parentNode()) {
        if (!node->hasID() || !node->isSVGElement())
            continue;

        SVGElementInstance::invalidateAllInstancesOfElement(static_cast<SVGElement*>(node));
        break;
    }
#endif
}

void EventTargetNode::addEventListener(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    Document* document = this->document();
    if (!document->attached())
        return;

    document->addListenerTypeIfNeeded(eventType);

    RegisteredEventListenerVector& listeners = ensureRareData()->ensureListeners();

    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener.get(), useCapture);

    // adding the first one
    if (listeners.isEmpty() && !inDocument())
        document->registerDisconnectedNodeWithEventListeners(this);

    listeners.append(RegisteredEventListener::create(eventType, listener, useCapture));
    updateSVGElementInstancesAfterEventListenerChange(this);
}

void EventTargetNode::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!hasRareData())
        return;

    RegisteredEventListenerVector* listeners = rareData()->listeners();
    if (!listeners)
        return;

    size_t size = listeners->size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *listeners->at(i);
        if (r.eventType() == eventType && r.listener() == listener && r.useCapture() == useCapture) {
            r.setRemoved(true);
            listeners->remove(i);

            // removed last
            if (listeners->isEmpty() && !inDocument())
                document()->unregisterDisconnectedNodeWithEventListeners(this);

            updateSVGElementInstancesAfterEventListenerChange(this);
            return;
        }
    }
}

void EventTargetNode::removeAllEventListenersSlowCase()
{
    ASSERT(hasRareData());

    RegisteredEventListenerVector* listeners = rareData()->listeners();
    if (!listeners)
        return;

    size_t size = listeners->size();
    for (size_t i = 0; i < size; ++i)
        listeners->at(i)->setRemoved(true);
    listeners->clear();
}

void EventTargetNode::handleLocalEvents(Event* event, bool useCapture)
{
    if (disabled() && event->isMouseEvent())
        return;

    RegisteredEventListenerVector listenersCopy = eventListeners();
    size_t size = listenersCopy.size();
    for (size_t i = 0; i < size; ++i) {
        const RegisteredEventListener& r = *listenersCopy[i];
        if (r.eventType() == event->type() && r.useCapture() == useCapture && !r.removed())
            r.listener()->handleEvent(event, false);
    }
}

#if ENABLE(SVG)
static inline SVGElementInstance* eventTargetAsSVGElementInstance(EventTargetNode* referenceNode)
{
    ASSERT(referenceNode);
    if (!referenceNode->isSVGElement())
        return 0;

    // Spec: The event handling for the non-exposed tree works as if the referenced element had been textually included
    // as a deeply cloned child of the 'use' element, except that events are dispatched to the SVGElementInstance objects
    for (Node* n = referenceNode; n; n = n->parentNode()) {
        if (!n->isShadowNode() || !n->isSVGElement())
            continue;

        Node* shadowTreeParentElement = n->shadowParentNode();
        ASSERT(shadowTreeParentElement->hasTagName(SVGNames::useTag));

        if (SVGElementInstance* instance = static_cast<SVGUseElement*>(shadowTreeParentElement)->instanceForShadowTreeElement(referenceNode))
            return instance;
    }

    return 0;
}
#endif

static inline EventTarget* eventTargetRespectingSVGTargetRules(EventTargetNode* referenceNode)
{
    ASSERT(referenceNode);

#if ENABLE(SVG)
    if (SVGElementInstance* instance = eventTargetAsSVGElementInstance(referenceNode)) {
        ASSERT(instance->shadowTreeElement() == referenceNode);
        return instance;
    }
#endif

    return referenceNode;
}

bool EventTargetNode::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) { 
        ec = EventException::UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    evt->setTarget(eventTargetRespectingSVGTargetRules(this));

    RefPtr<FrameView> view = document()->view();
    return dispatchGenericEvent(evt.release());
}

bool EventTargetNode::dispatchGenericEvent(PassRefPtr<Event> prpEvent)
{
    RefPtr<Event> event(prpEvent);

    ASSERT(!eventDispatchForbidden());
    ASSERT(event->target());
    ASSERT(!event->type().isNull()); // JavaScript code can create an event with an empty name, but not null.

    // Make a vector of ancestors to send the event to.
    // If the node is not in a document just send the event to it.
    // Be sure to ref all of nodes since event handlers could result in the last reference going away.
    RefPtr<EventTargetNode> thisNode(this);
    Vector<RefPtr<ContainerNode> > ancestors;
    if (inDocument()) {
        for (ContainerNode* ancestor = eventParentNode(); ancestor; ancestor = ancestor->eventParentNode()) {
#if ENABLE(SVG)
            // Skip <use> shadow tree elements.
            if (ancestor->isSVGElement() && ancestor->isShadowNode())
                continue;
#endif
            ancestors.append(ancestor);
        }
    }

    // Set up a pointer to indicate whether to dispatch window events.
    // We don't dispatch load events to the window. That quirk was originally
    // added because Mozilla doesn't propagate load events to the window object.
    Document* documentForWindowEvents = 0;
    if (event->type() != eventNames().loadEvent) {
        EventTargetNode* topLevelContainer = ancestors.isEmpty() ? this : ancestors.last().get();
        if (topLevelContainer->isDocumentNode())
            documentForWindowEvents = static_cast<Document*>(topLevelContainer);
    }

    // Give the target node a chance to do some work before DOM event handlers get a crack.
    void* data = preDispatchEventHandler(event.get());
    if (event->propagationStopped())
        goto doneDispatching;

    // Trigger capturing event handlers, starting at the top and working our way down.
    event->setEventPhase(Event::CAPTURING_PHASE);

    if (documentForWindowEvents) {
        event->setCurrentTarget(documentForWindowEvents);
        documentForWindowEvents->handleWindowEvent(event.get(), true);
        if (event->propagationStopped())
            goto doneDispatching;
    }
    for (size_t i = ancestors.size(); i; --i) {
        ContainerNode* ancestor = ancestors[i - 1].get();
        event->setCurrentTarget(eventTargetRespectingSVGTargetRules(ancestor));
        ancestor->handleLocalEvents(event.get(), true);
        if (event->propagationStopped())
            goto doneDispatching;
    }

    event->setEventPhase(Event::AT_TARGET);

    // We do want capturing event listeners to be invoked here, even though
    // that violates some versions of the DOM specification; Mozilla does it.
    event->setCurrentTarget(eventTargetRespectingSVGTargetRules(this));
    handleLocalEvents(event.get(), true);
    if (event->propagationStopped())
        goto doneDispatching;
    handleLocalEvents(event.get(), false);
    if (event->propagationStopped())
        goto doneDispatching;

    if (event->bubbles() && !event->cancelBubble()) {
        // Trigger bubbling event handlers, starting at the bottom and working our way up.
        event->setEventPhase(Event::BUBBLING_PHASE);

        size_t size = ancestors.size();
        for (size_t i = 0; i < size; ++i) {
            ContainerNode* ancestor = ancestors[i].get();
            event->setCurrentTarget(eventTargetRespectingSVGTargetRules(ancestor));
            ancestor->handleLocalEvents(event.get(), false);
            if (event->propagationStopped() || event->cancelBubble())
                goto doneDispatching;
        }
        if (documentForWindowEvents) {
            event->setCurrentTarget(documentForWindowEvents);
            documentForWindowEvents->handleWindowEvent(event.get(), false);
            if (event->propagationStopped() || event->cancelBubble())
                goto doneDispatching;
        }
    }

doneDispatching:
    event->setCurrentTarget(0);
    event->setEventPhase(0);

    // Pass the data from the preDispatchEventHandler to the postDispatchEventHandler.
    postDispatchEventHandler(event.get(), data);

    // Call default event handlers. While the DOM does have a concept of preventing
    // default handling, the detail of which handlers are called is an internal
    // implementation detail and not part of the DOM.
    if (!event->defaultPrevented() && !event->defaultHandled()) {
        // Non-bubbling events call only one default event handler, the one for the target.
        defaultEventHandler(event.get());
        ASSERT(!event->defaultPrevented());
        if (event->defaultHandled())
            goto doneWithDefault;
        // For bubbling events, call default event handlers on the same targets in the
        // same order as the bubbling phase.
        if (event->bubbles()) {
            size_t size = ancestors.size();
            for (size_t i = 0; i < size; ++i) {
                ContainerNode* ancestor = ancestors[i].get();
                ancestor->defaultEventHandler(event.get());
                ASSERT(!event->defaultPrevented());
                if (event->defaultHandled())
                    goto doneWithDefault;
            }
        }
    }

doneWithDefault:
    Document::updateDocumentsRendering();

    return !event->defaultPrevented();
}

bool EventTargetNode::dispatchSubtreeModifiedEvent()
{
    ASSERT(!eventDispatchForbidden());
    
    document()->incDOMTreeVersion();

    notifyNodeListsAttributeChanged(); // FIXME: Can do better some day. Really only care about the name attribute changing.
    
    if (!document()->hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return false;
    ExceptionCode ec = 0;
    return dispatchEvent(MutationEvent::create(eventNames().DOMSubtreeModifiedEvent, true, false, 0, String(), String(), String(), 0), ec);
}

void EventTargetNode::dispatchWindowEvent(PassRefPtr<Event> e)
{
    ASSERT(!eventDispatchForbidden());
    RefPtr<Event> evt(e);
    RefPtr<Document> doc = document();
    evt->setTarget(doc);
    doc->handleWindowEvent(evt.get(), true);
    doc->handleWindowEvent(evt.get(), false);
}

void EventTargetNode::dispatchWindowEvent(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    RefPtr<Document> doc = document();
    dispatchWindowEvent(Event::create(eventType, canBubbleArg, cancelableArg));
    
    if (eventType == eventNames().loadEvent) {
        // For onload events, send a separate load event to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.
        Element* ownerElement = doc->ownerElement();
        if (ownerElement) {
            RefPtr<Event> ownerEvent = Event::create(eventType, false, cancelableArg);
            ownerEvent->setTarget(ownerElement);
            ownerElement->dispatchGenericEvent(ownerEvent.release());
        }
    }
}

bool EventTargetNode::dispatchUIEvent(const AtomicString& eventType, int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    ASSERT(eventType == eventNames().DOMFocusInEvent || eventType == eventNames().DOMFocusOutEvent || eventType == eventNames().DOMActivateEvent);
    
    bool cancelable = eventType == eventNames().DOMActivateEvent;
    
    ExceptionCode ec = 0;
    RefPtr<UIEvent> evt = UIEvent::create(eventType, true, cancelable, document()->defaultView(), detail);
    evt->setUnderlyingEvent(underlyingEvent);
    return dispatchEvent(evt.release(), ec);
}

bool EventTargetNode::dispatchKeyEvent(const PlatformKeyboardEvent& key)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<KeyboardEvent> keyboardEvent = KeyboardEvent::create(key, document()->defaultView());
    bool r = dispatchEvent(keyboardEvent, ec);
    
    // we want to return false if default is prevented (already taken care of)
    // or if the element is default-handled by the DOM. Otherwise we let it just
    // let it get handled by AppKit 
    if (keyboardEvent->defaultHandled())
        r = false;
    
    return r;
}

bool EventTargetNode::dispatchMouseEvent(const PlatformMouseEvent& event, const AtomicString& eventType,
    int detail, Node* relatedTarget)
{
    ASSERT(!eventDispatchForbidden());
    
    IntPoint contentsPos;
    if (FrameView* view = document()->view())
        contentsPos = view->windowToContents(event.pos());

    short button = event.button();

    ASSERT(event.eventType() == MouseEventMoved || button != NoButton);
    
    return dispatchMouseEvent(eventType, button, detail,
        contentsPos.x(), contentsPos.y(), event.globalX(), event.globalY(),
        event.ctrlKey(), event.altKey(), event.shiftKey(), event.metaKey(),
        false, relatedTarget);
}

void EventTargetNode::dispatchSimulatedMouseEvent(const AtomicString& eventType,
    PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());

    bool ctrlKey = false;
    bool altKey = false;
    bool shiftKey = false;
    bool metaKey = false;
    if (UIEventWithKeyState* keyStateEvent = findEventWithKeyState(underlyingEvent.get())) {
        ctrlKey = keyStateEvent->ctrlKey();
        altKey = keyStateEvent->altKey();
        shiftKey = keyStateEvent->shiftKey();
        metaKey = keyStateEvent->metaKey();
    }

    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0,
        ctrlKey, altKey, shiftKey, metaKey, true, 0, underlyingEvent);
}

void EventTargetNode::dispatchSimulatedClick(PassRefPtr<Event> event, bool sendMouseEvents, bool showPressedLook)
{
    if (!gNodesDispatchingSimulatedClicks)
        gNodesDispatchingSimulatedClicks = new HashSet<EventTargetNode*>;
    else if (gNodesDispatchingSimulatedClicks->contains(this))
        return;
    
    gNodesDispatchingSimulatedClicks->add(this);
    
    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(eventNames().mousedownEvent, event.get());
    setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(eventNames().mouseupEvent, event.get());
    setActive(false);

    // always send click
    dispatchSimulatedMouseEvent(eventNames().clickEvent, event);
    
    gNodesDispatchingSimulatedClicks->remove(this);
}

bool EventTargetNode::dispatchMouseEvent(const AtomicString& eventType, int button, int detail,
    int pageX, int pageY, int screenX, int screenY,
    bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, 
    bool isSimulated, Node* relatedTargetArg, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    if (disabled()) // Don't even send DOM events for disabled controls..
        return true;
    
    if (eventType.isEmpty())
        return false; // Shouldn't happen.
    
    // Dispatching the first event can easily result in this node being destroyed.
    // Since we dispatch up to three events here, we need to make sure we're referenced
    // so the pointer will be good for the two subsequent ones.
    RefPtr<Node> protect(this);
    
    bool cancelable = eventType != eventNames().mousemoveEvent;
    
    ExceptionCode ec = 0;
    
    bool swallowEvent = false;
    
    // Attempting to dispatch with a non-EventTarget relatedTarget causes the relatedTarget to be silently ignored.
    RefPtr<EventTargetNode> relatedTarget = (relatedTargetArg && relatedTargetArg->isEventTargetNode())
        ? static_cast<EventTargetNode*>(relatedTargetArg) : 0;

    if (Frame* frame = document()->frame()) {
        float pageZoom = frame->pageZoomFactor();
        if (pageZoom != 1.0f) {
            // Adjust our pageX and pageY to account for the page zoom.
            pageX = lroundf(pageX / pageZoom);
            pageY = lroundf(pageY / pageZoom);
        }
    }

    RefPtr<Event> mouseEvent = MouseEvent::create(eventType,
        true, cancelable, document()->defaultView(),
        detail, screenX, screenY, pageX, pageY,
        ctrlKey, altKey, shiftKey, metaKey, button,
        relatedTarget, 0, isSimulated);
    mouseEvent->setUnderlyingEvent(underlyingEvent.get());
    
    dispatchEvent(mouseEvent, ec);
    bool defaultHandled = mouseEvent->defaultHandled();
    bool defaultPrevented = mouseEvent->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;
    
    // Special case: If it's a double click event, we also send the dblclick event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute.  This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == eventNames().clickEvent && detail == 2) {
        RefPtr<Event> doubleClickEvent = MouseEvent::create(eventNames().dblclickEvent,
            true, cancelable, document()->defaultView(),
            detail, screenX, screenY, pageX, pageY,
            ctrlKey, altKey, shiftKey, metaKey, button,
            relatedTarget, 0, isSimulated);
        doubleClickEvent->setUnderlyingEvent(underlyingEvent.get());
        if (defaultHandled)
            doubleClickEvent->setDefaultHandled();
        dispatchEvent(doubleClickEvent, ec);
        if (doubleClickEvent->defaultHandled() || doubleClickEvent->defaultPrevented())
            swallowEvent = true;
    }

    return swallowEvent;
}

void EventTargetNode::dispatchWheelEvent(PlatformWheelEvent& e)
{
    ASSERT(!eventDispatchForbidden());
    if (e.deltaX() == 0 && e.deltaY() == 0)
        return;
    
    FrameView* view = document()->view();
    if (!view)
        return;
    
    IntPoint pos = view->windowToContents(e.pos());
    
    // Convert the deltas from pixels to lines if we have a pixel scroll event.
    float deltaX = e.deltaX();
    float deltaY = e.deltaY();
    
    // FIXME: Should we do anything with a ScrollByPageWheelEvent here?
    // It will be treated like a line scroll of 1 right now.
    if (e.granularity() == ScrollByPixelWheelEvent) {
        deltaX /= cMouseWheelPixelsPerLineStep;
        deltaY /= cMouseWheelPixelsPerLineStep;
    }

    RefPtr<WheelEvent> we = WheelEvent::create(e.deltaX(), e.deltaY(),
        document()->defaultView(), e.globalX(), e.globalY(), pos.x(), pos.y(),
        e.ctrlKey(), e.altKey(), e.shiftKey(), e.metaKey());
    ExceptionCode ec = 0;
    if (!dispatchEvent(we.release(), ec))
        e.accept();
}

bool EventTargetNode::dispatchWebKitAnimationEvent(const AtomicString& eventType, const String& animationName, double elapsedTime)
{
    ASSERT(!eventDispatchForbidden());
    
    ExceptionCode ec = 0;
    return dispatchEvent(WebKitAnimationEvent::create(eventType, animationName, elapsedTime), ec);
}

bool EventTargetNode::dispatchWebKitTransitionEvent(const AtomicString& eventType, const String& propertyName, double elapsedTime)
{
    ASSERT(!eventDispatchForbidden());
    
    ExceptionCode ec = 0;
    return dispatchEvent(WebKitTransitionEvent::create(eventType, propertyName, elapsedTime), ec);
}

void EventTargetNode::dispatchFocusEvent()
{
    dispatchEventForType(eventNames().focusEvent, false, false);
}

void EventTargetNode::dispatchBlurEvent()
{
    dispatchEventForType(eventNames().blurEvent, false, false);
}

bool EventTargetNode::dispatchEventForType(const AtomicString& eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(Event::create(eventType, canBubbleArg, cancelableArg), ec);
}

bool EventTargetNode::dispatchProgressEvent(const AtomicString &eventType, bool lengthComputableArg, unsigned loadedArg, unsigned totalArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(ProgressEvent::create(eventType, lengthComputableArg, loadedArg, totalArg), ec);
}

void EventTargetNode::dispatchStorageEvent(const AtomicString &eventType, const String& key, const String& oldValue, const String& newValue, Frame* source)
{
#if ENABLE(DOM_STORAGE)
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    dispatchEvent(StorageEvent::create(eventType, key, oldValue, newValue, source->document()->documentURI(), source->domWindow()), ec);
#endif
}

void EventTargetNode::removeInlineEventListenerForType(const AtomicString& eventType)
{
    if (!hasRareData())
        return;

    RegisteredEventListenerVector* listeners = rareData()->listeners();
    if (!listeners)
        return;

    size_t size = listeners->size();
    for (size_t i = 0; i < size; ++i) {
        RegisteredEventListener& r = *listeners->at(i);
        if (r.eventType() != eventType || !r.listener()->isInline())
            continue;

        r.setRemoved(true);
        listeners->remove(i);

        // removed last
        if (listeners->isEmpty() && !inDocument())
            document()->unregisterDisconnectedNodeWithEventListeners(this);

        updateSVGElementInstancesAfterEventListenerChange(this);
        return;
    }
}

void EventTargetNode::setInlineEventListenerForType(const AtomicString& eventType, PassRefPtr<EventListener> listener)
{
    // In case we are the only one holding a reference to it, we don't want removeInlineEventListenerForType to destroy it.
    removeInlineEventListenerForType(eventType);
    if (listener)
        addEventListener(eventType, listener, false);
}

void EventTargetNode::setInlineEventListenerForTypeAndAttribute(const AtomicString& eventType, Attribute* attr)
{
    setInlineEventListenerForType(eventType, document()->createEventListener(attr->localName().string(), attr->value(), this));
}

EventListener* EventTargetNode::inlineEventListenerForType(const AtomicString& eventType) const
{
    const RegisteredEventListenerVector& listeners = eventListeners();
    size_t size = listeners.size();
    for (size_t i = 0; i < size; ++i) {
        const RegisteredEventListener& r = *listeners[i];
        if (r.eventType() == eventType && r.listener()->isInline())
            return r.listener();
    }
    return 0;
}

bool EventTargetNode::disabled() const
{
    return false;
}

void EventTargetNode::defaultEventHandler(Event* event)
{
    if (event->target() != this)
        return;
    const AtomicString& eventType = event->type();
    if (eventType == eventNames().keydownEvent || eventType == eventNames().keypressEvent) {
        if (event->isKeyboardEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultKeyboardEventHandler(static_cast<KeyboardEvent*>(event));
    } else if (eventType == eventNames().clickEvent) {
        int detail = event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
        dispatchUIEvent(eventNames().DOMActivateEvent, detail, event);
    } else if (eventType == eventNames().contextmenuEvent) {
        if (Frame* frame = document()->frame())
            if (Page* page = frame->page())
                page->contextMenuController()->handleContextMenuEvent(event);
    } else if (eventType == eventNames().textInputEvent) {
        if (event->isTextEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultTextInputEventHandler(static_cast<TextEvent*>(event));
    }
}

EventListener* EventTargetNode::onabort() const
{
    return inlineEventListenerForType(eventNames().abortEvent);
}

void EventTargetNode::setOnabort(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().abortEvent, eventListener);
}

EventListener* EventTargetNode::onblur() const
{
    return inlineEventListenerForType(eventNames().blurEvent);
}

void EventTargetNode::setOnblur(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().blurEvent, eventListener);
}

EventListener* EventTargetNode::onchange() const
{
    return inlineEventListenerForType(eventNames().changeEvent);
}

void EventTargetNode::setOnchange(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().changeEvent, eventListener);
}

EventListener* EventTargetNode::onclick() const
{
    return inlineEventListenerForType(eventNames().clickEvent);
}

void EventTargetNode::setOnclick(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().clickEvent, eventListener);
}

EventListener* EventTargetNode::oncontextmenu() const
{
    return inlineEventListenerForType(eventNames().contextmenuEvent);
}

void EventTargetNode::setOncontextmenu(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().contextmenuEvent, eventListener);
}

EventListener* EventTargetNode::ondblclick() const
{
    return inlineEventListenerForType(eventNames().dblclickEvent);
}

void EventTargetNode::setOndblclick(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dblclickEvent, eventListener);
}

EventListener* EventTargetNode::onerror() const
{
    return inlineEventListenerForType(eventNames().errorEvent);
}

void EventTargetNode::setOnerror(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().errorEvent, eventListener);
}

EventListener* EventTargetNode::onfocus() const
{
    return inlineEventListenerForType(eventNames().focusEvent);
}

void EventTargetNode::setOnfocus(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().focusEvent, eventListener);
}

EventListener* EventTargetNode::oninput() const
{
    return inlineEventListenerForType(eventNames().inputEvent);
}

void EventTargetNode::setOninput(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().inputEvent, eventListener);
}

EventListener* EventTargetNode::onkeydown() const
{
    return inlineEventListenerForType(eventNames().keydownEvent);
}

void EventTargetNode::setOnkeydown(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().keydownEvent, eventListener);
}

EventListener* EventTargetNode::onkeypress() const
{
    return inlineEventListenerForType(eventNames().keypressEvent);
}

void EventTargetNode::setOnkeypress(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().keypressEvent, eventListener);
}

EventListener* EventTargetNode::onkeyup() const
{
    return inlineEventListenerForType(eventNames().keyupEvent);
}

void EventTargetNode::setOnkeyup(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().keyupEvent, eventListener);
}

EventListener* EventTargetNode::onload() const
{
    return inlineEventListenerForType(eventNames().loadEvent);
}

void EventTargetNode::setOnload(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().loadEvent, eventListener);
}

EventListener* EventTargetNode::onmousedown() const
{
    return inlineEventListenerForType(eventNames().mousedownEvent);
}

void EventTargetNode::setOnmousedown(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mousedownEvent, eventListener);
}

EventListener* EventTargetNode::onmousemove() const
{
    return inlineEventListenerForType(eventNames().mousemoveEvent);
}

void EventTargetNode::setOnmousemove(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mousemoveEvent, eventListener);
}

EventListener* EventTargetNode::onmouseout() const
{
    return inlineEventListenerForType(eventNames().mouseoutEvent);
}

void EventTargetNode::setOnmouseout(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mouseoutEvent, eventListener);
}

EventListener* EventTargetNode::onmouseover() const
{
    return inlineEventListenerForType(eventNames().mouseoverEvent);
}

void EventTargetNode::setOnmouseover(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mouseoverEvent, eventListener);
}

EventListener* EventTargetNode::onmouseup() const
{
    return inlineEventListenerForType(eventNames().mouseupEvent);
}

void EventTargetNode::setOnmouseup(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mouseupEvent, eventListener);
}

EventListener* EventTargetNode::onmousewheel() const
{
    return inlineEventListenerForType(eventNames().mousewheelEvent);
}

void EventTargetNode::setOnmousewheel(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().mousewheelEvent, eventListener);
}

EventListener* EventTargetNode::onbeforecut() const
{
    return inlineEventListenerForType(eventNames().beforecutEvent);
}

void EventTargetNode::setOnbeforecut(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().beforecutEvent, eventListener);
}

EventListener* EventTargetNode::oncut() const
{
    return inlineEventListenerForType(eventNames().cutEvent);
}

void EventTargetNode::setOncut(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().cutEvent, eventListener);
}

EventListener* EventTargetNode::onbeforecopy() const
{
    return inlineEventListenerForType(eventNames().beforecopyEvent);
}

void EventTargetNode::setOnbeforecopy(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().beforecopyEvent, eventListener);
}

EventListener* EventTargetNode::oncopy() const
{
    return inlineEventListenerForType(eventNames().copyEvent);
}

void EventTargetNode::setOncopy(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().copyEvent, eventListener);
}

EventListener* EventTargetNode::onbeforepaste() const
{
    return inlineEventListenerForType(eventNames().beforepasteEvent);
}

void EventTargetNode::setOnbeforepaste(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().beforepasteEvent, eventListener);
}

EventListener* EventTargetNode::onpaste() const
{
    return inlineEventListenerForType(eventNames().pasteEvent);
}

void EventTargetNode::setOnpaste(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().pasteEvent, eventListener);
}

EventListener* EventTargetNode::ondragenter() const
{
    return inlineEventListenerForType(eventNames().dragenterEvent);
}

void EventTargetNode::setOndragenter(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dragenterEvent, eventListener);
}

EventListener* EventTargetNode::ondragover() const
{
    return inlineEventListenerForType(eventNames().dragoverEvent);
}

void EventTargetNode::setOndragover(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dragoverEvent, eventListener);
}

EventListener* EventTargetNode::ondragleave() const
{
    return inlineEventListenerForType(eventNames().dragleaveEvent);
}

void EventTargetNode::setOndragleave(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dragleaveEvent, eventListener);
}

EventListener* EventTargetNode::ondrop() const
{
    return inlineEventListenerForType(eventNames().dropEvent);
}

void EventTargetNode::setOndrop(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dropEvent, eventListener);
}

EventListener* EventTargetNode::ondragstart() const
{
    return inlineEventListenerForType(eventNames().dragstartEvent);
}

void EventTargetNode::setOndragstart(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dragstartEvent, eventListener);
}

EventListener* EventTargetNode::ondrag() const
{
    return inlineEventListenerForType(eventNames().dragEvent);
}

void EventTargetNode::setOndrag(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dragEvent, eventListener);
}

EventListener* EventTargetNode::ondragend() const
{
    return inlineEventListenerForType(eventNames().dragendEvent);
}

void EventTargetNode::setOndragend(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().dragendEvent, eventListener);
}

EventListener* EventTargetNode::onreset() const
{
    return inlineEventListenerForType(eventNames().resetEvent);
}

void EventTargetNode::setOnreset(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().resetEvent, eventListener);
}

EventListener* EventTargetNode::onresize() const
{
    return inlineEventListenerForType(eventNames().resizeEvent);
}

void EventTargetNode::setOnresize(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().resizeEvent, eventListener);
}

EventListener* EventTargetNode::onscroll() const
{
    return inlineEventListenerForType(eventNames().scrollEvent);
}

void EventTargetNode::setOnscroll(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().scrollEvent, eventListener);
}

EventListener* EventTargetNode::onsearch() const
{
    return inlineEventListenerForType(eventNames().searchEvent);
}

void EventTargetNode::setOnsearch(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().searchEvent, eventListener);
}

EventListener* EventTargetNode::onselect() const
{
    return inlineEventListenerForType(eventNames().selectEvent);
}

void EventTargetNode::setOnselect(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().selectEvent, eventListener);
}

EventListener* EventTargetNode::onselectstart() const
{
    return inlineEventListenerForType(eventNames().selectstartEvent);
}

void EventTargetNode::setOnselectstart(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().selectstartEvent, eventListener);
}

EventListener* EventTargetNode::onsubmit() const
{
    return inlineEventListenerForType(eventNames().submitEvent);
}

void EventTargetNode::setOnsubmit(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().submitEvent, eventListener);
}

EventListener* EventTargetNode::onunload() const
{
    return inlineEventListenerForType(eventNames().unloadEvent);
}

void EventTargetNode::setOnunload(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(eventNames().unloadEvent, eventListener);
}

} // namespace WebCore
