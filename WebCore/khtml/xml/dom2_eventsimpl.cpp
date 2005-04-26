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

#include "dom/dom2_views.h"

#include "xml/dom2_eventsimpl.h"
#include "xml/dom_stringimpl.h"
#include "xml/dom_nodeimpl.h"
#include "xml/dom_docimpl.h"
#include "rendering/render_object.h"
#include "rendering/render_layer.h"

#include <kdebug.h>

using namespace DOM;

EventImpl::EventImpl()
{
    m_type = 0;
    m_canBubble = false;
    m_cancelable = false;

    m_propagationStopped = false;
    m_defaultPrevented = false;
    m_cancelBubble = false;
    m_id = UNKNOWN_EVENT;
    m_currentTarget = 0;
    m_eventPhase = 0;
    m_target = 0;
    m_createTime = QDateTime::currentDateTime();
    m_defaultHandled = false;
}

EventImpl::EventImpl(EventId _id, bool canBubbleArg, bool cancelableArg)
{
    DOMString t = EventImpl::idToType(_id);
    m_type = t.implementation();
    if (m_type)
	m_type->ref();
    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;

    m_propagationStopped = false;
    m_defaultPrevented = false;
    m_cancelBubble = false;
    m_id = _id;
    m_currentTarget = 0;
    m_eventPhase = 0;
    m_target = 0;
    m_createTime = QDateTime::currentDateTime();
    m_defaultHandled = false;
}

EventImpl::~EventImpl()
{
    if (m_type)
        m_type->deref();
    if (m_target)
        m_target->deref();
}

DOMString EventImpl::type() const
{
    return m_type;
}

NodeImpl *EventImpl::target() const
{
    return m_target;
}

void EventImpl::setTarget(NodeImpl *_target)
{
    if (m_target)
        m_target->deref();
    m_target = _target;
    if (m_target)
        m_target->ref();
}

NodeImpl *EventImpl::currentTarget() const
{
    return m_currentTarget;
}

void EventImpl::setCurrentTarget(NodeImpl *_currentTarget)
{
    m_currentTarget = _currentTarget;
}

unsigned short EventImpl::eventPhase() const
{
    return m_eventPhase;
}

void EventImpl::setEventPhase(unsigned short _eventPhase)
{
    m_eventPhase = _eventPhase;
}

bool EventImpl::bubbles() const
{
    return m_canBubble;
}

bool EventImpl::cancelable() const
{
    return m_cancelable;
}

DOMTimeStamp EventImpl::timeStamp()
{
    QDateTime epoch(QDate(1970,1,1),QTime(0,0));
    // ### kjs does not yet support long long (?) so the value wraps around
    return epoch.secsTo(m_createTime)*1000+m_createTime.time().msec();
}

void EventImpl::stopPropagation()
{
    m_propagationStopped = true;
}

void EventImpl::preventDefault()
{
    if (m_cancelable)
	m_defaultPrevented = true;
}

void EventImpl::initEvent(const DOMString &eventTypeArg, bool canBubbleArg, bool cancelableArg)
{
    // ### ensure this is not called after we have been dispatched (also for subclasses)

    if (m_type)
	m_type->deref();

    m_type = eventTypeArg.implementation();
    if (m_type)
	m_type->ref();

    m_id = typeToId(eventTypeArg);

    m_canBubble = canBubbleArg;
    m_cancelable = cancelableArg;
}

static const char * const eventNames[EventImpl::numEventIds] = {
    0,
    "DOMFocusIn",
    "DOMFocusOut",
    "DOMActivate",
    "click",
    "mousedown",
    "mouseup",
    "mouseover",
    "mousemove",
    "mouseout",
    "onbeforecut",
    "oncut",
    "onbeforecopy",
    "oncopy",
    "onbeforepaste",
    "onpaste",
    "dragenter",
    "dragover",
    "dragleave",
    "drop",
    "dragstart",
    "drag",
    "dragend",
    "selectstart",
    "DOMSubtreeModified",
    "DOMNodeInserted",
    "DOMNodeRemoved",
    "DOMNodeRemovedFromDocument",
    "DOMNodeInsertedIntoDocument",
    "DOMAttrModified",
    "DOMCharacterDataModified",
    "load",
    "unload",
    "abort",
    "error",
    "select",
    "change",
    "submit",
    "reset",
    "focus",
    "blur",
    "resize",
    "scroll",
    "contextmenu",
#if APPLE_CHANGES
    "search",
#endif
    "input",
    "keydown",
    "keyup",
    "textInput", // FIXME: is the capital I correct?
    0, // KHTML_DBLCLICK_EVENT
    0, // KHTML_CLICK_EVENT
    0, // KHTML_DRAGDROP_EVENT
    0, // KHTML_ERROR_EVENT
    "keypress",
    0, // KHTML_MOVE_EVENT
    0, // KHTML_ORIGCLICK_MOUSEUP_EVENT
    "readystatechange",
    "mousewheel",
    0, // horizontal mouse wheel
};

EventImpl::EventId EventImpl::typeToId(const DOMString &type)
{
    for (int i = 0; i < numEventIds; ++i) {
        const char *n = eventNames[i];
        if (n && type == n)
            return static_cast<EventId>(i);
    }
    return UNKNOWN_EVENT;
}

DOMString EventImpl::idToType(EventId id)
{
    switch (id) {
	case KHTML_DBLCLICK_EVENT:
            return "dblclick";
	case KHTML_CLICK_EVENT:
            return "click";
	case KHTML_DRAGDROP_EVENT:
            return "khtml_dragdrop";
	case KHTML_ERROR_EVENT:
            return "khtml_error";
	case KHTML_MOVE_EVENT:
            return "khtml_move";
        case KHTML_ORIGCLICK_MOUSEUP_EVENT:
            return "khtml_origclick_mouseup_event";
        default:
            break;
    }
    if (id >= numEventIds)
        return DOMString();
    return eventNames[id];
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

UIEventImpl::UIEventImpl(EventId _id, bool canBubbleArg, bool cancelableArg,
		AbstractViewImpl *viewArg, long detailArg)
		: EventImpl(_id,canBubbleArg,cancelableArg)
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

void UIEventImpl::initUIEvent(const DOMString &typeArg,
			      bool canBubbleArg,
			      bool cancelableArg,
			      const AbstractView &viewArg,
			      long detailArg)
{
    EventImpl::initEvent(typeArg,canBubbleArg,cancelableArg);

    if (m_view)
	m_view->deref();

    m_view = viewArg.handle();
    if (m_view)
	m_view->ref();
    m_detail = detailArg;
}

bool UIEventImpl::isUIEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

// -----------------------------------------------------------------------------

MouseRelatedEventImpl::MouseRelatedEventImpl()
    : m_screenX(0), m_screenY(0), m_clientX(0), m_clientY(0), m_layerX(0), m_layerY(0)
{
}

MouseRelatedEventImpl::MouseRelatedEventImpl(EventId _id,
			       bool canBubbleArg,
			       bool cancelableArg,
			       AbstractViewImpl *viewArg,
			       long detailArg,
			       long screenXArg,
			       long screenYArg,
			       long clientXArg,
			       long clientYArg,
			       bool ctrlKeyArg,
			       bool altKeyArg,
			       bool shiftKeyArg,
			       bool metaKeyArg)
    : UIEventWithKeyStateImpl(_id, canBubbleArg, cancelableArg, viewArg, detailArg,
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

    DocumentImpl *doc = view()->document();

    if (!doc || !doc->renderer()) {
	return;
    }

    khtml::RenderObject::NodeInfo renderInfo(true, false);
    doc->renderer()->layer()->hitTest(renderInfo, m_clientX, m_clientY);

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

// -----------------------------------------------------------------------------

MouseEventImpl::MouseEventImpl()
{
    m_button = 0;
    m_relatedTarget = 0;
    m_clipboard = 0;
}

MouseEventImpl::MouseEventImpl(EventId _id,
			       bool canBubbleArg,
			       bool cancelableArg,
			       AbstractViewImpl *viewArg,
			       long detailArg,
			       long screenXArg,
			       long screenYArg,
			       long clientXArg,
			       long clientYArg,
			       bool ctrlKeyArg,
			       bool altKeyArg,
			       bool shiftKeyArg,
			       bool metaKeyArg,
			       unsigned short buttonArg,
			       NodeImpl *relatedTargetArg,
                               ClipboardImpl *clipboardArg)
    : MouseRelatedEventImpl(_id, canBubbleArg, cancelableArg, viewArg, detailArg,
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

void MouseEventImpl::initMouseEvent(const DOMString &typeArg,
                                    bool canBubbleArg,
                                    bool cancelableArg,
                                    const AbstractView &viewArg,
                                    long detailArg,
                                    long screenXArg,
                                    long screenYArg,
                                    long clientXArg,
                                    long clientYArg,
                                    bool ctrlKeyArg,
                                    bool altKeyArg,
                                    bool shiftKeyArg,
                                    bool metaKeyArg,
                                    unsigned short buttonArg,
                                    const Node &relatedTargetArg)
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
    m_relatedTarget = relatedTargetArg.handle();
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
    return (m_id == EventImpl::DRAGENTER_EVENT || m_id == EventImpl::DRAGOVER_EVENT
            || m_id == EventImpl::DRAGLEAVE_EVENT || m_id == EventImpl::DROP_EVENT 
            || m_id == EventImpl::DRAGSTART_EVENT || m_id == EventImpl::DRAG_EVENT
            || m_id == EventImpl::DRAGEND_EVENT);
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
  : UIEventWithKeyStateImpl(key->type() == QEvent::KeyRelease ? KEYUP_EVENT : key->isAutoRepeat() ? KEYPRESS_EVENT : KEYDOWN_EVENT,
    true, true, view, 0,
    key->state() & Qt::ControlButton,
    key->state() & Qt::ShiftButton,
    key->state() & Qt::AltButton,
    key->state() & Qt::MetaButton)
{
#if APPLE_CHANGES
    m_keyEvent = new QKeyEvent(*key);
#else
    m_keyEvent = new QKeyEvent(key->type(), key->key(), key->ascii(), key->state(), key->text(), key->isAutoRepeat(), key->count());
#endif

#if APPLE_CHANGES
    DOMString identifier(key->keyIdentifier());
    m_keyIdentifier = identifier.implementation();
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

KeyboardEventImpl::KeyboardEventImpl(EventId _id,
                                        bool canBubbleArg,
                                        bool cancelableArg,
                                        AbstractViewImpl *viewArg, 
                                        const DOMString &keyIdentifierArg, 
                                        unsigned long keyLocationArg, 
                                        bool ctrlKeyArg, 
                                        bool shiftKeyArg, 
                                        bool altKeyArg, 
                                        bool metaKeyArg, 
                                        bool altGraphKeyArg)
  : UIEventWithKeyStateImpl(_id, canBubbleArg, cancelableArg, viewArg, 0, ctrlKeyArg, shiftKeyArg, altKeyArg, metaKeyArg)
{
    m_keyEvent = 0;
    m_keyIdentifier = keyIdentifierArg.implementation();
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

void KeyboardEventImpl::initKeyboardEvent(const DOMString &typeArg,
                        bool canBubbleArg,
                        bool cancelableArg,
                        const AbstractView &viewArg, 
                        const DOMString &keyIdentifierArg, 
                        unsigned long keyLocationArg, 
                        bool ctrlKeyArg, 
                        bool shiftKeyArg, 
                        bool altKeyArg, 
                        bool metaKeyArg, 
                        bool altGraphKeyArg)
{
    if (m_keyIdentifier)
        m_keyIdentifier->deref();

    UIEventImpl::initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, 0);
    m_keyIdentifier = keyIdentifierArg.implementation();
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
    if (!m_keyEvent) {
        return 0;
    }
    switch (m_id) {
        case KEYDOWN_EVENT:
        case KEYUP_EVENT:
#if APPLE_CHANGES
            return m_keyEvent->WindowsKeyCode();
#else
            // need the equivalent of the above for KDE
#endif
        default:
            return charCode();
    }
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

// -----------------------------------------------------------------------------

MutationEventImpl::MutationEventImpl()
{
    m_relatedNode = 0;
    m_prevValue = 0;
    m_newValue = 0;
    m_attrName = 0;
    m_attrChange = 0;
}

MutationEventImpl::MutationEventImpl(EventId _id,
				     bool canBubbleArg,
				     bool cancelableArg,
				     const Node &relatedNodeArg,
				     const DOMString &prevValueArg,
				     const DOMString &newValueArg,
				     const DOMString &attrNameArg,
				     unsigned short attrChangeArg)
		      : EventImpl(_id,canBubbleArg,cancelableArg)
{
    m_relatedNode = relatedNodeArg.handle();
    if (m_relatedNode)
	m_relatedNode->ref();
    m_prevValue = prevValueArg.implementation();
    if (m_prevValue)
	m_prevValue->ref();
    m_newValue = newValueArg.implementation();
    if (m_newValue)
	m_newValue->ref();
    m_attrName = attrNameArg.implementation();
    if (m_newValue)
	m_newValue->ref();
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

void MutationEventImpl::initMutationEvent(const DOMString &typeArg,
					  bool canBubbleArg,
					  bool cancelableArg,
					  const Node &relatedNodeArg,
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

    m_relatedNode = relatedNodeArg.handle();
    if (m_relatedNode)
	m_relatedNode->ref();
    m_prevValue = prevValueArg.implementation();
    if (m_prevValue)
	m_prevValue->ref();
    m_newValue = newValueArg.implementation();
    if (m_newValue)
	m_newValue->ref();
    m_attrName = attrNameArg.implementation();
    if (m_newValue)
	m_newValue->ref();
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

ClipboardEventImpl::ClipboardEventImpl(EventId _id, bool canBubbleArg, bool cancelableArg, ClipboardImpl *clipboardArg)
  : EventImpl(_id, canBubbleArg, cancelableArg), m_clipboard(clipboardArg)
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

WheelEventImpl::WheelEventImpl(bool h, long d, AbstractViewImpl *v,
    long sx, long sy, long cx, long cy, bool ctrl, bool alt, bool shift, bool meta)
    : MouseRelatedEventImpl(h ? HORIZONTALMOUSEWHEEL_EVENT : MOUSEWHEEL_EVENT,
        true, true, v, 0, sx, sy, cx, cy, ctrl, alt, shift, meta)
    , m_horizontal(h), m_wheelDelta(d)
{
}

bool WheelEventImpl::isWheelEvent() const
{
    return true;
}

// -----------------------------------------------------------------------------

RegisteredEventListener::RegisteredEventListener(EventImpl::EventId _id, EventListener *_listener, bool _useCapture)
{
    id = _id;
    listener = _listener;
    useCapture = _useCapture;
    listener->ref();
}

RegisteredEventListener::~RegisteredEventListener() {
    listener->deref();
}

bool RegisteredEventListener::operator==(const RegisteredEventListener &other)
{
    return (id == other.id &&
	    listener == other.listener &&
	    useCapture == other.useCapture);
}

// -----------------------------------------------------------------------------

ClipboardImpl::~ClipboardImpl()
{
}
