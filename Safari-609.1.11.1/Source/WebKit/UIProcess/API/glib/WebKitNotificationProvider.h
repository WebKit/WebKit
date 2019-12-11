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

#pragma once

#include "WebKitNotification.h"
#include "WebKitWebContext.h"
#include <wtf/Forward.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
class WebNotificationManagerProxy;
class WebNotification;
class WebPageProxy;

class WebKitNotificationProvider {
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebKitNotificationProvider(WebNotificationManagerProxy*, WebKitWebContext*);
    ~WebKitNotificationProvider();

    void show(WebPageProxy&, const WebNotification&);
    void cancel(const WebNotification&);
    void clearNotifications(const Vector<uint64_t>&);

    HashMap<WTF::String, bool> notificationPermissions();
    void setNotificationPermissions(HashMap<String, bool>&&);

private:
    void cancelNotificationByID(uint64_t);
    static void notificationCloseCallback(WebKitNotification*, WebKitNotificationProvider*);
    static void notificationClickedCallback(WebKitNotification*, WebKitNotificationProvider*);

    void withdrawAnyPreviousNotificationMatchingTag(const CString&);

    WebKitWebContext* m_webContext;
    HashMap<WTF::String, bool> m_notificationPermissions;
    RefPtr<WebNotificationManagerProxy> m_notificationManager;
    HashMap<uint64_t, GRefPtr<WebKitNotification>> m_notifications;
};

} // namespace WebKit
