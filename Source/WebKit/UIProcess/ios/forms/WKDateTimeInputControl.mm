/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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
#import "WKDateTimeInputControl.h"

#if PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS)

#import "UIKitSPI.h"
#import "UserInterfaceIdiom.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebPageProxy.h"
#import <UIKit/UIDatePicker.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>

@interface WKDateTimePicker : NSObject<WKFormControl> {
    NSString *_formatString;
    RetainPtr<NSString> _initialValue;
    WKContentView *_view;
    CGPoint _interactionPoint;
    RetainPtr<UIDatePicker> _datePicker;
#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    BOOL _isDismissingDatePicker;

    RetainPtr<_UIDatePickerOverlayPresentation> _datePickerPresentation;
    RetainPtr<UIToolbar> _accessoryView;
#endif
}

- (instancetype)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode;

@property (nonatomic, readonly) NSString *calendarType;
@property (nonatomic, readonly) double hour;
@property (nonatomic, readonly) double minute;
- (void)setHour:(NSInteger)hour minute:(NSInteger)minute;

@end

@implementation WKDateTimePicker

static NSString * const kDateFormatString = @"yyyy-MM-dd"; // "2011-01-27".
static NSString * const kMonthFormatString = @"yyyy-MM"; // "2011-01".
static NSString * const kTimeFormatString = @"HH:mm"; // "13:45".
static NSString * const kDateTimeFormatString = @"yyyy-MM-dd'T'HH:mm"; // "2011-01-27T13:45"
static const NSTimeInterval kMillisecondsPerSecond = 1000;

static const CGFloat kDateTimePickerControlMargin = 6;
static const CGFloat kDateTimePickerToolbarHeight = 44;

- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _interactionPoint = [_view lastInteractionLocation];

    switch (view.focusedElementInformation.elementType) {
    case WebKit::InputType::Date:
        _formatString = kDateFormatString;
        break;
    case WebKit::InputType::Month:
        _formatString = kMonthFormatString;
        break;
    case WebKit::InputType::Time:
        _formatString = kTimeFormatString;
        break;
    case WebKit::InputType::DateTimeLocal:
        _formatString = kDateTimeFormatString;
        break;
    default:
        break;
    }

    _datePicker = adoptNS([[UIDatePicker alloc] init]);
    [_datePicker addTarget:self action:@selector(datePickerChanged:) forControlEvents:UIControlEventValueChanged];

    if ([self shouldForceGregorianCalendar])
        [_datePicker setCalendar:[NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian]];

    [_datePicker setDatePickerMode:mode];

#if HAVE(UIDATEPICKER_STYLE)
    if (mode == UIDatePickerModeTime || mode == (UIDatePickerMode)UIDatePickerModeYearAndMonth)
        [_datePicker setPreferredDatePickerStyle:UIDatePickerStyleWheels];
    else
        [_datePicker setPreferredDatePickerStyle:UIDatePickerStyleInline];
#endif

#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    _isDismissingDatePicker = NO;

    _accessoryView = adoptNS([[UIToolbar alloc] init]);
    [[_accessoryView heightAnchor] constraintEqualToConstant:kDateTimePickerToolbarHeight].active = YES;

#if HAVE(UITOOLBAR_STANDARD_APPEARANCE)
    auto toolbarAppearance = adoptNS([[UIToolbarAppearance alloc] init]);
    [toolbarAppearance setBackgroundEffect:nil];
    [_accessoryView setStandardAppearance:toolbarAppearance.get()];
#endif

    auto resetButton = adoptNS([[UIBarButtonItem alloc] initWithTitle:WEB_UI_STRING_KEY("Reset", "Reset Button Date/Time Context Menu", "Reset button in date input context menu") style:UIBarButtonItemStylePlain target:self action:@selector(reset:)]);
    auto doneButton = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemDone target:self action:@selector(done:)]);

    [_accessoryView setItems:@[ resetButton.get(), UIBarButtonItem.flexibleSpaceItem, doneButton.get() ]];
#endif // HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)

    return self;
}

#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)

- (void)handleDatePickerPresentationDismissal
{
    if (_isDismissingDatePicker)
        return;

    SetForScope isDismissingDatePicker { _isDismissingDatePicker, YES };
    [_view accessoryDone];
}

- (void)removeDatePickerPresentation
{
    if (_datePickerPresentation) {
        if (!_isDismissingDatePicker) {
            SetForScope isDismissingDatePicker { _isDismissingDatePicker, YES };
            [_datePickerPresentation dismissPresentationAnimated:NO];
        }

        _datePickerPresentation = nil;
        [_view.webView _didDismissContextMenu];
    }
}

- (void)showDateTimePicker
{
    _datePickerPresentation = adoptNS([[_UIDatePickerOverlayPresentation alloc] initWithSourceView:_view]);
    [_datePickerPresentation setSourceRect:_view.focusedElementInformation.interactionRect];
    [_datePickerPresentation setAccessoryView:_accessoryView.get()];
    [_datePickerPresentation setAccessoryViewIgnoresDefaultInsets:YES];
    [_datePickerPresentation setOverlayAnchor:_UIDatePickerOverlayAnchorSourceRect];

    [_datePickerPresentation presentDatePicker:_datePicker.get() onDismiss:[weakSelf = WeakObjCPtr<WKDateTimePicker>(self)](BOOL) {
        if (auto strongSelf = weakSelf.get())
            [strongSelf handleDatePickerPresentationDismissal];
    }];

    [_view.webView _didShowContextMenu];
}

#endif // HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)

- (void)datePickerChanged:(id)sender
{
    [self _dateChanged];
}

- (void)reset:(id)sender
{
    [self setDateTimePickerToInitialValue];
    [_view page]->setFocusedElementValue([_view focusedElementInformation].elementContext, String());
}

- (void)done:(id)sender
{
#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    [_datePickerPresentation dismissPresentationAnimated:YES];
#endif
}

- (BOOL)shouldForceGregorianCalendar
{
    auto autofillFieldName = _view.focusedElementInformation.autofillFieldName;
    return autofillFieldName == WebCore::AutofillFieldName::CcExpMonth
        || autofillFieldName == WebCore::AutofillFieldName::CcExp
        || autofillFieldName == WebCore::AutofillFieldName::CcExpYear;
}

- (void)dealloc
{
#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    [self removeDatePickerPresentation];
#endif
    [super dealloc];
}

- (NSString *)_sanitizeInputValueForFormatter:(NSString *)value
{
    ASSERT([value length]);

    // Times may have seconds and milliseconds information which we just
    // ignore. For example: "01:56:20.391" is shortened to just "01:56".

    if (_view.focusedElementInformation.elementType == WebKit::InputType::Time)
        return [value substringToIndex:[kTimeFormatString length]];

    if (_view.focusedElementInformation.elementType == WebKit::InputType::DateTimeLocal) {
        NSString *timeString = [[value componentsSeparatedByString:@"T"] objectAtIndex:1];
        NSString *sanitizedTimeString = [timeString substringToIndex:[kTimeFormatString length]];
        return [value stringByReplacingOccurrencesOfString:timeString withString:sanitizedTimeString];
    }

    return value;
}

- (RetainPtr<NSDateFormatter>)dateFormatterForPicker
{
    RetainPtr<NSLocale> englishLocale = adoptNS([[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"]);
    RetainPtr<NSDateFormatter> dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [dateFormatter setTimeZone:[_datePicker timeZone]];
    [dateFormatter setDateFormat:_formatString];
    // Force English locale because that is what HTML5 value parsing expects.
    [dateFormatter setLocale:englishLocale.get()];
    return dateFormatter;
}

- (void)_dateChanged
{
    RetainPtr<NSDateFormatter> dateFormatter = [self dateFormatterForPicker];
    [_view updateFocusedElementValue:[dateFormatter stringFromDate:[_datePicker date]]];
}

- (void)setDateTimePickerToInitialValue
{
    if (![_initialValue length]) {
        [_datePicker setDate:[NSDate date]];
        [self _dateChanged];
        return;
    }

    NSDate *parsedDate = [[self dateFormatterForPicker] dateFromString:[self _sanitizeInputValueForFormatter:_initialValue.get()]];
    [_datePicker setDate:parsedDate ? parsedDate : [NSDate date]];
}

- (UIView *)controlView
{
    return nil;
}

- (void)controlBeginEditing
{
    auto elementType = _view.focusedElementInformation.elementType;
    if (elementType == WebKit::InputType::Time || elementType == WebKit::InputType::DateTimeLocal)
        [_view startRelinquishingFirstResponderToFocusedElement];

    // Set the time zone in case it changed.
    [_datePicker setTimeZone:NSTimeZone.localTimeZone];

    // Currently no value for the <input>. Start the picker with the current time.
    // Also, update the actual <input> value.
    _initialValue = _view.focusedElementInformation.value;
    [self setDateTimePickerToInitialValue];

#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    [self showDateTimePicker];
#endif
}

- (void)controlEndEditing
{
    [_view stopRelinquishingFirstResponderToFocusedElement];

#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    [self removeDatePickerPresentation];
#endif
}

- (NSString *)calendarType
{
    return [_datePicker calendar].calendarIdentifier;
}

- (double)hour
{
    NSCalendar *calendar = [NSCalendar currentCalendar];
    NSDateComponents *components = [calendar components:NSCalendarUnitHour fromDate:[_datePicker date]];

    return components.hour;
}

- (double)minute
{
    NSCalendar *calendar = [NSCalendar currentCalendar];
    NSDateComponents *components = [calendar components:NSCalendarUnitMinute fromDate:[_datePicker date]];

    return components.minute;
}

- (void)setHour:(NSInteger)hour minute:(NSInteger)minute
{
    NSString *timeString = [NSString stringWithFormat:@"%.2ld:%.2ld", (long)hour, (long)minute];
    [_datePicker setDate:[[self dateFormatterForPicker] dateFromString:timeString]];
    [self _dateChanged];
}

@end

@implementation WKDateTimeInputControl

- (instancetype)initWithView:(WKContentView *)view
{
    UIDatePickerMode mode;

    switch (view.focusedElementInformation.elementType) {
    case WebKit::InputType::Date:
        mode = UIDatePickerModeDate;
        break;
    case WebKit::InputType::DateTimeLocal:
        mode = UIDatePickerModeDateAndTime;
        break;
    case WebKit::InputType::Time:
        mode = UIDatePickerModeTime;
        break;
    case WebKit::InputType::Month:
        mode = (UIDatePickerMode)UIDatePickerModeYearAndMonth;
        break;
    default:
        [self release];
        return nil;
    }

    return [super initWithView:view control:adoptNS([[WKDateTimePicker alloc] initWithView:view datePickerMode:mode])];
}

@end

@implementation WKDateTimeInputControl (WKTesting)

- (void)setTimePickerHour:(NSInteger)hour minute:(NSInteger)minute
{
    if ([self.control isKindOfClass:WKDateTimePicker.class])
        [(WKDateTimePicker *)self.control setHour:hour minute:minute];
}

- (NSString *)dateTimePickerCalendarType
{
    if ([self.control isKindOfClass:WKDateTimePicker.class])
        return [(WKDateTimePicker *)self.control calendarType];
    return nil;
}

- (double)timePickerValueHour
{
    if ([self.control isKindOfClass:WKDateTimePicker.class])
        return [(WKDateTimePicker *)self.control hour];
    return -1;
}

- (double)timePickerValueMinute
{
    if ([self.control isKindOfClass:WKDateTimePicker.class])
        return [(WKDateTimePicker *)self.control minute];
    return -1;
}

@end

#endif // PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS)
