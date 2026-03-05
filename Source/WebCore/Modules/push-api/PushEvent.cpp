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
#include "PushEvent.h"

#include "PushMessageData.h"
#include <JavaScriptCore/JSArrayBuffer.h>
#include <JavaScriptCore/JSArrayBufferView.h>
#include <JavaScriptCore/JSCInlines.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PushEvent);

static Vector<uint8_t> dataFromPushMessageDataInit(PushMessageDataInit& data)
{
    return WTF::switchOn(data,
        [](Ref<JSC::ArrayBuffer>& value) -> Vector<uint8_t> {
            return value->span();
        },
        [](Ref<JSC::ArrayBufferView>& value) -> Vector<uint8_t> {
            return value->span();
        },
        [](String& value) -> Vector<uint8_t> {
            return value.utf8().span();
        }
    );
}

Ref<PushEvent> PushEvent::create(const AtomString& type, PushEventInit&& initializer, IsTrusted isTrusted)
{
    std::optional<Vector<uint8_t>> data;
    if (initializer.data)
        data = dataFromPushMessageDataInit(*initializer.data);
    return adoptRef(*new PushEvent(type, WTF::move(initializer), WTF::move(data), isTrusted));
}

Ref<PushEvent> PushEvent::create(const AtomString& type, ExtendableEventInit&& initializer, std::optional<Vector<uint8_t>>&& data, IsTrusted isTrusted)
{
    return adoptRef(*new PushEvent(type, WTF::move(initializer), WTF::move(data), isTrusted));
}

static inline RefPtr<PushMessageData> pushMessageDataFromOptionalVector(std::optional<Vector<uint8_t>>&& data)
{
    if (!data)
        return nullptr;
    return PushMessageData::create(WTF::move(*data));
}


PushEvent::~PushEvent() = default;

PushEvent::PushEvent(const AtomString& type, ExtendableEventInit&& eventInit, std::optional<Vector<uint8_t>>&& data, IsTrusted isTrusted)
#if ENABLE(DECLARATIVE_WEB_PUSH) && ENABLE(NOTIFICATIONS)
    : PushEvent(type, WTF::move(eventInit), WTF::move(data), nullptr, std::nullopt, isTrusted)
#else
    : ExtendableEvent(EventInterfaceType::PushEvent, type, WTF::move(eventInit), isTrusted)
    , m_data(pushMessageDataFromOptionalVector(WTF::move(data)))
#endif
{
}

#if ENABLE(DECLARATIVE_WEB_PUSH) && ENABLE(NOTIFICATIONS)

Ref<PushEvent> PushEvent::create(const AtomString& type, ExtendableEventInit&& initializer, Ref<Notification> proposedNotification, std::optional<uint64_t> proposedAppBadge, IsTrusted isTrusted)
{
    return adoptRef(*new PushEvent(type, WTF::move(initializer), std::nullopt, WTF::move(proposedNotification), proposedAppBadge, isTrusted));
}

PushEvent::PushEvent(const AtomString& type, ExtendableEventInit&& eventInit, std::optional<Vector<uint8_t>>&& data, RefPtr<Notification> proposedNotification, std::optional<uint64_t> proposedAppBadge, IsTrusted isTrusted)
    : ExtendableEvent(EventInterfaceType::PushEvent, type, WTF::move(eventInit), isTrusted)
    , m_data(pushMessageDataFromOptionalVector(WTF::move(data)))
    , m_proposedNotification(proposedNotification)
    , m_proposedAppBadge(proposedAppBadge)
{
}

Notification* PushEvent::notification()
{
    if (m_updatedNotification)
        return m_updatedNotification.get();

    return m_proposedNotification.get();
}

std::optional<uint64_t> PushEvent::appBadge()
{
    if (m_updatedAppBadge.has_value())
        return m_updatedAppBadge.value();

    return m_proposedAppBadge;
}

std::optional<NotificationData> PushEvent::updatedNotificationData() const
{
    if (RefPtr updatedNotification = m_updatedNotification)
        return updatedNotification->data();

    return std::nullopt;
}

#endif // ENABLE(DECLARATIVE_WEB_PUSH) && ENABLE(NOTIFICATIONS)

} // namespace WebCore
