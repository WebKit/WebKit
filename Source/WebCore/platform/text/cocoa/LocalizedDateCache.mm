/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#import <WebCore/LocalizedStrings.h>
#import <math.h>
#import <ranges>
#import <wtf/Assertions.h>
#import <wtf/IndexedRange.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>
#import <wtf/cf/NotificationCenterCF.h>
#import <wtf/text/MakeString.h>
#import <wtf/text/StringConcatenateNumbers.h>

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
    CFNotificationCenterAddObserver(CFNotificationCenterGetLocalCenterSingleton(), (void*)this, _localeChanged, kCFLocaleCurrentLocaleDidChangeNotification, NULL, CFNotificationSuspensionBehaviorDeliverImmediately);
}

LocalizedDateCache::~LocalizedDateCache()
{
    // NOTE: Singleton does not expect to be deconstructed.
    CFNotificationCenterRemoveObserver(CFNotificationCenterGetLocalCenterSingleton(), (void*)this, kCFLocaleCurrentLocaleDidChangeNotification, NULL);
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
        return m_formatterMap.get(key);

    auto dateFormatter = createFormatterForType(type);
    m_formatterMap.set(key, dateFormatter.get());
    return dateFormatter.autorelease();
}

float LocalizedDateCache::estimatedMaximumWidthForDateType(DateComponentsType type, const FontCascade& font, const MeasureTextClient& measurer)
{
    int key = static_cast<int>(type);
    if (m_font == font) {
        if (m_maxWidthMap.contains(key))
            return m_maxWidthMap.get(key);
    } else {
        m_font = FontCascade(font);
        m_maxWidthMap.clear();
    }

    float estimatedMaximum = estimateMaximumWidth(type, measurer);
    m_maxWidthMap.set(key, estimatedMaximum);
    return estimatedMaximum;
}

RetainPtr<NSDateFormatter> LocalizedDateCache::createFormatterForType(DateComponentsType type)
{
    auto dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    NSLocale *currentLocale = [NSLocale currentLocale];
    [dateFormatter setLocale:currentLocale];
    [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

    switch (type) {
    case DateComponentsType::Invalid:
        ASSERT_NOT_REACHED();
        break;
    case DateComponentsType::Date:
        [dateFormatter setTimeStyle:NSDateFormatterNoStyle];
        [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
        break;
    case DateComponentsType::DateTimeLocal:
        [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
        [dateFormatter setDateStyle:NSDateFormatterMediumStyle];
        break;
    case DateComponentsType::Month:
        [dateFormatter setDateFormat:[NSDateFormatter dateFormatFromTemplate:@"MMMMyyyy" options:0 locale:currentLocale]];
        break;
    case DateComponentsType::Time:
        [dateFormatter setTimeStyle:NSDateFormatterShortStyle];
        [dateFormatter setDateStyle:NSDateFormatterNoStyle];
        break;
    case DateComponentsType::Week:
        ASSERT_NOT_REACHED();
        break;
    }

    return dateFormatter;
}

#if ENABLE(INPUT_TYPE_WEEK_PICKER)

static float estimateMaximumWidthForWeek(const MeasureTextClient& measurer)
{
    RetainPtr allDigitNumber = adoptNS([[NSNumber alloc] initWithUnsignedLongLong:9876543210]);
    RetainPtr localizedDigits = [NSNumberFormatter localizedStringFromNumber:allDigitNumber.get() numberStyle:NSNumberFormatterNoStyle];

    std::array<float, 10> numeralLengths;
    for (auto [i, numLength] : indexedRange(numeralLengths)) {
        numLength = measurer.measureText(String([localizedDigits substringWithRange:NSMakeRange(9 - i, 1)]));
        ASSERT(numeralLengths[i] == numLength);
    }

    std::span numeralLengthsSpan { numeralLengths };
    unsigned widestNum = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan));
    unsigned widestNumOneThroughFour = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(1, 4)));
    unsigned widestNumNonZero = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(1)));

    RetainPtr<NSString> weekString;
    // W50 is an edge case here; without this check, a suboptimal choice would be made when 5 and 0 are both large and all other numbers are narrow.
    if (numeralLengths[5] + numeralLengths[0] > numeralLengths[widestNumOneThroughFour] + numeralLengths[widestNum])
        weekString = [NSString stringWithFormat:@"%d%d%d%d-W50", widestNumNonZero, widestNum, widestNum, widestNum];
    else
        weekString = [NSString stringWithFormat:@"%d%d%d%d-W%d%d", widestNumNonZero, widestNum, widestNum, widestNum, widestNumOneThroughFour, widestNum];

    if (auto components = DateComponents::fromParsingWeek((String)weekString.get()))
        return measurer.measureText(inputWeekLabel(components.value()));

    ASSERT_NOT_REACHED();
    return 0;
}

#endif

// NOTE: This does not check for the widest day of the week.
// We assume no formatter option shows that information.
float LocalizedDateCache::estimateMaximumWidth(DateComponentsType type, const MeasureTextClient& measurer)
{
    // NOTE: The purpose of this method is to quickly estimate the width of the widest possible string
    // that a date/time control can display. It is only an estimate because it does not consider font
    // characteristics such as kerning, and instead assumes that the order in which characters are displayed
    // does not affect the overall width. All comments should be interpreted within this context.

#if ENABLE(INPUT_TYPE_WEEK_PICKER)
    if (type == DateComponentsType::Week)
        return estimateMaximumWidthForWeek(measurer);
#endif

    auto shouldCalculateWidthForTime = (type == DateComponentsType::Time || type == DateComponentsType::DateTimeLocal);
    auto shouldCalculateWidthForMonthAndYear = (type != DateComponentsType::Time && type != DateComponentsType::Week);
    auto shouldCalculateWidthForDayOfMonth = (type == DateComponentsType::Date || type == DateComponentsType::DateTimeLocal);

    // Get the formatter we would use, copy it because we will force its time zone to be UTC.
    RetainPtr dateFormatter = adoptNS([formatterForDateType(type) copy]);
    [dateFormatter setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

    RetainPtr gregorian = adoptNS([[NSCalendar alloc] initWithCalendarIdentifier:NSCalendarIdentifierGregorian]);
    [gregorian setTimeZone:[NSTimeZone timeZoneForSecondsFromGMT:0]];

    // Create our initial date components using arbitrary values. These will be updated as needed later on.
    RetainPtr components = adoptNS([[NSDateComponents alloc] init]);
    [components setMinute:55];
    [components setHour:14];
    [components setDay:19];
    [components setMonth:3];
    [components setYear:2025];

    // Retrieve localized numerals to measure.
    RetainPtr allDigitNumber = adoptNS([[NSNumber alloc] initWithUnsignedLongLong:9876543210]);
    RetainPtr localizedDigits = [NSNumberFormatter localizedStringFromNumber:allDigitNumber.get() numberStyle:NSNumberFormatterNoStyle];

    if ([localizedDigits length] != 10) {
        ASSERT_NOT_REACHED();
        return 0.f;
    }

    // Store the width of each numeral to use for comparison later.
    std::array<float, 10> numeralLengths;
    std::span numeralLengthsSpan { numeralLengths };
    for (auto [i, numLength] : indexedRange(numeralLengths)) {
        numLength = measurer.measureText(String([localizedDigits substringWithRange:NSMakeRange(9 - i, 1)]));
        ASSERT(numeralLengths[i] == numLength);
    }

    unsigned widestNum = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan));
    unsigned widestNumNonZero = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(1)));

    // Find the widest 4-digit year. Leading zeros are removed when the full year is displayed, so it can't start with zero.
    if (shouldCalculateWidthForMonthAndYear)
        [components setYear:(widestNumNonZero * 1000 + widestNum * 111)];

    if (shouldCalculateWidthForTime) {
        // Find the widest hour. Our strategy for this will differ depending on how time is displayed for the user's locale.
        unsigned hourCandidate;
        RetainPtr timeFormat = [NSDateFormatter dateFormatFromTemplate:@"j" options:0 locale:[dateFormatter locale]];
        if ([timeFormat containsString:@"a"]) {
            // Using 12 hour time.
            unsigned widestNumZeroThroughTwo = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(0, 2)));
            hourCandidate = 10 + widestNumZeroThroughTwo;

            float hourCandidateLength = numeralLengths[1] + numeralLengths[widestNumZeroThroughTwo];
            if (hourCandidateLength < numeralLengths[widestNumNonZero])
                hourCandidate = widestNumNonZero;

            // If the symbols used for PM are wider than AM, shift the hour forward by 12.
            if (measurer.measureText(String([dateFormatter AMSymbol])) < measurer.measureText(String([dateFormatter PMSymbol])))
                hourCandidate += 12;
        } else if ([timeFormat containsString:@"HH"]) {
            // Using 24 hour time with leading zero for single-digit hours.
            unsigned widestNumZeroThroughOne = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(0, 1)));
            unsigned widestNumZeroThroughThree = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(0, 3)));
            hourCandidate = widestNumZeroThroughOne * 10 + widestNum;

            float hourCandidateLength = numeralLengths[widestNumZeroThroughOne] + numeralLengths[widestNum];
            if (hourCandidateLength < numeralLengths[2] + numeralLengths[widestNumZeroThroughThree])
                hourCandidate = 20 + widestNumZeroThroughThree;
        } else {
            // Using 24 hour time with no leading zero for single-digit hours.
            unsigned widestNumZeroThroughThree = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(0, 3)));
            hourCandidate = 10 + widestNum;

            float hourCandidateLength = numeralLengths[1] + numeralLengths[widestNum];
            if (hourCandidateLength < numeralLengths[2] + numeralLengths[widestNumZeroThroughThree])
                hourCandidate = 20 + widestNumZeroThroughThree;
        }

        [components setHour:hourCandidate];

        // Find the widest minute.
        unsigned widestNumZeroThroughFive = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(0, 5)));
        [components setMinute:(10 * widestNumZeroThroughFive + widestNum)];
    }

    if (shouldCalculateWidthForMonthAndYear) {
        // Find the widest month.
        unsigned widestMonth = 1;
        unsigned secondWidestMonth = 2;
        float widestDateWidthForMonthCalculation = 0;
        float secondWidestDateWidthForMonthCalculation = 0;

        // NSDateComponents start at 1 for January.
        for (NSUInteger i = 1; i <= 12; ++i) {
            [components setMonth:i];
            RetainPtr tempDate = [gregorian dateFromComponents:components.get()];
            if (!tempDate) {
                ASSERT_NOT_REACHED();
                return 0.f;
            }

            RetainPtr tempFormattedDate = [dateFormatter stringFromDate:tempDate.get()];
            float currentLength = measurer.measureText(String(tempFormattedDate.get()));

            if (currentLength == widestDateWidthForMonthCalculation) {
                // Month i has the same width as the current widest month. We only need to
                // update the widest month if it's currently February, because this would allow
                // us to avoid checking an edge case later if we need to find the widest day of
                // month. The second-widest month will be updated regardless.

                if (widestMonth == 2) {
                    secondWidestMonth = widestMonth;
                    widestMonth = i;
                    secondWidestDateWidthForMonthCalculation = widestDateWidthForMonthCalculation;
                    widestDateWidthForMonthCalculation = currentLength;
                } else {
                    secondWidestMonth = i;
                    secondWidestDateWidthForMonthCalculation = currentLength;
                }
            } else if (currentLength > widestDateWidthForMonthCalculation) {
                // Month i is wider than our current widest month. Update accordingly.
                secondWidestMonth = widestMonth;
                widestMonth = i;
                secondWidestDateWidthForMonthCalculation = widestDateWidthForMonthCalculation;
                widestDateWidthForMonthCalculation = currentLength;
            }
        }

        [components setMonth:widestMonth];

        if (shouldCalculateWidthForDayOfMonth) {
            // Get the widest day of month. If the widest month has 31 or 30 days, then the widest date is guaranteed to use
            // the widest month. Even if the month only has 30 days and 3 and 1 are the two widest digits, we would still
            // get the widest day by using 13.
            unsigned widestNumOneThroughTwo = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(1, 2)));
            unsigned dayOfMonthCandidate;

            if (widestMonth == 2) {
                // The widest month is February. Find the widest day from 1-28, and check an edge case afterward. Note
                // that we never need to consider leap years; if the widest day of the month is 29, then we know the widest
                // year must be 9999, which is not a leap year.
                unsigned widestNumZeroThroughEight = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(0, 8)));
                dayOfMonthCandidate = widestNumOneThroughTwo * 10 + widestNumZeroThroughEight;
                float dayOfMonthCandidateLength = numeralLengths[widestNumOneThroughTwo] + numeralLengths[widestNumZeroThroughEight];

                // Days 29 are 30 are edge cases. If either are wider than the widest day of month in February, we
                // need to consider that the difference in day width might be larger than the difference in month width.
                unsigned widestNumOneThroughThree = std::distance(numeralLengthsSpan.begin(), std::ranges::max_element(numeralLengthsSpan.subspan(1, 3)));
                unsigned widerDayOfMonth = 0;

                if (widestNumOneThroughTwo == 2 && widestNum == 9)
                    widerDayOfMonth = 29;
                else if (widestNumOneThroughThree == 3 && widestNum == 0)
                    widerDayOfMonth = 30;

                if (widerDayOfMonth) {
                    float differenceForMonthWidths = widestDateWidthForMonthCalculation - secondWidestDateWidthForMonthCalculation;
                    float widestDayOfMonthWidth = numeralLengths[widerDayOfMonth / 10] + numeralLengths[widerDayOfMonth % 10];
                    float differenceForDayOfMonthWidths = widestDayOfMonthWidth - dayOfMonthCandidateLength;

                    if (differenceForMonthWidths < differenceForDayOfMonthWidths) {
                        // It is suboptimal to use February. Switch to the second-widest month
                        // and the true widest day of month.
                        [components setMonth:secondWidestMonth];
                        dayOfMonthCandidate = widerDayOfMonth;
                    }
                }

            } else {
                dayOfMonthCandidate = widestNumOneThroughTwo * 10 + widestNum;
                float dayOfMonthCandidateLength = numeralLengths[widestNumOneThroughTwo] + numeralLengths[widestNum];
                if (dayOfMonthCandidateLength < numeralLengths[3] + numeralLengths[0])
                    dayOfMonthCandidate = 30;
            }

            [components setDay:dayOfMonthCandidate];
        }
    }

    if (RetainPtr date = [gregorian dateFromComponents:components.get()])
        return measurer.measureText(String([dateFormatter stringFromDate:date.get()]));

    ASSERT_NOT_REACHED();
    return 0.f;
}

} // namespace WebCore
