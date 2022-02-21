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

#include "config.h"
#include "NotificationEvent.h"

#include <wtf/IsoMallocInlines.h>

#if ENABLE(NOTIFICATION_EVENT)

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(NotificationEvent);

Ref<NotificationEvent> NotificationEvent::create(const AtomString& type, Init&& init, IsTrusted isTrusted)
{
    auto* notification = init.notification.get();
    ASSERT(notification);
    auto action = init.action;
    return adoptRef(*new NotificationEvent(type, WTFMove(init), notification, action, isTrusted));
}

Ref<NotificationEvent> NotificationEvent::create(const AtomString& type, Notification* notification, const String& action, IsTrusted isTrusted)
{
    return adoptRef(*new NotificationEvent(type, { }, notification, action, isTrusted));
}

NotificationEvent::NotificationEvent(const AtomString& type, NotificationEventInit&& eventInit, Notification* notification, const String& action, IsTrusted isTrusted)
    : ExtendableEvent(type, WTFMove(eventInit), isTrusted)
    , m_notification(notification)
    , m_action(action)
{
}

NotificationEvent::~NotificationEvent()
{
}

} // namespace WebCore

#endif // ENABLE(NOTIFICATION_EVENT)


