/*
 *  Copyright (C) 2011 Samsung Electronics
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#ifndef NotificationPresenterClientEfl_h
#define NotificationPresenterClientEfl_h

#if ENABLE(NOTIFICATIONS)
#include "NotificationClient.h"
#include "ewk_security_origin.h"
#include <Evas.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Notification;

class NotificationPresenterClientEfl : public NotificationClient {
public:
    explicit NotificationPresenterClientEfl(Evas_Object* view);
    ~NotificationPresenterClientEfl();

    virtual bool show(Notification*);
    virtual void cancel(Notification*);
    virtual void notificationObjectDestroyed(Notification*);
    virtual void notificationControllerDestroyed();
#if ENABLE(LEGACY_NOTIFICATIONS)
    virtual void requestPermission(ScriptExecutionContext*, PassRefPtr<VoidCallback>) { }
#endif
    virtual void requestPermission(ScriptExecutionContext*, PassRefPtr<NotificationPermissionCallback>);
    virtual NotificationClient::Permission checkPermission(ScriptExecutionContext*);
    virtual void cancelRequestsForPermission(ScriptExecutionContext*) { }

    void addToPermissionCache(const String& domain, bool isPermitted);
    void setPermission(const Ewk_Security_Origin*, bool isPermitted);

private:
    Evas_Object* m_view;

    typedef HashMap<const Ewk_Security_Origin*, RefPtr<WebCore::NotificationPermissionCallback> > PermissionRequestMap;
    PermissionRequestMap m_pendingPermissionRequests;
    typedef HashMap<String, bool> PermissionsMap;
    PermissionsMap m_cachedPermissions;
};

}
#endif
#endif // NotificationPresenterClientEfl_h
