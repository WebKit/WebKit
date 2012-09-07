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

#include "config.h"
#include "NotificationPresenterClientEfl.h"

#if ENABLE(NOTIFICATIONS)
#include "NotImplemented.h"
#include "Notification.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "ewk_security_origin_private.h"
#include "ewk_view_private.h"

namespace WebCore {

NotificationPresenterClientEfl::NotificationPresenterClientEfl(Evas_Object* view)
    : m_view(view)
{
    ASSERT(m_view);
}

NotificationPresenterClientEfl::~NotificationPresenterClientEfl()
{
}

bool NotificationPresenterClientEfl::show(Notification* notification)
{
    notImplemented();
    return false;
}

void NotificationPresenterClientEfl::cancel(Notification* notification)
{
    notImplemented();
}

void NotificationPresenterClientEfl::notificationObjectDestroyed(Notification* notification)
{
    notImplemented();
}

void NotificationPresenterClientEfl::notificationControllerDestroyed()
{
    notImplemented();
}

void NotificationPresenterClientEfl::requestPermission(ScriptExecutionContext* context, PassRefPtr<NotificationPermissionCallback> callback)
{
    Ewk_Security_Origin* origin = ewk_security_origin_new(context->securityOrigin());
    m_pendingPermissionRequests.add(origin, callback);
    ewk_view_notification_permission_request(m_view, origin);
}

NotificationClient::Permission NotificationPresenterClientEfl::checkPermission(ScriptExecutionContext* context)
{
    PermissionsMap::iterator it = m_cachedPermissions.find(context->securityOrigin()->toString());
    if (it == m_cachedPermissions.end())
        return PermissionNotAllowed;
    if (it->second)
        return PermissionAllowed;

    return PermissionDenied;
}

void NotificationPresenterClientEfl::addToPermissionCache(const String& domain, const bool isPermitted)
{
    PermissionsMap::iterator it = m_cachedPermissions.find(domain);
    if (it != m_cachedPermissions.end())
        return;
    m_cachedPermissions.add(domain, isPermitted);
}

void NotificationPresenterClientEfl::setPermission(const Ewk_Security_Origin* origin, const bool isPermitted)
{
    PermissionRequestMap::iterator it = m_pendingPermissionRequests.find(origin);
    if (it == m_pendingPermissionRequests.end())
        return;

    it->second->handleEvent(Notification::permissionString(isPermitted ? NotificationClient::PermissionAllowed : NotificationClient::PermissionDenied));
    m_pendingPermissionRequests.remove(it);
    m_cachedPermissions.add(String::fromUTF8(ewk_security_origin_string_get(origin)), isPermitted);
    ewk_security_origin_free(const_cast<Ewk_Security_Origin*>(origin));
}

}
#endif
