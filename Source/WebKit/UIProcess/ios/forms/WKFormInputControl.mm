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
#import "WKFormInputControl.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKFormPopover.h"
#import "WebPageProxy.h"
#import <UIKit/UIBarButtonItem.h>
#import <UIKit/UIDatePicker.h>
#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>

using namespace WebKit;

@interface WKDateTimePopoverViewController : UIViewController {
    RetainPtr<NSObject<WKFormControl>> _innerControl;
}
- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)datePickerMode;
- (NSObject<WKFormControl> *)innerControl;
@end

@interface WKDateTimePicker : NSObject<WKFormControl> {
    RetainPtr<UIDatePicker> _datePicker;
    NSString *_formatString;
    BOOL _shouldRemoveTimeZoneInformation;
    BOOL _isTimeInput;
    WKContentView* _view;
}
- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode;
- (UIDatePicker *)datePicker;
@end

@interface WKDateTimePopover : WKFormRotatingAccessoryPopover<WKFormControl> {
    RetainPtr<WKDateTimePopoverViewController> _viewController;
    WKContentView* _view;
}
- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode;
- (WKDateTimePopoverViewController *) viewController;
@end

@implementation WKDateTimePicker

static NSString * const kDateFormatString = @"yyyy-MM-dd"; // "2011-01-27".
static NSString * const kMonthFormatString = @"yyyy-MM"; // "2011-01".
static NSString * const kTimeFormatString = @"HH:mm"; // "13:45".
static const NSTimeInterval kMillisecondsPerSecond = 1000;

- (UIDatePicker *)datePicker
{
    return _datePicker.get();
}

- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _shouldRemoveTimeZoneInformation = NO;
    _isTimeInput = NO;
    switch (view.assistedNodeInformation.elementType) {
    case InputType::Date:
        _formatString = kDateFormatString;
        break;
    case InputType::Month:
        _formatString = kMonthFormatString;
        break;
    case InputType::Time:
        _formatString = kTimeFormatString;
        _isTimeInput = YES;
        break;
    case InputType::DateTimeLocal:
        _shouldRemoveTimeZoneInformation = YES;
        break;
    default:
        break;
   }

    CGSize size = currentUserInterfaceIdiomIsPad() ? [UIPickerView defaultSizeForCurrentOrientation] : [UIKeyboard defaultSizeForInterfaceOrientation:[UIApp interfaceOrientation]];

    _datePicker = adoptNS([[UIDatePicker alloc] initWithFrame:CGRectMake(0, 0, size.width, size.height)]);
    _datePicker.get().datePickerMode = mode;
    _datePicker.get().hidden = NO;
    [_datePicker addTarget:self action:@selector(_dateChangeHandler:) forControlEvents:UIControlEventValueChanged];

    return self;
}

- (void)dealloc
{
    [_datePicker removeTarget:self action:NULL forControlEvents:UIControlEventValueChanged];
    [super dealloc];
}

- (UIView *)controlView
{
    return _datePicker.get();
}

- (NSInteger)_timeZoneOffsetFromGMT:(NSDate *)date
{
    if (!_shouldRemoveTimeZoneInformation)
        return 0;

    return [_datePicker.get().timeZone secondsFromGMTForDate:date];
}

- (NSString *)_sanitizeInputValueForFormatter:(NSString *)value
{
    // The "time" input type may have seconds and milliseconds information which we
    // just ignore. For example: "01:56:20.391" is shortened to just "01:56".
    if (_isTimeInput)
        return [value substringToIndex:[kTimeFormatString length]];

    return value;
}

- (void)_dateChangedSetAsNumber
{
    NSDate *date = [_datePicker date];
    [_view page]->setAssistedNodeValueAsNumber(([date timeIntervalSince1970] + [self _timeZoneOffsetFromGMT:date]) * kMillisecondsPerSecond);
}

- (RetainPtr<NSDateFormatter>)dateFormatterForPicker
{
    RetainPtr<NSLocale> englishLocale = adoptNS([[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"]);
    RetainPtr<NSDateFormatter> dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [dateFormatter setTimeZone:_datePicker.get().timeZone];
    [dateFormatter setDateFormat:_formatString];
    [dateFormatter setLocale:englishLocale.get()];
    return dateFormatter;
}

- (void)_dateChangedSetAsString
{
    // Force English locale because that is what HTML5 value parsing expects.
    RetainPtr<NSDateFormatter> dateFormatter = [self dateFormatterForPicker];

    [_view page]->setAssistedNodeValue([dateFormatter stringFromDate:[_datePicker date]]);
}

- (void)_dateChanged
{
    // Internally, DOMHTMLInputElement setValueAs* each take different values for
    // different date types. It is sometimes easier to set the date in different ways:
    //   - use setValueAsString for "date", "month", and "time".
    //   - use setValueAsNumber for "datetime-local".
    if (_formatString)
        [self _dateChangedSetAsString];
    else
        [self _dateChangedSetAsNumber];
}

- (void)_dateChangeHandler:(id)sender
{
    [self _dateChanged];
}

- (void)controlBeginEditing
{
    // Set the time zone in case it changed.
    _datePicker.get().timeZone = [NSTimeZone localTimeZone];

    // Currently no value for the <input>. Start the picker with the current time.
    // Also, update the actual <input> value.
    NSString *value = _view.assistedNodeInformation.value;
    if (_view.assistedNodeInformation.value.isEmpty()) {
        [_datePicker setDate:[NSDate date]];
        [self _dateChanged];
        return;
    }

    // Convert the string value to a date object for the fields where we have a format string.
    if (_formatString) {
        value = [self _sanitizeInputValueForFormatter:value];
        RetainPtr<NSDateFormatter> dateFormatter = [self dateFormatterForPicker];
        NSDate *parsedDate = [dateFormatter dateFromString:value];
        [_datePicker setDate:parsedDate ? parsedDate : [NSDate date]];
        return;
    }

    // Convert the number value to a date object for the fields affected by timezones.
    NSTimeInterval secondsSince1970 = _view.assistedNodeInformation.valueAsNumber / kMillisecondsPerSecond;
    NSInteger timeZoneOffset = [self _timeZoneOffsetFromGMT:[NSDate dateWithTimeIntervalSince1970:secondsSince1970]];
    NSTimeInterval adjustedSecondsSince1970 = secondsSince1970 - timeZoneOffset;
    [_datePicker setDate:[NSDate dateWithTimeIntervalSince1970:adjustedSecondsSince1970]];
}

- (void)controlEndEditing
{
}

@end

// WKFormInputControl
@implementation WKFormInputControl {
    RetainPtr<id<WKFormControl>> _control;
}

- (instancetype)initWithView:(WKContentView *)view
{
    if (!(self = [super init]))
        return nil;

    UIDatePickerMode mode;

    switch (view.assistedNodeInformation.elementType) {
    case InputType::Date:
        mode = UIDatePickerModeDate;
        break;

    case InputType::DateTimeLocal:
        mode = UIDatePickerModeDateAndTime;
        break;

    case InputType::Time:
        mode = UIDatePickerModeTime;
        break;

    case InputType::Month:
        mode = (UIDatePickerMode)UIDatePickerModeYearAndMonth;
        break;

    default:
        [self release];
        return nil;
    }

    if (currentUserInterfaceIdiomIsPad())
        _control = adoptNS([[WKDateTimePopover alloc] initWithView:view datePickerMode:mode]);
    else
        _control = adoptNS([[WKDateTimePicker alloc] initWithView:view datePickerMode:mode]);

    return self;

}

- (void)beginEditing
{
    [_control controlBeginEditing];
}

- (void)endEditing
{
    [_control controlEndEditing];
}

- (UIView *)assistantView
{
    return [_control controlView];
}

@end


@implementation WKDateTimePopoverViewController

- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)datePickerMode
{
    if (!(self = [super init]))
        return nil;

    _innerControl = adoptNS([[WKDateTimePicker alloc] initWithView:view datePickerMode:datePickerMode]);

    return self;
}

- (NSObject<WKFormControl> *)innerControl
{
    return _innerControl.get();
}

- (void)loadView
{
    self.view = [_innerControl controlView];
}

@end

@implementation WKDateTimePopover

- (void)clear:(id)sender
{
    [_view page]->setAssistedNodeValue(String());
}

- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode
{
    if (!(self = [super initWithView:view]))
        return nil;

    _view = view;
    _viewController = adoptNS([[WKDateTimePopoverViewController alloc] initWithView:view datePickerMode:mode]);
    UIDatePicker *datePicker = [(WKDateTimePicker *)_viewController.get().innerControl datePicker];
    CGFloat popoverWidth = [datePicker _contentWidth];
    CGFloat popoverHeight = _viewController.get().view.frame.size.height;
    [_viewController setPreferredContentSize:CGSizeMake(popoverWidth, popoverHeight)];
    [_viewController setEdgesForExtendedLayout:UIRectEdgeNone];
    [_viewController setTitle:_view.assistedNodeInformation.title];

    // Always have a navigation controller with a clear button, and a title if the input element has a title.
    RetainPtr<UINavigationController> navigationController = adoptNS([[UINavigationController alloc] initWithRootViewController:_viewController.get()]);
    UINavigationItem *navigationItem = navigationController.get().navigationBar.topItem;
    NSString *clearString = WEB_UI_STRING_KEY("Clear", "Clear Button Date Popover", "Clear button in date input popover");
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    UIBarButtonItem *clearButton = [[[UIBarButtonItem alloc] initWithTitle:clearString style:UIBarButtonItemStyleBordered target:self action:@selector(clear:)] autorelease];
    ALLOW_DEPRECATED_DECLARATIONS_END
    [navigationItem setRightBarButtonItem:clearButton];

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    RetainPtr<UIPopoverController> controller = adoptNS([[UIPopoverController alloc] initWithContentViewController:navigationController.get()]);
    ALLOW_DEPRECATED_DECLARATIONS_END
    [self setPopoverController:controller.get()];

    return self;
}

- (WKDateTimePopoverViewController *)viewController
{
    return _viewController.get();
}

- (void)controlBeginEditing
{
    [self presentPopoverAnimated:NO];
    [_viewController.get().innerControl controlBeginEditing];
}

- (void)controlEndEditing
{
}

- (UIView *)controlView
{
    return nil;
}

@end

#endif // PLATFORM(IOS_FAMILY)
