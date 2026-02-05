/*
 * Copyright (C) 2021-2024 Apple Inc. All rights reserved.
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

#if ENABLE(NOTIFICATION_EVENT)

#include "EventTargetInlines.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(NotificationEvent);

Ref<NotificationEvent> NotificationEvent::create(const AtomString& type, Init&& init, IsTrusted isTrusted)
{
    return adoptRef(*new NotificationEvent(type, WTF::move(init), isTrusted));
}

Ref<NotificationEvent> NotificationEvent::create(const AtomString& type, Ref<Notification>&& notification, const String& action, IsTrusted isTrusted)
{
    auto init = Init {
        { false, false, false },
        WTF::move(notification),
        action
    };
    return adoptRef(*new NotificationEvent(type, WTF::move(init), isTrusted));
}

NotificationEvent::NotificationEvent(const AtomString& type, Init&& init, IsTrusted isTrusted)
    : ExtendableEvent(EventInterfaceType::NotificationEvent, type, WTF::move(init), isTrusted)
    , m_notification(WTF::move(init.notification))
    , m_action(WTF::move(init.action))
{
}

NotificationEvent::~NotificationEvent() = default;

} // namespace WebCore

#endif // ENABLE(NOTIFICATION_EVENT)


