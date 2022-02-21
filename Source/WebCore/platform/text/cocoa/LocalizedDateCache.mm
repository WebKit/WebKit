/*
 * Copyright (C) 2011 Apple Inc. All Rights Reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "LocalizedDateCache.h"

#import "DateComponents.h"
#import "FontCascade.h"
#import <CoreFoundation/CFNotificationCenter.h>
#import <math.h>
#import <wtf/Assertions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>

namespace WebCore {

LocalizedDateCache& localizedDateCache()
{
    static NeverDestroyed<LocalizedDateCache> cache;
    return cache;
}

static void _localeChanged(CFNotificationCenterRef, void*, CFStringRef, const void*, CFDictionaryRef)
{
    localizedDateCache().localeChanged();
}

LocalizedDateCache::LocalizedDateCache()
{
    // Listen to CF Notifications for locale change, and clear the cache when it does.
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), (void*)this, _localeChanged, kCFLocaleCurrentLocaleDidChangeNotification, NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
}

LocalizedDateCache::~LocalizedDateCache()
{
    // NOTE: Singleton does not expect to be deconstructed.
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), (void*)this, kCFLocaleCurrentLocaleDidChangeNotification, NULL);
}

void LocalizedDateCache::localeChanged()
{
    m_maxWidthMap.clear();
    m_formatterMap.clear();
}

NSDateFormatter *LocalizedDateCache::formatterForDateType(DateComponentsType type)
{
    int key = static_cast<int>(type);
    if (m_formatterMap.contains(key))
        return m_formatterMap.get(key).get();

    auto dateFormatter = createFormatterForType(type);
    m_formatterMap.set(key, dateFormatter.get());
    return dateFormatter.autorelease();
}

float LocalizedDateCache::maximumWidthForDateType(DateComponentsType type, const FontCascade& font, const MeasureTextClient& measurer)
{
    int key = static_cast<int>(type);
    if (m_font == font) {
        if (m_maxWidthMap.contains(key))
            return m_maxWidthMap.get(key);
    } else {
        m_font = FontCascade(font);
        m_maxWidthMap.clear();
    }

    float calculatedMaximum = calculateMaximumWidth(type, measurer);
    m_maxWidthMap.set(key, calculatedMaximum);
    return calculatedMaximum;
}

RetainPtr<NSDateFormatter> LocalizedDateCache::createFormatterForType(DateComponentsType type)
{
    auto dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    NSLocale *currentLocale = [NSLocale currentLocale];
    [dateFormatter setLocale:currentLocale];

    switch (type) {
    case DateComponentsType::Invalid:
        ASSERT_NOT_REACHED();
        break;
    case DateComponentsType::Date:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setTimeStyle:NSDateFormatterNoStyle];
        [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
        break;
    case DateComponentsType::DateTimeLocal:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
        [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
        break;
    case DateComponentsType::Month:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setDateFormat:[NSDateFormatter dateFormatFromTemplate:@"MMMMyyyy" options:0 locale:currentLocale]];
        break;
    case DateComponentsType::Time:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
        [dateFormatter setDateStyle:NSDateFormatterNoStyle];
        break;
    case DateComponentsType::Week:
        ASSERT_NOT_REACHED();
        break;
    }

    return dateFormatter;
}

// NOTE: This does not check for the widest day of the week.
// We assume no formatter option shows that information.
float LocalizedDateCache::calculateMaximumWidth(DateComponentsType type, const MeasureTextClient& measurer)
{
    float maximumWidth = 0;

    // Get the formatter we would use, copy it because we will force its time zone to be UTC.
    auto dateFormatter = adoptNS([formatterForDateType(type) copy]);
    [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

    // Sample date with a 4 digit year and 2 digit day, hour, and minute. Digits are
    // typically all equally wide. Force UTC timezone for the test date below so the
    // date doesn't adjust for the current timezone. This is an arbitrary date
    // (x-27-2007) and time (10:45 PM).
    RetainPtr<NSCalendar> gregorian = adoptNS([[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian]);
    [gregorian setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
    RetainPtr<NSDateComponents> components = adoptNS([[NSDateComponents alloc] init]);
    [components setDay:27];
    [components setYear:2007];
    [components setHour:22];
    [components setMinute:45];

    static const NSUInteger numberOfGregorianMonths = [[dateFormatter monthSymbols] count];
    ASSERT(numberOfGregorianMonths == 12);

    // For each month (in the Gregorian Calendar), format a date and measure its length.
    NSUInteger totalMonthsToTest = 1;
    if (type == DateComponentsType::Date
        || type == DateComponentsType::DateTimeLocal
        || type == DateComponentsType::Month)
        totalMonthsToTest = numberOfGregorianMonths;
    for (NSUInteger i = 0; i < totalMonthsToTest; ++i) {
        [components setMonth:(i + 1)];
        NSDate *date = [gregorian dateFromComponents:components.get()];
        NSString *formattedDate = [dateFormatter stringFromDate:date];
        maximumWidth = std::max(maximumWidth, measurer.measureText(String(formattedDate)));
    }

    return maximumWidth;
}

} // namespace WebCore

