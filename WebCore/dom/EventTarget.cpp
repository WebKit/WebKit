/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007 Nikolas Zimmermann <zimmermann@kde.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "EventTarget.h"

#include "Node.h"
#include "NodeList.h"
#include "Document.h"
#include "Event.h"
#include "EventListener.h"
#include "EventNames.h"
#include "Frame.h"
#include "FrameView.h"
#include "kjs_proxy.h"
#include "RegisteredEventListener.h"

namespace WebCore {

using namespace EventNames;

#ifndef NDEBUG
static int gEventDispatchForbidden = 0;
#endif

EventTarget::~EventTarget()
{
}

EventTargetNode* EventTarget::toNode()
{
    return 0;
}

XMLHttpRequest* EventTarget::toXMLHttpRequest()
{
    return 0;
}

#if ENABLE(SVG)
SVGElementInstance* EventTarget::toSVGElementInstance()
{
    return 0;
}
#endif

static inline void addListenerTypeToDocumentIfNeeded(const AtomicString& eventType, Document* document)
{
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
        document->addListenerType(type);
}

void EventTarget::addEventListener(EventTargetNode* referenceNode, const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
{
    ASSERT(referenceNode);
    if (!referenceNode->document()->attached())
        return;

    addListenerTypeToDocumentIfNeeded(eventType, referenceNode->document());

    if (!referenceNode->m_regdListeners)
        referenceNode->m_regdListeners = new RegisteredEventListenerList;

    // Remove existing identical listener set with identical arguments.
    // The DOM2 spec says that "duplicate instances are discarded" in this case.
    removeEventListener(referenceNode, eventType, listener.get(), useCapture);

    // adding the first one
    if (referenceNode->m_regdListeners->isEmpty() && !referenceNode->inDocument())
        referenceNode->document()->registerDisconnectedNodeWithEventListeners(referenceNode);

    referenceNode->m_regdListeners->append(new RegisteredEventListener(eventType, listener.get(), useCapture));
}

void EventTarget::removeEventListener(EventTargetNode* referenceNode, const AtomicString& eventType, EventListener* listener, bool useCapture)
{
    ASSERT(referenceNode);
    if (!referenceNode->m_regdListeners)
        return;

    RegisteredEventListener rl(eventType, listener, useCapture);

    RegisteredEventListenerList::Iterator end = referenceNode->m_regdListeners->end();
    for (RegisteredEventListenerList::Iterator it = referenceNode->m_regdListeners->begin(); it != end; ++it) {
         if (*(*it).get() == rl) {
            (*it)->setRemoved(true);
            it = referenceNode->m_regdListeners->remove(it);

            // removed last
            if (referenceNode->m_regdListeners->isEmpty() && !referenceNode->inDocument())
                referenceNode->document()->unregisterDisconnectedNodeWithEventListeners(referenceNode);

            return;
        }
    }
}

bool EventTarget::dispatchGenericEvent(EventTargetNode* referenceNode, PassRefPtr<Event> e, ExceptionCode&, bool tempEvent)
{
    RefPtr<Event> evt(e);

    ASSERT(!eventDispatchForbidden());
    ASSERT(evt->target());
    ASSERT(!evt->type().isNull()); // JavaScript code could create an event with an empty name

    // work out what nodes to send event to
    DeprecatedPtrList<Node> nodeChain;

    if (referenceNode->inDocument()) {
        for (Node* n = referenceNode; n; n = n->eventParentNode()) {
            n->ref();
            nodeChain.prepend(n);
        }
    } else {
        // if node is not in the document just send event to itself 
        referenceNode->ref();
        nodeChain.prepend(referenceNode);
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
    for (; it.current() && it.current() != referenceNode && !evt->propagationStopped(); ++it) {
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

    // If tempEvent is true, this means that the DOM implementation
    // will not be storing a reference to the event, i.e.  there is no
    // way to retrieve it from javascript if a script does not already
    // have a reference to it in a variable.  So there is no need for
    // the interpreter to keep the event in it's cache
    Frame* frame = referenceNode->document()->frame();
    if (tempEvent && frame && frame->scriptProxy()->isEnabled())
        frame->scriptProxy()->finishedWithEvent(evt.get());

    return !evt->defaultPrevented(); // ### what if defaultPrevented was called before dispatchEvent?
}

void EventTarget::removeAllEventListeners(EventTargetNode* referenceNode)
{
    delete referenceNode->m_regdListeners;
    referenceNode->m_regdListeners = 0;
}

void EventTarget::insertedIntoDocument(EventTargetNode* referenceNode)
{
    if (referenceNode && referenceNode->m_regdListeners && !referenceNode->m_regdListeners->isEmpty())
        referenceNode->document()->unregisterDisconnectedNodeWithEventListeners(referenceNode);
}

void EventTarget::removedFromDocument(EventTargetNode* referenceNode)
{
    if (referenceNode && referenceNode->m_regdListeners && !referenceNode->m_regdListeners->isEmpty())
        referenceNode->document()->registerDisconnectedNodeWithEventListeners(referenceNode);
}

void EventTarget::handleLocalEvents(EventTargetNode* referenceNode, Event* evt, bool useCapture)
{
    ASSERT(referenceNode);
    if (!referenceNode->m_regdListeners || referenceNode->m_regdListeners->isEmpty())
        return;

    RegisteredEventListenerList listenersCopy = *referenceNode->m_regdListeners;
    RegisteredEventListenerList::Iterator end = listenersCopy.end();

    for (RegisteredEventListenerList::Iterator it = listenersCopy.begin(); it != end; ++it) {
        if ((*it)->eventType() == evt->type() && (*it)->useCapture() == useCapture && !(*it)->removed())
            (*it)->listener()->handleEvent(evt, false);
    }
}

EventTarget* EventTarget::eventTargetRespectingSVGTargetRules(EventTargetNode*& referenceNode)
{
    // TODO: SVG will add logic here soon.
    return referenceNode;
}

#ifndef NDEBUG
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
#endif // NDEBUG

} // end namespace
