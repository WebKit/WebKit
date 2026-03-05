/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "APIDictionary.h"
#include "APISecurityOrigin.h"
#include "Connection.h"
#include "WebNotificationIdentifier.h"
#include "WebPageProxyIdentifier.h"
#include <WebCore/NotificationData.h>
#include <wtf/Identified.h>
#include <wtf/ObjectIdentifier.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {
enum class NotificationDirection : uint8_t;
struct NotificationData;
}

namespace WebKit {

class WebNotification : public API::ObjectImpl<API::Object::Type::Notification>, public Identified<WebNotificationIdentifier> {
public:
    static Ref<WebNotification> createNonPersistent(const WebCore::NotificationData& data, std::optional<WebPageProxyIdentifier> pageIdentifier, IPC::Connection& sourceConnection)
    {
        ASSERT(!data.isPersistent());
        return adoptRef(*new WebNotification(data, pageIdentifier, std::nullopt, &sourceConnection));
    }

    static Ref<WebNotification> createPersistent(const WebCore::NotificationData& data, const std::optional<WTF::UUID>& dataStoreIdentifier, IPC::Connection* sourceConnection)
    {
        ASSERT(data.isPersistent());
        return adoptRef(*new WebNotification(data, std::nullopt, dataStoreIdentifier, sourceConnection));
    }

    const String& title() const LIFETIME_BOUND { return m_data.title; }
    const String& body() const LIFETIME_BOUND { return m_data.body; }
    const String& iconURL() const LIFETIME_BOUND { return m_data.iconURL; }
    const String& tag() const LIFETIME_BOUND { return m_data.tag; }
    const String& lang() const LIFETIME_BOUND { return m_data.language; }
    WebCore::NotificationDirection dir() const { return m_data.direction; }
    const WTF::UUID& coreNotificationID() const LIFETIME_BOUND { return m_data.notificationID; }
    const std::optional<WTF::UUID>& dataStoreIdentifier() const { return m_dataStoreIdentifier; }
    PAL::SessionID sessionID() const { return m_data.sourceSession; }

    const WebCore::NotificationData& data() const LIFETIME_BOUND { return m_data; }
    bool isPersistentNotification() const { return !m_data.serviceWorkerRegistrationURL.isEmpty(); }

    API::SecurityOrigin& origin() const { return m_origin; }

    std::optional<WebPageProxyIdentifier> pageIdentifier() const { return m_pageIdentifier; }
    RefPtr<IPC::Connection> sourceConnection() const { return m_sourceConnection.get(); }

private:
    WebNotification(const WebCore::NotificationData&, std::optional<WebPageProxyIdentifier>, const std::optional<WTF::UUID>& dataStoreIdentifier, IPC::Connection*);

    WebCore::NotificationData m_data;
    const Ref<API::SecurityOrigin> m_origin;
    Markable<WebPageProxyIdentifier> m_pageIdentifier;
    std::optional<WTF::UUID> m_dataStoreIdentifier;
    ThreadSafeWeakPtr<IPC::Connection> m_sourceConnection;
};

inline bool isNotificationIDValid(uint64_t id)
{
    // This check makes sure that the ID is not equal to values needed by
    // HashMap for bucketing.
    return id && id != static_cast<uint64_t>(-1);
}

} // namespace WebKit

SPECIALIZE_TYPE_TRAITS_BEGIN(WebKit::WebNotification)
static bool isType(const API::Object& object) { return object.type() == API::Object::Type::Notification; }
SPECIALIZE_TYPE_TRAITS_END()
