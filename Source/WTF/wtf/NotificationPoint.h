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

#pragma once

#include <cstdint>
#include <wtf/CanMakeWeakPtr.h>
#include <wtf/Expected.h>
#include <wtf/Function.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Platform.h>
#include <wtf/RefPtr.h>
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/WTFString.h>

#if OS(DARWIN)
#include <dispatch/dispatch.h>
#include <notify.h>
#elif USE(GLIB)
#include <gio/gio.h>
#include <glib.h>
#endif

#if PLATFORM(COCOA) || USE(GLIB)
#define HAVE_NOTIFICATIONPOINT 1
#endif

namespace WTF {

class OwnedNotificationPoint;

class NotificationPoint : public ThreadSafeRefCounted<NotificationPoint>
#if USE(GLIB)
, public CanMakeWeakPtr<NotificationPoint>
#endif
{
public:
    enum class Error : int {
        ConnectionError,
        InvalidArgument,
        PermissionError,
        Other,
    };
    WTF_EXPORT_PRIVATE static Expected<RefPtr<NotificationPoint>, Error> create(ASCIILiteral providedPath, Function<void()>&& callback);
    WTF_EXPORT_PRIVATE static Expected<RefPtr<NotificationPoint>, Error> createWithName(ASCIILiteral name, ASCIILiteral providedPath, Function<void()>&& callback = nullptr);
    WTF_EXPORT_PRIVATE Expected<bool, Error> getAndClearNotificationsPosted();
    bool isLive() const;
    WTF_EXPORT_PRIVATE ~NotificationPoint();
    WTF_EXPORT_PRIVATE Expected<uint64_t, Error> getState();
    WTF_EXPORT_PRIVATE static Expected<uint64_t, Error> getState(ASCIILiteral providedName, String providedPath);
    WTF_EXPORT_PRIVATE Expected<void, Error> setState(uint64_t);
    WTF_EXPORT_PRIVATE static Expected<void, Error> setState(ASCIILiteral providedName, String providedPath, uint64_t state);
    WTF_EXPORT_PRIVATE Expected<void, Error> notify();
    WTF_EXPORT_PRIVATE static Expected<void, Error> notify(ASCIILiteral providedName, String providedPath);
    static bool testWTFisEmpty();

#if OS(DARWIN)
    // Unfortunately, moving this def to the .cpp file causes `tap installapi`
    // (in the XCode build) to fail, since it apparently uses an .mm file to
    // import things, but dispatch_queue_t expands to NSObject<> there but a
    // struct on the C++ side, so the decl doesn't match the definition..
    static void testWTFsetDispatchQueue(dispatch_queue_t queue)
    {
        s_dispatchQueue = queue;
    }
    const String& key() const;
#elif USE(GLIB)
    void actionCallback();
#endif // OS(DARWIN)

private:
#if OS(DARWIN)
    static void dispatchCallback(const String&);
    NotificationPoint(String key, int token);
    int m_token;
    const String m_key;
    static std::optional<dispatch_queue_t> s_dispatchQueue;
#elif USE(GLIB)
    NotificationPoint(OwnedNotificationPoint*, Function<void()>&&);
    bool m_notificationsPosted;
    OwnedNotificationPoint* m_ownedNotificationPoint;
    Function<void()> m_callback;
#endif // OS(DARWIN)
};

#if USE(GLIB)
class OwnedNotificationPoint {
    friend class NotificationPoint;
public:
    OwnedNotificationPoint(String name, String path) WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    ~OwnedNotificationPoint() WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    void addNotificationPoint(WeakPtr<NotificationPoint>) WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    void removeNotificationPoint(WeakPtr<NotificationPoint>) WTF_REQUIRES_LOCK(ownedNotificationPointLock);
private:
    static Lock ownedNotificationPointLock;
    bool isLive() const WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    uint64_t getState() WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    void setState(uint64_t) WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    void notify() WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    static OwnedNotificationPoint* ownedNotificationPoint(String name, String path) WTF_REQUIRES_LOCK(OwnedNotificationPoint::ownedNotificationPointLock);
    static void addOwnedNotificationPoint(HashMap<String, HashMap<String, OwnedNotificationPoint*>>&, OwnedNotificationPoint*) WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    static void actionCallback(std::pair<String, String>*);
    static void processPendingForName(String) WTF_REQUIRES_LOCK(ownedNotificationPointLock);
    static void onBusNameAcquired(GDBusConnection*, const char*, void*);

    const String m_name;
    const String m_path;
    guint m_exportID WTF_GUARDED_BY_LOCK(ownedNotificationPointLock);
    GSimpleActionGroup* m_actionGroup WTF_GUARDED_BY_LOCK(ownedNotificationPointLock);
    GSimpleAction* m_action WTF_GUARDED_BY_LOCK(ownedNotificationPointLock);
    Vector<WeakPtr<NotificationPoint>> m_listeners WTF_GUARDED_BY_LOCK(ownedNotificationPointLock);
};
#endif

} // namespace WTF

using WTF::NotificationPoint;
