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
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKDatePickerPopoverController.h"
#import "WKWebViewIOS.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebPageProxy.h"
#import <UIKit/UIDatePicker.h>
#import <WebCore/LocalizedStrings.h>
#import <pal/system/ios/UserInterfaceIdiom.h>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>

@interface WKDateTimePicker : NSObject<WKFormControl, WKDatePickerPopoverControllerDelegate> {
    NSString *_formatString;
    RetainPtr<NSString> _initialValue;
    WKContentView *_view;
    CGPoint _interactionPoint;
    RetainPtr<UIDatePicker> _datePicker;
    BOOL _isDismissingDatePicker;
    RetainPtr<WKDatePickerPopoverController> _datePickerController;
}

- (instancetype)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode;

@property (nonatomic, readonly) WKDatePickerPopoverController *datePickerController;
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
static constexpr auto yearAndMonthDatePickerMode = static_cast<UIDatePickerMode>(4269);

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
    [_datePicker addTarget:self action:@selector(_dateChanged) forControlEvents:UIControlEventValueChanged];

    if ([self shouldForceGregorianCalendar])
        [_datePicker setCalendar:[NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian]];

    [_datePicker setDatePickerMode:mode];

#if HAVE(UIDATEPICKER_STYLE)
    if (mode == UIDatePickerModeTime || mode == yearAndMonthDatePickerMode)
        [_datePicker setPreferredDatePickerStyle:UIDatePickerStyleWheels];
    else
        [_datePicker setPreferredDatePickerStyle:UIDatePickerStyleInline];
#endif
    _isDismissingDatePicker = NO;

    return self;
}

- (void)datePickerPopoverControllerDidDismiss:(WKDatePickerPopoverController *)controller
{
    [self handleDatePickerPresentationDismissal];
}

- (void)datePickerPopoverControllerDidReset:(WKDatePickerPopoverController *)controller
{
    [self setDateTimePickerToInitialValue];
    [_view page]->setFocusedElementValue([_view focusedElementInformation].elementContext, { });
}

- (void)handleDatePickerPresentationDismissal
{
    if (_isDismissingDatePicker)
        return;

    SetForScope isDismissingDatePicker { _isDismissingDatePicker, YES };
    [_view accessoryDone];
}

- (void)removeDatePickerPresentation
{
    if (_datePickerController) {
        if (!_isDismissingDatePicker) {
            SetForScope isDismissingDatePicker { _isDismissingDatePicker, YES };
            [_datePickerController dismissViewControllerAnimated:NO completion:nil];
        }

        _datePickerController = nil;
        [_view.webView _didDismissContextMenu];
    }
}

- (WKDatePickerPopoverController *)datePickerController
{
    return _datePickerController.get();
}

- (void)showDateTimePicker
{
    _datePickerController = adoptNS([[WKDatePickerPopoverController alloc] initWithDatePicker:_datePicker.get() delegate:self]);
    [_datePickerController presentInView:_view sourceRect:_view.focusedElementInformation.interactionRect completion:[strongSelf = retainPtr(self)] {
        [strongSelf->_view.webView _didShowContextMenu];
    }];
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
    [self removeDatePickerPresentation];
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
    auto englishLocale = adoptNS([[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"]);
    auto dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [dateFormatter setTimeZone:[_datePicker timeZone]];
    [dateFormatter setDateFormat:_formatString];
    // Force English locale because that is what HTML5 value parsing expects.
    [dateFormatter setLocale:englishLocale.get()];
    return dateFormatter;
}

- (void)_dateChanged
{
    RetainPtr dateFormatter = [self dateFormatterForPicker];
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
#if PLATFORM(MACCATALYST)
    // The date/time input popover always attempts to steal first responder from the web view upon
    // presentation due to the Catalyst-specific `_UIPopoverHostManagerMac`, so we need to relinquish
    // first responder to the focused element to avoid immediately blurring the focused element.
    bool shouldRelinquishFirstResponder = true;
#else
    auto elementType = _view.focusedElementInformation.elementType;
    bool shouldRelinquishFirstResponder = elementType == WebKit::InputType::Time || elementType == WebKit::InputType::DateTimeLocal;
#endif
    if (shouldRelinquishFirstResponder)
        [_view startRelinquishingFirstResponderToFocusedElement];

    // Set the time zone in case it changed.
    [_datePicker setTimeZone:NSTimeZone.localTimeZone];

    // Currently no value for the <input>. Start the picker with the current time.
    // Also, update the actual <input> value.
    _initialValue = _view.focusedElementInformation.value;
    [self setDateTimePickerToInitialValue];
    [self showDateTimePicker];
}

- (void)controlUpdateEditing
{
}

- (void)controlEndEditing
{
    [_view stopRelinquishingFirstResponderToFocusedElement];
    [self removeDatePickerPresentation];
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
        mode = yearAndMonthDatePickerMode;
        break;
    default:
        [self release];
        return nil;
    }

    self = [super initWithView:view control:adoptNS([[WKDateTimePicker alloc] initWithView:view datePickerMode:mode])];
    return self;
}

@end

@implementation WKDateTimeInputControl (WKTesting)

- (void)setTimePickerHour:(NSInteger)hour minute:(NSInteger)minute
{
    if (auto picker = dynamic_objc_cast<WKDateTimePicker>(self.control))
        [picker setHour:hour minute:minute];
}

- (NSString *)dateTimePickerCalendarType
{
    if (auto picker = dynamic_objc_cast<WKDateTimePicker>(self.control))
        return picker.calendarType;
    return nil;
}

- (double)timePickerValueHour
{
    if (auto picker = dynamic_objc_cast<WKDateTimePicker>(self.control))
        return picker.hour;
    return -1;
}

- (double)timePickerValueMinute
{
    if (auto picker = dynamic_objc_cast<WKDateTimePicker>(self.control))
        return picker.minute;
    return -1;
}

- (BOOL)dismissWithAnimationForTesting
{
    if (auto picker = dynamic_objc_cast<WKDateTimePicker>(self.control)) {
        [picker.datePickerController assertAccessoryViewCanBeHitTestedForTesting];
        [picker.datePickerController dismissDatePicker];
        return YES;
    }
    return NO;
}

@end

#endif // PLATFORM(IOS_FAMILY) && !PLATFORM(WATCHOS)
