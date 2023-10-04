/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#if ENABLE(DECLARATIVE_WEB_PUSH)

#include "ExtendableEvent.h"
#include "Notification.h"
#include "NotificationData.h"
#include "PushNotificationEventInit.h"

namespace WebCore {

class PushNotificationEvent final : public ExtendableEvent {
    WTF_MAKE_ISO_ALLOCATED(PushNotificationEvent);
public:
    static Ref<PushNotificationEvent> create(const AtomString&, PushNotificationEventInit&&, IsTrusted = IsTrusted::No);
    static Ref<PushNotificationEvent> create(const AtomString&, ExtendableEventInit&&, Notification&, std::optional<uint64_t> proposedAppBadge, IsTrusted);

    ~PushNotificationEvent() = default;

    Notification* proposedNotification() const { return m_proposedNotification.get(); }
    std::optional<uint64_t> proposedAppBadge() const { return m_proposedAppBadge; }

    void setUpdatedNotificationData(NotificationData&& updatedData) { m_updatedNotificationData = WTFMove(updatedData); }
    const std::optional<NotificationData>& updatedNotificationData() const { return m_updatedNotificationData; }

    void setUpdatedAppBadge(std::optional<uint64_t>&& updatedAppBadge) { m_updatedAppBadge = WTFMove(updatedAppBadge); }
    const std::optional<std::optional<uint64_t>>& updatedAppBadge() const { return m_updatedAppBadge; }

private:
    PushNotificationEvent(const AtomString&, ExtendableEventInit&&, Notification*, std::optional<uint64_t> proposedAppBadge, IsTrusted);

    EventInterface eventInterface() const final { return PushNotificationEventInterfaceType; }

    RefPtr<Notification> m_proposedNotification;
    std::optional<uint64_t> m_proposedAppBadge;
    std::optional<NotificationData> m_updatedNotificationData;
    std::optional<std::optional<uint64_t>> m_updatedAppBadge;
};

} // namespace WebCore

#endif // ENABLE(DECLARATIVE_WEB_PUSH)
