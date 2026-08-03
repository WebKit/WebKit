/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Igalia, S.L. All rights reserved.
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
#include "WebExtensionAPIAlarms.h"

#include "WebExtensionAPIKeys.h"

#if ENABLE(WK_WEB_EXTENSIONS)
#include "Logging.h"
#include "MessageSenderInlines.h"
#include "WebExtensionAPINamespace.h"
#include "WebExtensionConstants.h"
#include "WebExtensionContextMessages.h"
#include "WebExtensionContextProxy.h"
#include "WebExtensionUtilities.h"
#include "WebProcess.h"
#include <wtf/DateMath.h>

namespace WebKit {

static constexpr auto nameKey = "name"_s;

static inline JSValueRef toWebAPI(JSContextRef context, const WebExtensionAlarmParameters& alarm)
{
    JSObjectRef result = JSObjectMake(context, 0, 0);

    JSObjectSetProperty(context, result, toJSString(nameKey).get(), toJSValueRef(context, alarm.name), 0, nullptr);
    JSObjectSetProperty(context, result, toJSString(scheduledTimeKey).get(), JSValueMakeNumber(context, floor(alarm.nextScheduledTime.approximate<WallTime>().secondsSinceEpoch().milliseconds())), 0, nullptr);

    if (alarm.repeatInterval)
        JSObjectSetProperty(context, result, toJSString(periodInMinutesKey).get(), JSValueMakeNumber(context, alarm.repeatInterval.minutes()), 0, nullptr);

    return result;
}

void WebExtensionAPIAlarms::createAlarm(const String& name, RefPtr<JSON::Value> alarmInfo, Ref<WebExtensionCallbackHandler>&& callback, String& outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/create

    if (!validateDictionary(alarmInfo, "info"_s, { }, {
        { nameKey, JSON::Value::Type::String },
        { whenKey, JSON::Value::Type::Double },
        { delayInMinutesKey, JSON::Value::Type::Double },
        { periodInMinutesKey, JSON::Value::Type::Double },
    }, outExceptionString))
        return;

    auto alarmObject = alarmInfo->asObject();
    if (!alarmObject)
        return;

    auto infoName = alarmObject->getString(nameKey);

    if (!name.isEmpty() && !infoName.isNull()) {
        callback->reportError(toErrorString("alarms.create()"_s, "info"_s, "it cannot specify 'name' when a name is also passed as an argument"_s));
        return;
    }

    auto whenNumber = alarmObject->getDouble(whenKey);
    auto delayNumber = alarmObject->getDouble(delayInMinutesKey);
    auto periodNumber = alarmObject->getDouble(periodInMinutesKey);

    if (whenNumber && delayNumber) {
        outExceptionString = toErrorString(nullString(), "info"_s, "it cannot specify both 'delayInMinutes' and 'when'"_s);
        return;
    }

    Seconds when = Seconds::fromMilliseconds(whenNumber.value_or(0));
    Seconds delay = Seconds::fromMinutes(delayNumber.value_or(0));
    Seconds period = Seconds::fromMinutes(periodNumber.value_or(0));
    Seconds currentTime = Seconds::fromMilliseconds(jsCurrentTime());

    Seconds initialInterval;
    Seconds repeatInterval;

    if (when)
        initialInterval = when - currentTime;
    else if (delay)
        initialInterval = delay;

    if (period) {
        repeatInterval = period;

        if (!initialInterval)
            initialInterval = repeatInterval;
    }

    if (!extensionContext().inTestingMode()) {
        // Enforce a minimum interval outside of testing.
        initialInterval = std::max(initialInterval, webExtensionMinimumAlarmInterval);
        repeatInterval = repeatInterval ? std::max(repeatInterval, webExtensionMinimumAlarmInterval) : 0_s;
    }

    String alarmName = !name.isEmpty() ? name : infoName;

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsCreate(!alarmName.isEmpty() ? alarmName : emptyAlarmName, initialInterval, repeatInterval), [protectedThis = Ref { *this }, callback = WTF::move(callback)]() {
        callback->call();
    }, extensionContext().identifier());
}

void WebExtensionAPIAlarms::get(const String& name, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/get

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsGet(!name.isEmpty() ? name : emptyAlarmName), [protectedThis = Ref { *this }, callback = WTF::move(callback)](std::optional<WebExtensionAlarmParameters>&& alarm) {
        callback->call(toWebAPI(callback->globalContext(), alarm, UseNullValue::No));
    }, extensionContext().identifier());
}

void WebExtensionAPIAlarms::getAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/getAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsGetAll(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](Vector<WebExtensionAlarmParameters> alarms) {
        callback->call(toWebAPI(callback->globalContext(), alarms));
    }, extensionContext().identifier());
}

void WebExtensionAPIAlarms::clear(const String& name, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/clear

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsClear(!name.isEmpty() ? name : emptyAlarmName), [protectedThis = Ref { *this }, callback = WTF::move(callback)](bool success) {
        callback->call(JSValueMakeBoolean(callback->globalContext(), success));
    }, extensionContext().identifier());
}

void WebExtensionAPIAlarms::clearAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/clearAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsClearAll(), [protectedThis = Ref { *this }, callback = WTF::move(callback)](bool success) {
        callback->call(JSValueMakeBoolean(callback->globalContext(), success));
    }, extensionContext().identifier());
}

WebExtensionAPIEvent& WebExtensionAPIAlarms::onAlarm()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/onAlarm

    if (!m_onAlarm)
        m_onAlarm = WebExtensionAPIEvent::create(*this, WebExtensionEventListenerType::AlarmsOnAlarm);

    return *m_onAlarm;
}

void WebExtensionContextProxy::dispatchAlarmsEvent(const WebExtensionAlarmParameters& alarm)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/onAlarm

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        namespaceObject.alarms().onAlarm().invokeListenersWithParametersArgument(alarm);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
