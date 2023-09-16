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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionAPIAlarms.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "CocoaHelpers.h"
#import "Logging.h"
#import "MessageSenderInlines.h"
#import "WebExtensionAPINamespace.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextProxy.h"
#import "WebExtensionUtilities.h"
#import "WebProcess.h"
#import <wtf/DateMath.h>

namespace WebKit {

static NSString * const whenKey = @"when";
static NSString * const delayInMinutesKey = @"delayInMinutes";
static NSString * const periodInMinutesKey = @"periodInMinutes";

static NSString * const nameKey = @"name";
static NSString * const scheduledTimeKey = @"scheduledTime";

static NSString * const emptyAlarmName = @"";

static inline NSDictionary *toAPI(const WebExtensionAlarmParameters& alarm)
{
    NSMutableDictionary *result = [NSMutableDictionary dictionaryWithCapacity:3];

    result[nameKey] = static_cast<NSString *>(alarm.name);
    result[scheduledTimeKey] = @(floor(alarm.nextScheduledTime.approximateWallTime().secondsSinceEpoch().milliseconds()));

    if (alarm.repeatInterval)
        result[periodInMinutesKey] = @(alarm.repeatInterval.minutes());

    return [result copy];
}

static inline NSDictionary *toAPI(const std::optional<WebExtensionAlarmParameters>& alarm)
{
    return alarm ? toAPI(alarm.value()) : nil;
}

static inline NSArray *toAPI(const Vector<WebExtensionAlarmParameters>& alarms)
{
    NSMutableArray *result = [NSMutableArray arrayWithCapacity:alarms.size()];

    for (auto& alarm : alarms)
        [result addObject:toAPI(alarm)];

    return [result copy];
}

void WebExtensionAPIAlarms::createAlarm(NSString *name, NSDictionary *alarmInfo, NSString **outExceptionString)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/create

    static NSDictionary<NSString *, id> *types = @{
        whenKey: NSNumber.class,
        delayInMinutesKey: NSNumber.class,
        periodInMinutesKey: NSNumber.class,
    };

    if (!validateDictionary(alarmInfo, @"info", nil, types, outExceptionString))
        return;

    auto *whenNumber = objectForKey<NSNumber>(alarmInfo, whenKey);
    auto *delayNumber = objectForKey<NSNumber>(alarmInfo, delayInMinutesKey);
    auto *periodNumber = objectForKey<NSNumber>(alarmInfo, periodInMinutesKey);

    if (whenNumber && delayNumber) {
        *outExceptionString = toErrorString(nil, @"info", @"it cannot specify both 'delayInMinutes' and 'when'");
        return;
    }

    Seconds when = Seconds::fromMilliseconds(whenNumber.doubleValue);
    Seconds delay = Seconds::fromMinutes(delayNumber.doubleValue);
    Seconds period = Seconds::fromMinutes(periodNumber.doubleValue);
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
        // Enforce a minimum of 1 minute intervals outside of testing.
        initialInterval = std::max(initialInterval, 1_min);
        repeatInterval = repeatInterval ? std::max(repeatInterval, 1_min) : 0_s;
    }

    WebProcess::singleton().send(Messages::WebExtensionContext::AlarmsCreate(name ?: emptyAlarmName, initialInterval, repeatInterval), extensionContext().identifier());
}

void WebExtensionAPIAlarms::get(NSString *name, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/get

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsGet(name ?: emptyAlarmName), [protectedThis = Ref { *this }, callback = WTFMove(callback)](std::optional<WebExtensionAlarmParameters> alarm) {
        callback->call(toAPI(alarm));
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAlarms::getAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/getAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsGetAll(), [protectedThis = Ref { *this }, callback = WTFMove(callback)](Vector<WebExtensionAlarmParameters> alarms) {
        callback->call(toAPI(alarms));
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAlarms::clear(NSString *name, Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/clear

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsClear(name ?: emptyAlarmName), [protectedThis = Ref { *this }, callback = WTFMove(callback)]() {
        callback->call();
    }, extensionContext().identifier().toUInt64());
}

void WebExtensionAPIAlarms::clearAll(Ref<WebExtensionCallbackHandler>&& callback)
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/clearAll

    WebProcess::singleton().sendWithAsyncReply(Messages::WebExtensionContext::AlarmsClearAll(), [protectedThis = Ref { *this }, callback = WTFMove(callback)]() {
        callback->call();
    }, extensionContext().identifier().toUInt64());
}

WebExtensionAPIEvent& WebExtensionAPIAlarms::onAlarm()
{
    // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/onAlarm

    if (!m_onAlarm)
        m_onAlarm = WebExtensionAPIEvent::create(forMainWorld(), runtime(), extensionContext(), WebExtensionEventListenerType::AlarmsOnAlarm);

    return *m_onAlarm;
}

void WebExtensionContextProxy::dispatchAlarmsEvent(const WebExtensionAlarmParameters& alarm)
{
    auto *details = toAPI(alarm);

    enumerateNamespaceObjects([&](auto& namespaceObject) {
        // Documentation: https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/alarms/onAlarm
        namespaceObject.alarms().onAlarm().invokeListenersWithArgument(details);
    });
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
