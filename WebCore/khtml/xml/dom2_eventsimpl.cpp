/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
 * (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2003 Apple Computer, Inc.
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
#include "xml/dom2_eventsimpl.h"

#include "dom/dom2_events.h"
#include "xml/dom_stringimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "xml/dom2_viewsimpl.h"
#include "xml/EventNames.h"
#include "rendering/render_object.h"
#include "rendering/render_layer.h"

#include <kdebug.h>

using khtml::RenderObject;

using namespace DOM::EventNames;

namespace DOM {

EventImpl::EventImpl()
{
    m_canBubble = false;
    m_cancelable = false;

    m_propagationStopped = false;
    m_defaultPrevented = false;
    m_cancelBubble = false;
    m_currentTarget = 0;
    m_eventPhase = 0;
    m_target = 0;
    m_createTime = QDateTime::currentDateTime();
    m_defaultHandled = false;
}

EventImpl::EventImpl(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg)
    : m_type(eventType)
{
    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;

    m_propagationStopped = false;
    m_defaultPrevented = false;
    m_cancelBubble = false;
    m_currentTarget = 0;
    m_eventPhase = 0;
    m_target = 0;
    m_createTime = QDateTime::currentDateTime();
    m_defaultHandled = false;
}

EventImpl::~EventImpl()
{
    if (m_target)
        m_target->deref();
}

void EventImpl::setTarget(NodeImpl *_target)
{
    if (m_target)
        m_target->deref();
    m_target = _target;
    if (m_target)
        m_target->ref();
}

DOMTimeStamp EventImpl::timeStamp()
{
    QDateTime epoch(QDate(1970, 1, 1), QTime(0, 0));
    return static_cast<DOMTimeStamp>(epoch.secsTo(m_createTime)) * 1000 + m_createTime.time().msec();
}

void EventImpl::preventDefault()
{
    if (m_cancelable)
	m_defaultPrevented = true;
}

void EventImpl::initEvent(const AtomicString &eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    // ### ensure this is not called after we have been dispatched (also for subclasses)

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

// -----------------------------------------------------------------------------

UIEventImpl::UIEventImpl()
{
    m_view = 0;
    m_detail = 0;
}

UIEventImpl::UIEventImpl(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg, AbstractViewImpl *viewArg, int detailArg)
    : EventImpl(eventType, canBubbleArg, cancelableArg)
{
    m_view = viewArg;
    if (m_view)
        m_view->ref();
    m_detail = detailArg;
}

UIEventImpl::~UIEventImpl()
{
    if (m_view)
        m_view->deref();
}

void UIEventImpl::initUIEvent(const AtomicString &typeArg,
			      bool canBubbleArg,
			      bool cancelableArg,
			      AbstractViewImpl *viewArg,
			      int detailArg)
{
    EventImpl::initEvent(typeArg,canBubbleArg,cancelableArg);

    if (m_view)
	m_view->deref();

    m_view = viewArg;
    if (m_view)
	m_view->ref();
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
    : m_screenX(0), m_screenY(0), m_clientX(0), m_clientY(0), m_layerX(0), m_layerY(0)
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
			       bool metaKeyArg)
    : UIEventWithKeyStateImpl(eventType, canBubbleArg, cancelableArg, viewArg, detailArg,
        ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg)
{
    m_screenX = screenXArg;
    m_screenY = screenYArg;
    m_clientX = clientXArg;
    m_clientY = clientYArg;
    computeLayerPos();
}

void MouseRelatedEventImpl::computeLayerPos()
{
    m_layerX = m_clientX;
    m_layerY = m_clientY;

    AbstractViewImpl *av = view();
    if (!av)
        return;
    DocumentImpl *doc = av->document();
    if (!doc)
	return;
    RenderObject *docRenderer = doc->renderer();
    if (!docRenderer)
        return;

    khtml::RenderObject::NodeInfo renderInfo(true, false);
    docRenderer->layer()->hitTest(renderInfo, m_clientX, m_clientY);

    NodeImpl *node = renderInfo.innerNonSharedNode();
    while (node && !node->renderer()) {
	node = node->parent();
    }

    if (!node) {
	return;
    }

    node->renderer()->enclosingLayer()->updateLayerPosition();
    
    for (khtml::RenderLayer *layer = node->renderer()->enclosingLayer(); layer != NULL; layer = layer->parent()) {
	m_layerX -= layer->xPos();
	m_layerY -= layer->yPos();
    }
}

int MouseRelatedEventImpl::pageX() const
{
    return m_clientX;
}

int MouseRelatedEventImpl::pageY() const
{
    return m_clientY;
}

// -----------------------------------------------------------------------------

MouseEventImpl::MouseEventImpl()
{
    m_button = 0;
    m_relatedTarget = 0;
    m_clipboard = 0;
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
                               ClipboardImpl *clipboardArg)
    : MouseRelatedEventImpl(eventType, canBubbleArg, cancelableArg, viewArg, detailArg,
        screenXArg, screenYArg, clientXArg, clientYArg,
        ctrlKeyArg, altKeyArg, shiftKeyArg, metaKeyArg)
{
    m_button = buttonArg;
    m_relatedTarget = relatedTargetArg;
    if (m_relatedTarget)
	m_relatedTarget->ref();
    m_clipboard = clipboardArg;
    if (m_clipboard)
	m_clipboard->ref();
}

MouseEventImpl::~MouseEventImpl()
{
    if (m_relatedTarget)
	m_relatedTarget->deref();
    if (m_clipboard)
	m_clipboard->deref();
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
    UIEventImpl::initUIEvent(typeArg,canBubbleArg,cancelableArg,viewArg,detailArg);

    if (m_relatedTarget)
	m_relatedTarget->deref();

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
    if (m_relatedTarget)
	m_relatedTarget->ref();
    computeLayerPos();
}

bool MouseEventImpl::isMouseEvent() const
{
    return true;
}

bool MouseEventImpl::isDragEvent() const
{
    return (m_type == dragenterEvent || m_type == dragoverEvent
            || m_type == dragleaveEvent || m_type == dropEvent 
            || m_type == dragstartEvent || m_type == dragEvent
            || m_type == dragendEvent);
}

int MouseEventImpl::which() const
{
    // For KHTML, the return values for left, middle and right mouse buttons are 0, 1, 2, respectively.
    // For the Netscape "which" property, the return values for left, middle and right mouse buttons are 1, 2, 3, respectively. 
    // So we must add 1.
    return m_button + 1;
}

//---------------------------------------------------------------------------------------------

KeyboardEventImpl::KeyboardEventImpl()
{
  m_keyEvent = 0;
  m_keyIdentifier = 0;
  m_keyLocation = KeyboardEvent::DOM_KEY_LOCATION_STANDARD;
  m_altGraphKey = false;
}

KeyboardEventImpl::KeyboardEventImpl(QKeyEvent *key, AbstractViewImpl *view)
  : UIEventWithKeyStateImpl(key->type() == QEvent::KeyRelease ? keyupEvent : key->isAutoRepeat() ? keypressEvent : keydownEvent,
    true, true, view, 0,
    key->state() & Qt::ControlButton,
    key->state() & Qt::AltButton,
    key->state() & Qt::ShiftButton,
    key->state() & Qt::MetaButton)
{
#if APPLE_CHANGES
    m_keyEvent = new QKeyEvent(*key);
#else
    m_keyEvent = new QKeyEvent(key->type(), key->key(), key->ascii(), key->state(), key->text(), key->isAutoRepeat(), key->count());
#endif

#if APPLE_CHANGES
    DOMString identifier(key->keyIdentifier());
    m_keyIdentifier = identifier.impl();
    m_keyIdentifier->ref();
#else
    m_keyIdentifier = 0;
    // need the equivalent of the above for KDE
#endif

    int keyState = key->state();

    m_altGraphKey = false; // altGraphKey is not supported by Qt.
    
    // Note: we only support testing for num pad
    m_keyLocation = (keyState & Qt::Keypad) ? KeyboardEvent::DOM_KEY_LOCATION_NUMPAD : KeyboardEvent::DOM_KEY_LOCATION_STANDARD;
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
{
    m_keyEvent = 0;
    m_keyIdentifier = keyIdentifierArg.impl();
    if (m_keyIdentifier)
        m_keyIdentifier->ref();
    m_keyLocation = keyLocationArg;
    m_altGraphKey = altGraphKeyArg;
}

KeyboardEventImpl::~KeyboardEventImpl()
{
    delete m_keyEvent;
    if (m_keyIdentifier)
        m_keyIdentifier->deref();
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
    if (m_keyIdentifier)
        m_keyIdentifier->deref();

    UIEventImpl::initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, 0);
    m_keyIdentifier = keyIdentifierArg.impl();
    if (m_keyIdentifier)
        m_keyIdentifier->ref();
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
    if (m_type == keydownEvent || m_type == keyupEvent)
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
{
    m_relatedNode = 0;
    m_prevValue = 0;
    m_newValue = 0;
    m_attrName = 0;
    m_attrChange = 0;
}

MutationEventImpl::MutationEventImpl(const AtomicString &eventType,
				     bool canBubbleArg,
				     bool cancelableArg,
				     NodeImpl *relatedNodeArg,
				     const DOMString &prevValueArg,
				     const DOMString &newValueArg,
				     const DOMString &attrNameArg,
				     unsigned short attrChangeArg)
		      : EventImpl(eventType,canBubbleArg,cancelableArg)
{
    m_relatedNode = relatedNodeArg;
    if (m_relatedNode)
	m_relatedNode->ref();
    m_prevValue = prevValueArg.impl();
    if (m_prevValue)
	m_prevValue->ref();
    m_newValue = newValueArg.impl();
    if (m_newValue)
	m_newValue->ref();
    m_attrName = attrNameArg.impl();
    if (m_attrName)
	m_attrName->ref();
    m_attrChange = attrChangeArg;
}

MutationEventImpl::~MutationEventImpl()
{
    if (m_relatedNode)
	m_relatedNode->deref();
    if (m_prevValue)
	m_prevValue->deref();
    if (m_newValue)
	m_newValue->deref();
    if (m_attrName)
	m_attrName->deref();
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
    EventImpl::initEvent(typeArg,canBubbleArg,cancelableArg);

    if (m_relatedNode)
	m_relatedNode->deref();
    if (m_prevValue)
	m_prevValue->deref();
    if (m_newValue)
	m_newValue->deref();
    if (m_attrName)
	m_attrName->deref();

    m_relatedNode = relatedNodeArg;
    if (m_relatedNode)
	m_relatedNode->ref();
    m_prevValue = prevValueArg.impl();
    if (m_prevValue)
	m_prevValue->ref();
    m_newValue = newValueArg.impl();
    if (m_newValue)
	m_newValue->ref();
    m_attrName = attrNameArg.impl();
    if (m_attrName)
	m_attrName->ref();
    m_attrChange = attrChangeArg;
}

bool MutationEventImpl::isMutationEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

ClipboardEventImpl::ClipboardEventImpl()
{
    m_clipboard = 0;
}

ClipboardEventImpl::ClipboardEventImpl(const AtomicString &eventType, bool canBubbleArg, bool cancelableArg, ClipboardImpl *clipboardArg)
    : EventImpl(eventType, canBubbleArg, cancelableArg), m_clipboard(clipboardArg)
{
      if (m_clipboard)
          m_clipboard->ref();
}

ClipboardEventImpl::~ClipboardEventImpl()
{
    if (m_clipboard)
        m_clipboard->deref();
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
    assert(listener);
    listener->ref();
}

RegisteredEventListener::~RegisteredEventListener()
{
    m_listener->deref();
}

bool operator==(const RegisteredEventListener &a, const RegisteredEventListener &b)
{
    return a.eventType() == b.eventType() && a.listener() == b.listener() && a.useCapture() == b.useCapture();
}

// -----------------------------------------------------------------------------

ClipboardImpl::~ClipboardImpl()
{
}

}
