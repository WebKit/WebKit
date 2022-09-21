/*
 * Copyright (C) 2021 Igalia S.L.
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#if USE(ATSPI)
#include "AXObjectCache.h"
#include "AccessibilityAtspiEnums.h"
#include "AccessibilityAtspiInterfaces.h"
#include "AccessibilityObjectAtspi.h"
#include "AccessibilityRootAtspi.h"
#include <gio/gio.h>
#include <glib/gi18n-lib.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/UUID.h>
#include <wtf/glib/RunLoopSourcePriority.h>

namespace WebCore {

AccessibilityAtspi& AccessibilityAtspi::singleton()
{
    static NeverDestroyed<AccessibilityAtspi> atspi;
    return atspi;
}

AccessibilityAtspi::AccessibilityAtspi()
    : m_cacheUpdateTimer(RunLoop::main(), this, &AccessibilityAtspi::cacheUpdateTimerFired)
    , m_cacheClearTimer(RunLoop::main(), this, &AccessibilityAtspi::cacheClearTimerFired)
{
    m_cacheUpdateTimer.setPriority(RunLoopSourcePriority::RunLoopDispatcher);
    m_cacheClearTimer.setPriority(RunLoopSourcePriority::ReleaseUnusedResourcesTimer);
}

void AccessibilityAtspi::connect(const String& busAddress)
{
    if (busAddress.isEmpty())
        return;

    m_isConnecting = true;
    g_dbus_connection_new_for_address(busAddress.utf8().data(),
        static_cast<GDBusConnectionFlags>(G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT | G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION), nullptr, nullptr,
        [](GObject*, GAsyncResult* result, gpointer userData) {
            auto& atspi = *static_cast<AccessibilityAtspi*>(userData);
            GUniqueOutPtr<GError> error;
            atspi.didConnect(adoptGRef(g_dbus_connection_new_for_address_finish(result, &error.outPtr())));
            if (error)
                g_warning("Can't connect to a11y bus: %s", error->message);
        }, this);
}

void AccessibilityAtspi::didConnect(GRefPtr<GDBusConnection>&& connection)
{
    m_isConnecting = false;
    m_connection = WTFMove(connection);
    if (!m_connection)
        return;

    for (auto& pendingRegistration : m_pendingRootRegistrations)
        registerRoot(pendingRegistration.root, WTFMove(pendingRegistration.interfaces), WTFMove(pendingRegistration.completionHandler));
    m_pendingRootRegistrations.clear();

    initializeRegistry();
}

void AccessibilityAtspi::initializeRegistry()
{
    RELEASE_ASSERT(m_connection);
    g_dbus_proxy_new(m_connection.get(), G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, nullptr,
        "org.a11y.atspi.Registry", "/org/a11y/atspi/registry", "org.a11y.atspi.Registry", nullptr, [](GObject*, GAsyncResult* result, gpointer userData) {
        auto& atspi = *static_cast<AccessibilityAtspi*>(userData);
        GUniqueOutPtr<GError> error;
        atspi.m_registry = adoptGRef(g_dbus_proxy_new_finish(result, &error.outPtr()));
        if (!atspi.m_registry) {
            g_warning("Failed to connect to atspi registry: %s\n", error->message);
            return;
        }

        g_signal_connect(atspi.m_registry.get(), "g-signal", G_CALLBACK(+[](GDBusProxy*, char*, char* signal, GVariant* parameters, AccessibilityAtspi* atspi) {
            const char* dbusName;
            const char* eventName;
            if (!g_strcmp0(signal, "EventListenerRegistered")) {
                g_variant_get(parameters, "(&s&s@as)", &dbusName, &eventName, nullptr);
                atspi->addEventListener(dbusName, eventName);
            } else if (!g_strcmp0(signal, "EventListenerDeregistered")) {
                g_variant_get(parameters, "(&s&s)", &dbusName, &eventName);
                atspi->removeEventListener(dbusName, eventName);
            }
        }), &atspi);

        g_dbus_proxy_call(atspi.m_registry.get(), "GetRegisteredEvents", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, [](GObject* proxy, GAsyncResult* result, gpointer userData) {
            auto& atspi = *static_cast<AccessibilityAtspi*>(userData);
            GUniqueOutPtr<GError> error;
            GRefPtr<GVariant> resultVariant = adoptGRef(g_dbus_proxy_call_finish(G_DBUS_PROXY(proxy), result, &error.outPtr()));
            if (!resultVariant) {
                g_warning("Failed to get atspi registered event listeners: %s\n", error->message);
                return;
            }

            GRefPtr<GVariant> events;
            g_variant_get(resultVariant.get(), "(@a(ss))", &events.outPtr());
            GVariantIter iter;
            g_variant_iter_init(&iter, events.get());
            const char* dbusName;
            const char* eventName;
            while (g_variant_iter_loop(&iter, "(&s&s)", &dbusName, &eventName))
                atspi.addEventListener(dbusName, eventName);
        }, &atspi);
    }, this);
}

static GUniquePtr<char*> eventConvertingDetailToNonCamelCase(const char* eventName)
{
    GUniquePtr<char*> event(g_strsplit(eventName, ":", 3));
    if (!event.get()[0] || !event.get()[1] || !event.get()[2] || !*event.get()[2])
        return event;

    char* converted = static_cast<char*>(g_malloc(strlen(event.get()[2]) * 2 + 1));
    char* convertedPtr = converted;
    char* detailPtr = event.get()[2];

    while (*detailPtr) {
        if (isASCIIUpper(*detailPtr)) {
            if (convertedPtr > converted)
                *convertedPtr++ = '-';
            *convertedPtr++ = toASCIILower(*detailPtr++);
        } else
            *convertedPtr++ = *detailPtr++;
    }
    *convertedPtr = '\0';

    g_free(event.get()[2]);
    event.get()[2] = converted;

    return event;
}

void AccessibilityAtspi::addEventListener(const char* dbusName, const char* eventName)
{
    auto& listeners = m_eventListeners.ensure(dbusName, [] {
        return Vector<GUniquePtr<char*>> { };
    }).iterator->value;
    listeners.append(eventConvertingDetailToNonCamelCase(eventName));
    addClient(dbusName);
}

static bool eventIsSubtype(char** needle, char** haystack)
{
    while (*haystack && **haystack) {
        if (g_strcmp0(*needle, *haystack))
            return false;
        needle++;
        haystack++;
    }

    return true;
}

void AccessibilityAtspi::removeEventListener(const char* dbusName, const char* eventName)
{
    if (!eventName || !*eventName) {
        m_eventListeners.remove(dbusName);
        return;
    }

    auto it = m_eventListeners.find(dbusName);
    if (it == m_eventListeners.end())
        return;

    auto needle = eventConvertingDetailToNonCamelCase(eventName);
    it->value.removeAllMatching([&](const GUniquePtr<char*>& event) {
        return eventIsSubtype(needle.get(), event.get());
    });

    if (it->value.isEmpty())
        m_eventListeners.remove(it);
}

void AccessibilityAtspi::addClient(const char* dbusName)
{
    if (m_clients.isEmpty())
        AXObjectCache::enableAccessibility();

    auto addResult = m_clients.add(dbusName, 0);
    if (!addResult.isNewEntry)
        return;

    m_cacheClearTimer.stop();

    addResult.iterator->value = g_dbus_connection_signal_subscribe(m_connection.get(), nullptr, "org.freedesktop.DBus", "NameOwnerChanged", nullptr, dbusName,
        G_DBUS_SIGNAL_FLAGS_MATCH_ARG0_NAMESPACE, [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar*, GVariant* parameters, gpointer userData) {
            auto& atspi = *static_cast<AccessibilityAtspi*>(userData);
            const char* interface;
            const char* oldName;
            const char* newName;
            g_variant_get(parameters, "(&s&s&s)", &interface, &oldName, &newName);
            if (*oldName != '\0' && *newName == '\0')
                atspi.removeClient(oldName);
        }, this, nullptr);
}

void AccessibilityAtspi::removeClient(const char* dbusName)
{
    auto id = m_clients.take(dbusName);
    if (!id)
        return;

    g_dbus_connection_signal_unsubscribe(m_connection.get(), id);

    if (!m_clients.isEmpty())
        return;

    m_cacheUpdateList.clear();
    m_cacheUpdateTimer.stop();
    m_cacheClearTimer.startOneShot(10_s);
}

bool AccessibilityAtspi::shouldEmitSignal(const char* interface, const char* name, const char* detail)
{
    // Always emit signals if we couldn't connect to the registry.
    if (!m_registry)
        return true;

    if (m_eventListeners.isEmpty())
        return false;

    const char* needle[4] = { interface, name, detail, nullptr };
    for (const auto& listeners : m_eventListeners.values()) {
        auto result = listeners.findIf([&](const GUniquePtr<char*>& event) {
            return eventIsSubtype(const_cast<char**>(needle), event.get());
        });
        if (result != notFound)
            return true;
    }

    return false;
}

const char* AccessibilityAtspi::uniqueName() const
{
    return m_connection ? g_dbus_connection_get_unique_name(m_connection.get()) : nullptr;
}

GVariant* AccessibilityAtspi::nullReference() const
{
    return g_variant_new("(so)", uniqueName(), "/org/a11y/atspi/null");
}

GVariant* AccessibilityAtspi::applicationReference() const
{
    // The application is the same for all root objects, so just use the first root object that is already embedded.
    for (auto* rootObject : m_rootObjects.keys()) {
        if (!rootObject->path().isNull())
            return rootObject->applicationReference();
    }

    return nullReference();
}

void AccessibilityAtspi::registerRoot(AccessibilityRootAtspi& rootObject, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&& interfaces, CompletionHandler<void(const String&)>&& completionHandler)
{
    if (m_isConnecting) {
        m_pendingRootRegistrations.append({ rootObject, WTFMove(interfaces), WTFMove(completionHandler) });
        return;
    }

    if (!m_connection) {
        completionHandler({ });
        return;
    }

    ensureCache();
    String path = makeString("/org/a11y/webkit/accessible/", makeStringByReplacingAll(createVersion4UUIDString(), '-', '_'));
    Vector<unsigned, 3> registeredObjects;
    registeredObjects.reserveInitialCapacity(interfaces.size());
    for (const auto& interface : interfaces) {
        auto id = g_dbus_connection_register_object(m_connection.get(), path.utf8().data(), interface.first, interface.second, &rootObject, nullptr, nullptr);
        registeredObjects.uncheckedAppend(id);
    }
    m_rootObjects.add(&rootObject, WTFMove(registeredObjects));
    String reference = makeString(uniqueName(), ':', path);
    rootObject.setPath(WTFMove(path));
    completionHandler(reference);
}

void AccessibilityAtspi::unregisterRoot(AccessibilityRootAtspi& rootObject)
{
    for (unsigned i = 0; i < m_pendingRootRegistrations.size(); ++i) {
        auto& pendingRootRegistration = m_pendingRootRegistrations[i];
        if (pendingRootRegistration.root.ptr() == &rootObject) {
            pendingRootRegistration.completionHandler({ });
            m_pendingRootRegistrations.remove(i);
            return;
        }
    }

    if (!m_connection)
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, rootObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "StateChanged",
        g_variant_new("(siiva{sv})", "defunct", TRUE, 0, g_variant_new_string("0"), nullptr), nullptr);

    auto registeredObjects = m_rootObjects.take(&rootObject);
    g_dbus_connection_emit_signal(m_connection.get(), nullptr, "/org/a11y/atspi/cache", "org.a11y.atspi.Cache", "RemoveAccessible",
        g_variant_new("((so))", uniqueName(), rootObject.path().utf8().data()), nullptr);
    for (auto id : registeredObjects)
        g_dbus_connection_unregister_object(m_connection.get(), id);
}

String AccessibilityAtspi::registerObject(AccessibilityObjectAtspi& atspiObject, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&& interfaces)
{
    RELEASE_ASSERT(!m_isConnecting);
    if (!m_connection)
        return { };

    ensureCache();
    String path = makeString("/org/a11y/atspi/accessible/", makeStringByReplacingAll(createVersion4UUIDString(), '-', '_'));
    Vector<unsigned, 7> registeredObjects;
    registeredObjects.reserveInitialCapacity(interfaces.size());
    for (const auto& interface : interfaces) {
        auto id = g_dbus_connection_register_object(m_connection.get(), path.utf8().data(), interface.first, interface.second, &atspiObject, nullptr, nullptr);
        registeredObjects.uncheckedAppend(id);
    }
    m_atspiObjects.add(&atspiObject, WTFMove(registeredObjects));

    m_cacheUpdateList.add(&atspiObject);
    if (!m_cacheUpdateTimer.isActive())
        m_cacheUpdateTimer.startOneShot(0_s);

    return path;
}

void AccessibilityAtspi::unregisterObject(AccessibilityObjectAtspi& atspiObject)
{
    RELEASE_ASSERT(!m_isConnecting);
    if (!m_connection)
        return;

    if (m_atspiHyperlinks.contains(&atspiObject)) {
        auto registeredObjects = m_atspiHyperlinks.take(&atspiObject);
        for (auto id : registeredObjects)
            g_dbus_connection_unregister_object(m_connection.get(), id);
    }

    const auto& path = atspiObject.path();
    g_dbus_connection_emit_signal(m_connection.get(), nullptr, path.utf8().data(), "org.a11y.atspi.Event.Object", "StateChanged",
        g_variant_new("(siiva{sv})", "defunct", TRUE, 0, g_variant_new_string("0"), nullptr), nullptr);

    if (!m_cacheUpdateList.remove(&atspiObject) && m_cache.remove(path)) {
        g_dbus_connection_emit_signal(m_connection.get(), nullptr, "/org/a11y/atspi/cache", "org.a11y.atspi.Cache", "RemoveAccessible",
            g_variant_new("((so))", uniqueName(), path.utf8().data()), nullptr);
    }

    if (m_cacheUpdateList.isEmpty())
        m_cacheUpdateTimer.stop();

    auto registeredObjects = m_atspiObjects.take(&atspiObject);
    for (auto id : registeredObjects)
        g_dbus_connection_unregister_object(m_connection.get(), id);
}

String AccessibilityAtspi::registerHyperlink(AccessibilityObjectAtspi& atspiObject, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&& interfaces)
{
    if (!m_connection)
        return { };

    String path = makeString("/org/a11y/atspi/accessible/", makeStringByReplacingAll(createVersion4UUIDString(), '-', '_'));
    Vector<unsigned, 1> registeredObjects;
    registeredObjects.reserveInitialCapacity(interfaces.size());
    for (const auto& interface : interfaces) {
        auto id = g_dbus_connection_register_object(m_connection.get(), path.utf8().data(), interface.first, interface.second, &atspiObject, nullptr, nullptr);
        registeredObjects.uncheckedAppend(id);
    }
    m_atspiHyperlinks.add(&atspiObject, WTFMove(registeredObjects));

    return path;
}

void AccessibilityAtspi::parentChanged(AccessibilityObjectAtspi& atspiObject)
{
    if (!m_connection)
        return;

    // Always emit parentChanged when there are clients because the atspi cache always consumes it.
    if (!m_clients.isEmpty())
        return;

    // Do not emit parent-changed for pending objects, since AddAccessible will update the cache.
    if (m_cacheUpdateList.contains(&atspiObject))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "PropertyChange",
        g_variant_new("(siiva{sv})", "accessible-parent", 0, 0, atspiObject.parentReference(), nullptr), nullptr);
}

void AccessibilityAtspi::parentChanged(AccessibilityRootAtspi& rootObject)
{
    if (!m_connection)
        return;

    // Always emit parentChanged when there are clients because the atspi cache always consumes it.
    if (m_clients.isEmpty())
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, rootObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "PropertyChange",
        g_variant_new("(siiva{sv})", "accessible-parent", 0, 0, rootObject.parentReference(), nullptr), nullptr);
}

void AccessibilityAtspi::childrenChanged(AccessibilityObjectAtspi& atspiObject, AccessibilityObjectAtspi& child, ChildrenChanged change)
{
    if (!m_connection)
        return;

    // Always emit ChildrenChanged when there are clients because the atspi cache always consumes it.
    if (m_clients.isEmpty())
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "ChildrenChanged",
        g_variant_new("(siiv(so))", change == ChildrenChanged::Added ? "add" : "remove", child.indexInParentForChildrenChanged(change),
        0, g_variant_new("(so)", uniqueName(), child.path().utf8().data()), uniqueName(), atspiObject.path().utf8().data()), nullptr);
}

void AccessibilityAtspi::childrenChanged(AccessibilityRootAtspi& rootObject, AccessibilityObjectAtspi& child, ChildrenChanged change)
{
    if (!m_connection)
        return;

    // Always emit ChildrenChanged when there are clients because the atspi cache always consumes it.
    if (m_clients.isEmpty())
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, rootObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "ChildrenChanged",
        g_variant_new("(siiv(so))", change == ChildrenChanged::Added ? "add" : "remove", 0,
        0, g_variant_new("(so)", uniqueName(), child.path().utf8().data()), uniqueName(), rootObject.path().utf8().data()), nullptr);
}

void AccessibilityAtspi::stateChanged(AccessibilityObjectAtspi& atspiObject, const char* name, bool value)
{
#if ENABLE(DEVELOPER_MODE)
    notifyStateChanged(atspiObject, name, value);
#endif

    if (!m_connection)
        return;

    if (!shouldEmitSignal("Object", "StateChanged", name))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "StateChanged",
        g_variant_new("(siiva{sv})", name, value, 0, g_variant_new_string("0"), nullptr), nullptr);
}

void AccessibilityAtspi::textChanged(AccessibilityObjectAtspi& atspiObject, const char* changeType, CString&& text, unsigned offset, unsigned length)
{
#if ENABLE(DEVELOPER_MODE)
    notifyTextChanged(atspiObject);
#endif

    if (!m_connection)
        return;

    if (!shouldEmitSignal("Object", "TextChanged", changeType))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "TextChanged",
        g_variant_new("(siiva{sv})", changeType, offset, length, g_variant_new_string(text.data()), nullptr), nullptr);
}

void AccessibilityAtspi::textAttributesChanged(AccessibilityObjectAtspi& atspiObject)
{
    if (!m_connection)
        return;

    if (!shouldEmitSignal("Object", "TextAttributesChanged"))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "TextAttributesChanged",
        g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
}

void AccessibilityAtspi::textCaretMoved(AccessibilityObjectAtspi& atspiObject, unsigned caretOffset)
{
#if ENABLE(DEVELOPER_MODE)
    notifyTextCaretMoved(atspiObject, caretOffset);
#endif

    if (!m_connection)
        return;

    if (!shouldEmitSignal("Object", "TextCaretMoved"))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "TextCaretMoved",
        g_variant_new("(siiva{sv})", "", caretOffset, 0, g_variant_new_string(""), nullptr), nullptr);
}

void AccessibilityAtspi::textSelectionChanged(AccessibilityObjectAtspi& atspiObject)
{
    if (!m_connection)
        return;

    if (!shouldEmitSignal("Object", "TextSelectionChanged"))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "TextSelectionChanged",
        g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
}

void AccessibilityAtspi::valueChanged(AccessibilityObjectAtspi& atspiObject, double value)
{
#if ENABLE(DEVELOPER_MODE)
    notifyValueChanged(atspiObject);
#endif

    if (!m_connection)
        return;

    if (!shouldEmitSignal("Object", "PropertyChange", "accessible-value"))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "PropertyChange",
        g_variant_new("(siiva{sv})", "accessible-value", 0, 0, g_variant_new_double(value), nullptr), nullptr);
}

void AccessibilityAtspi::selectionChanged(AccessibilityObjectAtspi& atspiObject)
{
#if ENABLE(DEVELOPER_MODE)
    if (atspiObject.role() == Atspi::Role::Menu)
        notifyMenuSelectionChanged(atspiObject);
    else
        notifySelectionChanged(atspiObject);
#endif

    if (!m_connection)
        return;

    if (!shouldEmitSignal("Object", "SelectionChanged"))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "SelectionChanged",
        g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
}

void AccessibilityAtspi::loadEvent(AccessibilityObjectAtspi& atspiObject, CString&& event)
{
#if ENABLE(DEVELOPER_MODE)
    notifyLoadEvent(atspiObject, event);
#endif

    if (!m_connection)
        return;

    if (!shouldEmitSignal("Document", event.data()))
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject.path().utf8().data(), "org.a11y.atspi.Event.Document", event.data(),
        g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
}

struct RoleNameEntry {
    const char* name;
    const char* localizedName;
};

static constexpr std::pair<AccessibilityRole, RoleNameEntry> roleNames[] = {
    { AccessibilityRole::Application, { "application", N_("application") } },
    { AccessibilityRole::ApplicationAlert, { "notification", N_("notification") } },
    { AccessibilityRole::ApplicationAlertDialog, { "alert", N_("alert") } },
    { AccessibilityRole::ApplicationDialog, { "dialog", N_("dialog") } },
    { AccessibilityRole::ApplicationGroup, { "grouping", N_("grouping") } },
    { AccessibilityRole::ApplicationLog, { "log", N_("log") } },
    { AccessibilityRole::ApplicationMarquee, { "marquee", N_("marquee") } },
    { AccessibilityRole::ApplicationStatus, { "statusbar", N_("statusbar") } },
    { AccessibilityRole::ApplicationTextGroup, { "section", N_("section") } },
    { AccessibilityRole::ApplicationTimer, { "timer", N_("timer") } },
    { AccessibilityRole::Audio, { "audio", N_("audio") } },
    { AccessibilityRole::Blockquote, { "block quote", N_("block quote") } },
    { AccessibilityRole::BusyIndicator, { "progress bar", N_("progress bar") } },
    { AccessibilityRole::Button, { "push button", N_("push button") } },
    { AccessibilityRole::Canvas, { "canvas", N_("canvas") } },
    { AccessibilityRole::Caption, { "caption", N_("caption") } },
    { AccessibilityRole::Cell, { "table cell", N_("table cell") } },
    { AccessibilityRole::CheckBox, { "check box", N_("check box") } },
    { AccessibilityRole::ColorWell, { "push button", N_("push button") } },
    { AccessibilityRole::ColumnHeader, { "column header", N_("column header") } },
    { AccessibilityRole::ComboBox, { "combo box", N_("combo box") } },
    { AccessibilityRole::Definition, { "definition", N_("definition") } },
    { AccessibilityRole::Deletion, { "content deletion", N_("content deletion") } },
    { AccessibilityRole::DescriptionList, { "description list", N_("description list") } },
    { AccessibilityRole::DescriptionListDetail, { "description value", N_("description value") } },
    { AccessibilityRole::DescriptionListTerm, { "description term", N_("description term") } },
    { AccessibilityRole::Directory, { "directory pane", N_("directory pane") } },
    { AccessibilityRole::Div, { "section", N_("section") } },
    { AccessibilityRole::Document, { "document frame", N_("document frame") } },
    { AccessibilityRole::DocumentArticle, { "article", N_("article") } },
    { AccessibilityRole::DocumentMath, { "math", N_("math") } },
    { AccessibilityRole::DocumentNote, { "comment", N_("comment") } },
    { AccessibilityRole::Feed, { "panel", N_("panel") } },
    { AccessibilityRole::Figure, { "panel", N_("panel") } },
    { AccessibilityRole::Footer, { "footer", N_("footer") } },
    { AccessibilityRole::Footnote, { "footnote", N_("footnote") } },
    { AccessibilityRole::Form, { "form", N_("form") } },
    { AccessibilityRole::GraphicsDocument, { "document frame", N_("document frame") } },
    { AccessibilityRole::GraphicsObject, { "panel", N_("panel") } },
    { AccessibilityRole::GraphicsSymbol, { "image", N_("image") } },
    { AccessibilityRole::Grid, { "table", N_("table") } },
    { AccessibilityRole::GridCell, { "table cell", N_("table cell") } },
    { AccessibilityRole::Group, { "panel", N_("panel") } },
    { AccessibilityRole::Heading, { "heading", N_("heading") } },
    { AccessibilityRole::HorizontalRule, { "separator", N_("separator") } },
    { AccessibilityRole::Inline, { "text", N_("text") } },
    { AccessibilityRole::Image, { "image", N_("image") } },
    { AccessibilityRole::ImageMap, { "image map", N_("image map") } },
    { AccessibilityRole::ImageMapLink, { "link", N_("link") } },
    { AccessibilityRole::Insertion, { "content insertion", N_("content insertion") } },
    { AccessibilityRole::Label, { "label", N_("label") } },
    { AccessibilityRole::LandmarkBanner, { "landmark", N_("landmark") } },
    { AccessibilityRole::LandmarkComplementary, { "landmark", N_("landmark") } },
    { AccessibilityRole::LandmarkContentInfo, { "landmark", N_("landmark") } },
    { AccessibilityRole::LandmarkDocRegion, { "landmark", N_("landmark") } },
    { AccessibilityRole::LandmarkMain, { "landmark", N_("landmark") } },
    { AccessibilityRole::LandmarkNavigation, { "landmark", N_("landmark") } },
    { AccessibilityRole::LandmarkRegion, { "landmark", N_("landmark") } },
    { AccessibilityRole::LandmarkSearch, { "landmark", N_("landmark") } },
    { AccessibilityRole::Legend, { "label", N_("label") } },
    { AccessibilityRole::Link, { "link", N_("link") } },
    { AccessibilityRole::List, { "list", N_("list") } },
    { AccessibilityRole::ListBox, { "heading", N_("list box") } },
    { AccessibilityRole::ListBoxOption, { "list item", N_("list item") } },
    { AccessibilityRole::ListItem, { "list item", N_("list item") } },
    { AccessibilityRole::ListMarker, { "text", N_("text") } },
    { AccessibilityRole::Mark, { "mar", N_("mark") } },
    { AccessibilityRole::MathElement, { "math", N_("math") } },
    { AccessibilityRole::Menu, { "menu", N_("menu") } },
    { AccessibilityRole::MenuBar, { "menu bar", N_("menu bar") } },
    { AccessibilityRole::MenuButton, { "menu item", N_("menu item") } },
    { AccessibilityRole::MenuItem, { "menu item", N_("menu item") } },
    { AccessibilityRole::MenuItemCheckbox, { "check menu item", N_("check menu item") } },
    { AccessibilityRole::MenuItemRadio, { "radio menu item", N_("radio menu item") } },
    { AccessibilityRole::MenuListPopup, { "menu", N_("menu") } },
    { AccessibilityRole::MenuListOption, { "menu item", N_("menu item") } },
    { AccessibilityRole::Meter, { "level bar", N_("level bar") } },
    { AccessibilityRole::Outline, { "tree", N_("tree") } },
    { AccessibilityRole::Paragraph, { "paragraph", N_("paragraph") } },
    { AccessibilityRole::PopUpButton, { "combo box", N_("combo box") } },
    { AccessibilityRole::Pre, { "section", N_("section") } },
    { AccessibilityRole::ProgressIndicator, { "progress bar", N_("progress bar") } },
    { AccessibilityRole::RadioButton, { "radio button", N_("radio button") } },
    { AccessibilityRole::RadioGroup, { "panel", N_("panel") } },
    { AccessibilityRole::RowHeader, { "row header", N_("row header") } },
    { AccessibilityRole::Row, { "table row", N_("table row") } },
    { AccessibilityRole::ScrollArea, { "scroll pane", N_("scroll pane") } },
    { AccessibilityRole::ScrollBar, { "scroll bar", N_("scroll bar") } },
    { AccessibilityRole::SearchField, { "entry", N_("entry") } },
    { AccessibilityRole::Slider, { "slider", N_("slider") } },
    { AccessibilityRole::SpinButton, { "spin button", N_("spin button") } },
    { AccessibilityRole::SplitGroup, { "split pane", N_("split pane") } },
    { AccessibilityRole::Splitter, { "separator", N_("separator") } },
    { AccessibilityRole::StaticText, { "text", N_("text") } },
    { AccessibilityRole::Subscript, { "subscript", N_("subscript") } },
    { AccessibilityRole::Superscript, { "superscript", N_("superscript") } },
    { AccessibilityRole::Switch, { "toggle button", N_("toggle button") } },
    { AccessibilityRole::SVGRoot, { "panel", N_("panel") } },
    { AccessibilityRole::SVGText, { "section", N_("section") } },
    { AccessibilityRole::SVGTSpan, { "text", N_("text") } },
    { AccessibilityRole::SVGTextPath, { "text", N_("text") } },
    { AccessibilityRole::TabGroup, { "page tab list", N_("page tab list") } },
    { AccessibilityRole::TabList, { "page tab list", N_("page tab list") } },
    { AccessibilityRole::TabPanel, { "scroll pane", N_("scroll pane") } },
    { AccessibilityRole::Tab, { "page tab", N_("page tab") } },
    { AccessibilityRole::Table, { "table", N_("table") } },
    { AccessibilityRole::Term, { "description term", N_("description term") } },
    { AccessibilityRole::TextArea, { "entry", N_("entry") } },
    { AccessibilityRole::TextField, { "entry", N_("entry") } },
    { AccessibilityRole::TextGroup, { "section", N_("section") } },
    { AccessibilityRole::Time, { "text", N_("text") } },
    { AccessibilityRole::Tree, { "tree", N_("tree") } },
    { AccessibilityRole::TreeGrid, { "tree table", N_("tree table") } },
    { AccessibilityRole::TreeItem, { "tree item", N_("tree item") } },
    { AccessibilityRole::ToggleButton, { "toggle button", N_("toggle button") } },
    { AccessibilityRole::Toolbar, { "tool bar", N_("tool bar") } },
    { AccessibilityRole::Unknown, { "unknown", N_("unknown") } },
    { AccessibilityRole::UserInterfaceTooltip, { "tool tip", N_("tool tip") } },
    { AccessibilityRole::Video, { "video", N_("video") } },
    { AccessibilityRole::WebArea, { "document web", N_("document web") } },
    { AccessibilityRole::WebCoreLink, { "link", N_("link") } },
    { AccessibilityRole::Window, { "window", N_("window") } }
};

const char* AccessibilityAtspi::localizedRoleName(AccessibilityRole role)
{
    static constexpr SortedArrayMap roleNamesMap { roleNames };
    if (auto entry = roleNamesMap.tryGet(role))
        return entry->localizedName;

    return _("unknown");
}

#define ITEM_SIGNATURE "(so)(so)(so)iiassusau"
#define GET_ITEMS_SIGNATURE "a(" ITEM_SIGNATURE ")"

GDBusInterfaceVTable AccessibilityAtspi::s_cacheFunctions = {
    // method_call
    [](GDBusConnection*, const gchar* sender, const gchar*, const gchar*, const gchar* methodName, GVariant*, GDBusMethodInvocation* invocation, gpointer userData) {
        if (!g_strcmp0(methodName, "GetItems")) {
            auto& atspi = *static_cast<AccessibilityAtspi*>(userData);
            atspi.addClient(sender);
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(" GET_ITEMS_SIGNATURE ")"));
            g_variant_builder_open(&builder, G_VARIANT_TYPE(GET_ITEMS_SIGNATURE));
            for (auto* rootObject : atspi.m_rootObjects.keys()) {
                g_variant_builder_open(&builder, G_VARIANT_TYPE("(" ITEM_SIGNATURE ")"));
                rootObject->serialize(&builder);
                g_variant_builder_close(&builder);
            }

            // We need to call updateBackingStore() on every object before calling serialize()
            // and updating the backing store can detach the object and remove it from the cache.
            auto paths = copyToVector(atspi.m_cache.keys());
            for (const auto& path : paths) {
                auto wrapper = atspi.m_cache.get(path);
                wrapper->updateBackingStore();
                if (!atspi.m_cache.contains(path) || wrapper->isDefunct())
                    continue;
                g_variant_builder_open(&builder, G_VARIANT_TYPE("(" ITEM_SIGNATURE ")"));
                wrapper->serialize(&builder);
                g_variant_builder_close(&builder);
            }

            g_variant_builder_close(&builder);
            g_dbus_method_invocation_return_value(invocation, g_variant_builder_end(&builder));
        }
    },
    // get_property
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* propertyName, GError** error, gpointer) -> GVariant* {
        g_set_error(error, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED, "Unknown property '%s'", propertyName);
        return nullptr;
    },
    // set_property,
    nullptr,
    // padding
    { nullptr }
};

void AccessibilityAtspi::ensureCache()
{
    if (m_cacheID || !m_connection)
        return;

    m_cacheID = g_dbus_connection_register_object(m_connection.get(), "/org/a11y/atspi/cache", const_cast<GDBusInterfaceInfo*>(&webkit_cache_interface), &s_cacheFunctions, this, nullptr, nullptr);
}

void AccessibilityAtspi::addToCacheIfNeeded(AccessibilityObjectAtspi& atspiObject)
{
    atspiObject.updateBackingStore();
    if (atspiObject.isDefunct())
        return;

    auto addResult = m_cache.add(atspiObject.path(), &atspiObject);
    if (!addResult.isNewEntry)
        return;

    GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(" ITEM_SIGNATURE ")"));
    atspiObject.serialize(&builder);
    g_dbus_connection_emit_signal(m_connection.get(), nullptr, "/org/a11y/atspi/cache", "org.a11y.atspi.Cache", "AddAccessible",
        g_variant_new("(@(" ITEM_SIGNATURE "))", g_variant_builder_end(&builder)), nullptr);
}

void AccessibilityAtspi::cacheUpdateTimerFired()
{
    auto cacheUpdateList = std::exchange(m_cacheUpdateList, { });
    for (auto& atspiObject : cacheUpdateList)
        addToCacheIfNeeded(*atspiObject);
}

void AccessibilityAtspi::cacheClearTimerFired()
{
    for (const auto& registeredObjects : m_atspiHyperlinks.values()) {
        for (auto id : registeredObjects)
            g_dbus_connection_unregister_object(m_connection.get(), id);
    }
    m_atspiHyperlinks.clear();
    for (const auto& it : m_atspiObjects) {
        for (auto id : it.value)
            g_dbus_connection_unregister_object(m_connection.get(), id);

        it.key->didUnregisterObject();
    }
    m_atspiObjects.clear();
    m_cache.clear();

    RELEASE_ASSERT(m_cacheUpdateList.isEmpty());
    m_cacheUpdateTimer.stop();
    m_cacheClearTimer.stop();
}

namespace Accessibility {

PlatformRoleMap createPlatformRoleMap()
{
    PlatformRoleMap roleMap;
    for (const auto& entry : roleNames)
        roleMap.add(static_cast<unsigned>(entry.first), String::fromUTF8(entry.second.name));
    return roleMap;
}

} // namespace Accessibility

#if ENABLE(DEVELOPER_MODE)
void AccessibilityAtspi::addNotificationObserver(void* context, NotificationObserver&& observer)
{
    AXObjectCache::enableAccessibility();
    m_notificationObservers.add(context, WTFMove(observer));
}

void AccessibilityAtspi::removeNotificationObserver(void* context)
{
    m_notificationObservers.remove(context);
}

void AccessibilityAtspi::notify(AccessibilityObjectAtspi& atspiObject, const char* name, NotificationObserverParameter parameter) const
{
    if (m_notificationObservers.isEmpty())
        return;

    for (auto* context : copyToVector(m_notificationObservers.keys())) {
        auto it = m_notificationObservers.find(context);
        ASSERT(it != m_notificationObservers.end());
        it->value(atspiObject, name, parameter);
    }
}

void AccessibilityAtspi::notifyStateChanged(AccessibilityObjectAtspi& atspiObject, const char* name, bool value) const
{
    auto notificationName = [&](const char* name) -> const char* {
        if (!g_strcmp0(name, "checked"))
            return "CheckedStateChanged";
        if (!g_strcmp0(name, "invalid-entry"))
            return "AXInvalidStatusChanged";
        if (!g_strcmp0(name, "active"))
            return "ActiveStateChanged";
        if (!g_strcmp0(name, "busy"))
            return "AXElementBusyChanged";
        if (!g_strcmp0(name, "enabled"))
            return "AXDisabledStateChanged";
        if (!g_strcmp0(name, "expanded"))
            return "AXExpandedChanged";
        if (!g_strcmp0(name, "pressed"))
            return "AXPressedStateChanged";
        if (!g_strcmp0(name, "read-only"))
            return "AXReadOnlyStatusChanged";
        if (!g_strcmp0(name, "required"))
            return "AXRequiredStatusChanged";
        if (!g_strcmp0(name, "sensitive"))
            return "AXSensitiveStateChanged";
        if (!g_strcmp0(name, "focused") && value)
            return "AXFocusedUIElementChanged";

        return nullptr;
    };

    const char* notification = notificationName(name);
    if (!notification)
        return;
    
    notify(atspiObject, notification, value);
}

void AccessibilityAtspi::notifySelectionChanged(AccessibilityObjectAtspi& atspiObject) const
{   
    notify(atspiObject, "AXSelectedChildrenChanged", nullptr);
}

void AccessibilityAtspi::notifyMenuSelectionChanged(AccessibilityObjectAtspi& atspiObject) const
{
    notify(atspiObject, "AXMenuItemSelected", nullptr);
}

void AccessibilityAtspi::notifyTextChanged(AccessibilityObjectAtspi& atspiObject) const
{
    notify(atspiObject, "AXTextChanged", nullptr);
}

void AccessibilityAtspi::notifyTextCaretMoved(AccessibilityObjectAtspi& atspiObject, unsigned caretOffset) const
{
    notify(atspiObject, "AXTextCaretMoved", caretOffset);
}

void AccessibilityAtspi::notifyValueChanged(AccessibilityObjectAtspi& atspiObject) const
{
    notify(atspiObject, "AXValueChanged", nullptr);
}

void AccessibilityAtspi::notifyLoadEvent(AccessibilityObjectAtspi& atspiObject, const CString& event) const
{
    if (event != "LoadComplete")
        return;

    notify(atspiObject, "AXLoadComplete", nullptr);
}

#endif

} // namespace WebCore

#endif // USE(ATSPI)
