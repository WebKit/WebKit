/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#import "WKTimePickerViewController.h"

#if PLATFORM(WATCHOS)

#import "ClockKitSPI.h"
#import "UIKitSPI.h"

#import <WebCore/LocalizedStrings.h>
#import <wtf/SoftLinking.h>
#import <wtf/text/WTFString.h>

SOFT_LINK_PRIVATE_FRAMEWORK(ClockKitUI)
SOFT_LINK_CLASS(ClockKitUI, CLKUIWheelsOfTimeView)

static NSString *timePickerTimeZoneForFormatting = @"UTC";
static NSString *timePickerDateFormat = @"HH:mm";

@interface WKTimePickerViewController () <CLKUIWheelsOfTimeDelegate>
@end

@implementation WKTimePickerViewController {
    RetainPtr<CLKUIWheelsOfTimeView> _timePicker;
    RetainPtr<NSDateFormatter> _dateFormatter;
}

@dynamic delegate;

- (instancetype)initWithDelegate:(id <WKQuickboardViewControllerDelegate>)delegate
{
    return [super initWithDelegate:delegate];
}

- (NSDateFormatter *)dateFormatter
{
    if (_dateFormatter)
        return _dateFormatter.get();

    _dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [_dateFormatter setLocale:[NSLocale localeWithLocaleIdentifier:@"en_US_POSIX"]];
    [_dateFormatter setDateFormat:timePickerDateFormat];
    [_dateFormatter setTimeZone:[NSTimeZone timeZoneWithName:timePickerTimeZoneForFormatting]];
    return _dateFormatter.get();
}

- (NSString *)timeValueForFormControls
{
    NSCalendar *calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    calendar.timeZone = [NSTimeZone timeZoneWithName:timePickerTimeZoneForFormatting];

    NSDate *epochDate = [NSDate dateWithTimeIntervalSince1970:0];
    NSDate *timePickerDateAsOffsetFromEpoch = [calendar dateBySettingHour:[_timePicker hour] minute:[_timePicker minute] second:0 ofDate:epochDate options:0];
    return [self.dateFormatter stringFromDate:timePickerDateAsOffsetFromEpoch];
}

- (NSDateComponents *)dateComponentsFromInitialValue
{
    NSString *initialText = [self.delegate initialValueForViewController:self];
    if (initialText.length < timePickerDateFormat.length)
        return nil;

    NSString *truncatedInitialValue = [initialText substringToIndex:timePickerDateFormat.length];
    NSDate *parsedDate = [self.dateFormatter dateFromString:truncatedInitialValue];
    if (!parsedDate)
        return nil;

    NSCalendar *calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    calendar.timeZone = [NSTimeZone timeZoneWithName:timePickerTimeZoneForFormatting];
    return [calendar components:NSCalendarUnitHour | NSCalendarUnitMinute fromDate:parsedDate];
}

#pragma mark - UIViewController overrides

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    [self becomeFirstResponder];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.headerView.hidden = YES;

    _timePicker = adoptNS([allocCLKUIWheelsOfTimeViewInstance() initWithFrame:self.view.bounds style:CLKUIWheelsOfTimeStyleAlarm12]);
    [_timePicker setDelegate:self];

    NSDateComponents *components = self.dateComponentsFromInitialValue;
    if (components)
        [_timePicker setHour:components.hour andMinute:components.minute];

    [self.contentView addSubview:_timePicker.get()];
}

- (BOOL)becomeFirstResponder
{
    return [_timePicker becomeFirstResponder];
}

- (void)setHour:(NSInteger)hour minute:(NSInteger)minute
{
    [_timePicker setHour:hour andMinute:minute];
    [self rightButtonWOTAction];
}

#pragma mark - CLKUIWheelsOfTimeDelegate

- (void)leftButtonWOTAction
{
    // Handle an action on the 'Cancel' button.
    [self.delegate quickboardInputCancelled:self];
}

- (void)rightButtonWOTAction
{
    // Handle an action on the 'Set' button.
    auto valueAsAttributedString = adoptNS([[NSAttributedString alloc] initWithString:self.timeValueForFormControls]);
    [self.delegate quickboard:self textEntered:valueAsAttributedString.get()];
}

@end

#endif // PLATFORM(WATCHOS)
