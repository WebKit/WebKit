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

#ifndef _DOM_EventsImpl_h_
#define _DOM_EventsImpl_h_

#include "dom/dom2_events.h"
#include "misc/shared.h"
#include "xml/dom2_viewsimpl.h"
#include <qdatetime.h>
#include <qevent.h>

class KHTMLPart;
class QPoint;
class QStringList;

namespace DOM {

class AbstractViewImpl;
class DOMStringImpl;
class NodeImpl;
class ClipboardImpl;

// ### support user-defined events

class EventImpl : public khtml::Shared<EventImpl>
{
public:
    enum EventId {
	UNKNOWN_EVENT = 0,
	// UI events
        DOMFOCUSIN_EVENT,
        DOMFOCUSOUT_EVENT,
        DOMACTIVATE_EVENT,
        // Mouse events
        CLICK_EVENT,
        MOUSEDOWN_EVENT,
        MOUSEUP_EVENT,
        MOUSEOVER_EVENT,
        MOUSEMOVE_EVENT,
        MOUSEOUT_EVENT,
        // IE copy/paste events
        BEFORECUT_EVENT,
        CUT_EVENT,
        BEFORECOPY_EVENT,
        COPY_EVENT,
        BEFOREPASTE_EVENT,
        PASTE_EVENT,
        // IE drag and drop events
        DRAGENTER_EVENT,
        DRAGOVER_EVENT,
        DRAGLEAVE_EVENT,
        DROP_EVENT,
        DRAGSTART_EVENT,
        DRAG_EVENT,
        DRAGEND_EVENT,
	// IE selection events
	SELECTSTART_EVENT,
        // Mutation events
        DOMSUBTREEMODIFIED_EVENT,
        DOMNODEINSERTED_EVENT,
        DOMNODEREMOVED_EVENT,
        DOMNODEREMOVEDFROMDOCUMENT_EVENT,
        DOMNODEINSERTEDINTODOCUMENT_EVENT,
        DOMATTRMODIFIED_EVENT,
        DOMCHARACTERDATAMODIFIED_EVENT,
	// HTML events
	LOAD_EVENT,
	UNLOAD_EVENT,
	ABORT_EVENT,
	ERROR_EVENT,
	SELECT_EVENT,
	CHANGE_EVENT,
	SUBMIT_EVENT,
	RESET_EVENT,
	FOCUS_EVENT,
	BLUR_EVENT,
	RESIZE_EVENT,
	SCROLL_EVENT,
        CONTEXTMENU_EVENT,
#if APPLE_CHANGES
        SEARCH_EVENT,
#endif
        INPUT_EVENT,
        // Keyboard events
	KEYDOWN_EVENT,
	KEYUP_EVENT,
        // Text events
        TEXTINPUT_EVENT,
	// khtml events (not part of DOM)
	KHTML_DBLCLICK_EVENT, // for html ondblclick
	KHTML_CLICK_EVENT, // for html onclick
	KHTML_DRAGDROP_EVENT,
	KHTML_ERROR_EVENT,
	KEYPRESS_EVENT,
	KHTML_MOVE_EVENT,
	KHTML_ORIGCLICK_MOUSEUP_EVENT,
	// XMLHttpRequest events
	KHTML_READYSTATECHANGE_EVENT,
        // extensions
        MOUSEWHEEL_EVENT,
        HORIZONTALMOUSEWHEEL_EVENT,
        numEventIds
    };

    EventImpl();
    EventImpl(EventId _id, bool canBubbleArg, bool cancelableArg);
    virtual ~EventImpl();

    MAIN_THREAD_ALLOCATED;

    EventId id() const { return m_id; }

    DOMString type() const;
    NodeImpl *target() const;
    void setTarget(NodeImpl *_target);
    NodeImpl *currentTarget() const;
    void setCurrentTarget(NodeImpl *_currentTarget);
    unsigned short eventPhase() const;
    void setEventPhase(unsigned short _eventPhase);
    bool bubbles() const;
    bool cancelable() const;
    DOMTimeStamp timeStamp();
    void stopPropagation();
    void preventDefault();
    void initEvent(const DOMString &eventTypeArg, bool canBubbleArg, bool cancelableArg);

    virtual bool isUIEvent() const;
    virtual bool isMouseEvent() const;
    virtual bool isMutationEvent() const;
    virtual bool isKeyboardEvent() const;
    virtual bool isDragEvent() const;   // a subset of mouse events
    virtual bool isClipboardEvent() const;
    virtual bool isWheelEvent() const;

    bool propagationStopped() const { return m_propagationStopped; }
    bool defaultPrevented() const { return m_defaultPrevented; }

    static EventId typeToId(const DOMString &type);
    static DOMString idToType(EventId id);

    void setDefaultHandled() { m_defaultHandled = true; }
    bool defaultHandled() const { return m_defaultHandled; }

    void setCancelBubble(bool cancel) { m_cancelBubble = cancel; }
    void setDefaultPrevented(bool defaultPrevented) { m_defaultPrevented = defaultPrevented; }
    bool getCancelBubble() const { return m_cancelBubble; }

protected:
    DOMStringImpl *m_type;
    bool m_canBubble;
    bool m_cancelable;

    bool m_propagationStopped;
    bool m_defaultPrevented;
    bool m_defaultHandled;
    bool m_cancelBubble;

    EventId m_id;
    NodeImpl *m_currentTarget; // ref > 0 maintained externally
    unsigned short m_eventPhase;
    NodeImpl *m_target;
    QDateTime m_createTime;
};



class UIEventImpl : public EventImpl
{
public:
    UIEventImpl();
    UIEventImpl(EventId _id,
		bool canBubbleArg,
		bool cancelableArg,
		AbstractViewImpl *viewArg,
		long detailArg);
    virtual ~UIEventImpl();
    AbstractViewImpl *view() const { return m_view; }
    long detail() const { return m_detail; }
    void initUIEvent(const DOMString &typeArg,
		     bool canBubbleArg,
		     bool cancelableArg,
		     const AbstractView &viewArg,
		     long detailArg);
    virtual bool isUIEvent() const;

protected:
    AbstractViewImpl *m_view;
    long m_detail;

};

class UIEventWithKeyStateImpl : public UIEventImpl {
public:
    UIEventWithKeyStateImpl() : m_ctrlKey(false), m_altKey(false), m_shiftKey(false), m_metaKey(false) { }
    UIEventWithKeyStateImpl(EventId eventID, bool canBubbleArg, bool cancelableArg, AbstractViewImpl *viewArg,
        long detailArg, bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg)
        : UIEventImpl(eventID, canBubbleArg, cancelableArg, viewArg, detailArg)
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
    MouseRelatedEventImpl(EventId _id,
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
                          bool metaKeyArg);
    long screenX() const { return m_screenX; }
    long screenY() const { return m_screenY; }
    long clientX() const { return m_clientX; }
    long clientY() const { return m_clientY; }
    long layerX() const { return m_layerX; }
    long layerY() const { return m_layerY; }
protected: // expose these so MouseEventImpl::initMouseEvent can set them
    long m_screenX;
    long m_screenY;
    long m_clientX;
    long m_clientY;
    void computeLayerPos();
private:
    long m_layerX;
    long m_layerY;
};

// Introduced in DOM Level 2
class MouseEventImpl : public MouseRelatedEventImpl {
public:
    MouseEventImpl();
    MouseEventImpl(EventId _id,
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
                   ClipboardImpl *clipboardArg=0);
    virtual ~MouseEventImpl();
    unsigned short button() const { return m_button; }
    NodeImpl *relatedTarget() const { return m_relatedTarget; }
    ClipboardImpl *clipboard() const { return m_clipboard; }
    void initMouseEvent(const DOMString &typeArg,
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
			const Node &relatedTargetArg);
    virtual bool isMouseEvent() const;
    virtual bool isDragEvent() const;
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
    KeyboardEventImpl(EventId _id,
                bool canBubbleArg,
                bool cancelableArg,
                AbstractViewImpl *viewArg,
                const DOMString &keyIdentifierArg,
                unsigned long keyLocationArg,
                bool ctrlKeyArg,
                bool shiftKeyArg,
                bool altKeyArg,
                bool metaKeyArg,
                bool altGraphKeyArg);
    virtual ~KeyboardEventImpl();
    
    void initKeyboardEvent(const DOMString &typeArg,
                bool canBubbleArg,
                bool cancelableArg,
                const AbstractView &viewArg,
                const DOMString &keyIdentifierArg,
                unsigned long keyLocationArg,
                bool ctrlKeyArg,
                bool shiftKeyArg,
                bool altKeyArg,
                bool metaKeyArg,
                bool altGraphKeyArg);
    
    DOMString keyIdentifier() const { return m_keyIdentifier; }
    unsigned long keyLocation() const { return m_keyLocation; }
    
    bool altGraphKey() const { return m_altGraphKey; }
    
    QKeyEvent *qKeyEvent() const { return m_keyEvent; }

    int keyCode() const; // key code for keydown and keyup, character for other events
    int charCode() const;
    
    virtual bool isKeyboardEvent() const;

private:
    QKeyEvent *m_keyEvent;
    DOMStringImpl *m_keyIdentifier;
    unsigned long m_keyLocation;
    bool m_altGraphKey : 1;
};

class MutationEventImpl : public EventImpl {
// ### fire these during parsing (if necessary)
public:
    MutationEventImpl();
    MutationEventImpl(EventId _id,
		      bool canBubbleArg,
		      bool cancelableArg,
		      const Node &relatedNodeArg,
		      const DOMString &prevValueArg,
		      const DOMString &newValueArg,
		      const DOMString &attrNameArg,
		      unsigned short attrChangeArg);
    ~MutationEventImpl();

    Node relatedNode() const { return m_relatedNode; }
    DOMString prevValue() const { return m_prevValue; }
    DOMString newValue() const { return m_newValue; }
    DOMString attrName() const { return m_attrName; }
    unsigned short attrChange() const { return m_attrChange; }
    void initMutationEvent(const DOMString &typeArg,
			   bool canBubbleArg,
			   bool cancelableArg,
			   const Node &relatedNodeArg,
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
    ClipboardEventImpl(EventId _id, bool canBubbleArg, bool cancelableArg, ClipboardImpl *clipboardArg);
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
    WheelEventImpl(bool horizontal, long wheelDelta, AbstractViewImpl *,
                   long screenXArg, long screenYArg,
                   long clientXArg, long clientYArg,
                   bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, bool metaKeyArg);
    bool isHorizontal() const { return m_horizontal; }
    long wheelDelta() const { return m_wheelDelta; }

private:
    virtual bool isWheelEvent() const;

    bool m_horizontal;
    long m_wheelDelta;
};

class RegisteredEventListener {
public:
    RegisteredEventListener(EventImpl::EventId _id, EventListener *_listener, bool _useCapture);
    ~RegisteredEventListener();

    MAIN_THREAD_ALLOCATED;
    
    bool operator==(const RegisteredEventListener &other);

    EventImpl::EventId id;
    EventListener *listener;
    bool useCapture;
private:
    RegisteredEventListener( const RegisteredEventListener & );
    RegisteredEventListener & operator=( const RegisteredEventListener & );
};

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
    virtual const Node dragImageElement() = 0;
    virtual void setDragImageElement(const Node &, const QPoint &) = 0;
};

} // namespace

#endif
