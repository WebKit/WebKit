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

#import "Font.h"
#import "TextRun.h"
#import <math.h>
#import <wtf/Assertions.h>
#import <wtf/StdLibExtras.h>
#import <CoreFoundation/CFNotificationCenter.h>

// FIXME: Rename this file to LocalizedDataCacheIOS.mm and remove this guard.
#if PLATFORM(IOS)

using namespace std;

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
    : m_font(Font())
{
    // Listen to CF Notifications for locale change, and clear the cache when it does.
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenter(), (void*)this, _localeChanged,
                                    kCFLocaleCurrentLocaleDidChangeNotification,
                                    NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
}

LocalizedDateCache::~LocalizedDateCache()
{
    // NOTE: Singleton does not expect to be deconstructed.
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenter(), (void*)this,
                                       kCFLocaleCurrentLocaleDidChangeNotification, NULL);
}

void LocalizedDateCache::localeChanged()
{
    m_maxWidthMap.clear();
    m_formatterMap.clear();
}

NSDateFormatter *LocalizedDateCache::formatterForDateType(DateComponents::Type type)
{
    int key = static_cast<int>(type);
    if (m_formatterMap.contains(key))
        return m_formatterMap.get(key).get();

    NSDateFormatter *dateFormatter = [createFormatterForType(type) autorelease];
    m_formatterMap.set(key, dateFormatter);
    return dateFormatter;
}

float LocalizedDateCache::maximumWidthForDateType(DateComponents::Type type, const Font& font)
{
    int key = static_cast<int>(type);
    if (m_font == font) {
        if (m_maxWidthMap.contains(key))
            return m_maxWidthMap.get(key);
    } else {
        m_font = Font(font);
        m_maxWidthMap.clear();
    }

    float calculatedMaximum = calculateMaximumWidth(type, font);
    m_maxWidthMap.set(key, calculatedMaximum);
    return calculatedMaximum;
}

NSDateFormatter *LocalizedDateCache::createFormatterForType(DateComponents::Type type)
{
    NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
    NSLocale *currentLocale = [NSLocale currentLocale];
    [dateFormatter setLocale:currentLocale];

    switch (type) {
    case DateComponents::Invalid:
        ASSERT_NOT_REACHED();
        break;
    case DateComponents::Date:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setTimeStyle:NSDateFormatterNoStyle];
        [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
        break;
    case DateComponents::DateTime:
        [dateFormatter setTimeZone:[NSTimeZone localTimeZone]];
        [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
        [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
        break;
    case DateComponents::DateTimeLocal:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
        [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
        break;
    case DateComponents::Month:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setDateFormat:[NSDateFormatter dateFormatFromTemplate:@"MMMMyyyy" options:0 locale:currentLocale]];
        break;
    case DateComponents::Time:
        [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
        [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
        [dateFormatter setDateStyle:NSDateFormatterNoStyle];
        break;
    case DateComponents::Week:
        ASSERT_NOT_REACHED();
        break;
    }

    return dateFormatter;
}

// NOTE: This does not check for the widest day of the week.
// We assume no formatter option shows that information.
float LocalizedDateCache::calculateMaximumWidth(DateComponents::Type type, const Font& font)
{
    float maximumWidth = 0;

    // Get the formatter we would use, copy it because we will force its time zone to be UTC.
    NSDateFormatter *dateFormatter = [[formatterForDateType(type) copy] autorelease];
    [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

    // Sample date with a 4 digit year and 2 digit day, hour, and minute. Digits are
    // typically all equally wide. Force UTC timezone for the test date below so the
    // date doesn't adjust for the current timezone. This is an arbitrary date
    // (x-27-2007) and time (10:45 PM).
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= 80000
    RetainPtr<NSCalendar> gregorian = adoptNS([[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian]);
#else
    RetainPtr<NSCalendar> gregorian = adoptNS([[NSCalendar alloc] initWithCalendarIdentifier:NSGregorianCalendar]);
#endif
    [gregorian.get() setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];
    RetainPtr<NSDateComponents> components = adoptNS([[NSDateComponents alloc] init]);
    [components.get() setDay:27];
    [components.get() setYear:2007];
    [components.get() setHour:22];
    [components.get() setMinute:45];

    static NSUInteger numberOfGregorianMonths = [[dateFormatter monthSymbols] count];
    ASSERT(numberOfGregorianMonths == 12);

    // For each month (in the Gregorian Calendar), format a date and measure its length.
    NSUInteger totalMonthsToTest = 1;
    if (type == DateComponents::Date
        || type == DateComponents::DateTime
        || type == DateComponents::DateTimeLocal
        || type == DateComponents::Month)
        totalMonthsToTest = numberOfGregorianMonths;
    for (NSUInteger i = 0; i < totalMonthsToTest; ++i) {
        [components.get() setMonth:(i + 1)];
        NSDate *date = [gregorian.get() dateFromComponents:components.get()];
        NSString *formattedDate = [dateFormatter stringFromDate:date];
        String str = String(formattedDate);
        maximumWidth = max(maximumWidth, font.width(str));
    }

    return maximumWidth;
}

} // namespace WebCore

#endif // PLATFORM(IOS)
