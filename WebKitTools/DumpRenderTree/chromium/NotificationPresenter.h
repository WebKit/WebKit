/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef NotificationPresenter_h
#define NotificationPresenter_h

#include "WebNotification.h"
#include "WebNotificationPresenter.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/WTFString.h>

class TestShell;

// A class that implements WebNotificationPresenter for DRT.
class NotificationPresenter : public WebKit::WebNotificationPresenter {
public:
    explicit NotificationPresenter(TestShell* shell) : m_shell(shell) {}

    // Called by the LayoutTestController to simulate a user granting permission.
    void grantPermission(const WebKit::WebString& origin);

    // Called by the LayoutTestController to simulate a user clicking on a notification.
    bool simulateClick(const WebKit::WebString& notificationIdentifier);

    // WebKit::WebNotificationPresenter interface
    virtual bool show(const WebKit::WebNotification&);
    virtual void cancel(const WebKit::WebNotification&);
    virtual void objectDestroyed(const WebKit::WebNotification&);
    virtual Permission checkPermission(const WebKit::WebURL&);
    virtual void requestPermission(const WebKit::WebSecurityOrigin&, WebKit::WebNotificationPermissionCallback*);

    void reset() { m_allowedOrigins.clear(); }

private:
    // Non-owned pointer. The NotificationPresenter is owned by the test shell.
    TestShell* m_shell;

    // Set of allowed origins.
    HashSet<WTF::String> m_allowedOrigins;

    // Map of active notifications.
    HashMap<WTF::String, WebKit::WebNotification> m_activeNotifications;

    // Map of active replacement IDs to the titles of those notifications
    HashMap<WTF::String, WTF::String> m_replacements;
};

#endif // NotificationPresenter_h
