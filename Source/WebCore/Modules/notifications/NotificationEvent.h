/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#if ENABLE(NOTIFICATION_EVENT)

#include "ExtendableEvent.h"
#include "ExtendableEventInit.h"
#include "Notification.h"
#include "NotificationEventType.h"

namespace WebCore {

struct NotificationEventInit : ExtendableEventInit {
    RefPtr<Notification> notification;
    String action;
};

class NotificationEvent final : public ExtendableEvent {
    WTF_MAKE_TZONE_OR_ISO_ALLOCATED(NotificationEvent);
public:
    ~NotificationEvent();

    using Init = NotificationEventInit;

    static Ref<NotificationEvent> create(const AtomString&, Init&&, IsTrusted = IsTrusted::No);
    static Ref<NotificationEvent> create(const AtomString&, Notification*, const String& action, IsTrusted = IsTrusted::No);

    Notification* notification() { return m_notification.get(); }
    const String& action() { return m_action; }

private:
    NotificationEvent(const AtomString&, NotificationEventInit&&, Notification*, const String& action, IsTrusted = IsTrusted::No);

    RefPtr<Notification> m_notification;
    String m_action;
};

} // namespace WebCore

#endif // ENABLE(NOTIFICATION_EVENT)
