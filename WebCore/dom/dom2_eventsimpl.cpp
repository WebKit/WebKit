/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
 * (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2003, 2005, 2006 Apple Computer, Inc.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "dom2_eventsimpl.h"

#include "Document.h"
#include "EventListener.h"
#include "EventNames.h"
#include "FrameView.h"
#include "SystemTime.h"
#include "PlatformKeyboardEvent.h"
#include "SystemTime.h"
#include "RenderLayer.h"

namespace WebCore {

using namespace EventNames;

Event::Event()
    : m_canBubble(false)
    , m_cancelable(false)
    , m_propagationStopped(false)
    , m_defaultPrevented(false)
    , m_defaultHandled(false)
    , m_cancelBubble(false)
    , m_currentTarget(0)
    , m_eventPhase(0)
    , m_createTime(static_cast<DOMTimeStamp>(currentTime() * 1000.0))
{
}

Event::Event(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
    : m_type(eventType)
    , m_canBubble(canBubbleArg)
    , m_cancelable(cancelableArg)
    , m_propagationStopped(false)
    , m_defaultPrevented(false)
    , m_defaultHandled(false)
    , m_cancelBubble(false)
    , m_currentTarget(0)
    , m_eventPhase(0)
    , m_createTime(static_cast<DOMTimeStamp>(currentTime() * 1000.0))
{
}

Event::~Event()
{
}

void Event::initEvent(const AtomicString &eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    if (dispatched())
        return;

    m_type = eventTypeArg;
    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;
}

bool Event::isUIEvent() const
{
    return false;
}

bool Event::isMouseEvent() const
{
    return false;
}

bool Event::isMutationEvent() const
{
    return false;
}

bool Event::isKeyboardEvent() const
{
    return false;
}

bool Event::isDragEvent() const
{
    return false;
}

bool Event::isClipboardEvent() const
{
    return false;
}

bool Event::isWheelEvent() const
{
    return false;
}

bool Event::isBeforeTextInsertedEvent() const
{
    return false;
}

bool Event::storesResultAsString() const
{
    return false;
}

void Event::storeResult(const String&)
{
}

void Event::setTarget(Node* target)
{
    m_target = target;
    if (target)
        receivedTarget();
}

void Event::receivedTarget()
{
}

// -----------------------------------------------------------------------------

UIEvent::UIEvent()
    : m_detail(0)
{
}

UIEvent::UIEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg, AbstractView *viewArg, int detailArg)
    : Event(eventType, canBubbleArg, cancelableArg)
    , m_view(viewArg)
    , m_detail(detailArg)
{
}

void UIEvent::initUIEvent(const AtomicString &typeArg,
                              bool canBubbleArg,
                              bool cancelableArg,
                              AbstractView *viewArg,
                              int detailArg)
{
    if (dispatched())
        return;

    initEvent(typeArg, canBubbleArg, cancelableArg);

    m_view = viewArg;
    m_detail = detailArg;
}

bool UIEvent::isUIEvent() const
{
    return true;
}

int UIEvent::keyCode() const
{
    return 0;
}

int UIEvent::charCode() const
{
    return 0;
}

int UIEvent::layerX() const
{
    return 0;
}

int UIEvent::layerY() const
{
    return 0;
}

int UIEvent::pageX() const
{
    return 0;
}

int UIEvent::pageY() const
{
    return 0;
}

int UIEvent::which() const
{
    return 0;
}

// -----------------------------------------------------------------------------

MouseRelatedEvent::MouseRelatedEvent()
    : m_screenX(0), m_screenY(0), m_clientX(0), m_clientY(0)
    , m_pageX(0), m_pageY(0), m_layerX(0), m_layerY(0), m_offsetX(0), m_offsetY(0), m_isSimulated(false)
{
}

MouseRelatedEvent::MouseRelatedEvent(const AtomicString &eventType,
                               bool canBubbleArg,
                               bool cancelableArg,
                               AbstractView *viewArg,
                               int detailArg,
                               int screenXArg,
                               int screenYArg,
                               int clientXArg,
                               int clientYArg,
                               bool ctrlKeyArg,
                               bool altKeyArg,
                               bool shiftKeyArg,
                               bool metaKeyArg,
                               bool isSimulated)
    : UIEventWithKeyState(eventType, canBubbleArg, cancelableArg, viewArg, detailArg,
        ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg)
    , m_screenX(screenXArg), m_screenY(screenYArg)
    , m_clientX(clientXArg), m_clientY(clientYArg)
    , m_isSimulated(isSimulated)
{
    initCoordinates();
}

void MouseRelatedEvent::initCoordinates()
{
    // Set up initial values for coordinates.
    // Correct values can't be computed until we have at target, so receivedTarget
    // does the "real" computation.
    m_pageX = m_clientX;
    m_pageY = m_clientY;
    m_layerX = m_pageX;
    m_layerY = m_pageY;
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;
}

void MouseRelatedEvent::receivedTarget()
{
    // Compute coordinates that are based on the target.
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;
    m_layerX = m_pageX;    
    m_layerY = m_pageY;    

    Node* targ = target();
    ASSERT(targ);

    // Must have an updated render tree for this math to work correctly.
    targ->document()->updateRendering();

    // FIXME: clientX/Y should not be the same as pageX/Y!
    // Currently the passed-in clientX and clientY are incorrectly actually
    // pageX and pageY values, so we don't have any work to do here, but if
    // we started passing in correct clientX and clientY, we'd want to compute
    // pageX and pageY here.

    // Adjust offsetX/Y to be relative to the target's position.
    if (!isSimulated()) {
        if (RenderObject* r = targ->renderer()) {
            int rx, ry;
            if (r->absolutePosition(rx, ry)) {
                m_offsetX -= rx;
                m_offsetY -= ry;
            }
        }
    }

    // Adjust layerX/Y to be relative to the layer.
    // FIXME: We're pretty sure this is the wrong defintion of "layer."
    // Our RenderLayer is a more modern concept, and layerX/Y is some
    // other notion about groups of elements; we should test and fix this.
    Node* n = targ;
    while (n && !n->renderer())
        n = n->parent();
    if (n) {
        RenderLayer* layer = n->renderer()->enclosingLayer();
        layer->updateLayerPosition();
        for (; layer; layer = layer->parent()) {
            m_layerX -= layer->xPos();
            m_layerY -= layer->yPos();
        }
    }
}

int MouseRelatedEvent::pageX() const
{
    return m_pageX;
}

int MouseRelatedEvent::pageY() const
{
    return m_pageY;
}

int MouseRelatedEvent::x() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientX;
}

int MouseRelatedEvent::y() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientY;
}

// -----------------------------------------------------------------------------

MouseEvent::MouseEvent()
    : m_button(0)
{
}

MouseEvent::MouseEvent(const AtomicString& eventType,
                               bool canBubbleArg,
                               bool cancelableArg,
                               AbstractView *viewArg,
                               int detailArg,
                               int screenXArg,
                               int screenYArg,
                               int clientXArg,
                               int clientYArg,
                               bool ctrlKeyArg,
                               bool altKeyArg,
                               bool shiftKeyArg,
                               bool metaKeyArg,
                               unsigned short buttonArg,
                               EventTargetNode* relatedTargetArg,
                               Clipboard* clipboardArg,
                               bool isSimulated)
    : MouseRelatedEvent(eventType, canBubbleArg, cancelableArg, viewArg, detailArg,
        screenXArg, screenYArg, clientXArg, clientYArg,
        ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg, isSimulated)
    , m_button(buttonArg)
    , m_relatedTarget(relatedTargetArg)
    , m_clipboard(clipboardArg)
{
}

MouseEvent::~MouseEvent()
{
}

void MouseEvent::initMouseEvent(const AtomicString& typeArg,
                                    bool canBubbleArg,
                                    bool cancelableArg,
                                    AbstractView* viewArg,
                                    int detailArg,
                                    int screenXArg,
                                    int screenYArg,
                                    int clientXArg,
                                    int clientYArg,
                                    bool ctrlKeyArg,
                                    bool altKeyArg,
                                    bool shiftKeyArg,
                                    bool metaKeyArg,
                                    unsigned short buttonArg,
                                    EventTargetNode* relatedTargetArg)
{
    if (dispatched())
        return;

    initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, detailArg);

    m_screenX = screenXArg;
    m_screenY = screenYArg;
    m_clientX = clientXArg;
    m_clientY = clientYArg;
    m_ctrlKey = ctrlKeyArg;
    m_altKey = altKeyArg;
    m_shiftKey = shiftKeyArg;
    m_metaKey = metaKeyArg;
    m_button = buttonArg;
    m_relatedTarget = relatedTargetArg;

    initCoordinates();
}

bool MouseEvent::isMouseEvent() const
{
    return true;
}

bool MouseEvent::isDragEvent() const
{
    const AtomicString &t = type();
    return t == dragenterEvent || t == dragoverEvent || t == dragleaveEvent
        || t == dropEvent
        || t == dragstartEvent || t == dragEvent || t == dragendEvent;
}

int MouseEvent::which() const
{
    // For KHTML, the return values for left, middle and right mouse buttons are 0, 1, 2, respectively.
    // For the Netscape "which" property, the return values for left, middle and right mouse buttons are 1, 2, 3, respectively. 
    // So we must add 1.
    return m_button + 1;
}

Node* MouseEvent::toElement() const
{
    // MSIE extension - "the object toward which the user is moving the mouse pointer"
    return type() == mouseoutEvent ? relatedTarget() : target();
}

Node* MouseEvent::fromElement() const
{
    // MSIE extension - "object from which activation or the mouse pointer is exiting during the event" (huh?)
    return type() == mouseoutEvent ? target() : relatedTarget();
}

//---------------------------------------------------------------------------------------------

KeyboardEvent::KeyboardEvent()
  : m_keyEvent(0)
  , m_keyLocation(DOM_KEY_LOCATION_STANDARD)
  , m_altGraphKey(false)
{
}

KeyboardEvent::KeyboardEvent(const PlatformKeyboardEvent& key, AbstractView *view)
  : UIEventWithKeyState(key.isKeyUp() ? keyupEvent : key.isAutoRepeat() ? keypressEvent : keydownEvent,
    true, true, view, 0, key.ctrlKey(), key.altKey(), key.shiftKey(), key.metaKey())
  , m_keyEvent(new PlatformKeyboardEvent(key))
  , m_keyIdentifier(String(key.keyIdentifier()).impl())
  , m_keyLocation(key.isKeypad() ? DOM_KEY_LOCATION_NUMPAD : DOM_KEY_LOCATION_STANDARD)
  , m_altGraphKey(false)
{
}

KeyboardEvent::KeyboardEvent(const AtomicString &eventType,
                                        bool canBubbleArg,
                                        bool cancelableArg,
                                        AbstractView *viewArg, 
                                        const String &keyIdentifierArg, 
                                        unsigned keyLocationArg, 
                                        bool ctrlKeyArg, 
                                        bool altKeyArg, 
                                        bool shiftKeyArg, 
                                        bool metaKeyArg, 
                                        bool altGraphKeyArg)
  : UIEventWithKeyState(eventType, canBubbleArg, cancelableArg, viewArg, 0, ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg)
  , m_keyEvent(0)
  , m_keyIdentifier(keyIdentifierArg.impl())
  , m_keyLocation(keyLocationArg)
  , m_altGraphKey(altGraphKeyArg)
{
}

KeyboardEvent::~KeyboardEvent()
{
    delete m_keyEvent;
}

void KeyboardEvent::initKeyboardEvent(const AtomicString &typeArg,
                        bool canBubbleArg,
                        bool cancelableArg,
                        AbstractView *viewArg, 
                        const String &keyIdentifierArg, 
                        unsigned keyLocationArg, 
                        bool ctrlKeyArg, 
                        bool altKeyArg, 
                        bool shiftKeyArg, 
                        bool metaKeyArg, 
                        bool altGraphKeyArg)
{
    if (dispatched())
        return;

    initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, 0);

    m_keyIdentifier = keyIdentifierArg.impl();
    m_keyLocation = keyLocationArg;
    m_ctrlKey = ctrlKeyArg;
    m_shiftKey = shiftKeyArg;
    m_altKey = altKeyArg;
    m_metaKey = metaKeyArg;
    m_altGraphKey = altGraphKeyArg;
}

int KeyboardEvent::keyCode() const
{
    if (!m_keyEvent)
        return 0;
    if (type() == keydownEvent || type() == keyupEvent)
        return m_keyEvent->WindowsKeyCode();
    return charCode();
}

int KeyboardEvent::charCode() const
{
    if (!m_keyEvent)
        return 0;
    String text = m_keyEvent->text();
    if (text.length() != 1)
        return 0;
    return text[0];
}

bool KeyboardEvent::isKeyboardEvent() const
{
    return true;
}

int KeyboardEvent::which() const
{
    // Netscape's "which" returns a virtual key code for keydown and keyup, and a character code for keypress.
    // That's exactly what IE's "keyCode" returns. So they are the same for keyboard events.
    return keyCode();
}

// -----------------------------------------------------------------------------

MutationEvent::MutationEvent()
    : m_attrChange(0)
{
}

MutationEvent::MutationEvent(const AtomicString &eventType,
                                     bool canBubbleArg,
                                     bool cancelableArg,
                                     Node *relatedNodeArg,
                                     const String &prevValueArg,
                                     const String &newValueArg,
                                     const String &attrNameArg,
                                     unsigned short attrChangeArg)
    : Event(eventType, canBubbleArg, cancelableArg)
    , m_relatedNode(relatedNodeArg)
    , m_prevValue(prevValueArg.impl())
    , m_newValue(newValueArg.impl())
    , m_attrName(attrNameArg.impl())
    , m_attrChange(attrChangeArg)
{
}

void MutationEvent::initMutationEvent(const AtomicString &typeArg,
                                          bool canBubbleArg,
                                          bool cancelableArg,
                                          Node *relatedNodeArg,
                                          const String &prevValueArg,
                                          const String &newValueArg,
                                          const String &attrNameArg,
                                          unsigned short attrChangeArg)
{
    if (dispatched())
        return;

    initEvent(typeArg, canBubbleArg, cancelableArg);

    m_relatedNode = relatedNodeArg;
    m_prevValue = prevValueArg.impl();
    m_newValue = newValueArg.impl();
    m_attrName = attrNameArg.impl();
    m_attrChange = attrChangeArg;
}

bool MutationEvent::isMutationEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

ClipboardEvent::ClipboardEvent()
{
}

ClipboardEvent::ClipboardEvent(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg, Clipboard *clipboardArg)
    : Event(eventType, canBubbleArg, cancelableArg), m_clipboard(clipboardArg)
{
}

bool ClipboardEvent::isClipboardEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

WheelEvent::WheelEvent() : m_horizontal(false), m_wheelDelta(0)
{
}

WheelEvent::WheelEvent(bool h, int d, AbstractView *v,
    int sx, int sy, int cx, int cy, bool ctrl, bool alt, bool shift, bool meta)
    : MouseRelatedEvent(h ? khtmlHorizontalmousewheelEvent : mousewheelEvent,
        true, true, v, 0, sx, sy, cx, cy, ctrl, alt, shift, meta)
    , m_horizontal(h), m_wheelDelta(d)
{
}

bool WheelEvent::isWheelEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

RegisteredEventListener::RegisteredEventListener(const AtomicString &eventType, PassRefPtr<EventListener> listener, bool useCapture)
    : m_eventType(eventType)
    , m_listener(listener)
    , m_useCapture(useCapture)
    , m_removed(false)
{
}

bool operator==(const RegisteredEventListener &a, const RegisteredEventListener &b)
{
    return a.eventType() == b.eventType() && a.listener() == b.listener() && a.useCapture() == b.useCapture();
}

// -----------------------------------------------------------------------------

Clipboard::~Clipboard()
{
}

// -----------------------------------------------------------------------------

BeforeUnloadEvent::BeforeUnloadEvent()
    : Event(beforeunloadEvent, false, true)
{
}

bool BeforeUnloadEvent::storesResultAsString() const
{
    return true;
}

void BeforeUnloadEvent::storeResult(const String& s)
{
    m_result = s.impl();
}

}
