/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CoreIPCDateComponents.h"

namespace WebKit {

std::array<NSCalendarUnit, CoreIPCDateComponents::numberOfComponentIndexes> calendarUnitForComponentIndex {
    NSCalendarUnitEra,
    NSCalendarUnitYear,
    NSCalendarUnitYearForWeekOfYear,
    NSCalendarUnitQuarter,
    NSCalendarUnitMonth,
    NSCalendarUnitHour,
    NSCalendarUnitMinute,
    NSCalendarUnitSecond,
    NSCalendarUnitNanosecond,
    NSCalendarUnitWeekOfYear,
    NSCalendarUnitWeekOfMonth,
    NSCalendarUnitWeekday,
    NSCalendarUnitWeekdayOrdinal,
    NSCalendarUnitDay
};

CoreIPCDateComponents::CoreIPCDateComponents(NSDateComponents *components)
{
    if (components.calendar)
        m_calendarIdentifier = StringImpl::create(components.calendar.calendarIdentifier);
    if (components.timeZone)
        m_timeZoneName = StringImpl::create(components.timeZone.name);

    for (size_t i = 0; i < numberOfComponentIndexes; ++i)
        m_componentValues[i] = [components valueForComponent:calendarUnitForComponentIndex[i]];
}

RetainPtr<id> CoreIPCDateComponents::toID() const
{
    RetainPtr<NSDateComponents> components = adoptNS([NSDateComponents new]);

    for (size_t i = 0; i < numberOfComponentIndexes; ++i)
        [components setValue:m_componentValues[i] forComponent:calendarUnitForComponentIndex[i]];

    if (!isEmpty(m_calendarIdentifier))
        components.get().calendar = [NSCalendar calendarWithIdentifier:createNSString(m_calendarIdentifier).get()];
    if (!isEmpty(m_timeZoneName))
        components.get().timeZone = [NSTimeZone timeZoneWithName:createNSString(m_timeZoneName).get()];

    return components;
}

} // namespace WebKit
