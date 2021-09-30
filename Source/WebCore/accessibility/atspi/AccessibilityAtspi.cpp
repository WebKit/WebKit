/*
 * Copyright (C) 2021 Igalia S.L.
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
 */

#include "config.h"
#include "AccessibilityAtspi.h"

#if ENABLE(ACCESSIBILITY) && USE(ATSPI)
#include "AccessibilityAtspiInterfaces.h"
#include "AccessibilityRootAtspi.h"
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <wtf/MainThread.h>
#include <wtf/UUID.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebCore {

AccessibilityAtspi::AccessibilityAtspi(const String& busAddress)
    : m_queue(WorkQueue::create("org.webkit.a11y"))
{
    RELEASE_ASSERT(isMainThread());
    if (busAddress.isEmpty())
        return;
    m_queue->dispatch([this, busAddress = busAddress.isolatedCopy()] {
        GUniqueOutPtr<GError> error;
        m_connection = adoptGRef(g_dbus_connection_new_for_address_sync(busAddress.utf8().data(),
            static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION),
            nullptr, nullptr, &error.outPtr()));
        if (!m_connection)
            g_warning("Can't connect to a11y bus: %s", error->message);
    });
}

const char* AccessibilityAtspi::uniqueName() const
{
    RELEASE_ASSERT(!isMainThread());
    return m_connection ? g_dbus_connection_get_unique_name(m_connection.get()) : nullptr;
}

GVariant* AccessibilityAtspi::nullReference() const
{
    RELEASE_ASSERT(!isMainThread());
    return g_variant_new("(so)", uniqueName(), "/org/a11y/atspi/null");
}

void AccessibilityAtspi::registerRoot(AccessibilityRootAtspi& rootObject, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&& interfaces, CompletionHandler<void(const String&)>&& completionHandler)
{
    RELEASE_ASSERT(isMainThread());
    m_queue->dispatch([this, rootObject = Ref { rootObject }, interfaces = WTFMove(interfaces), completionHandler = WTFMove(completionHandler)]() mutable {
        String reference;
        if (m_connection) {
            String path = makeString("/org/a11y/webkit/accessible/", createCanonicalUUIDString().replace('-', '_'));
            Vector<unsigned, 2> registeredObjects;
            registeredObjects.reserveInitialCapacity(interfaces.size());
            for (const auto& interface : interfaces) {
                auto id = g_dbus_connection_register_object(m_connection.get(), path.utf8().data(), interface.first, interface.second, rootObject.ptr(), nullptr, nullptr);
                registeredObjects.uncheckedAppend(id);
            }
            m_rootObjects.add(rootObject.ptr(), WTFMove(registeredObjects));
            reference = makeString(uniqueName(), ':', path);
            rootObject->setPath(WTFMove(path));
        }
        RunLoop::main().dispatch([reference = reference.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(reference);
        });
    });
}

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
