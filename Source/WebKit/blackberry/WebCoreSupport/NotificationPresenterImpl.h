/*
 * Copyright (C) 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef NotificationPresenterImpl_h
#define NotificationPresenterImpl_h

#if ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#include <NotificationAckListener.h>
#include <NotificationClient.h>
#include <NotificationPresenterBlackBerry.h>
#include <string>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OwnPtr.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class NotificationPresenterImpl : public WebCore::NotificationClient, public BlackBerry::Platform::NotificationAckListener {
public:
    static NotificationClient* instance();
    virtual ~NotificationPresenterImpl();

    // Requests that a notification be shown.
    virtual bool show(WebCore::Notification*);

    // Requests that a notification that has already been shown be canceled.
    virtual void cancel(WebCore::Notification*);

    // Informs the presenter that a WebCore::Notification object has been destroyed
    // (such as by a page transition). The presenter may continue showing
    // the notification, but must not attempt to call the event handlers.
    virtual void notificationObjectDestroyed(WebCore::Notification*);

    virtual void notificationControllerDestroyed();

    // Requests user permission to show desktop notifications from a particular
    // script context. The callback parameter should be run when the user has
    // made a decision.
    virtual void requestPermission(WebCore::ScriptExecutionContext*, PassRefPtr<WebCore::VoidCallback>);

    // Cancel all outstanding requests for the ScriptExecutionContext.
    virtual void cancelRequestsForPermission(WebCore::ScriptExecutionContext*);

    // Checks the current level of permission.
    virtual Permission checkPermission(WebCore::ScriptExecutionContext*);

    // Interfaces inherited from NotificationAckListener.
    virtual void notificationClicked(const std::string& id);
    virtual void onPermission(const std::string& domain, bool isAllowed);

private:
    NotificationPresenterImpl();

private:
    OwnPtr<BlackBerry::Platform::NotificationPresenterBlackBerry> m_platformPresenter;

    typedef HashMap<RefPtr<WebCore::Notification>, String> NotificationMap;
    NotificationMap m_notifications;

    typedef HashMap<RefPtr<WebCore::ScriptExecutionContext>, RefPtr<WebCore::VoidCallback> > PermissionRequestMap;
    PermissionRequestMap m_permissionRequests;

    HashSet<String> m_allowedDomains;
};

} // namespace WebKit

#endif // ENABLE(NOTIFICATIONS) || ENABLE(LEGACY_NOTIFICATIONS)
#endif // NotificationPresenterImpl_h
