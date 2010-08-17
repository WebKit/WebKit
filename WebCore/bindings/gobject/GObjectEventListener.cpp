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

#include "config.h"
#include "GObjectEventListener.h"

#include "Event.h"
#include "EventListener.h"
#include "webkit/WebKitDOMEvent.h"
#include "webkit/WebKitDOMEventPrivate.h"
#include <wtf/HashMap.h>
#include <wtf/text/CString.h>

namespace WebCore {

void GObjectEventListener::handleEvent(ScriptExecutionContext*, Event* event)
{
    gboolean handled = FALSE;
    WebKitDOMEvent* gobjectEvent = WEBKIT_DOM_EVENT(WebKit::kit(event));
    g_signal_emit_by_name(m_object, m_signalName.utf8().data(), gobjectEvent, &handled);
}

bool GObjectEventListener::operator==(const EventListener& listener)
{
    if (const GObjectEventListener* gobjectEventListener = GObjectEventListener::cast(&listener))
        return m_signalName == gobjectEventListener->m_signalName && m_object == gobjectEventListener->m_object;

    return false;
}

}
