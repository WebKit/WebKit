/*
 *  Copyright (C) 2010 Igalia S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef GObjectEventListener_h
#define GObjectEventListener_h

#include "EventListener.h"

#include <glib-object.h>
#include <glib.h>
#include <wtf/PassRefPtr.h>

namespace WebCore {
class GObjectEventListener : public EventListener {
public:
    static PassRefPtr<GObjectEventListener> create(GObject* object, const char* signalName) { return adoptRef(new GObjectEventListener(object, signalName)); }
    static const GObjectEventListener* cast(const EventListener* listener)
    {
        return listener->type() == GObjectEventListenerType
            ? static_cast<const GObjectEventListener*>(listener)
            : 0;
    }

    virtual bool operator==(const EventListener& other);

private:
    GObjectEventListener(GObject* object, const char* signalName)
        : EventListener(GObjectEventListenerType)
        , m_object(object)
        , m_signalName(signalName)
    {
    }

    virtual void handleEvent(ScriptExecutionContext*, Event*);

    GObject* m_object;
    String m_signalName;
};
} // namespace WebCore

#endif
