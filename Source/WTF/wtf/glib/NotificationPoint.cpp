/*
 *  Copyright (C) 2026 Igalia. S.L. . All rights reserved.
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
 *
 */

#include "config.h"
#include <wtf/NotificationPoint.h>

#include <wtf/Assertions.h>
#include <wtf/Logging.h>
#include <wtf/NeverDestroyed.h>

namespace WTF {

Lock OwnedNotificationPoint::ownedNotificationPointLock;

static void destroyPair(void* pair, GClosure*)
{
    auto namePath = static_cast<std::pair<String, String>*>(pair);
    delete namePath;
}

OwnedNotificationPoint::OwnedNotificationPoint(String name, String path)
    : m_name(name)
    , m_path(path)
    , m_exportID(0)
    , m_listeners()
{
    m_actionGroup = g_simple_action_group_new();
    m_action = g_simple_action_new_stateful("notify", nullptr, g_variant_new_int64(0));

    // We can't store a reference to the OwnedNotificationPoint here, since the object might get destroyed before the invocation of the callback.
    g_signal_connect_data(m_action, "activate", G_CALLBACK(&OwnedNotificationPoint::actionCallback), new std::pair(name, path), destroyPair, G_CONNECT_SWAPPED);
    g_action_map_add_action(G_ACTION_MAP(m_actionGroup), G_ACTION(m_action));
}

void OwnedNotificationPoint::addNotificationPoint(WeakPtr<NotificationPoint> np)
{
    m_listeners.append(np);
}

void OwnedNotificationPoint::removeNotificationPoint(WeakPtr<NotificationPoint> np)
{
    m_listeners.removeAll(np);
    if (m_listeners.isEmpty()) {
        // We hold ownedNotificationPointLock here, so it's safe to delete this object.
        delete this;
    }

}

static GDBusConnection* s_connection;

// Notification points that have truly been exported to the bus under the owned name.
static HashMap<String, HashMap<String, OwnedNotificationPoint*>>& ownedNotificationPoints()
{
    static LazyNeverDestroyed<HashMap<String, HashMap<String, OwnedNotificationPoint*>>> map;
    static std::once_flag once;
    std::call_once(once, [] {
        map.construct();
    });
    return map;
}

// Before we've acquired a connection or bus name, we don't want to block, so
// we handle the export asynchronously.
static HashMap<String, HashMap<String, OwnedNotificationPoint*>>& pendingNotificationPoints()
{
    static LazyNeverDestroyed<HashMap<String, HashMap<String, OwnedNotificationPoint*>>> map;
    static std::once_flag once;
    std::call_once(once, [] {
        map.construct();
    });
    return map;
}

static OwnedNotificationPoint* findOwnedNotificationPoint(HashMap<String, HashMap<String, OwnedNotificationPoint*>>& container, String name, String path)
{
    auto nameMap = container.find(name);
    if (nameMap == container.end())
        return nullptr;
    auto map = nameMap->value;
    auto iter = map.find(path);
    if (iter == map.end())
        return nullptr;
    return iter->value;
}

// It's important that we not try to do a d-bus call to owned objects or we'd
// deadlock (we're the ones that need to respond to the message).
OwnedNotificationPoint* OwnedNotificationPoint::ownedNotificationPoint(String name, String path)
{
    auto ownedPoint = findOwnedNotificationPoint(pendingNotificationPoints(), name, path);
    if (ownedPoint != nullptr)
        return ownedPoint;
    ownedPoint = findOwnedNotificationPoint(ownedNotificationPoints(), name, path);
    if (ownedPoint != nullptr)
        return ownedPoint;
    return nullptr;
}

static HashMap<String, guint>& ownerIDs()
{
    static LazyNeverDestroyed<HashMap<String, guint>> map;
    static std::once_flag once;
    std::call_once(once, [] {
        map.construct();
    });
    return map;
}

static void deregisterOwnedName(String name)
{
    g_bus_unown_name(ownerIDs().get(name));
    ownerIDs().remove(name);
    ownedNotificationPoints().get(name).clear();
    ownedNotificationPoints().remove(name);
}

static int ensureConnection()
{
    if (!s_connection) {
        g_autoptr(GError) error = nullptr;
        s_connection = g_bus_get_sync(G_BUS_TYPE_SESSION, nullptr, &error);
        if (!s_connection) {
            g_warning("Cannot connect to bus: %s", error->message);
            return error->code;
        }
    }
    return 0;
}

static void onBusAcquired(GDBusConnection* connection, const char*, void*)
{
    s_connection = connection;
}

static void onBusNameLost(GDBusConnection* connection, const char* name, void*)
{
    if (connection) {
        RELEASE_LOG_ERROR(Process, "Lost D-Bus well-known name %s", name);
        auto ownedName = String::fromUTF8(name);
        pendingNotificationPoints().remove(ownedName);
        ownedNotificationPoints().remove(ownedName);
        ownerIDs().remove(ownedName);
    } else {
        RELEASE_LOG_ERROR(Process, "Lost D-Bus connection");
        pendingNotificationPoints().clear();
        ownedNotificationPoints().clear();
        ownerIDs().clear();
    }
}

#if PLATFORM(GTK)
static const ASCIILiteral ownerNamePrefix = "org.webkitgtk-";
#elif PLATFORM(WPE)
static const ASCIILiteral ownerNamePrefix = "org.wpewebkit-";
#else
#error No platform name defined
#endif

bool OwnedNotificationPoint::isLive() const
{
    return m_exportID;
}

uint64_t OwnedNotificationPoint::getState()
{
    return static_cast<uint64_t>(g_variant_get_int64(g_action_get_state(G_ACTION(m_action))));
}

void OwnedNotificationPoint::setState(uint64_t state)
{
    g_action_change_state(G_ACTION(m_action), g_variant_new_int64(static_cast<int64_t>(state)));
}

void OwnedNotificationPoint::notify()
{
    for (auto np : m_listeners) {
        NotificationPoint* point = np.get();
        // We remove the point from m_listeners before destroying the object, so this shouldn't be possible.
        RELEASE_ASSERT(point);
        point->actionCallback();
    }
}

void OwnedNotificationPoint::processPendingForName(String name)
{
    RELEASE_ASSERT(pendingNotificationPoints().contains(name));
    for (auto [_, ownedPoint] : pendingNotificationPoints().get(name)) {
        GActionGroup* actionGroup = G_ACTION_GROUP(ownedPoint->m_actionGroup);
        g_autoptr(GError) error = nullptr;
        RELEASE_LOG_INFO(Process, "Registering path %s for %s", ownedPoint->m_path.utf8().data(), ownedPoint->m_name.utf8().data());
        guint exportID = g_dbus_connection_export_action_group(s_connection, ownedPoint->m_path.utf8().data(), actionGroup, &error);
        if (!exportID)
            g_warning("Cannot expose remote control interface to bus: %s", error->message);
        ownedPoint->m_exportID = exportID;
        addOwnedNotificationPoint(ownedNotificationPoints(), ownedPoint);
    }
    pendingNotificationPoints().remove(name);
}

void OwnedNotificationPoint::addOwnedNotificationPoint(HashMap<String, HashMap<String, OwnedNotificationPoint*>>& container, OwnedNotificationPoint* ownedPoint)
{
    auto iter = container.find(ownedPoint->m_name);
    if (iter != container.end()) {
        RELEASE_ASSERT(!iter->value.contains(ownedPoint->m_path));
        iter->value.set(ownedPoint->m_path, ownedPoint);
    } else {
        auto map = HashMap<String, OwnedNotificationPoint*> { { ownedPoint->m_path, ownedPoint } };
        container.set(ownedPoint->m_name, map);
    }
}

void OwnedNotificationPoint::onBusNameAcquired(GDBusConnection*, const char* name, void*)
{
    RELEASE_LOG_INFO(Process, "Acquired D-Bus well-known name %s INFO", name);
    Locker locker { ownedNotificationPointLock };
    OwnedNotificationPoint::processPendingForName(String::fromUTF8(name));
}

void OwnedNotificationPoint::actionCallback(std::pair<String, String>* namePath)
{
    Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
    auto ownedPoint = findOwnedNotificationPoint(ownedNotificationPoints(), namePath->first, namePath->second);
    if (!ownedPoint)
        return; // We raced against all registered notification points having been destroyed in another thread,
    ownedPoint->notify();
}

OwnedNotificationPoint::~OwnedNotificationPoint()
{
    if (s_connection) {
        // We may end up here if we destroy the notification point before we've
        // managed to acquire a d-bus connection.
        g_dbus_connection_unexport_action_group(s_connection, m_exportID);
    }
    g_action_map_remove_action(G_ACTION_MAP(m_actionGroup), "activate");
    auto iter = pendingNotificationPoints().find(m_name);
    if (iter != pendingNotificationPoints().end()) {
        auto& map = iter->value;
        RELEASE_ASSERT(map.remove(m_path));
        if (map.isEmpty()) {
            RELEASE_ASSERT(pendingNotificationPoints().remove(m_name));
            if (!ownedNotificationPoints().contains(m_name)) {
                // We don't want to own this name any more and it
                // hasn't been registered, so simply drop it --
                // no need to unown it.
                ownerIDs().remove(m_name);
            }
        }
        return;
    }
    auto map = ownedNotificationPoints().find(m_name);
    RELEASE_ASSERT(map != ownedNotificationPoints().end());
    RELEASE_ASSERT(map->value.remove(m_path));
    if (map->value.isEmpty()) {
        RELEASE_ASSERT(ownedNotificationPoints().remove(m_name));
        deregisterOwnedName(m_name);
    }
    RELEASE_ASSERT(m_exportID);
}

bool NotificationPoint::isLive() const
{
    Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
    return m_ownedNotificationPoint->isLive();
}

Expected<RefPtr<NotificationPoint>, NotificationPoint::Error> NotificationPoint::create(ASCIILiteral providedPath, Function<void()>&& callback)
{
    return createWithName(ownerNamePrefix, providedPath, WTF::move(callback));
}

Expected<RefPtr<NotificationPoint>, NotificationPoint::Error> NotificationPoint::createWithName(ASCIILiteral providedName, ASCIILiteral providedPath, Function<void()>&& callback)
{
    Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
    String path = makeString('/', makeStringByReplacingAll(String(providedPath), '.', '/'));
    auto name = makeString(providedName, '-', getpid());

    OwnedNotificationPoint* ownedPoint = nullptr;
    if (!(ownedPoint = findOwnedNotificationPoint(pendingNotificationPoints(), name, path)))
        ownedPoint = findOwnedNotificationPoint(ownedNotificationPoints(), name, path);
    if (ownedPoint) {
        NotificationPoint* point = new NotificationPoint(ownedPoint, WTF::move(callback));
        ownedPoint->addNotificationPoint(WeakPtr(point));
        return adoptRef(point);
    }

    ownedPoint = new WTF::OwnedNotificationPoint(name, path);
    NotificationPoint* point = new NotificationPoint(ownedPoint, WTF::move(callback));
    ownedPoint->addNotificationPoint(WeakPtr(point));
    bool needToRequestBusName = !ownerIDs().contains(name);

    // If we haven't claimed the provided bus name yet, place the
    // NotificationPoint in a worklist for the callback to find and then export.
    OwnedNotificationPoint::addOwnedNotificationPoint(pendingNotificationPoints(), ownedPoint);

    if (needToRequestBusName) {
        // We need to wait for a bus address to do the export, will be
        // handled in the callback we register here.
        guint ownerId = g_bus_own_name(G_BUS_TYPE_SESSION,
            name.utf8().data(),
            G_BUS_NAME_OWNER_FLAGS_NONE,
            onBusAcquired,
            OwnedNotificationPoint::onBusNameAcquired,
            onBusNameLost,
            nullptr,
            nullptr);
        ownerIDs().set(name, ownerId);
    } else if (ownedNotificationPoints().contains(name)) {
        // We already have an address, we can do the export now.
        OwnedNotificationPoint::processPendingForName(name);
    }
    return adoptRef(point);
}

Expected<bool, NotificationPoint::Error> NotificationPoint::getAndClearNotificationsPosted()
{
    auto ret = m_notificationsPosted;
    m_notificationsPosted = false;
    return ret;
}

NotificationPoint::~NotificationPoint()
{
    Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
    m_ownedNotificationPoint->removeNotificationPoint(this);
}

Expected<uint64_t, NotificationPoint::Error> NotificationPoint::getState()
{
    Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
    return m_ownedNotificationPoint->getState();
}

Expected<uint64_t, NotificationPoint::Error> NotificationPoint::getState(ASCIILiteral providedName, String providedPath)
{

    auto name = makeString(providedName, '-', getpid());
    auto path = makeString('/', providedPath);
    {
        Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };

        if (auto ownedPoint = OwnedNotificationPoint::ownedNotificationPoint(name, path))
            return ownedPoint->getState();
    }
    if (ensureConnection())
        return makeUnexpected(NotificationPoint::Error::ConnectionError);
    g_autoptr(GError) error = nullptr;
    auto result = g_dbus_connection_call_sync(s_connection,
        name.utf8().data(),
        path.utf8().data(),
        "org.gtk.Actions",
        "Describe",
        g_variant_new("(s)", "notify"),
        g_variant_type_new("((bgav))"),
        G_DBUS_CALL_FLAGS_NONE,
        1000,
        nullptr,
        &error);
    if (!result) {
        g_warning("Couldn't getState for path %s under %s: %s", path.utf8().data(), name.utf8().data(), error->message);
        return makeUnexpected(NotificationPoint::Error::Other);
    }

    GVariantIter* iter;
    g_variant_get(result, "((bgav))", nullptr, nullptr, &iter);

    GVariant* returnedValue;
    RELEASE_ASSERT(g_variant_iter_loop(iter, "v", &returnedValue));
    g_variant_iter_free(iter);

    gint64 intValue = g_variant_get_int64(returnedValue);
    return static_cast<uint64_t>(intValue);
}

Expected<void, NotificationPoint::Error> NotificationPoint::setState(uint64_t value)
{
    Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
    m_ownedNotificationPoint->setState(value);
    return { };
}

Expected<void, NotificationPoint::Error> NotificationPoint::setState(ASCIILiteral providedName, String providedPath, uint64_t value)
{
    auto path = makeString('/', providedPath);
    auto name = makeString(providedName, '-', getpid());
    {
        Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
        if (auto ownedPoint = OwnedNotificationPoint::ownedNotificationPoint(name, path)) {
            ownedPoint->setState(value);
            return { };
        }
    }
    if (ensureConnection())
        return makeUnexpected(NotificationPoint::Error::ConnectionError);
    g_autoptr(GError) error = nullptr;
    auto callSyncresult = g_dbus_connection_call_sync(s_connection,
        name.utf8().data(),
        path.utf8().data(),
        "org.gtk.Actions",
        "SetState",
        g_variant_new_int64(value),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        1000,
        nullptr,
        &error);
    if (!callSyncresult) {
        g_warning("Couldn't setState for path %s under %s: %s", path.utf8().data(), name.utf8().data(), error->message);
        return makeUnexpected(NotificationPoint::Error::Other);
    }
    return { };
}

Expected<void, NotificationPoint::Error> NotificationPoint::notify()
{
    Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };
    m_ownedNotificationPoint->notify();
    return { };
}

Expected<void, NotificationPoint::Error> NotificationPoint::notify(ASCIILiteral providedName, String providedPath)
{
    auto name = makeString(providedName, '-', getpid());
    auto path = makeString('/', providedPath);
    {
        Locker locker { OwnedNotificationPoint::ownedNotificationPointLock };

        if (auto ownedPoint = OwnedNotificationPoint::ownedNotificationPoint(name, path)) {
            ownedPoint->notify();
            return { };
        }
    }
    if (ensureConnection())
        return makeUnexpected(NotificationPoint::Error::ConnectionError);
    g_dbus_connection_call(s_connection,
        name.utf8().data(),
        path.utf8().data(),
        "org.gtk.Actions",
        "Activate",
        g_variant_new("(sava{sv})",
            "notify",
            nullptr,
            nullptr),
        nullptr,
        G_DBUS_CALL_FLAGS_NONE,
        -1,
        nullptr,
        nullptr,
        nullptr);
    return { };
}

bool NotificationPoint::testWTFisEmpty()
{
    if (!pendingNotificationPoints().isEmpty()) {
        g_message("Pending notification points:");
        for (auto [name, map] : pendingNotificationPoints()) {
            for (auto [_, np] : map)
                g_message("%s %s", name.utf8().data(), np->m_path.utf8().data());
        }
        return false;
    }
    if (!ownedNotificationPoints().isEmpty()) {
        g_message("Active notification points:");
        for (auto [name, map] : ownedNotificationPoints()) {
            for (auto [path, _] : map)
                g_message("%s %s", name.utf8().data(), path.utf8().data());
        }
        return false;
    }
    if (!ownerIDs().isEmpty()) {
        g_message("Active owner IDs:");
        for (auto [name, id] : ownerIDs())
            g_message("%s %u", name.utf8().data(), id);
        return false;
    }
    return true;
}

NotificationPoint::NotificationPoint(OwnedNotificationPoint* ownedPoint, Function<void()>&& callback)
    : m_notificationsPosted(true)
    , m_ownedNotificationPoint(ownedPoint)
    , m_callback(WTF::move(callback))
{
}

void NotificationPoint::actionCallback()
{
    m_notificationsPosted = true;
    if (m_callback)
        m_callback();
}
};
