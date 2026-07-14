/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionContext.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "WebExtensionAlarm.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionPermission.h"

namespace WebKit {

bool WebExtensionContext::isAlarmsMessageAllowed(IPC::Decoder& message)
{
    return isLoadedAndPrivilegedMessage(message) && hasPermission(WebExtensionPermission::alarms());
}

void WebExtensionContext::alarmsCreate(const String& name, Seconds initialInterval, Seconds repeatInterval)
{
    m_alarmMap.set(name, WebExtensionAlarm::create(name, initialInterval, repeatInterval, [this, protectedThis = Ref { *this }](const WebExtensionAlarm& alarm) {
        fireAlarmsEventIfNeeded(alarm);
    }));
}

CompletionHandlerCalledToken WebExtensionContext::alarmsGet(const String& name, CompletionHandler<void(std::optional<WebExtensionAlarmParameters>&&), true>&& completionHandler)
{
    if (RefPtr alarm = m_alarmMap.get(name))
        return completionHandler(alarm->parameters());
    else
        return completionHandler(std::nullopt);
}

CompletionHandlerCalledToken WebExtensionContext::alarmsClear(const String& name, CompletionHandler<void(), true>&& completionHandler)
{
    m_alarmMap.remove(name);

    return completionHandler();
}

CompletionHandlerCalledToken WebExtensionContext::alarmsGetAll(CompletionHandler<void(Vector<WebExtensionAlarmParameters>&&), true>&& completionHandler)
{
    auto alarms = WTF::map(m_alarmMap.values(), [](auto&& alarm) {
        return alarm->parameters();
    });

    return completionHandler(WTF::move(alarms));
}

CompletionHandlerCalledToken WebExtensionContext::alarmsClearAll(CompletionHandler<void(), true>&& completionHandler)
{
    m_alarmMap.clear();

    return completionHandler();
}

void WebExtensionContext::fireAlarmsEventIfNeeded(const WebExtensionAlarm& alarm)
{
    constexpr auto type = WebExtensionEventListenerType::AlarmsOnAlarm;
    wakeUpBackgroundContentIfNecessaryToFireEvents({ type }, Function<void()>([=, this, protectedThis = Ref { *this }, alarm = Ref { alarm }] {
        sendToProcessesForEvent(type, Messages::WebExtensionContextProxy::DispatchAlarmsEvent(alarm->parameters()));
    }));
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
