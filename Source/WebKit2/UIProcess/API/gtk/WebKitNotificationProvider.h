/*
 * Copyright (C) 2013 Igalia S.L.
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

#ifndef WebKitNotificationProvider_h
#define WebKitNotificationProvider_h

#include "WebKitPrivate.h"
#include "WebKitNotification.h"
#include <wtf/HashMap.h>
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>

namespace API {
class Array;
}

namespace WebKit {

class WebKitNotificationProvider : public RefCounted<WebKitNotificationProvider> {
public:
    virtual ~WebKitNotificationProvider();
    static Ref<WebKitNotificationProvider> create(WebNotificationManagerProxy*);

    void show(WebPageProxy*, const WebNotification&);
    void cancel(const WebNotification&);
    void clearNotifications(const API::Array*);

private:
    WebKitNotificationProvider(WebNotificationManagerProxy*);

    void cancelNotificationByID(uint64_t);
    static void notificationCloseCallback(WebKitNotification*, WebKitNotificationProvider*);
    static void notificationClickedCallback(WebKitNotification*, WebKitNotificationProvider*);

    RefPtr<WebNotificationManagerProxy> m_notificationManager;
    HashMap<uint64_t, GRefPtr<WebKitNotification>> m_notifications;
};

} // namespace WebKit

#endif
