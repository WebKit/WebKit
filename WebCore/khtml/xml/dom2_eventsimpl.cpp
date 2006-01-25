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

#include "DocumentImpl.h"
#include "EventNames.h"
#include "FrameView.h"
#include "SystemTime.h"
#include "dom2_events.h"
#include "dom2_viewsimpl.h"
#include "render_layer.h"

namespace WebCore {

using namespace EventNames;

EventImpl::EventImpl()
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

EventImpl::EventImpl(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
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

EventImpl::~EventImpl()
{
}

void EventImpl::initEvent(const AtomicString &eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    if (dispatched())
        return;

    m_type = eventTypeArg;
    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;
}

bool EventImpl::isUIEvent() const
{
    return false;
}

bool EventImpl::isMouseEvent() const
{
    return false;
}

bool EventImpl::isMutationEvent() const
{
    return false;
}

bool EventImpl::isKeyboardEvent() const
{
    return false;
}

bool EventImpl::isDragEvent() const
{
    return false;
}

bool EventImpl::isClipboardEvent() const
{
    return false;
}

bool EventImpl::isWheelEvent() const
{
    return false;
}

bool EventImpl::storesResultAsString() const
{
    return false;
}

void EventImpl::storeResult(const DOMString&)
{
}

// -----------------------------------------------------------------------------

UIEventImpl::UIEventImpl()
    : m_detail(0)
{
}

UIEventImpl::UIEventImpl(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg, AbstractViewImpl *viewArg, int detailArg)
    : EventImpl(eventType, canBubbleArg, cancelableArg)
    , m_view(viewArg)
    , m_detail(detailArg)
{
}

void UIEventImpl::initUIEvent(const AtomicString &typeArg,
                              bool canBubbleArg,
                              bool cancelableArg,
                              AbstractViewImpl *viewArg,
                              int detailArg)
{
    if (dispatched())
        return;

    initEvent(typeArg, canBubbleArg, cancelableArg);

    m_view = viewArg;
    m_detail = detailArg;
}

bool UIEventImpl::isUIEvent() const
{
    return true;
}

int UIEventImpl::keyCode() const
{
    return 0;
}

int UIEventImpl::charCode() const
{
    return 0;
}

int UIEventImpl::layerX() const
{
    return 0;
}

int UIEventImpl::layerY() const
{
    return 0;
}

int UIEventImpl::pageX() const
{
    return 0;
}

int UIEventImpl::pageY() const
{
    return 0;
}

int UIEventImpl::which() const
{
    return 0;
}

// -----------------------------------------------------------------------------

MouseRelatedEventImpl::MouseRelatedEventImpl()
    : m_screenX(0), m_screenY(0), m_clientX(0), m_clientY(0)
    , m_pageX(0), m_pageY(0), m_layerX(0), m_layerY(0), m_offsetX(0), m_offsetY(0), m_isSimulated(0)
{
}

MouseRelatedEventImpl::MouseRelatedEventImpl(const AtomicString &eventType,
                               bool canBubbleArg,
                               bool cancelableArg,
                               AbstractViewImpl *viewArg,
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
    : UIEventWithKeyStateImpl(eventType, canBubbleArg, cancelableArg, viewArg, detailArg,
        ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg)
    , m_screenX(screenXArg), m_screenY(screenYArg)
    , m_clientX(clientXArg), m_clientY(clientYArg)
{
    computePositions();
}

void MouseRelatedEventImpl::computePositions()
{
    m_pageX = m_clientX;
    m_pageY = m_clientY;

    m_layerX = m_pageX;
    m_layerY = m_pageY;

    m_offsetX = m_pageX;
    m_offsetY = m_pageY;

    AbstractViewImpl* av = view();
    if (!av)
        return;
    DocumentImpl* doc = av->document();
    if (!doc)
        return;
    FrameView* kv = doc->view();
    if (!kv)
        return;

    doc->updateRendering();

    // FIXME: clientX/Y should not be the same as pageX/Y!
    // Currently the passed-in clientX and clientY are incorrectly actually
    // pageX and pageY values, so we don't have any work to do here, but if
    // we started passing in correct clientX and clientY, we'd want to compute
    // pageX and pageY here.

    // Compute offset position.
    // FIXME: This won't work because setTarget wasn't called yet!
    m_offsetX = m_pageX;
    m_offsetY = m_pageY;
    if (!isSimulated()) {
        if (NodeImpl *n = target())
            if (RenderObject *r = n->renderer()) {
                int rx, ry;
                if (r->absolutePosition(rx, ry)) {
                    m_offsetX -= rx;
                    m_offsetY -= ry;
                }
            }
    }

    // Compute layer position.
    m_layerX = m_pageX;
    m_layerY = m_pageY;
    if (RenderObject* docRenderer = doc->renderer()) {
        // FIXME: Should we use the target node instead of hit testing?
        // If we want to, then we'll have to wait until setTarget is called.
        RenderObject::NodeInfo hitTestResult(true, false);
        docRenderer->layer()->hitTest(hitTestResult, m_pageX, m_pageY);
        NodeImpl* n = hitTestResult.innerNonSharedNode();
        while (n && !n->renderer())
            n = n->parent();
        if (n) {
            n->renderer()->enclosingLayer()->updateLayerPosition();    
            for (RenderLayer* layer = n->renderer()->enclosingLayer(); layer; layer = layer->parent()) {
                m_layerX -= layer->xPos();
                m_layerY -= layer->yPos();
            }
        }
    }
}

int MouseRelatedEventImpl::pageX() const
{
    return m_pageX;
}

int MouseRelatedEventImpl::pageY() const
{
    return m_pageY;
}

int MouseRelatedEventImpl::x() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientX;
}

int MouseRelatedEventImpl::y() const
{
    // FIXME: This is not correct.
    // See Microsoft documentation and <http://www.quirksmode.org/dom/w3c_events.html>.
    return m_clientY;
}

// -----------------------------------------------------------------------------

MouseEventImpl::MouseEventImpl()
    : m_button(0)
{
}

MouseEventImpl::MouseEventImpl(const AtomicString &eventType,
                               bool canBubbleArg,
                               bool cancelableArg,
                               AbstractViewImpl *viewArg,
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
                               NodeImpl *relatedTargetArg,
                               ClipboardImpl *clipboardArg,
                               bool isSimulated)
    : MouseRelatedEventImpl(eventType, canBubbleArg, cancelableArg, viewArg, detailArg,
        screenXArg, screenYArg, clientXArg, clientYArg,
        ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg)
    , m_button(buttonArg)
    , m_relatedTarget(relatedTargetArg)
    , m_clipboard(clipboardArg)
    , m_isSimulated(isSimulated)
{
}

MouseEventImpl::~MouseEventImpl()
{
}

void MouseEventImpl::initMouseEvent(const AtomicString &typeArg,
                                    bool canBubbleArg,
                                    bool cancelableArg,
                                    AbstractViewImpl *viewArg,
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
                                    NodeImpl *relatedTargetArg)
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

    computePositions();
}

bool MouseEventImpl::isMouseEvent() const
{
    return true;
}

bool MouseEventImpl::isDragEvent() const
{
    const AtomicString &t = type();
    return t == dragenterEvent || t == dragoverEvent || t == dragleaveEvent
        || t == dropEvent
        || t == dragstartEvent || t == dragEvent || t == dragendEvent;
}

int MouseEventImpl::which() const
{
    // For KHTML, the return values for left, middle and right mouse buttons are 0, 1, 2, respectively.
    // For the Netscape "which" property, the return values for left, middle and right mouse buttons are 1, 2, 3, respectively. 
    // So we must add 1.
    return m_button + 1;
}

NodeImpl* MouseEventImpl::toElement() const
{
    // MSIE extension - "the object toward which the user is moving the mouse pointer"
    return type() == mouseoutEvent ? relatedTarget() : target();
}

NodeImpl* MouseEventImpl::fromElement() const
{
    // MSIE extension - "object from which activation or the mouse pointer is exiting during the event" (huh?)
    return type() == mouseoutEvent ? target() : relatedTarget();
}

//---------------------------------------------------------------------------------------------

KeyboardEventImpl::KeyboardEventImpl()
  : m_keyEvent(0)
  , m_keyLocation(KeyboardEvent::DOM_KEY_LOCATION_STANDARD)
  , m_altGraphKey(false)
{
}

KeyboardEventImpl::KeyboardEventImpl(QKeyEvent *key, AbstractViewImpl *view)
  : UIEventWithKeyStateImpl(key->type() == QEvent::KeyRelease ? keyupEvent : key->isAutoRepeat() ? keypressEvent : keydownEvent,
    true, true, view, 0,
    key->state() & Qt::ControlButton,
    key->state() & Qt::AltButton,
    key->state() & Qt::ShiftButton,
    key->state() & Qt::MetaButton)
  , m_keyEvent(new QKeyEvent(*key))
  , m_keyIdentifier(DOMString(key->keyIdentifier()).impl())
  , m_keyLocation((key->state() & Qt::Keypad) ? KeyboardEvent::DOM_KEY_LOCATION_NUMPAD : KeyboardEvent::DOM_KEY_LOCATION_STANDARD)
  , m_altGraphKey(false)
{
}

KeyboardEventImpl::KeyboardEventImpl(const AtomicString &eventType,
                                        bool canBubbleArg,
                                        bool cancelableArg,
                                        AbstractViewImpl *viewArg, 
                                        const DOMString &keyIdentifierArg, 
                                        unsigned keyLocationArg, 
                                        bool ctrlKeyArg, 
                                        bool altKeyArg, 
                                        bool shiftKeyArg, 
                                        bool metaKeyArg, 
                                        bool altGraphKeyArg)
  : UIEventWithKeyStateImpl(eventType, canBubbleArg, cancelableArg, viewArg, 0, ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg)
  , m_keyEvent(0)
  , m_keyIdentifier(keyIdentifierArg.impl())
  , m_keyLocation(keyLocationArg)
  , m_altGraphKey(altGraphKeyArg)
{
}

KeyboardEventImpl::~KeyboardEventImpl()
{
    delete m_keyEvent;
}

void KeyboardEventImpl::initKeyboardEvent(const AtomicString &typeArg,
                        bool canBubbleArg,
                        bool cancelableArg,
                        AbstractViewImpl *viewArg, 
                        const DOMString &keyIdentifierArg, 
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

int KeyboardEventImpl::keyCode() const
{
    if (!m_keyEvent)
        return 0;
    if (type() == keydownEvent || type() == keyupEvent)
        return m_keyEvent->WindowsKeyCode();
    return charCode();
}

int KeyboardEventImpl::charCode() const
{
    if (!m_keyEvent) {
        return 0;
    }
    QString text = m_keyEvent->text();
    if (text.length() != 1) {
        return 0;
    }
    return text[0].unicode();
}

bool KeyboardEventImpl::isKeyboardEvent() const
{
    return true;
}

int KeyboardEventImpl::which() const
{
    // Netscape's "which" returns a virtual key code for keydown and keyup, and a character code for keypress.
    // That's exactly what IE's "keyCode" returns. So they are the same for keyboard events.
    return keyCode();
}

// -----------------------------------------------------------------------------

MutationEventImpl::MutationEventImpl()
    : m_attrChange(0)
{
}

MutationEventImpl::MutationEventImpl(const AtomicString &eventType,
                                     bool canBubbleArg,
                                     bool cancelableArg,
                                     NodeImpl *relatedNodeArg,
                                     const DOMString &prevValueArg,
                                     const DOMString &newValueArg,
                                     const DOMString &attrNameArg,
                                     unsigned short attrChangeArg)
    : EventImpl(eventType, canBubbleArg, cancelableArg)
    , m_relatedNode(relatedNodeArg)
    , m_prevValue(prevValueArg.impl())
    , m_newValue(newValueArg.impl())
    , m_attrName(attrNameArg.impl())
    , m_attrChange(attrChangeArg)
{
}

void MutationEventImpl::initMutationEvent(const AtomicString &typeArg,
                                          bool canBubbleArg,
                                          bool cancelableArg,
                                          NodeImpl *relatedNodeArg,
                                          const DOMString &prevValueArg,
                                          const DOMString &newValueArg,
                                          const DOMString &attrNameArg,
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

bool MutationEventImpl::isMutationEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

ClipboardEventImpl::ClipboardEventImpl()
{
}

ClipboardEventImpl::ClipboardEventImpl(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg, ClipboardImpl *clipboardArg)
    : EventImpl(eventType, canBubbleArg, cancelableArg), m_clipboard(clipboardArg)
{
}

bool ClipboardEventImpl::isClipboardEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

WheelEventImpl::WheelEventImpl() : m_horizontal(false), m_wheelDelta(0)
{
}

WheelEventImpl::WheelEventImpl(bool h, int d, AbstractViewImpl *v,
    int sx, int sy, int cx, int cy, bool ctrl, bool alt, bool shift, bool meta)
    : MouseRelatedEventImpl(h ? khtmlHorizontalmousewheelEvent : mousewheelEvent,
        true, true, v, 0, sx, sy, cx, cy, ctrl, alt, shift, meta)
    , m_horizontal(h), m_wheelDelta(d)
{
}

bool WheelEventImpl::isWheelEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

RegisteredEventListener::RegisteredEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture)
    : m_eventType(eventType), m_listener(listener), m_useCapture(useCapture)
{
}

bool operator==(const RegisteredEventListener &a, const RegisteredEventListener &b)
{
    return a.eventType() == b.eventType() && a.listener() == b.listener() && a.useCapture() == b.useCapture();
}

// -----------------------------------------------------------------------------

ClipboardImpl::~ClipboardImpl()
{
}

// -----------------------------------------------------------------------------

BeforeUnloadEventImpl::BeforeUnloadEventImpl()
    : EventImpl(beforeunloadEvent, false, false)
{
}

bool BeforeUnloadEventImpl::storesResultAsString() const
{
    return true;
}

void BeforeUnloadEventImpl::storeResult(const DOMString& s)
{
    m_result = s.impl();
}

}
