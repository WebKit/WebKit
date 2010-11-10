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

#include "DOMWindow.h"
#include "EventListener.h"
#include "Node.h"

#include <wtf/RefPtr.h>
#include <wtf/text/CString.h>

typedef struct _GObject GObject;

namespace WebCore {

class GObjectEventListener : public EventListener {
public:

    static void addEventListener(GObject* object, DOMWindow* window, const char* domEventName, const char* signalName)
    {
        RefPtr<GObjectEventListener> listener(adoptRef(new GObjectEventListener(object, window, 0, domEventName, signalName)));
        window->addEventListener(domEventName, listener.release(), false);
    }

    static void addEventListener(GObject* object, Node* node, const char* domEventName, const char* signalName)
    {
        RefPtr<GObjectEventListener> listener(adoptRef(new GObjectEventListener(object, 0, node, domEventName, signalName)));
        node->addEventListener(domEventName, listener.release(), false);
    }

    static void gobjectDestroyedCallback(GObjectEventListener* listener, GObject*)
    {
        listener->gobjectDestroyed();
    }

    static const GObjectEventListener* cast(const EventListener* listener)
    {
        return listener->type() == GObjectEventListenerType
            ? static_cast<const GObjectEventListener*>(listener)
            : 0;
    }

    virtual bool operator==(const EventListener& other);

private:
    GObjectEventListener(GObject*, DOMWindow*, Node*, const char* domEventName, const char* signalName);
    ~GObjectEventListener();
    void gobjectDestroyed();

    virtual void handleEvent(ScriptExecutionContext*, Event*);

    GObject* m_object;

    // We do not need to keep a reference to these WebCore objects, because
    // we only use them when the GObject and thus the WebCore object is alive.
    Node* m_coreNode;
    DOMWindow* m_coreWindow;
    CString m_domEventName;
    CString m_signalName;
};
} // namespace WebCore

#endif
