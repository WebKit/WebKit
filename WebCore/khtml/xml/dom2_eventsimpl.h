/**
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
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
 * $Id$
 */

#ifndef _DOM_EventsImpl_h_
#define _DOM_EventsImpl_h_

#include "dom2_events.h"
#include "dom2_viewsimpl.h"
#include "dom/dom_misc.h"
#include <qdatetime.h>

class KHTMLPart;

namespace DOM {

// ### support user-defined events

class EventImpl : public DomShared
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
	// khtml events (not part of DOM)
	KHTML_DBLCLICK_EVENT, // for html ondblclick
	KHTML_CLICK_EVENT, // for html onclick
	KHTML_DRAGDROP_EVENT,
	KHTML_ERROR_EVENT,
	KHTML_KEYDOWN_EVENT,
	KHTML_KEYPRESS_EVENT,
	KHTML_KEYUP_EVENT,
	KHTML_MOVE_EVENT	
    };

    EventImpl();
    EventImpl(EventId _id, bool canBubbleArg, bool cancelableArg);
    virtual ~EventImpl();

    EventId id() { return m_id; }

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

    virtual bool isUIEvent() { return false; }
    virtual bool isMouseEvent() { return false; }
    virtual bool isMutationEvent() { return false; }
    virtual DOMString eventModuleName() { return ""; }

    virtual bool propagationStopped() { return m_propagationStopped; }
    virtual bool defaultPrevented() { return m_defaultPrevented; }

    static EventId typeToId(DOMString type);
    static DOMString idToType(EventId id);

    void setDefaultHandled();
    bool defaultHandled() const { return m_defaultHandled; }

protected:
    DOMStringImpl *m_type;
    bool m_canBubble;
    bool m_cancelable;

    bool m_propagationStopped;
    bool m_defaultPrevented;
    bool m_defaultHandled;
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
    AbstractViewImpl *view() const;
    long detail() const;
    void initUIEvent(const DOMString &typeArg,
		     bool canBubbleArg,
		     bool cancelableArg,
		     const AbstractView &viewArg,
		     long detailArg);
    virtual bool isUIEvent() { return true; }
    virtual DOMString eventModuleName() { return "UIEvents"; }
protected:
    AbstractViewImpl *m_view;
    long m_detail;

};




// Introduced in DOM Level 2: - internal
class MouseEventImpl : public UIEventImpl {
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
		   NodeImpl *relatedTargetArg);
    virtual ~MouseEventImpl();
    long screenX() const;
    long screenY() const;
    long clientX() const;
    long clientY() const;
    bool ctrlKey() const;
    bool shiftKey() const;
    bool altKey() const;
    bool metaKey() const;
    unsigned short button() const;
    NodeImpl *relatedTarget() const;
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
    virtual bool isMouseEvent() { return true; }
    virtual DOMString eventModuleName() { return "MouseEvents"; }
protected:
    long m_screenX;
    long m_screenY;
    long m_clientX;
    long m_clientY;
    bool m_ctrlKey;
    bool m_altKey;
    bool m_shiftKey;
    bool m_metaKey;
    unsigned short m_button;
    NodeImpl *m_relatedTarget;
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

    Node relatedNode() const;
    DOMString prevValue() const;
    DOMString newValue() const;
    DOMString attrName() const;
    unsigned short attrChange() const;
    void initMutationEvent(const DOMString &typeArg,
			   bool canBubbleArg,
			   bool cancelableArg,
			   const Node &relatedNodeArg,
			   const DOMString &prevValueArg,
			   const DOMString &newValueArg,
			   const DOMString &attrNameArg,
			   unsigned short attrChangeArg);
    virtual bool isMutationEvent() { return true; }
    virtual DOMString eventModuleName() { return "MutationEvents"; }
protected:
    NodeImpl *m_relatedNode;
    DOMStringImpl *m_prevValue;
    DOMStringImpl *m_newValue;
    DOMStringImpl *m_attrName;
    unsigned short m_attrChange;
};

class RegisteredEventListener {
public:
    RegisteredEventListener(EventImpl::EventId _id, EventListener *_listener, bool _useCapture);
    ~RegisteredEventListener();

    bool operator==(const RegisteredEventListener &other);

    EventImpl::EventId id;
    EventListener *listener;
    bool useCapture;
};

}; //namespace
#endif
// vim:ts=4:sw=4
