/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
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
 *
 */

#ifndef DOM_EVENTSIMPL_H
#define DOM_EVENTSIMPL_H

#include <qdatetime.h>
#include "dom/dom_node.h"
#include "xml/dom_atomicstring.h"
#include "misc/shared.h"

class KHTMLPart;
class QKeyEvent;
class QPixmap;
class QPoint;
class QStringList;

namespace DOM {

class AbstractViewImpl;
class DOMStringImpl;
class EventListener;
class NodeImpl;
class ClipboardImpl;

class EventImpl : public khtml::Shared<EventImpl>
{
public:
    EventImpl();
    EventImpl(const AtomicString &type, bool canBubbleArg, bool cancelableArg);
    virtual ~EventImpl();

    MAIN_THREAD_ALLOCATED;

    const AtomicString &type() const { return m_type; }
    NodeImpl *target() const { return m_target; }
    void setTarget(NodeImpl *target);
    NodeImpl *currentTarget() const { return m_currentTarget; }
    void setCurrentTarget(NodeImpl *currentTarget) { m_currentTarget = currentTarget; }
    unsigned short eventPhase() const { return m_eventPhase; }
    void setEventPhase(unsigned short eventPhase) { m_eventPhase = eventPhase; }
    bool bubbles() const { return m_canBubble; }
    bool cancelable() const { return m_cancelable; }
    DOMTimeStamp timeStamp();
    void stopPropagation() { m_propagationStopped = true; }
    void preventDefault();
    void initEvent(const AtomicString &eventTypeArg, bool canBubbleArg, bool cancelableArg);

    virtual bool isUIEvent() const;
    virtual bool isMouseEvent() const;
    virtual bool isMutationEvent() const;
    virtual bool isKeyboardEvent() const;
    virtual bool isDragEvent() const;   // a subset of mouse events
    virtual bool isClipboardEvent() const;
    virtual bool isWheelEvent() const;

    bool propagationStopped() const { return m_propagationStopped; }
    bool defaultPrevented() const { return m_defaultPrevented; }

    void setDefaultHandled() { m_defaultHandled = true; }
    bool defaultHandled() const { return m_defaultHandled; }

    void setCancelBubble(bool cancel) { m_cancelBubble = cancel; }
    void setDefaultPrevented(bool defaultPrevented) { m_defaultPrevented = defaultPrevented; }
    bool getCancelBubble() const { return m_cancelBubble; }

protected:
    AtomicString m_type;
    bool m_canBubble;
    bool m_cancelable;

    bool m_propagationStopped;
    bool m_defaultPrevented;
    bool m_defaultHandled;
    bool m_cancelBubble;

    NodeImpl *m_currentTarget; // ref > 0 maintained externally
    unsigned short m_eventPhase;
    NodeImpl *m_target;
    QDateTime m_createTime;
};



class UIEventImpl : public EventImpl
{
public:
    UIEventImpl();
    UIEventImpl(const AtomicString &type,
		bool canBubbleArg,
		bool cancelableArg,
		AbstractViewImpl *viewArg,
		int detailArg);
    virtual ~UIEventImpl();
    AbstractViewImpl *view() const { return m_view; }
    int detail() const { return m_detail; }
    void initUIEvent(const AtomicString &typeArg,
		     bool canBubbleArg,
		     bool cancelableArg,
		     AbstractViewImpl *viewArg,
		     int detailArg);
    virtual bool isUIEvent() const;

    virtual int keyCode() const;
    virtual int charCode() const;

    virtual int layerX() const;
    virtual int layerY() const;

    virtual int pageX() const;
    virtual int pageY() const;

    virtual int which() const;

protected:
    AbstractViewImpl *m_view;
    int m_detail;

};

class UIEventWithKeyStateImpl : public UIEventImpl {
public:
    UIEventWithKeyStateImpl() : m_ctrlKey(false), m_altKey(false), m_shiftKey(false), m_metaKey(false) { }
    UIEventWithKeyStateImpl(const AtomicString &type, bool canBubbleArg, bool cancelableArg, AbstractViewImpl *viewArg,
        int detailArg, bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg)
        : UIEventImpl(type, canBubbleArg, cancelableArg, viewArg, detailArg)
        , m_ctrlKey(ctrlKeyArg), m_altKey(altKeyArg), m_shiftKey(shiftKeyArg), m_metaKey(metaKeyArg) { }

    bool ctrlKey() const { return m_ctrlKey; }
    bool shiftKey() const { return m_shiftKey; }
    bool altKey() const { return m_altKey; }
    bool metaKey() const { return m_metaKey; }

protected: // expose these so init functions can set them
    bool m_ctrlKey : 1;
    bool m_altKey : 1;
    bool m_shiftKey : 1;
    bool m_metaKey : 1;
};

// Internal only: Helper class for what's common between mouse and wheel events.
class MouseRelatedEventImpl : public UIEventWithKeyStateImpl {
public:
    MouseRelatedEventImpl();
    MouseRelatedEventImpl(const AtomicString &type,
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
                          bool metaKeyArg);
    int screenX() const { return m_screenX; }
    int screenY() const { return m_screenY; }
    int clientX() const { return m_clientX; }
    int clientY() const { return m_clientY; }
    int layerX() const { return m_layerX; }
    int layerY() const { return m_layerY; }
    virtual int pageX() const;
    virtual int pageY() const;
protected: // expose these so MouseEventImpl::initMouseEvent can set them
    int m_screenX;
    int m_screenY;
    int m_clientX;
    int m_clientY;
    void computeLayerPos();
private:
    int m_layerX;
    int m_layerY;
};

// Introduced in DOM Level 2
class MouseEventImpl : public MouseRelatedEventImpl {
public:
    MouseEventImpl();
    MouseEventImpl(const AtomicString &type,
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
                   ClipboardImpl *clipboardArg=0);
    virtual ~MouseEventImpl();
    unsigned short button() const { return m_button; }
    NodeImpl *relatedTarget() const { return m_relatedTarget; }
    ClipboardImpl *clipboard() const { return m_clipboard; }
    void initMouseEvent(const AtomicString &typeArg,
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
			NodeImpl *relatedTargetArg);
    virtual bool isMouseEvent() const;
    virtual bool isDragEvent() const;
    virtual int which() const;
private:
    unsigned short m_button;
    NodeImpl *m_relatedTarget;
    ClipboardImpl *m_clipboard;
};


// Introduced in DOM Level 3
class KeyboardEventImpl : public UIEventWithKeyStateImpl {
public:
    KeyboardEventImpl();
    KeyboardEventImpl(QKeyEvent *key, AbstractViewImpl *view);
    KeyboardEventImpl(const AtomicString &type,
                bool canBubbleArg,
                bool cancelableArg,
                AbstractViewImpl *viewArg,
                const DOMString &keyIdentifierArg,
                unsigned keyLocationArg,
                bool ctrlKeyArg,
                bool altKeyArg,
                bool shiftKeyArg,
                bool metaKeyArg,
                bool altGraphKeyArg);
    virtual ~KeyboardEventImpl();
    
    void initKeyboardEvent(const AtomicString &typeArg,
                bool canBubbleArg,
                bool cancelableArg,
                AbstractViewImpl *viewArg,
                const DOMString &keyIdentifierArg,
                unsigned keyLocationArg,
                bool ctrlKeyArg,
                bool altKeyArg,
                bool shiftKeyArg,
                bool metaKeyArg,
                bool altGraphKeyArg);
    
    DOMString keyIdentifier() const { return m_keyIdentifier; }
    unsigned keyLocation() const { return m_keyLocation; }
    
    bool altGraphKey() const { return m_altGraphKey; }
    
    QKeyEvent *qKeyEvent() const { return m_keyEvent; }

    int keyCode() const; // key code for keydown and keyup, character for other events
    int charCode() const;
    
    virtual bool isKeyboardEvent() const;
    virtual int which() const;

private:
    QKeyEvent *m_keyEvent;
    DOMStringImpl *m_keyIdentifier;
    unsigned m_keyLocation;
    bool m_altGraphKey : 1;
};

class MutationEventImpl : public EventImpl {
// ### fire these during parsing (if necessary)
public:
    MutationEventImpl();
    MutationEventImpl(const AtomicString &type,
		      bool canBubbleArg,
		      bool cancelableArg,
		      NodeImpl *relatedNodeArg,
		      const DOMString &prevValueArg,
		      const DOMString &newValueArg,
		      const DOMString &attrNameArg,
		      unsigned short attrChangeArg);
    ~MutationEventImpl();

    NodeImpl *relatedNode() const { return m_relatedNode; }
    DOMString prevValue() const { return m_prevValue; }
    DOMString newValue() const { return m_newValue; }
    DOMString attrName() const { return m_attrName; }
    unsigned short attrChange() const { return m_attrChange; }
    void initMutationEvent(const AtomicString &typeArg,
			   bool canBubbleArg,
			   bool cancelableArg,
			   NodeImpl *relatedNodeArg,
			   const DOMString &prevValueArg,
			   const DOMString &newValueArg,
			   const DOMString &attrNameArg,
			   unsigned short attrChangeArg);
    virtual bool isMutationEvent() const;
protected:
    NodeImpl *m_relatedNode;
    DOMStringImpl *m_prevValue;
    DOMStringImpl *m_newValue;
    DOMStringImpl *m_attrName;
    unsigned short m_attrChange;
};

class ClipboardEventImpl : public EventImpl {
public:
    ClipboardEventImpl();
    ClipboardEventImpl(const AtomicString &type, bool canBubbleArg, bool cancelableArg, ClipboardImpl *clipboardArg);
    ~ClipboardEventImpl();

    ClipboardImpl *clipboard() const { return m_clipboard; }
    virtual bool isClipboardEvent() const;
protected:
    ClipboardImpl *m_clipboard;
};

// extension: mouse wheel event
class WheelEventImpl : public MouseRelatedEventImpl
{
public:
    WheelEventImpl();
    WheelEventImpl(bool horizontal, int wheelDelta, AbstractViewImpl *,
                   int screenXArg, int screenYArg,
                   int clientXArg, int clientYArg,
                   bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg);
    bool isHorizontal() const { return m_horizontal; }
    int wheelDelta() const { return m_wheelDelta; }

private:
    virtual bool isWheelEvent() const;

    bool m_horizontal;
    int m_wheelDelta;
};

class RegisteredEventListener {
public:
    RegisteredEventListener(const AtomicString &eventType, EventListener *listener, bool useCapture);
    ~RegisteredEventListener();

    MAIN_THREAD_ALLOCATED;
    
    const AtomicString &eventType() const { return m_eventType; }
    EventListener *listener() const { return m_listener; }
    bool useCapture() const { return m_useCapture; }

private:
    AtomicString m_eventType;
    EventListener *m_listener;
    bool m_useCapture;

    RegisteredEventListener(const RegisteredEventListener &);
    RegisteredEventListener &operator=(const RegisteredEventListener &);
};

bool operator==(const RegisteredEventListener &, const RegisteredEventListener &);
inline bool operator!=(const RegisteredEventListener &a, const RegisteredEventListener &b) { return !(a == b); }

// State available during IE's events for drag and drop and copy/paste
class ClipboardImpl : public khtml::Shared<ClipboardImpl> {
public:
    ClipboardImpl() { }
    virtual ~ClipboardImpl();

    MAIN_THREAD_ALLOCATED;
    
    // Is this operation a drag-drop or a copy-paste?
    virtual bool isForDragging() const = 0;

    virtual DOMString dropEffect() const = 0;
    virtual void setDropEffect(const DOMString &s) = 0;
    virtual DOMString effectAllowed() const = 0;
    virtual void setEffectAllowed(const DOMString &s) = 0;
    
    virtual void clearData(const DOMString &type) = 0;
    virtual void clearAllData() = 0;
    virtual DOMString getData(const DOMString &type, bool &success) const = 0;
    virtual bool setData(const DOMString &type, const DOMString &data) = 0;
    
    // extensions beyond IE's API
    virtual QStringList types() const = 0;
    
    virtual QPoint dragLocation() const = 0;
    virtual QPixmap dragImage() const = 0;
    virtual void setDragImage(const QPixmap &, const QPoint &) = 0;
    virtual NodeImpl *dragImageElement() = 0;
    virtual void setDragImageElement(NodeImpl *, const QPoint &) = 0;
};

} // namespace

#endif
