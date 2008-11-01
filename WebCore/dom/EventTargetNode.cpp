/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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

using namespace EventNames;
    
static HashSet<EventTargetNode*>* gNodesDispatchingSimulatedClicks = 0; 

EventTargetNode::EventTargetNode(Document* doc, bool isElement, bool isContainer)
    : Node(doc, isElement, isContainer)
    , m_regdListeners(0)
{
}

EventTargetNode::~EventTargetNode()
{
    if (m_regdListeners && !m_regdListeners->isEmpty() && !inDocument())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    delete m_regdListeners;
    m_regdListeners = 0;
}

ScriptExecutionContext* EventTargetNode::scriptExecutionContext() const
{
    return document();
}

void EventTargetNode::insertedIntoDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    Node::insertedIntoDocument();
}

void EventTargetNode::removedFromDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        document()->registerDisconnectedNodeWithEventListeners(this);

    Node::removedFromDocument();
}

void EventTargetNode::willMoveToNewOwnerDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
        document()->unregisterDisconnectedNodeWithEventListeners(this);

    Node::willMoveToNewOwnerDocument();
}

void EventTargetNode::didMoveToNewOwnerDocument()
{
    if (m_regdListeners && !m_regdListeners->isEmpty())
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
    Document* doc = document();
    if (!doc->attached())
        return;

    doc->addListenerTypeIfNeeded(eventType);

    if (!m_regdListeners)
        m_regdListeners = new RegisteredEventListenerList;

    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener.get(), useCapture);

    // adding the first one
    if (m_regdListeners->isEmpty() && !inDocument())
        doc->registerDisconnectedNodeWithEventListeners(this);

    m_regdListeners->append(RegisteredEventListener::create(eventType, listener, useCapture));
    updateSVGElementInstancesAfterEventListenerChange(this);
}

void EventTargetNode::removeEventListener(const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    if (!m_regdListeners)
        return;

    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it) {
        RegisteredEventListener& r = **it;
        if (r.eventType() == eventType && r.listener() == listener && r.useCapture() == useCapture) {
            (*it)->setRemoved(true);
            it = m_regdListeners->remove(it);

            // removed last
            if (m_regdListeners->isEmpty() && !inDocument())
                document()->unregisterDisconnectedNodeWithEventListeners(this);

            updateSVGElementInstancesAfterEventListenerChange(this);
            return;
        }
    }
}

void EventTargetNode::removeAllEventListeners()
{
    delete m_regdListeners;
    m_regdListeners = 0;
}

void EventTargetNode::handleLocalEvents(Event *evt, bool useCapture)
{
    if (disabled() && evt->isMouseEvent())
        return;

    if (!m_regdListeners || m_regdListeners->isEmpty())
        return;

    RegisteredEventListenerList listenersCopy = *m_regdListeners;
    RegisteredEventListenerList::Iterator end = listenersCopy.end();

    for (RegisteredEventListenerList::Iterator it = listenersCopy.begin(); it != end; ++it) {
        if ((*it)->eventType() == evt->type() && (*it)->useCapture() == useCapture && !(*it)->removed())
            (*it)->listener()->handleEvent(evt, false);
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
    return dispatchGenericEvent(evt.release(), ec);
}

bool EventTargetNode::dispatchGenericEvent(PassRefPtr<Event> e, ExceptionCode& ec)
{
    RefPtr<Event> evt(e);

    ASSERT(!eventDispatchForbidden());
    ASSERT(evt->target());
    ASSERT(!evt->type().isNull()); // JavaScript code could create an event with an empty name

    // work out what nodes to send event to
    DeprecatedPtrList<Node> nodeChain;

    if (inDocument()) {
            for (Node* n = this; n; n = n->eventParentNode()) {
#if ENABLE(SVG)
            // Skip <use> shadow tree elements    
            if (n->isSVGElement() && n->isShadowNode())
                continue;
#endif

            n->ref();
            nodeChain.prepend(n);
        }
    } else {
        // if node is not in the document just send event to itself 
        ref();
        nodeChain.prepend(this);
    }

    DeprecatedPtrListIterator<Node> it(nodeChain);

    // Before we begin dispatching events, give the target node a chance to do some work prior
    // to the DOM event handlers getting a crack.
    void* data = preDispatchEventHandler(evt.get());

    // trigger any capturing event handlers on our way down
    evt->setEventPhase(Event::CAPTURING_PHASE);
    it.toFirst();

    // Handle window events for capture phase, except load events, this quirk is needed
    // because Mozilla used to never propagate load events to the window object
    if (evt->type() != loadEvent && it.current()->isDocumentNode() && !evt->propagationStopped())
        static_cast<Document*>(it.current())->handleWindowEvent(evt.get(), true);

    EventTargetNode* eventTargetNode = 0;
    for (; it.current() && it.current() != this && !evt->propagationStopped(); ++it) {
        eventTargetNode = EventTargetNodeCast(it.current());
        evt->setCurrentTarget(eventTargetRespectingSVGTargetRules(eventTargetNode));

        eventTargetNode->handleLocalEvents(evt.get(), true);
    }

    // dispatch to the actual target node
    it.toLast();

    if (!evt->propagationStopped()) {
        evt->setEventPhase(Event::AT_TARGET);

        eventTargetNode = EventTargetNodeCast(it.current());
        evt->setCurrentTarget(eventTargetRespectingSVGTargetRules(eventTargetNode));

        // We do want capturing event listeners to be invoked here, even though
        // that violates the specification since Mozilla does it.
        eventTargetNode->handleLocalEvents(evt.get(), true);

        eventTargetNode->handleLocalEvents(evt.get(), false);
    }

    --it;

    // ok, now bubble up again (only non-capturing event handlers will be called)
    // ### recalculate the node chain here? (e.g. if target node moved in document by previous event handlers)
    // no. the DOM specs says:
    // The chain of EventTargets from the event target to the top of the tree
    // is determined before the initial dispatch of the event.
    // If modifications occur to the tree during event processing,
    // event flow will proceed based on the initial state of the tree.
    //
    // since the initial dispatch is before the capturing phase,
    // there's no need to recalculate the node chain.
    // (tobias)

    if (evt->bubbles()) {
        evt->setEventPhase(Event::BUBBLING_PHASE);

        for (; it.current() && !evt->propagationStopped() && !evt->cancelBubble(); --it) {
            eventTargetNode = EventTargetNodeCast(it.current());
            evt->setCurrentTarget(eventTargetRespectingSVGTargetRules(eventTargetNode));

            eventTargetNode->handleLocalEvents(evt.get(), false);
        }

        it.toFirst();

        // Handle window events for bubbling phase, except load events, this quirk is needed
        // because Mozilla used to never propagate load events at all
        if (evt->type() != loadEvent && it.current()->isDocumentNode() && !evt->propagationStopped() && !evt->cancelBubble()) {
            evt->setCurrentTarget(EventTargetNodeCast(it.current()));
            static_cast<Document*>(it.current())->handleWindowEvent(evt.get(), false);
        } 
    }

    evt->setCurrentTarget(0);
    evt->setEventPhase(0); // I guess this is correct, the spec does not seem to say
                           // anything about the default event handler phase.


    // Now call the post dispatch.
    postDispatchEventHandler(evt.get(), data);

    // now we call all default event handlers (this is not part of DOM - it is internal to WebCore)
    it.toLast();

    if (evt->bubbles())
        for (; it.current() && !evt->defaultPrevented() && !evt->defaultHandled(); --it)
            EventTargetNodeCast(it.current())->defaultEventHandler(evt.get());
    else if (!evt->defaultPrevented() && !evt->defaultHandled())
        EventTargetNodeCast(it.current())->defaultEventHandler(evt.get());

    // deref all nodes in chain
    it.toFirst();
    for (; it.current(); ++it)
        it.current()->deref(); // this may delete us

    Document::updateDocumentsRendering();

    return !evt->defaultPrevented(); // ### what if defaultPrevented was called before dispatchEvent?
}

bool EventTargetNode::dispatchSubtreeModifiedEvent()
{
    ASSERT(!eventDispatchForbidden());
    
    document()->incDOMTreeVersion();

    notifyNodeListsAttributeChanged(); // FIXME: Can do better some day. Really only care about the name attribute changing.
    
    if (!document()->hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return false;
    ExceptionCode ec = 0;
    return dispatchEvent(MutationEvent::create(DOMSubtreeModifiedEvent, true, false, 0, String(), String(), String(), 0), ec);
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
    
    if (eventType == loadEvent) {
        // For onload events, send a separate load event to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.
        Element* ownerElement = doc->ownerElement();
        if (ownerElement) {
            RefPtr<Event> ownerEvent = Event::create(eventType, false, cancelableArg);
            ownerEvent->setTarget(ownerElement);
            ExceptionCode ec = 0;
            ownerElement->dispatchGenericEvent(ownerEvent.release(), ec);
        }
    }
}

bool EventTargetNode::dispatchUIEvent(const AtomicString& eventType, int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    ASSERT(eventType == DOMFocusInEvent || eventType == DOMFocusOutEvent || eventType == DOMActivateEvent);
    
    bool cancelable = eventType == DOMActivateEvent;
    
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
        dispatchSimulatedMouseEvent(mousedownEvent, event.get());
    setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(mouseupEvent, event.get());
    setActive(false);

    // always send click
    dispatchSimulatedMouseEvent(clickEvent, event);
    
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
    
    bool cancelable = eventType != mousemoveEvent;
    
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
    if (eventType == clickEvent && detail == 2) {
        RefPtr<Event> doubleClickEvent = MouseEvent::create(dblclickEvent,
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
    dispatchEventForType(focusEvent, false, false);
}

void EventTargetNode::dispatchBlurEvent()
{
    dispatchEventForType(blurEvent, false, false);
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
    if (!m_regdListeners) // nothing to remove
        return;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it) {
        EventListener* listener = (*it)->listener();
        if ((*it)->eventType() != eventType || !listener->isInline())
            continue;

        it = m_regdListeners->remove(it);

        // removed last
        if (m_regdListeners->isEmpty() && !inDocument())
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
    if (!m_regdListeners)
        return 0;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it)
        if ((*it)->eventType() == eventType && (*it)->listener()->isInline())
            return (*it)->listener();
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
    if (eventType == keydownEvent || eventType == keypressEvent) {
        if (event->isKeyboardEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultKeyboardEventHandler(static_cast<KeyboardEvent*>(event));
    } else if (eventType == clickEvent) {
        int detail = event->isUIEvent() ? static_cast<UIEvent*>(event)->detail() : 0;
        dispatchUIEvent(DOMActivateEvent, detail, event);
    } else if (eventType == contextmenuEvent) {
        if (Frame* frame = document()->frame())
            if (Page* page = frame->page())
                page->contextMenuController()->handleContextMenuEvent(event);
    } else if (eventType == textInputEvent) {
        if (event->isTextEvent())
            if (Frame* frame = document()->frame())
                frame->eventHandler()->defaultTextInputEventHandler(static_cast<TextEvent*>(event));
    }
}

EventListener* EventTargetNode::onabort() const
{
    return inlineEventListenerForType(abortEvent);
}

void EventTargetNode::setOnabort(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(abortEvent, eventListener);
}

EventListener* EventTargetNode::onblur() const
{
    return inlineEventListenerForType(blurEvent);
}

void EventTargetNode::setOnblur(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(blurEvent, eventListener);
}

EventListener* EventTargetNode::onchange() const
{
    return inlineEventListenerForType(changeEvent);
}

void EventTargetNode::setOnchange(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(changeEvent, eventListener);
}

EventListener* EventTargetNode::onclick() const
{
    return inlineEventListenerForType(clickEvent);
}

void EventTargetNode::setOnclick(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(clickEvent, eventListener);
}

EventListener* EventTargetNode::oncontextmenu() const
{
    return inlineEventListenerForType(contextmenuEvent);
}

void EventTargetNode::setOncontextmenu(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(contextmenuEvent, eventListener);
}

EventListener* EventTargetNode::ondblclick() const
{
    return inlineEventListenerForType(dblclickEvent);
}

void EventTargetNode::setOndblclick(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dblclickEvent, eventListener);
}

EventListener* EventTargetNode::onerror() const
{
    return inlineEventListenerForType(errorEvent);
}

void EventTargetNode::setOnerror(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(errorEvent, eventListener);
}

EventListener* EventTargetNode::onfocus() const
{
    return inlineEventListenerForType(focusEvent);
}

void EventTargetNode::setOnfocus(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(focusEvent, eventListener);
}

EventListener* EventTargetNode::oninput() const
{
    return inlineEventListenerForType(inputEvent);
}

void EventTargetNode::setOninput(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(inputEvent, eventListener);
}

EventListener* EventTargetNode::onkeydown() const
{
    return inlineEventListenerForType(keydownEvent);
}

void EventTargetNode::setOnkeydown(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(keydownEvent, eventListener);
}

EventListener* EventTargetNode::onkeypress() const
{
    return inlineEventListenerForType(keypressEvent);
}

void EventTargetNode::setOnkeypress(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(keypressEvent, eventListener);
}

EventListener* EventTargetNode::onkeyup() const
{
    return inlineEventListenerForType(keyupEvent);
}

void EventTargetNode::setOnkeyup(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(keyupEvent, eventListener);
}

EventListener* EventTargetNode::onload() const
{
    return inlineEventListenerForType(loadEvent);
}

void EventTargetNode::setOnload(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(loadEvent, eventListener);
}

EventListener* EventTargetNode::onmousedown() const
{
    return inlineEventListenerForType(mousedownEvent);
}

void EventTargetNode::setOnmousedown(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(mousedownEvent, eventListener);
}

EventListener* EventTargetNode::onmousemove() const
{
    return inlineEventListenerForType(mousemoveEvent);
}

void EventTargetNode::setOnmousemove(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(mousemoveEvent, eventListener);
}

EventListener* EventTargetNode::onmouseout() const
{
    return inlineEventListenerForType(mouseoutEvent);
}

void EventTargetNode::setOnmouseout(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(mouseoutEvent, eventListener);
}

EventListener* EventTargetNode::onmouseover() const
{
    return inlineEventListenerForType(mouseoverEvent);
}

void EventTargetNode::setOnmouseover(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(mouseoverEvent, eventListener);
}

EventListener* EventTargetNode::onmouseup() const
{
    return inlineEventListenerForType(mouseupEvent);
}

void EventTargetNode::setOnmouseup(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(mouseupEvent, eventListener);
}

EventListener* EventTargetNode::onmousewheel() const
{
    return inlineEventListenerForType(mousewheelEvent);
}

void EventTargetNode::setOnmousewheel(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(mousewheelEvent, eventListener);
}

EventListener* EventTargetNode::onbeforecut() const
{
    return inlineEventListenerForType(beforecutEvent);
}

void EventTargetNode::setOnbeforecut(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(beforecutEvent, eventListener);
}

EventListener* EventTargetNode::oncut() const
{
    return inlineEventListenerForType(cutEvent);
}

void EventTargetNode::setOncut(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(cutEvent, eventListener);
}

EventListener* EventTargetNode::onbeforecopy() const
{
    return inlineEventListenerForType(beforecopyEvent);
}

void EventTargetNode::setOnbeforecopy(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(beforecopyEvent, eventListener);
}

EventListener* EventTargetNode::oncopy() const
{
    return inlineEventListenerForType(copyEvent);
}

void EventTargetNode::setOncopy(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(copyEvent, eventListener);
}

EventListener* EventTargetNode::onbeforepaste() const
{
    return inlineEventListenerForType(beforepasteEvent);
}

void EventTargetNode::setOnbeforepaste(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(beforepasteEvent, eventListener);
}

EventListener* EventTargetNode::onpaste() const
{
    return inlineEventListenerForType(pasteEvent);
}

void EventTargetNode::setOnpaste(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(pasteEvent, eventListener);
}

EventListener* EventTargetNode::ondragenter() const
{
    return inlineEventListenerForType(dragenterEvent);
}

void EventTargetNode::setOndragenter(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dragenterEvent, eventListener);
}

EventListener* EventTargetNode::ondragover() const
{
    return inlineEventListenerForType(dragoverEvent);
}

void EventTargetNode::setOndragover(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dragoverEvent, eventListener);
}

EventListener* EventTargetNode::ondragleave() const
{
    return inlineEventListenerForType(dragleaveEvent);
}

void EventTargetNode::setOndragleave(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dragleaveEvent, eventListener);
}

EventListener* EventTargetNode::ondrop() const
{
    return inlineEventListenerForType(dropEvent);
}

void EventTargetNode::setOndrop(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dropEvent, eventListener);
}

EventListener* EventTargetNode::ondragstart() const
{
    return inlineEventListenerForType(dragstartEvent);
}

void EventTargetNode::setOndragstart(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dragstartEvent, eventListener);
}

EventListener* EventTargetNode::ondrag() const
{
    return inlineEventListenerForType(dragEvent);
}

void EventTargetNode::setOndrag(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dragEvent, eventListener);
}

EventListener* EventTargetNode::ondragend() const
{
    return inlineEventListenerForType(dragendEvent);
}

void EventTargetNode::setOndragend(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(dragendEvent, eventListener);
}

EventListener* EventTargetNode::onreset() const
{
    return inlineEventListenerForType(resetEvent);
}

void EventTargetNode::setOnreset(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(resetEvent, eventListener);
}

EventListener* EventTargetNode::onresize() const
{
    return inlineEventListenerForType(resizeEvent);
}

void EventTargetNode::setOnresize(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(resizeEvent, eventListener);
}

EventListener* EventTargetNode::onscroll() const
{
    return inlineEventListenerForType(scrollEvent);
}

void EventTargetNode::setOnscroll(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(scrollEvent, eventListener);
}

EventListener* EventTargetNode::onsearch() const
{
    return inlineEventListenerForType(searchEvent);
}

void EventTargetNode::setOnsearch(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(searchEvent, eventListener);
}

EventListener* EventTargetNode::onselect() const
{
    return inlineEventListenerForType(selectEvent);
}

void EventTargetNode::setOnselect(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(selectEvent, eventListener);
}

EventListener* EventTargetNode::onselectstart() const
{
    return inlineEventListenerForType(selectstartEvent);
}

void EventTargetNode::setOnselectstart(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(selectstartEvent, eventListener);
}

EventListener* EventTargetNode::onsubmit() const
{
    return inlineEventListenerForType(submitEvent);
}

void EventTargetNode::setOnsubmit(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(submitEvent, eventListener);
}

EventListener* EventTargetNode::onunload() const
{
    return inlineEventListenerForType(unloadEvent);
}

void EventTargetNode::setOnunload(PassRefPtr<EventListener> eventListener)
{
    setInlineEventListenerForType(unloadEvent, eventListener);
}

} // namespace WebCore
