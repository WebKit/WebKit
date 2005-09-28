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

#ifndef KDOM_EventTargetImpl_H
#define KDOM_EventTargetImpl_H

#include <q3ptrlist.h>

#include <kdom/DOMString.h>
#include <kdom/TreeShared.h>

namespace KDOM
{
    class EventImpl;
    class EventListenerImpl;
    class RegisteredEventListener;
    class EventTargetImpl : public TreeShared<EventTargetImpl>
    {
    public:
        EventTargetImpl();
        virtual ~EventTargetImpl();

        virtual void addEventListener(DOMStringImpl *type, EventListenerImpl *listener, bool useCapture) = 0;
        virtual void removeEventListener(DOMStringImpl *type, EventListenerImpl *listener, bool useCapture) = 0;
        virtual bool dispatchEvent(EventImpl *evt) = 0;

        void handleLocalEvents(EventImpl *evt, bool useCapture);

        // Helpers
        void addListenerType(int eventId);
        void removeListenerType(int eventId);
        virtual bool hasListenerType(int eventId) const;

        /**
         * Perform the default action for an event e.g. submitting a form
         */
        virtual void defaultEventHandler(EventImpl *) {}

    protected:
        Q3PtrList<RegisteredEventListener> *m_eventListeners;

    private:
        int m_listenerTypes;
    };
};

#endif

// vim:ts=4:noet
