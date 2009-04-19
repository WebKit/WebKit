/*
 * Copyright (C) 2001 Peter Kelly (pmk@post.com)
 * Copyright (C) 2001 Tobias Anton (anton@stud.fbi.fh-darmstadt.de)
 * Copyright (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006, 2008 Apple Inc. All rights reserved.
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
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef RegisteredEventListener_h
#define RegisteredEventListener_h

#include "AtomicString.h"
#include "EventListener.h"

namespace WebCore {

    class RegisteredEventListener : public RefCounted<RegisteredEventListener> {
    public:
        static PassRefPtr<RegisteredEventListener> create(const AtomicString& eventType, PassRefPtr<EventListener> listener, bool useCapture)
        {
            return adoptRef(new RegisteredEventListener(eventType, listener, useCapture));
        }

        const AtomicString& eventType() const { return m_eventType; }
        EventListener* listener() const { return m_listener.get(); }
        bool useCapture() const { return m_useCapture; }
        
        bool removed() const { return m_removed; }
        void setRemoved(bool removed) { m_removed = removed; }
    
    private:
        RegisteredEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, bool useCapture);

        AtomicString m_eventType;
        RefPtr<EventListener> m_listener;
        bool m_useCapture;
        bool m_removed;
    };

    typedef Vector<RefPtr<RegisteredEventListener> > RegisteredEventListenerVector;

#if USE(JSC)
    inline void markEventListeners(const RegisteredEventListenerVector& listeners)
    {
        for (size_t i = 0; i < listeners.size(); ++i)
            listeners[i]->listener()->markJSFunction();
    }

    inline void invalidateEventListeners(const RegisteredEventListenerVector& listeners)
    {
        // For efficiency's sake, we just set the "removed" bit, instead of
        // actually removing the event listener. The node that owns these
        // listeners is about to be deleted, anyway.
        for (size_t i = 0; i < listeners.size(); ++i)
            listeners[i]->setRemoved(true);
    }
#endif

} // namespace WebCore

#endif // RegisteredEventListener_h
