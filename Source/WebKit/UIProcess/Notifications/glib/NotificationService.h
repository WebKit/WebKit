/*
 * Copyright (C) 2022 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebNotificationIdentifier.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/text/WTFString.h>

typedef struct _GDBusProxy GDBusProxy;
typedef struct _GVariant GVariant;

namespace WebCore {
class NotificationResources;
}

namespace WebKit {

class WebNotification;

class NotificationService {
    WTF_MAKE_NONCOPYABLE(NotificationService);
    WTF_MAKE_TZONE_ALLOCATED(NotificationService);
    friend LazyNeverDestroyed<NotificationService>;
public:
    static NotificationService& singleton();

    bool showNotification(const WebNotification&, const RefPtr<WebCore::NotificationResources>&);
    void cancelNotification(WebNotificationIdentifier);

    class Observer {
    public:
        virtual void didClickNotification(WebNotificationIdentifier) = 0;
        virtual void didCloseNotification(WebNotificationIdentifier) = 0;
    };
    void addObserver(Observer&);
    void removeObserver(Observer&);

private:
    NotificationService();

    struct Notification {
        uint32_t id { 0 };
        String portalID;
        String tag;
        String iconURL;
    };

    enum class Capabilities : uint16_t {
        ActionIcons = 1 << 0,
        Actions = 1 << 1,
        Body = 1 << 2,
        BodyHyperlinks = 1 << 3,
        BodyImages = 1 << 4,
        BodyMarkup = 1 << 5,
        IconMulti = 1 << 6,
        IconStatic = 1 << 7,
        Persistence = 1 << 8,
        Sound =  1 << 9
    };
    void processCapabilities(GVariant*);

    void setNotificationID(WebNotificationIdentifier, uint32_t);
    std::optional<WebNotificationIdentifier> findNotification(uint32_t);
    std::optional<WebNotificationIdentifier> findNotification(const String&);

    static void handleSignal(GDBusProxy*, char*, char*, GVariant*, NotificationService*);
    void didClickNotification(std::optional<WebNotificationIdentifier>);
    void didCloseNotification(std::optional<WebNotificationIdentifier>);

    GRefPtr<GDBusProxy> m_proxy;
    OptionSet<Capabilities> m_capabilities;
    HashMap<WebNotificationIdentifier, Notification> m_notifications;
    HashSet<Observer*> m_observers;
};

} // namespace WebKit
