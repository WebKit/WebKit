/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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

#ifndef KeyboardEvent_h
#define KeyboardEvent_h

#include "UIEventWithKeyState.h"

namespace WebCore {

    class PlatformKeyboardEvent;

    // Introduced in DOM Level 3
    class KeyboardEvent : public UIEventWithKeyState {
    public:
        enum KeyLocationCode {
            DOM_KEY_LOCATION_STANDARD      = 0x00,
            DOM_KEY_LOCATION_LEFT          = 0x01,
            DOM_KEY_LOCATION_RIGHT         = 0x02,
            DOM_KEY_LOCATION_NUMPAD        = 0x03,
        };
        
        KeyboardEvent();
        KeyboardEvent(const PlatformKeyboardEvent&, AbstractView*);
        KeyboardEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
                      const String& keyIdentifier, unsigned keyLocation,
                      bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey);
        virtual ~KeyboardEvent();
    
        void initKeyboardEvent(const AtomicString& type, bool canBubble, bool cancelable, AbstractView* view,
                               const String& keyIdentifier, unsigned keyLocation,
                               bool ctrlKey, bool altKey, bool shiftKey, bool metaKey, bool altGraphKey);
    
        String keyIdentifier() const { return m_keyIdentifier.get(); }
        unsigned keyLocation() const { return m_keyLocation; }
    
        bool altGraphKey() const { return m_altGraphKey; }
    
        const PlatformKeyboardEvent* keyEvent() const { return m_keyEvent; }

        int keyCode() const; // key code for keydown and keyup, character for other events
        int charCode() const;
    
        virtual bool isKeyboardEvent() const;
        virtual int which() const;

    private:
        PlatformKeyboardEvent* m_keyEvent;
        RefPtr<StringImpl> m_keyIdentifier;
        unsigned m_keyLocation;
        bool m_altGraphKey : 1;
    };

} // namespace WebCore

#endif // KeyboardEvent_h
