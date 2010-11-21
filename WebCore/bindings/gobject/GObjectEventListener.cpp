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
#include <glib-object.h>
#include <glib.h>
#include <wtf/HashMap.h>

namespace WebCore {

GObjectEventListener::GObjectEventListener(GObject* object, DOMWindow* window, Node* node, const char* domEventName, const char* signalName)
    : EventListener(GObjectEventListenerType)
    , m_object(object)
    , m_coreNode(node)
    , m_coreWindow(window)
    , m_domEventName(domEventName)
    , m_signalName(signalName)
{
    ASSERT(!m_coreWindow || !m_coreNode);

    g_object_weak_ref(object, reinterpret_cast<GWeakNotify>(GObjectEventListener::gobjectDestroyedCallback), this);
}

GObjectEventListener::~GObjectEventListener()
{
    if (!m_coreWindow && !m_coreNode)
        return;
    g_object_weak_unref(m_object, reinterpret_cast<GWeakNotify>(GObjectEventListener::gobjectDestroyedCallback), this);
}

void GObjectEventListener::gobjectDestroyed()
{
    ASSERT(!m_coreWindow || !m_coreNode);

    // We must set m_coreWindow and m_coreNode to null, because removeEventListener may call the
    // destructor as a side effect and we must be in the proper state to prevent g_object_weak_unref.
    if (DOMWindow* window = m_coreWindow) {
        m_coreWindow = 0;
        window->removeEventListener(m_domEventName.data(), this, false);
        return;
    }

    Node* node = m_coreNode;
    m_coreNode = 0; // See above.
    node->removeEventListener(m_domEventName.data(), this, false);
}

void GObjectEventListener::handleEvent(ScriptExecutionContext*, Event* event)
{
    gboolean handled = FALSE;
    WebKitDOMEvent* gobjectEvent = WEBKIT_DOM_EVENT(WebKit::kit(event));
    g_signal_emit_by_name(m_object, m_signalName.data(), gobjectEvent, &handled);
    g_object_unref(gobjectEvent);
}

bool GObjectEventListener::operator==(const EventListener& listener)
{
    if (const GObjectEventListener* gobjectEventListener = GObjectEventListener::cast(&listener))
        return m_signalName == gobjectEventListener->m_signalName && m_object == gobjectEventListener->m_object;

    return false;
}

}
