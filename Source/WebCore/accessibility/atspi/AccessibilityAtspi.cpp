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
#include <wtf/SetForScope.h>
#include <wtf/SortedArrayMap.h>
#include <wtf/UUID.h>

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

        if (m_connection)
            initializeRegistry();
        else
            g_warning("Can't connect to a11y bus: %s", error->message);
    });
}

AccessibilityAtspi::~AccessibilityAtspi()
{
    if (m_registry)
        g_signal_handlers_disconnect_by_data(m_registry.get(), this);
}

void AccessibilityAtspi::registerTrees() const
{
    RELEASE_ASSERT(!isMainThread());
    for (auto* rootObject : m_rootObjects.keys()) {
        if (!rootObject->isTreeRegistered())
            rootObject->registerTree();
    }
}

void AccessibilityAtspi::initializeRegistry()
{
    RELEASE_ASSERT(!isMainThread());
    RELEASE_ASSERT(m_connection);
    GUniqueOutPtr<GError> error;
    m_registry = adoptGRef(g_dbus_proxy_new_sync(m_connection.get(), G_DBUS_PROXY_FLAGS_DO_NOT_LOAD_PROPERTIES, nullptr,
        "org.a11y.atspi.Registry", "/org/a11y/atspi/registry", "org.a11y.atspi.Registry", nullptr, &error.outPtr()));
    if (!m_registry) {
        g_warning("Failed to connect to atspi registry: %s\n", error->message);
        return;
    }

    g_signal_connect(m_registry.get(), "g-signal", G_CALLBACK(+[](GDBusProxy*, char*, char* signal, GVariant* parameters, AccessibilityAtspi* atspi) {
        const char* dbusName;
        const char* eventName;
        if (!g_strcmp0(signal, "EventListenerRegistered")) {
            g_variant_get(parameters, "(&s&s@as)", &dbusName, &eventName, nullptr);
            atspi->addEventListener(dbusName, eventName);
            atspi->registerTrees();
        } else if (!g_strcmp0(signal, "EventListenerDeregistered")) {
            g_variant_get(parameters, "(&s&s)", &dbusName, &eventName);
            atspi->removeEventListener(dbusName, eventName);
        }
    }), this);

    GRefPtr<GVariant> result = adoptGRef(g_dbus_proxy_call_sync(m_registry.get(), "GetRegisteredEvents", nullptr, G_DBUS_CALL_FLAGS_NONE, -1, nullptr, &error.outPtr()));
    if (!result) {
        g_warning("Failed to get atspi registered event listeners: %s\n", error->message);
        return;
    }

    GRefPtr<GVariant> events;
    g_variant_get(result.get(), "(@a(ss))", &events.outPtr());
    GVariantIter iter;
    auto eventCount = g_variant_iter_init(&iter, events.get());
    const char* dbusName;
    const char* eventName;
    while (g_variant_iter_loop(&iter, "(&s&s)", &dbusName, &eventName))
        addEventListener(dbusName, eventName);

    if (eventCount)
        registerTrees();
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
    RELEASE_ASSERT(!isMainThread());
    auto& listeners = m_eventListeners.ensure(dbusName, [] {
        return Vector<GUniquePtr<char*>> { };
    }).iterator->value;
    listeners.append(eventConvertingDetailToNonCamelCase(eventName));
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
    RELEASE_ASSERT(!isMainThread());
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

bool AccessibilityAtspi::shouldEmitSignal(const char* interface, const char* name, const char* detail)
{
    RELEASE_ASSERT(!isMainThread());

    // Always emit signals if we couldn't connect to the registry.
    if (!m_registry)
        return true;

    if (m_eventListeners.isEmpty())
        return false;

    const char* needle[4] = { interface, name, detail, nullptr };
    for (const auto& listeners : m_eventListeners.values()) {
        auto result = listeners.findMatching([&](const GUniquePtr<char*>& event) {
            return eventIsSubtype(const_cast<char**>(needle), event.get());
        });
        if (result != notFound)
            return true;
    }

    return false;
}

RunLoop& AccessibilityAtspi::runLoop() const
{
    return m_queue->runLoop();
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
            ensureCache();
            String path = makeString("/org/a11y/webkit/accessible/", createCanonicalUUIDString().replace('-', '_'));
            Vector<unsigned, 3> registeredObjects;
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

void AccessibilityAtspi::unregisterRoot(AccessibilityRootAtspi& rootObject)
{
    RELEASE_ASSERT(isMainThread());
    m_queue->dispatch([this, rootObject = Ref { rootObject }] {
        if (!m_connection)
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, rootObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "StateChanged",
            g_variant_new("(siiva{sv})", "defunct", TRUE, 0, g_variant_new_string("0"), nullptr), nullptr);

        auto registeredObjects = m_rootObjects.take(rootObject.ptr());
        g_dbus_connection_emit_signal(m_connection.get(), nullptr, "/org/a11y/atspi/cache", "org.a11y.atspi.Cache", "RemoveAccessible",
            g_variant_new("((so))", uniqueName(), rootObject->path().utf8().data()), nullptr);
        for (auto id : registeredObjects)
            g_dbus_connection_unregister_object(m_connection.get(), id);
    });
}

String AccessibilityAtspi::registerObject(AccessibilityObjectAtspi& atspiObject, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&& interfaces)
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_connection)
        return { };

    ensureCache();
    String path = makeString("/org/a11y/atspi/accessible/", createCanonicalUUIDString().replace('-', '_'));
    Vector<unsigned, 20> registeredObjects;
    registeredObjects.reserveInitialCapacity(interfaces.size());
    for (const auto& interface : interfaces) {
        auto id = g_dbus_connection_register_object(m_connection.get(), path.utf8().data(), interface.first, interface.second, &atspiObject, nullptr, nullptr);
        registeredObjects.uncheckedAppend(id);
    }
    m_atspiObjects.add(&atspiObject, WTFMove(registeredObjects));

    return path;
}

void AccessibilityAtspi::unregisterObject(AccessibilityObjectAtspi& atspiObject)
{
    RELEASE_ASSERT(isMainThread());
    m_queue->dispatch([this, atspiObject = Ref { atspiObject }] {
        if (!m_connection)
            return;

        if (m_atspiHyperlinks.contains(atspiObject.ptr())) {
            auto registeredObjects = m_atspiHyperlinks.take(atspiObject.ptr());
            for (auto id : registeredObjects)
                g_dbus_connection_unregister_object(m_connection.get(), id);
        }

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "StateChanged",
            g_variant_new("(siiva{sv})", "defunct", TRUE, 0, g_variant_new_string("0"), nullptr), nullptr);

        removeAccessible(atspiObject);

        auto registeredObjects = m_atspiObjects.take(atspiObject.ptr());
        for (auto id : registeredObjects)
            g_dbus_connection_unregister_object(m_connection.get(), id);
    });
}

String AccessibilityAtspi::registerHyperlink(AccessibilityObjectAtspi& atspiObject, Vector<std::pair<GDBusInterfaceInfo*, GDBusInterfaceVTable*>>&& interfaces)
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_connection)
        return { };

    String path = makeString("/org/a11y/atspi/accessible/", createCanonicalUUIDString().replace('-', '_'));
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
    RELEASE_ASSERT(isMainThread());
    m_queue->dispatch([this, atspiObject = Ref { atspiObject }] {
        if (!m_connection)
            return;

        // Always emit parentChanged when the tree is registered because the atspi cache always consumes it.
        if (!atspiObject->root().isTreeRegistered())
            return;

        // Emit parentChanged only if the object is already registered, otherwise register the object,
        // without emitting the signal, because org.a11y.atspi.Cache.AddAccessible() will update the cache.
        if (atspiObject->registerObject())
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "PropertyChange",
            g_variant_new("(siiva{sv})", "accessible-parent", 0, 0, atspiObject->parentReference(), nullptr), nullptr);
    });
}

void AccessibilityAtspi::parentChanged(AccessibilityRootAtspi& rootObject)
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_connection)
        return;

    g_dbus_connection_emit_signal(m_connection.get(), nullptr, rootObject.path().utf8().data(), "org.a11y.atspi.Event.Object", "PropertyChange",
        g_variant_new("(siiva{sv})", "accessible-parent", 0, 0, rootObject.parentReference(), nullptr), nullptr);
}

void AccessibilityAtspi::childrenChanged(AccessibilityObjectAtspi& atspiObject, AccessibilityObjectAtspi& child, ChildrenChanged change)
{
    RELEASE_ASSERT(isMainThread());

#if ENABLE(DEVELOPER_MODE)
    notifyChildrenChanged(atspiObject, child, change);
#endif

    m_queue->dispatch([this, atspiObject = Ref { atspiObject }, child = Ref { child }, change] {
        if (!m_connection)
            return;

        // Always emit ChildrenChanged when the tree is registered because the atspi cache always consumes it.
        if (!atspiObject->root().isTreeRegistered())
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "ChildrenChanged",
            g_variant_new("(siiv(so))", change == ChildrenChanged::Added ? "add" : "remove", child->indexInParentForChildrenChanged(change),
            0, g_variant_new("(so)", uniqueName(), child->path().utf8().data()), uniqueName(), atspiObject->path().utf8().data()), nullptr);
    });
}

void AccessibilityAtspi::childrenChanged(AccessibilityRootAtspi& rootObject, AccessibilityObjectAtspi& child, ChildrenChanged change)
{
    RELEASE_ASSERT(isMainThread());

    m_queue->dispatch([this, rootObject = Ref { rootObject }, child = Ref { child }, change] {
        if (!m_connection)
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, rootObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "ChildrenChanged",
            g_variant_new("(siiv(so))", change == ChildrenChanged::Added ? "add" : "remove", 0,
            0, g_variant_new("(so)", uniqueName(), child->path().utf8().data()), uniqueName(), rootObject->path().utf8().data()), nullptr);
    });
}

void AccessibilityAtspi::stateChanged(AccessibilityObjectAtspi& atspiObject, const char* name, bool value)
{
    RELEASE_ASSERT(isMainThread());

#if ENABLE(DEVELOPER_MODE)
    notifyStateChanged(atspiObject, name, value);
#endif

    m_queue->dispatch([this, atspiObject = Ref { atspiObject }, name = CString(name), value] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Object", "StateChanged", name.data()))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "StateChanged",
            g_variant_new("(siiva{sv})", name.data(), value, 0, g_variant_new_string("0"), nullptr), nullptr);
    });
}

void AccessibilityAtspi::textChanged(AccessibilityObjectAtspi& atspiObject, const char* changeType, CString&& text, unsigned offset, unsigned length)
{
    RELEASE_ASSERT(isMainThread());

#if ENABLE(DEVELOPER_MODE)
    notifyTextChanged(atspiObject);
#endif

    m_queue->dispatch([this, atspiObject = Ref { atspiObject }, changeType = CString(changeType), text = WTFMove(text), offset, length] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Object", "TextChanged", changeType.data()))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "TextChanged",
            g_variant_new("(siiva{sv})", changeType.data(), offset, length, g_variant_new_string(text.data()), nullptr), nullptr);
    });
}

void AccessibilityAtspi::textAttributesChanged(AccessibilityObjectAtspi& atspiObject)
{
    RELEASE_ASSERT(isMainThread());
    m_queue->dispatch([this, atspiObject = Ref { atspiObject }] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Object", "TextAttributesChanged"))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "TextAttributesChanged",
            g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
    });
}

void AccessibilityAtspi::textCaretMoved(AccessibilityObjectAtspi& atspiObject, unsigned caretOffset)
{
    RELEASE_ASSERT(isMainThread());

#if ENABLE(DEVELOPER_MODE)
    notifyTextCaretMoved(atspiObject, caretOffset);
#endif

    m_queue->dispatch([this, atspiObject = Ref { atspiObject }, caretOffset] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Object", "TextCaretMoved"))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "TextCaretMoved",
            g_variant_new("(siiva{sv})", "", caretOffset, 0, g_variant_new_string(""), nullptr), nullptr);
    });
}

void AccessibilityAtspi::textSelectionChanged(AccessibilityObjectAtspi& atspiObject)
{
    RELEASE_ASSERT(isMainThread());
    m_queue->dispatch([this, atspiObject = Ref { atspiObject }] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Object", "TextSelectionChanged"))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "TextSelectionChanged",
            g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
    });
}

void AccessibilityAtspi::valueChanged(AccessibilityObjectAtspi& atspiObject, double value)
{
    RELEASE_ASSERT(isMainThread());

#if ENABLE(DEVELOPER_MODE)
    notifyValueChanged(atspiObject);
#endif

    m_queue->dispatch([this, atspiObject = Ref { atspiObject }, value] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Object", "PropertyChange", "accessible-value"))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "PropertyChange",
            g_variant_new("(siiva{sv})", "accessible-value", 0, 0, g_variant_new_double(value), nullptr), nullptr);
    });
}

void AccessibilityAtspi::selectionChanged(AccessibilityObjectAtspi& atspiObject)
{
    RELEASE_ASSERT(isMainThread());

#if ENABLE(DEVELOPER_MODE)
    notifySelectionChanged(atspiObject);
#endif

    m_queue->dispatch([this, atspiObject = Ref { atspiObject }] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Object", "SelectionChanged"))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Object", "SelectionChanged",
            g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
    });
}

void AccessibilityAtspi::loadEvent(AccessibilityObjectAtspi& atspiObject, CString&& event)
{
    RELEASE_ASSERT(isMainThread());

#if ENABLE(DEVELOPER_MODE)
    notifyLoadEvent(atspiObject, event);
#endif

    m_queue->dispatch([this, atspiObject = Ref { atspiObject }, event = WTFMove(event)] {
        if (!m_connection)
            return;

        if (!shouldEmitSignal("Document", event.data()))
            return;

        g_dbus_connection_emit_signal(m_connection.get(), nullptr, atspiObject->path().utf8().data(), "org.a11y.atspi.Event.Document", event.data(),
            g_variant_new("(siiva{sv})", "", 0, 0, g_variant_new_string(""), nullptr), nullptr);
    });
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
    { AccessibilityRole::DescriptionListTerm, { "description term", N_("description term") } },
    { AccessibilityRole::DescriptionListDetail, { "description value", N_("description value") } },
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
    [](GDBusConnection*, const gchar*, const gchar*, const gchar*, const gchar* methodName, GVariant*, GDBusMethodInvocation* invocation, gpointer userData) {
        RELEASE_ASSERT(!isMainThread());
        if (!g_strcmp0(methodName, "GetItems")) {
            auto& atspi = *static_cast<AccessibilityAtspi*>(userData);
            SetForScope<bool> inGetItems(atspi.m_inGetItems, true);
            GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(" GET_ITEMS_SIGNATURE ")"));
            g_variant_builder_open(&builder, G_VARIANT_TYPE(GET_ITEMS_SIGNATURE));
            for (auto* rootObject : atspi.m_rootObjects.keys()) {
                g_variant_builder_open(&builder, G_VARIANT_TYPE("(" ITEM_SIGNATURE ")"));
                rootObject->registerTree();
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
    nullptr
};

void AccessibilityAtspi::ensureCache()
{
    RELEASE_ASSERT(!isMainThread());
    if (m_cacheID || !m_connection)
        return;

    m_cacheID = g_dbus_connection_register_object(m_connection.get(), "/org/a11y/atspi/cache", const_cast<GDBusInterfaceInfo*>(&webkit_cache_interface), &s_cacheFunctions, this, nullptr, nullptr);
}

void AccessibilityAtspi::addAccessible(AccessibilityObjectAtspi& atspiObject)
{
    RELEASE_ASSERT(!isMainThread());
    if (!m_connection)
        return;

    auto addResult = m_cache.add(atspiObject.path(), &atspiObject);
    if (!addResult.isNewEntry)
        return;

    if (m_inGetItems)
        return;

    GVariantBuilder builder = G_VARIANT_BUILDER_INIT(G_VARIANT_TYPE("(" ITEM_SIGNATURE ")"));
    atspiObject.serialize(&builder);
    g_dbus_connection_emit_signal(m_connection.get(), nullptr, "/org/a11y/atspi/cache", "org.a11y.atspi.Cache", "AddAccessible",
        g_variant_new("(@(" ITEM_SIGNATURE "))", g_variant_builder_end(&builder)), nullptr);
}

void AccessibilityAtspi::removeAccessible(AccessibilityObjectAtspi& atspiObject)
{
    RELEASE_ASSERT(!isMainThread());
    const auto& path = atspiObject.path();
    m_cache.remove(path);
    g_dbus_connection_emit_signal(m_connection.get(), nullptr, "/org/a11y/atspi/cache", "org.a11y.atspi.Cache", "RemoveAccessible",
        g_variant_new("((so))", uniqueName(), path.utf8().data()), nullptr);
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
    m_notificationObservers.add(context, WTFMove(observer));
}

void AccessibilityAtspi::removeNotificationObserver(void* context)
{
    m_notificationObservers.remove(context);
}

void AccessibilityAtspi::notifyStateChanged(AccessibilityObjectAtspi& atspiObject, const char* name, bool value) const
{
    if (m_notificationObservers.isEmpty())
        return;

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

    for (const auto& observer : m_notificationObservers.values())
        observer(atspiObject, notification, value);
}

void AccessibilityAtspi::notifySelectionChanged(AccessibilityObjectAtspi& atspiObject) const
{
    for (const auto& observer : m_notificationObservers.values())
        observer(atspiObject, "AXSelectedChildrenChanged", nullptr);
}

void AccessibilityAtspi::notifyTextChanged(AccessibilityObjectAtspi& atspiObject) const
{
    for (const auto& observer : m_notificationObservers.values())
        observer(atspiObject, "AXTextChanged", nullptr);
}

void AccessibilityAtspi::notifyTextCaretMoved(AccessibilityObjectAtspi& atspiObject, unsigned caretOffset) const
{
    for (const auto& observer : m_notificationObservers.values())
        observer(atspiObject, "AXTextCaretMoved", caretOffset);
}

void AccessibilityAtspi::notifyChildrenChanged(AccessibilityObjectAtspi& atspiObject, AccessibilityObjectAtspi& child, ChildrenChanged change) const
{
    const char* notification = change == ChildrenChanged::Added ? "AXChildrenAdded" : "AXChildrenRemoved";
    for (const auto& observer : m_notificationObservers.values())
        observer(atspiObject, notification, child);
}

void AccessibilityAtspi::notifyValueChanged(AccessibilityObjectAtspi& atspiObject) const
{
    for (const auto& observer : m_notificationObservers.values())
        observer(atspiObject, "AXValueChanged", nullptr);
}

void AccessibilityAtspi::notifyLoadEvent(AccessibilityObjectAtspi& atspiObject, const CString& event) const
{
    if (event != "LoadComplete")
        return;

    for (const auto& observer : m_notificationObservers.values())
        observer(atspiObject, "AXLoadComplete", nullptr);
}

#endif

} // namespace WebCore

#endif // ENABLE(ACCESSIBILITY) && USE(ATSPI)
