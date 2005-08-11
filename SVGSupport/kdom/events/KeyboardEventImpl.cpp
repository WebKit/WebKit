/*
    Copyright (C) 2004, 2005 Nikolas Zimmermann <wildfox@kde.org>
				  2004, 2005 Rob Buis <buis@kde.org>

    This file is part of the KDE project

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.
*/

#include <qevent.h>

#include "KeyboardEventImpl.h"
#include "kdomevents.h"
#include "DOMStringImpl.h"

using namespace KDOM;

KeyboardEventImpl::KeyboardEventImpl(EventImplType identifier) : UIEventImpl(identifier)
{
	m_keyEvent = 0;
	m_keyIdentifier = 0;
	//m_keyLocation = KeyboardEvent::DOM_KEY_LOCATION_STANDARD;
	m_ctrlKey = false;
	m_altKey = false;
	m_shiftKey = false;
	m_metaKey = false;
}

KeyboardEventImpl::~KeyboardEventImpl()
{
	delete m_keyEvent;
	if(m_keyIdentifier)
		m_keyIdentifier->deref();
}

bool KeyboardEventImpl::ctrlKey() const
{
	return m_ctrlKey;
}

bool KeyboardEventImpl::shiftKey() const
{
	return m_shiftKey;
}

bool KeyboardEventImpl::altKey() const
{
	return m_altKey;
}

bool KeyboardEventImpl::metaKey() const
{
	return m_metaKey;
}


void KeyboardEventImpl::initKeyboardEvent(const DOMString &typeArg, bool canBubbleArg,
                        bool cancelableArg, AbstractViewImpl *viewArg, 
                        const DOMString &keyIdentifierArg, 
                        unsigned long keyLocationArg, 
                        bool ctrlKeyArg, bool altKeyArg, bool shiftKeyArg, 
                        bool metaKeyArg)
{
	if(m_keyIdentifier)
		m_keyIdentifier->deref();

	initUIEvent(typeArg, canBubbleArg, cancelableArg, viewArg, 0);
	m_keyIdentifier = keyIdentifierArg.implementation();
	if(m_keyIdentifier)
		m_keyIdentifier->ref();
	m_keyLocation = keyLocationArg;
	m_ctrlKey = ctrlKeyArg;
	m_shiftKey = shiftKeyArg;
	m_altKey = altKeyArg;
	m_metaKey = metaKeyArg;
}

void KeyboardEventImpl::initKeyboardEvent(QKeyEvent *key)
{
	initUIEvent(key->type() == QEvent::KeyRelease ? "keyup" : key->isAutoRepeat() ? "keypress" : "keydown", true, true, 0, 0);
	m_ctrlKey = key->state() & Qt::ControlButton;
	m_shiftKey = key->state() & Qt::AltButton;
	m_altKey = key->state() & Qt::ShiftButton;
	m_metaKey = key->state() & Qt::MetaButton;

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

#ifndef APPLE_COMPILE_HACK // Hack out KeyboardEvent support for now.
	int keyState = key->state();

	m_numPad = false;
	m_virtKeyVal = DOM_VK_UNDEFINED;
    
	// Note: we only support testing for num pad
//	m_keyLocation = (keyState & Qt::Keypad) ? KeyboardEvent::DOM_KEY_LOCATION_NUMPAD : KeyboardEvent::DOM_KEY_LOCATION_STANDARD;
  switch(key->key())
  {
  case Qt::Key_Enter:
      m_numPad = true;
      /* fall through */
  case Qt::Key_Return:
      m_virtKeyVal = DOM_VK_ENTER;
      break;
  case Qt::Key_NumLock:
      m_numPad = true;
      m_virtKeyVal = DOM_VK_NUM_LOCK;
      break;
  case Qt::Key_Alt:
      m_virtKeyVal = DOM_VK_RIGHT_ALT;
      // ### DOM_VK_LEFT_ALT;
      break;
  case Qt::Key_Control:
      m_virtKeyVal = DOM_VK_LEFT_CONTROL;
      // ### DOM_VK_RIGHT_CONTROL
      break;
  case Qt::Key_Shift:
      m_virtKeyVal = DOM_VK_LEFT_SHIFT;
      // ### DOM_VK_RIGHT_SHIFT
      break;
  case Qt::Key_Meta:
      m_virtKeyVal = DOM_VK_META;
      break;
  case Qt::Key_CapsLock:
      m_virtKeyVal = DOM_VK_CAPS_LOCK;
      break;
  case Qt::Key_Delete:
      m_virtKeyVal = DOM_VK_DELETE;
      break;
  case Qt::Key_End:
      m_virtKeyVal = DOM_VK_END;
      break;
  case Qt::Key_Escape:
      m_virtKeyVal = DOM_VK_ESCAPE;
      break;
  case Qt::Key_Home:
      m_virtKeyVal = DOM_VK_HOME;
      break;
//   case Qt::Key_Insert:
//       m_virtKeyVal = DOM_VK_INSERT;
//       break;
  case Qt::Key_Pause:
      m_virtKeyVal = DOM_VK_PAUSE;
      break;
  case Qt::Key_Print:
      m_virtKeyVal = DOM_VK_PRINTSCREEN;
      break;
  case Qt::Key_ScrollLock:
      m_virtKeyVal = DOM_VK_SCROLL_LOCK;
      break;
  case Qt::Key_Left:
      m_virtKeyVal = DOM_VK_LEFT;
      break;
  case Qt::Key_Right:
      m_virtKeyVal = DOM_VK_RIGHT;
      break;
  case Qt::Key_Up:
      m_virtKeyVal = DOM_VK_UP;
      break;
  case Qt::Key_Down:
      m_virtKeyVal = DOM_VK_DOWN;
      break;
  case Qt::Key_Next:
      m_virtKeyVal = DOM_VK_PAGE_DOWN;
      break;
  case Qt::Key_Prior:
      m_virtKeyVal = DOM_VK_PAGE_UP;
      break;
  case Qt::Key_F1:
      m_virtKeyVal = DOM_VK_F1;
      break;
  case Qt::Key_F2:
      m_virtKeyVal = DOM_VK_F2;
      break;
  case Qt::Key_F3:
      m_virtKeyVal = DOM_VK_F3;
      break;
  case Qt::Key_F4:
      m_virtKeyVal = DOM_VK_F4;
      break;
  case Qt::Key_F5:
      m_virtKeyVal = DOM_VK_F5;
      break;
  case Qt::Key_F6:
      m_virtKeyVal = DOM_VK_F6;
      break;
  case Qt::Key_F7:
      m_virtKeyVal = DOM_VK_F7;
      break;
  case Qt::Key_F8:
      m_virtKeyVal = DOM_VK_F8;
      break;
  case Qt::Key_F9:
      m_virtKeyVal = DOM_VK_F9;
      break;
  case Qt::Key_F10:
      m_virtKeyVal = DOM_VK_F10;
      break;
  case Qt::Key_F11:
      m_virtKeyVal = DOM_VK_F11;
      break;
  case Qt::Key_F12:
      m_virtKeyVal = DOM_VK_F12;
      break;
  case Qt::Key_F13:
      m_virtKeyVal = DOM_VK_F13;
      break;
  case Qt::Key_F14:
      m_virtKeyVal = DOM_VK_F14;
      break;
  case Qt::Key_F15:
      m_virtKeyVal = DOM_VK_F15;
      break;
  case Qt::Key_F16:
      m_virtKeyVal = DOM_VK_F16;
      break;
  case Qt::Key_F17:
      m_virtKeyVal = DOM_VK_F17;
      break;
  case Qt::Key_F18:
      m_virtKeyVal = DOM_VK_F18;
      break;
  case Qt::Key_F19:
      m_virtKeyVal = DOM_VK_F19;
      break;
  case Qt::Key_F20:
      m_virtKeyVal = DOM_VK_F20;
      break;
  case Qt::Key_F21:
      m_virtKeyVal = DOM_VK_F21;
      break;
  case Qt::Key_F22:
      m_virtKeyVal = DOM_VK_F22;
      break;
  case Qt::Key_F23:
      m_virtKeyVal = DOM_VK_F23;
      break;
  case Qt::Key_F24:
      m_virtKeyVal = DOM_VK_F24;
      break;
  default:
      m_virtKeyVal = DOM_VK_UNDEFINED;
      break;
  }
#endif // APPLE_COMPILE_HACK

  // m_keyVal should contain the unicode value
  // of the pressed key if available.
  //if (m_virtKeyVal == DOM_VK_UNDEFINED && !key->text().isEmpty())
  //    m_keyVal = key->text().unicode()[0];

  //  m_numPad = ???

  // key->state returns enum ButtonState, which is ShiftButton, ControlButton and AltButton or'ed together.
  //m_modifier = key->state();

  // key->text() returns the unicode sequence as a QString
  //m_outputString = DOMString(key->text());
}

bool KeyboardEventImpl::getModifierState(const DOMString &keyIdentifierArg) const
{
	// TODO
	return false;
}

int KeyboardEventImpl::keyCode() const
{
	if(!m_keyEvent)
		return 0;

	if(m_virtKeyVal != DOM_VK_UNDEFINED)
		return m_virtKeyVal;
	else
	{
		int c = charCode();
		if(c != 0)
			return QChar(c).upper().unicode();
		else
		{
#ifndef APPLE_CHANGES
			c = m_keyEvent->key();
			if(c == Qt::Key_unknown)
				kdDebug( 6020 ) << "Unknown key" << endl;
#else
			c = m_keyEvent->WindowsKeyCode();
#endif
			return c;
		}
	}
}

int KeyboardEventImpl::charCode() const
{
	if(!m_keyEvent)
		return 0;

	QString text = m_keyEvent->text();
	if(text.length() != 1)
		return 0;

	return text[0].unicode();
}

long KeyboardEventImpl::which() const
{
	// Netscape's "which" returns a virtual key code for keydown and
	// keyup, and a character code for keypress.
	// That's exactly what IE's "keyCode" returns. So they are the
	// same for keyboard events.
	return keyCode();
}

// vim:ts=4:noet
