/*
 * This file is part of the DOM implementation for KDE.
 *
 * (C) 2001 Peter Kelly (pmk@post.com)
 * (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
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

namespace DOM {

class AbstractViewImpl;
class DOMStringImpl;
class NodeImpl;

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
	// khtml events (not part of DOM)
	KHTML_DBLCLICK_EVENT, // for html ondblclick
	KHTML_CLICK_EVENT, // for html onclick
	KHTML_DRAGDROP_EVENT,
	KHTML_ERROR_EVENT,
	KHTML_KEYDOWN_EVENT,
	KHTML_KEYPRESS_EVENT,
	KHTML_KEYUP_EVENT,
	KHTML_MOVE_EVENT,
	KHTML_ORIGCLICK_MOUSEUP_EVENT
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
    long layerX() const;
    long layerY() const;
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
    long m_layerX;
    long m_layerY;
    bool m_ctrlKey;
    bool m_altKey;
    bool m_shiftKey;
    bool m_metaKey;
    unsigned short m_button;
    NodeImpl *m_relatedTarget;
 private:
    void computeLayerPos();
};


// Introduced in DOM Level 3:
/**
 * DOM::KeyEvent
 * The detail attribute inherited from UIEvent is used to indicate
 * the number of keypresses which have occurred during key repetition.
 * If this information is not available this value should be 0.
 */
class KeyEventImpl : public UIEventImpl {
public:
  KeyEventImpl();
  KeyEventImpl(EventId _id,
	       bool canBubbleArg,
	       bool cancelableArg,
	       AbstractViewImpl *viewArg,
	       unsigned short detailArg,
	       DOMString &outputStringArg,
	       unsigned long keyValArg,
	       unsigned long virtKeyValArg,
	       bool inputGeneratedArg,
	       bool numPadArg);

  KeyEventImpl(QKeyEvent *key, AbstractViewImpl *view);

  virtual ~KeyEventImpl();

  // VirtualKeyCode
  enum KeyCodes  {
         DOM_VK_UNDEFINED               = 0x0,
         DOM_VK_RIGHT_ALT               = 0x01,
         DOM_VK_LEFT_ALT                = 0x02,
         DOM_VK_LEFT_CONTROL            = 0x03,
         DOM_VK_RIGHT_CONTROL           = 0x04,
         DOM_VK_LEFT_SHIFT              = 0x05,
         DOM_VK_RIGHT_SHIFT             = 0x06,
         DOM_VK_LEFT_META               = 0x07,
         DOM_VK_RIGHT_META              = 0x08,
         DOM_VK_CAPS_LOCK               = 0x09,
         DOM_VK_DELETE                  = 0x0A,
         DOM_VK_END                     = 0x0B,
         DOM_VK_ENTER                   = 0x0C,
         DOM_VK_ESCAPE                  = 0x0D,
         DOM_VK_HOME                    = 0x0E,
         DOM_VK_INSERT                  = 0x0F,
         DOM_VK_NUM_LOCK                = 0x10,
         DOM_VK_PAUSE                   = 0x11,
         DOM_VK_PRINTSCREEN             = 0x12,
         DOM_VK_SCROLL_LOCK             = 0x13,
         DOM_VK_LEFT                    = 0x14,
         DOM_VK_RIGHT                   = 0x15,
         DOM_VK_UP                      = 0x16,
         DOM_VK_DOWN                    = 0x17,
         DOM_VK_PAGE_DOWN               = 0x18,
         DOM_VK_PAGE_UP                 = 0x19,
         DOM_VK_F1                      = 0x1A,
         DOM_VK_F2                      = 0x1B,
         DOM_VK_F3                      = 0x1C,
         DOM_VK_F4                      = 0x1D,
         DOM_VK_F5                      = 0x1E,
         DOM_VK_F6                      = 0x1F,
         DOM_VK_F7                      = 0x20,
         DOM_VK_F8                      = 0x21,
         DOM_VK_F9                      = 0x22,
         DOM_VK_F10                     = 0x23,
         DOM_VK_F11                     = 0x24,
         DOM_VK_F12                     = 0x25,
         DOM_VK_F13                     = 0x26,
         DOM_VK_F14                     = 0x27,
         DOM_VK_F15                     = 0x28,
         DOM_VK_F16                     = 0x29,
         DOM_VK_F17                     = 0x2A,
         DOM_VK_F18                     = 0x2B,
         DOM_VK_F19                     = 0x2C,
         DOM_VK_F20                     = 0x2D,
         DOM_VK_F21                     = 0x2E,
         DOM_VK_F22                     = 0x2F,
         DOM_VK_F23                     = 0x30,
         DOM_VK_F24                     = 0x31
  };

 /**
  *  checkModifier
  *
  * Note: the below description does not match the actual behaviour.
  *       it's extended in a way that you can query multiple modifiers
  *       at once by logically OR`ing them.
  *       also, we use the Qt modifier enum instead of the DOM one.
  *
  * The CheckModifier method is used to check the status of a single
  * modifier key associated with a KeyEvent. The identifier of the
  * modifier in question is passed into the CheckModifier function. If
  * the modifier is triggered it will return true. If not, it will
  * return false.  The list of keys below represents the allowable
  * modifier paramaters for this method:
  *     DOM_VK_LEFT_ALT
  *     DOM_VK_RIGHT_ALT
  *     DOM_VK_LEFT_CONTROL
  *     DOM_VK_RIGHT_CONTROL
  *     DOM_VK_LEFT_SHIFT
  *     DOM_VK_RIGHT_SHIFT
  *     DOM_VK_META
  *
  * Parameters:
  *
  * modifer of type unsigned long
  *   The modifier which the user wishes to query.
  *
  * Return Value: boolean
  *   The status of the modifier represented as a boolean.
  *
  * No Exceptions
  */
 bool checkModifier(unsigned long modiferArg);

 /**
  * initKeyEvent
  *
  * The initKeyEvent method is used to initialize the value of a
  * MouseEvent created through the DocumentEvent interface. This
  * method may only be called before the KeyEvent has been dispatched
  * via the dispatchEvent method, though it may be called multiple
  * times during that phase if necessary. If called multiple times,
  * the final invocation takes precedence. This method has no effect
  * if called after the event has been dispatched.
  *
  * Parameters:
  *
  * typeArg of type DOMString
  *   Specifies the event type.
  * canBubbleArg of type boolean
  *   Specifies whether or not the event can bubble.
  * cancelableArg of type boolean
  *   Specifies whether or not the event's default action can be prevent.
  * viewArg of type views::AbstractView
  *   Specifies the KeyEvent's AbstractView.
  * detailArg of type unsigned short
  *   Specifies the number of repeated keypresses, if available.
  * outputStringArg of type DOMString
  *   Specifies the KeyEvent's outputString attribute
  * keyValArg of type unsigned long
  *   Specifies the KeyEvent's keyValattribute
  * virtKeyValArg of type unsigned long
  *   Specifies the KeyEvent's virtKeyValattribute
  * inputGeneratedArg of type boolean
  *   Specifies the KeyEvent's inputGeneratedattribute
  * numPadArg of type boolean
  *   Specifies the KeyEvent's numPadattribute
  *
  * No Return Value.
  * No Exceptions.
  */
 void initKeyEvent(DOMString &typeArg,
		   bool canBubbleArg,
		   bool cancelableArg,
		   const AbstractView &viewArg,
		   long detailArg,
		   DOMString &outputStringArg,
		   unsigned long keyValArg,
		   unsigned long virtKeyValArg,
		   bool inputGeneratedArg,
		   bool numPadArg);
 /**
  * initModifier
  *
  * The initModifier method is used to initialize the values of any
  * modifiers associated with a KeyEvent created through the
  * DocumentEvent interface. This method may only be called before the
  * KeyEvent has been dispatched via the dispatchEvent method, though
  * it may be called multiple times during that phase if necessary. If
  * called multiple times with the same modifier property the final
  * invocation takes precedence. Unless explicitly give a value of
  * true, all modifiers have a value of false. This method has no
  * effect if called after the event has been dispatched.  The list of
  * keys below represents the allowable modifier paramaters for this
  * method:
  *    DOM_VK_LEFT_ALT
  *    DOM_VK_RIGHT_ALT
  *    DOM_VK_LEFT_CONTROL
  *    DOM_VK_RIGHT_CONTROL
  *    DOM_VK_LEFT_SHIFT
  *    DOM_VK_RIGHT_SHIFT
  *    DOM_VK_META
  *
  * Parameters:
  *
  * modifier of type unsigned long
  *   The modifier which the user wishes to initialize
  * value of type boolean
  *   The new value of the modifier.
  *
  * No Return Value
  * No Exceptions
  */
 void initModifier(unsigned long modifierArg, bool valueArg);

 //Attributes:

 /**
  * inputGenerated of type boolean
  *
  *  The inputGenerated attribute indicates whether the key event will
  *  normally cause visible output. If the key event does not
  *  generate any visible output, such as the use of a function key
  *  or the combination of certain modifier keys used in conjunction
  *  with another key, then the value will be false. If visible
  *  output is normally generated by the key event then the value
  *  will be true.  The value of inputGenerated does not guarantee
  *  the creation of a character. If a key event causing visible
  *  output is cancelable it may be prevented from causing
  *  output. This attribute is intended primarily to differentiate
  *  between keys events which may or may not produce visible output
  *  depending on the system state.
  */
 bool             inputGenerated() const;

 /** keyVal of type unsigned long
  *
  *  The value of keyVal holds the value of the Unicode character
  *  associated with the depressed key. If the key has no Unicode
  *  representation or no Unicode character is available the value is
  *  0.
  */
 unsigned long    keyVal() const;

 /** numPad of type boolean
  *
  *  The numPad attribute indicates whether or not the key event was
  *  generated on the number pad section of the keyboard. If the number
  *  pad was used to generate the key event the value is true,
  *  otherwise the value is false.
  */
    bool             numPad() const { return m_numPad; }

 /**
  *outputString of type DOMString
  *
  *  outputString holds the value of the output generated by the key
  *  event. This may be a single Unicode character or it may be a
  *  string. It may also be null in the case where no output was
  *  generated by the key event.
  */
 DOMString        outputString() const;

 /** virtKeyVal of type unsigned long
  *
  *  When the key associated with a key event is not representable via
  *  a Unicode character virtKeyVale holds the virtual key code
  *  associated with the depressed key. If the key has a Unicode
  *  representation or no virtual code is available the value is
  *  DOM_VK_UNDEFINED.
  */
    unsigned long virtKeyVal() const { return m_virtKeyVal; }

 QKeyEvent *qKeyEvent;

private:
  unsigned long m_keyVal;
  unsigned long m_virtKeyVal;
  bool m_inputGenerated;
  DOMString m_outputString;
  bool m_numPad;
  // bitfield containing state of modifiers. not part of the dom.
  unsigned long    m_modifier;
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
private:
    RegisteredEventListener( const RegisteredEventListener & );
    RegisteredEventListener & operator=( const RegisteredEventListener & );
};

}; //namespace
#endif
