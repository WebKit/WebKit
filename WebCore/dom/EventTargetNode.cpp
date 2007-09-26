/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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

#include "ContextMenuController.h"
#include "Document.h"
#include "Element.h"
#include "Event.h"
#include "EventHandler.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FocusController.h"
#include "Frame.h"
#include "FrameView.h"
#include "HTMLNames.h"
#include "TextStream.h"
#include "KeyboardEvent.h"
#include "MouseEvent.h"
#include "MutationEvent.h"
#include "Page.h"
#include "PlatformMouseEvent.h"
#include "PlatformWheelEvent.h"
#include "RegisteredEventListener.h"
#include "TextEvent.h"
#include "UIEvent.h"
#include "WheelEvent.h"
#include "kjs_proxy.h"

namespace WebCore {

using namespace EventNames;
using namespace HTMLNames;

#ifndef NDEBUG
static int gEventDispatchForbidden = 0;
#endif

EventTargetNode::EventTargetNode(Document *doc)
    : Node(doc)
    , m_regdListeners(0)
{
}

EventTargetNode::~EventTargetNode()
{
    if (m_regdListeners && !m_regdListeners->isEmpty() && !inDocument())
        document()->unregisterDisconnectedNodeWithEventListeners(this);
    delete m_regdListeners;
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

void EventTargetNode::addEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener, const bool useCapture)
{
    if (!document()->attached())
        return;
    
    Document::ListenerType type = static_cast<Document::ListenerType>(0);
    if (eventType == DOMSubtreeModifiedEvent)
        type = Document::DOMSUBTREEMODIFIED_LISTENER;
    else if (eventType == DOMNodeInsertedEvent)
        type = Document::DOMNODEINSERTED_LISTENER;
    else if (eventType == DOMNodeRemovedEvent)
        type = Document::DOMNODEREMOVED_LISTENER;
    else if (eventType == DOMNodeRemovedFromDocumentEvent)
        type = Document::DOMNODEREMOVEDFROMDOCUMENT_LISTENER;
    else if (eventType == DOMNodeInsertedIntoDocumentEvent)
        type = Document::DOMNODEINSERTEDINTODOCUMENT_LISTENER;
    else if (eventType == DOMAttrModifiedEvent)
        type = Document::DOMATTRMODIFIED_LISTENER;
    else if (eventType == DOMCharacterDataModifiedEvent)
        type = Document::DOMCHARACTERDATAMODIFIED_LISTENER;
    else if (eventType == overflowchangedEvent)
        type = Document::OVERFLOWCHANGED_LISTENER;
        
    if (type)
        document()->addListenerType(type);
    
    if (!m_regdListeners)
        m_regdListeners = new RegisteredEventListenerList;
    
    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(eventType, listener.get(), useCapture);
    
    // adding the first one
    if (m_regdListeners->isEmpty() && !inDocument())
        document()->registerDisconnectedNodeWithEventListeners(this);
    
    m_regdListeners->append(new RegisteredEventListener(eventType, listener.get(), useCapture));
}

void EventTargetNode::removeEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture)
{
    if (!m_regdListeners) // nothing to remove
        return;
    
    RegisteredEventListener rl(eventType, listener, useCapture);
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it)
        if (*(*it).get() == rl) {
            (*it)->setRemoved(true);
            
            it = m_regdListeners->remove(it);
            // removed last
            if (m_regdListeners->isEmpty() && !inDocument())
                document()->unregisterDisconnectedNodeWithEventListeners(this);
            return;
        }
}

void EventTargetNode::removeAllEventListeners()
{
    delete m_regdListeners;
    m_regdListeners = 0;
}

void EventTargetNode::handleLocalEvents(Event *evt, bool useCapture)
{
    if (!m_regdListeners)
        return;
    
    if (disabled() && evt->isMouseEvent())
        return;
    
    RegisteredEventListenerList listenersCopy = *m_regdListeners;
    RegisteredEventListenerList::Iterator end = listenersCopy.end();

    for (RegisteredEventListenerList::Iterator it = listenersCopy.begin(); it != end; ++it)
        if ((*it)->eventType() == evt->type() && (*it)->useCapture() == useCapture && !(*it)->removed())
            (*it)->listener()->handleEvent(evt, false);
}

bool EventTargetNode::dispatchGenericEvent(PassRefPtr<Event> e, ExceptionCode&, bool tempEvent)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    ASSERT(evt->target());
    ASSERT(!evt->type().isNull()); // JavaScript code could create an event with an empty name
    
    // work out what nodes to send event to
    DeprecatedPtrList<Node> nodeChain;
    
    if (inDocument()) {
        for (Node* n = this; n; n = n->eventParentNode()) {
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
    
    for (; it.current() && it.current() != this && !evt->propagationStopped(); ++it) {
        evt->setCurrentTarget(EventTargetNodeCast(it.current()));
        EventTargetNodeCast(it.current())->handleLocalEvents(evt.get(), true);
    }
    
    // dispatch to the actual target node
    it.toLast();
    if (!evt->propagationStopped()) {
        evt->setEventPhase(Event::AT_TARGET);
        evt->setCurrentTarget(EventTargetNodeCast(it.current()));
        
        // We do want capturing event listeners to be invoked here, even though
        // that violates the specification since Mozilla does it.
        EventTargetNodeCast(it.current())->handleLocalEvents(evt.get(), true);
        
        EventTargetNodeCast(it.current())->handleLocalEvents(evt.get(), false);
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
            evt->setCurrentTarget(EventTargetNodeCast(it.current()));
            EventTargetNodeCast(it.current())->handleLocalEvents(evt.get(), false);
        }
        // Handle window events for bubbling phase, except load events, this quirk is needed
        // because Mozilla used to never propagate load events at all

        it.toFirst();
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
    
    // now we call all default event handlers (this is not part of DOM - it is internal to khtml)
    
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
    
    // If tempEvent is true, this means that the DOM implementation
    // will not be storing a reference to the event, i.e.  there is no
    // way to retrieve it from javascript if a script does not already
    // have a reference to it in a variable.  So there is no need for
    // the interpreter to keep the event in it's cache
    Frame *frame = document()->frame();
    if (tempEvent && frame && frame->scriptProxy())
        frame->scriptProxy()->finishedWithEvent(evt.get());
    
    return !evt->defaultPrevented(); // ### what if defaultPrevented was called before dispatchEvent?
}

bool EventTargetNode::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent)
{
    return dispatchEvent(e, ec, tempEvent, this);
}

bool EventTargetNode::dispatchEvent(PassRefPtr<Event> e, ExceptionCode& ec, bool tempEvent, EventTarget* target)
{
    RefPtr<Event> evt(e);
    ASSERT(!eventDispatchForbidden());
    if (!evt || evt->type().isEmpty()) { 
        ec = UNSPECIFIED_EVENT_TYPE_ERR;
        return false;
    }

    evt->setTarget(target);
    
    RefPtr<FrameView> view = document()->view();
    
    return dispatchGenericEvent(evt.release(), ec, tempEvent);
}

bool EventTargetNode::dispatchSubtreeModifiedEvent(bool sendChildrenChanged)
{
    ASSERT(!eventDispatchForbidden());
    
    // FIXME: Pull this whole if clause out of this function.
    if (sendChildrenChanged) {
        notifyNodeListsChildrenChanged();
        childrenChanged();
    } else
        notifyNodeListsAttributeChanged(); // FIXME: Can do better some day. Really only care about the name attribute changing.
    
    if (!document()->hasListenerType(Document::DOMSUBTREEMODIFIED_LISTENER))
        return false;
    ExceptionCode ec = 0;
    return dispatchEvent(new MutationEvent(DOMSubtreeModifiedEvent,
                                               true,false,0,String(),String(),String(),0),ec,true);
}

void EventTargetNode::dispatchWindowEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<Event> evt = new Event(eventType, canBubbleArg, cancelableArg);
    RefPtr<Document> doc = document();
    evt->setTarget(doc);
    doc->handleWindowEvent(evt.get(), true);
    doc->handleWindowEvent(evt.get(), false);
    
    if (eventType == loadEvent) {
        // For onload events, send a separate load event to the enclosing frame only.
        // This is a DOM extension and is independent of bubbling/capturing rules of
        // the DOM.
        Element* ownerElement = doc->ownerElement();
        if (ownerElement) {
            RefPtr<Event> ownerEvent = new Event(eventType, false, cancelableArg);
            ownerEvent->setTarget(ownerElement);
            ownerElement->dispatchGenericEvent(ownerEvent.release(), ec, true);
        }
    }
}

bool EventTargetNode::dispatchUIEvent(const AtomicString& eventType, int detail, PassRefPtr<Event> underlyingEvent)
{
    ASSERT(!eventDispatchForbidden());
    ASSERT(eventType == DOMFocusInEvent || eventType == DOMFocusOutEvent || eventType == DOMActivateEvent);
    
    bool cancelable = eventType == DOMActivateEvent;
    
    ExceptionCode ec = 0;
    RefPtr<UIEvent> evt = new UIEvent(eventType, true, cancelable, document()->defaultView(), detail);
    evt->setUnderlyingEvent(underlyingEvent);
    return dispatchEvent(evt.release(), ec, true);
}

bool EventTargetNode::dispatchKeyEvent(const PlatformKeyboardEvent& key)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    RefPtr<KeyboardEvent> keyboardEvent = new KeyboardEvent(key, document()->defaultView());
    bool r = dispatchEvent(keyboardEvent,ec,true);
    
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
    
    if (m_dispatchingSimulatedEvent)
        return;

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
    
    m_dispatchingSimulatedEvent = true;

    // Like Gecko, we just pass 0 for everything when we make a fake mouse event.
    // Internet Explorer instead gives the current mouse position and state.
    dispatchMouseEvent(eventType, 0, 0, 0, 0, 0, 0,
        ctrlKey, altKey, shiftKey, metaKey, true, 0, underlyingEvent);
    
    m_dispatchingSimulatedEvent = false;
}

void EventTargetNode::dispatchSimulatedClick(PassRefPtr<Event> event, bool sendMouseEvents, bool showPressedLook)
{
    if (m_dispatchingSimulatedEvent)
        return;
    
    // send mousedown and mouseup before the click, if requested
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(mousedownEvent, event.get());
    setActive(true, showPressedLook);
    if (sendMouseEvents)
        dispatchSimulatedMouseEvent(mouseupEvent, event.get());
    setActive(false);

    // always send click
    dispatchSimulatedMouseEvent(clickEvent, event);
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

    RefPtr<Event> mouseEvent = new MouseEvent(eventType,
        true, cancelable, document()->defaultView(),
        detail, screenX, screenY, pageX, pageY,
        ctrlKey, altKey, shiftKey, metaKey, button,
        relatedTarget.get(), 0, isSimulated);
    mouseEvent->setUnderlyingEvent(underlyingEvent.get());
    
    dispatchEvent(mouseEvent, ec, true);
    bool defaultHandled = mouseEvent->defaultHandled();
    bool defaultPrevented = mouseEvent->defaultPrevented();
    if (defaultHandled || defaultPrevented)
        swallowEvent = true;
    
    // Special case: If it's a double click event, we also send the dblclick event. This is not part
    // of the DOM specs, but is used for compatibility with the ondblclick="" attribute.  This is treated
    // as a separate event in other DOM-compliant browsers like Firefox, and so we do the same.
    if (eventType == clickEvent && detail == 2) {
        RefPtr<Event> doubleClickEvent = new MouseEvent(dblclickEvent,
            true, cancelable, document()->defaultView(),
            detail, screenX, screenY, pageX, pageY,
            ctrlKey, altKey, shiftKey, metaKey, button,
            relatedTarget.get(), 0, isSimulated);
        doubleClickEvent->setUnderlyingEvent(underlyingEvent.get());
        if (defaultHandled)
            doubleClickEvent->setDefaultHandled();
        dispatchEvent(doubleClickEvent, ec, true);
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
    
    RefPtr<WheelEvent> we = new WheelEvent(e.deltaX(), e.deltaY(),
                                           document()->defaultView(), e.globalX(), e.globalY(), pos.x(), pos.y(),
                                           e.ctrlKey(), e.altKey(), e.shiftKey(), e.metaKey());
    ExceptionCode ec = 0;
    if (!dispatchEvent(we, ec, true))
        e.accept();
}


void EventTargetNode::dispatchFocusEvent()
{
    dispatchHTMLEvent(focusEvent, false, false);
}

void EventTargetNode::dispatchBlurEvent()
{
    dispatchHTMLEvent(blurEvent, false, false);
}

bool EventTargetNode::dispatchHTMLEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
{
    ASSERT(!eventDispatchForbidden());
    ExceptionCode ec = 0;
    return dispatchEvent(new Event(eventType, canBubbleArg, cancelableArg), ec, true);
}

void EventTargetNode::removeHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners) // nothing to remove
        return;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it)
        if ((*it)->eventType() == eventType && (*it)->listener()->isHTMLEventListener()) {
            it = m_regdListeners->remove(it);
            // removed last
            if (m_regdListeners->isEmpty() && !inDocument())
                document()->unregisterDisconnectedNodeWithEventListeners(this);
            return;
        }
}

void EventTargetNode::setHTMLEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener)
{
    // In case we are the only one holding a reference to it, we don't want removeHTMLEventListener to destroy it.
    removeHTMLEventListener(eventType);
    if (listener)
        addEventListener(eventType, listener.get(), false);
}

EventListener *EventTargetNode::getHTMLEventListener(const AtomicString &eventType)
{
    if (!m_regdListeners)
        return 0;
    
    RegisteredEventListenerList::Iterator end = m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = m_regdListeners->begin(); it != end; ++it)
        if ((*it)->eventType() == eventType && (*it)->listener()->isHTMLEventListener())
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

#ifndef NDEBUG

void EventTargetNode::dump(TextStream* stream, DeprecatedString ind) const
{
    if (m_regdListeners)
        *stream << " #regdListeners=" << m_regdListeners->count(); // ### more detail
    
    Node::dump(stream,ind);
}

void forbidEventDispatch()
{
    ++gEventDispatchForbidden;
}

void allowEventDispatch()
{
    if (gEventDispatchForbidden > 0)
        --gEventDispatchForbidden;
}

bool eventDispatchForbidden()
{
    return gEventDispatchForbidden > 0;
}

#endif

} // namespace WebCore
