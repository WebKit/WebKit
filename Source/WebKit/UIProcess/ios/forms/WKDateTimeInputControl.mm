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

#if PLATFORM(IOS_FAMILY)

#import "UserInterfaceIdiom.h"
#import "WKContentView.h"
#import "WKContentViewInteraction.h"
#import "WKWebViewPrivateForTesting.h"
#import "WebPageProxy.h"
#import <UIKit/UIBarButtonItem.h>
#import <UIKit/UIDatePicker.h>
#import <WebCore/LocalizedStrings.h>
#import <algorithm>
#import <wtf/RetainPtr.h>

using namespace WebKit;

@interface WKDateTimeContextMenuViewController : UIViewController
@end

@interface WKDateTimePicker : NSObject<WKFormControl
#if USE(UICONTEXTMENU)
, UIContextMenuInteractionDelegate
#endif
> {
    RetainPtr<UIDatePicker> _datePicker;
    NSString *_formatString;
    NSString *_initialValue;
    NSTimeInterval _initialValueAsNumber;
    BOOL _shouldRemoveTimeZoneInformation;
    BOOL _isTimeInput;
    WKContentView *_view;
    CGPoint _interactionPoint;
    RetainPtr<WKDateTimeContextMenuViewController> _viewController;
#if USE(UICONTEXTMENU)
    RetainPtr<UIContextMenuInteraction> _dateTimeContextMenuInteraction;
#endif
    BOOL _presenting;
    BOOL _preservingFocus;
}
- (instancetype)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode;
- (WKDateTimeContextMenuViewController *)viewController;
@property (nonatomic, readonly) NSString *calendarType;
@property (nonatomic, readonly) double hour;
@property (nonatomic, readonly) double minute;
- (void)setHour:(NSInteger)hour minute:(NSInteger)minute;
@end

@implementation WKDateTimeContextMenuViewController

- (CGSize)preferredContentSize
{
    // FIXME: Workaround, should be able to be readdressed after <rdar://problem/64143534>
    UIView *view = self.view.subviews[0];
    if (UIEdgeInsetsEqualToEdgeInsets(view.layoutMargins, UIEdgeInsetsZero)) {
        view.translatesAutoresizingMaskIntoConstraints = NO;
        [view layoutIfNeeded];
        view.translatesAutoresizingMaskIntoConstraints = YES;
        view.layoutMargins = UIEdgeInsetsMake(16, 16, 16, 16);
    }
    auto size = [view systemLayoutSizeFittingSize:UILayoutFittingCompressedSize];
    
    size.width = std::max<CGFloat>(size.width, 250.0);
    return size;
}

@end

@implementation WKDateTimePicker

static NSString * const kDateFormatString = @"yyyy-MM-dd"; // "2011-01-27".
static NSString * const kMonthFormatString = @"yyyy-MM"; // "2011-01".
static NSString * const kTimeFormatString = @"HH:mm"; // "13:45".
static const NSTimeInterval kMillisecondsPerSecond = 1000;

#if HAVE(UIDATEPICKER_STYLE)
- (UIDatePickerStyle)datePickerStyle
{
    if ([_view focusedElementInformation].elementType == WebKit::InputType::Month)
        return UIDatePickerStyleWheels;
    return UIDatePickerStyleInline;
}
#endif

- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode
{
    if (!(self = [super init]))
        return nil;
    _view = view;
    _interactionPoint = [_view lastInteractionLocation];
    _shouldRemoveTimeZoneInformation = NO;
    _isTimeInput = NO;
    switch (view.focusedElementInformation.elementType) {
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
    
    _datePicker = adoptNS([[UIDatePicker alloc] init]);

    [_datePicker setDatePickerMode:mode];
    [_datePicker setHidden:NO];
    
#if HAVE(UIDATEPICKER_STYLE)
    [_datePicker setPreferredDatePickerStyle:[self datePickerStyle]];
#endif
    if ([self shouldPresentGregorianCalendar:view.focusedElementInformation])
        _datePicker.get().calendar = [NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian];
    
    [_datePicker addTarget:self action:@selector(_dateChangeHandler:) forControlEvents:UIControlEventValueChanged];
    
    return self;
}

#if USE(UICONTEXTMENU)

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    return [_view _createTargetedContextMenuHintPreviewIfPossible];
}

- (_UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    _UIContextMenuStyle *style = [_UIContextMenuStyle defaultStyle];
    style.hasInteractivePreview = YES;
    style.preferredLayout = _UIContextMenuLayoutAutomatic;
    return style;
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
    return [UIContextMenuConfiguration configurationWithIdentifier:@"_UIDatePickerCompactEditor" previewProvider:^{
        [_viewController setView:nil];
        _viewController = adoptNS([[WKDateTimeContextMenuViewController alloc] init]);
        RetainPtr<UINavigationController> navigationController = adoptNS([[UINavigationController alloc] initWithRootViewController:_viewController.get()]);
        
        NSString *resetString = WEB_UI_STRING_KEY("Reset", "Reset Button Date/Time Context Menu", "Reset button in date input context menu");
        NSString *okString = WEB_UI_STRING_KEY("OK", "OK (OK button title in date/time picker)", "Title of the OK button in date/time form controls.");

        RetainPtr<UIBarButtonItem> okBarButton = adoptNS([[UIBarButtonItem alloc] initWithTitle:okString style:UIBarButtonItemStyleDone target:self action:@selector(ok:)]);
        RetainPtr<UIBarButtonItem> blankBarButton = adoptNS([[UIBarButtonItem alloc] initWithBarButtonSystemItem:UIBarButtonSystemItemFlexibleSpace target:nil action:nil]);
        RetainPtr<UIBarButtonItem> resetBarButton = adoptNS([[UIBarButtonItem alloc] initWithTitle:resetString style:UIBarButtonItemStylePlain target:self action:@selector(reset:)]);
        
        [_viewController setToolbarItems:@[resetBarButton.get(), blankBarButton.get(), okBarButton.get()]];
        [navigationController setToolbarHidden:NO];
        
        auto centeringView = adoptNS([[UIView alloc] init]);
        
        [centeringView addSubview:_datePicker.get()];
        
        _datePicker.get().translatesAutoresizingMaskIntoConstraints = NO;
        auto widthConstraint = [[centeringView widthAnchor] constraintGreaterThanOrEqualToAnchor:[_datePicker widthAnchor]];
        auto heightConstraint = [[centeringView heightAnchor] constraintEqualToAnchor:[_datePicker heightAnchor]];
        auto horizontalConstraint = [[centeringView centerXAnchor] constraintEqualToAnchor:[_datePicker centerXAnchor]];
        auto verticalConstraint = [[centeringView centerYAnchor] constraintEqualToAnchor:[_datePicker centerYAnchor]];

        [NSLayoutConstraint activateConstraints:@[verticalConstraint, horizontalConstraint, widthConstraint, heightConstraint]];

        [_viewController setView:centeringView.get()];

        NSString *titleText = _view.inputLabelText;
        if (![titleText isEqual:@""]) {
            RetainPtr<UILabel> title = adoptNS([[UILabel alloc] init]);
            [title setFont:[UIFont preferredFontForTextStyle:UIFontTextStyleFootnote]];
            [title setText:titleText];
            [title setTextColor:[UIColor secondaryLabelColor]];
            [title setTextAlignment:NSTextAlignmentNatural];
            RetainPtr<UIBarButtonItem> titleButton = adoptNS([[UIBarButtonItem alloc] initWithCustomView:title.get()]);
            [_viewController navigationItem].leftBarButtonItem = titleButton.get();
        } else
            [navigationController setNavigationBarHidden:YES animated:NO];
        
        [navigationController navigationBar].translucent = NO;
        [navigationController navigationBar].barTintColor = [UIColor systemBackgroundColor];
        [navigationController toolbar].translucent = NO;
        [navigationController toolbar].barTintColor = [UIColor systemBackgroundColor];
        return navigationController.get();
    } actionProvider:nil];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willDisplayMenuForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKDateTimePicker>(self)] {
        auto strongSelf = weakSelf.get();
        if (strongSelf)
            [strongSelf->_view.webView _didShowContextMenu];
    }];
}

- (void)contextMenuInteraction:(UIContextMenuInteraction *)interaction willEndForConfiguration:(UIContextMenuConfiguration *)configuration animator:(id <UIContextMenuInteractionAnimating>)animator
{
    [animator addCompletion:[weakSelf = WeakObjCPtr<WKDateTimePicker>(self)] {
        auto strongSelf = weakSelf.get();
        if (strongSelf) {
            [strongSelf->_view accessoryDone];
            [strongSelf->_view.webView _didDismissContextMenu];
        }
    }];
}

- (void)removeContextMenuInteraction
{
    if (_dateTimeContextMenuInteraction) {
        [_view removeInteraction:_dateTimeContextMenuInteraction.get()];
        _dateTimeContextMenuInteraction = nil;
        [_view _removeContextMenuViewIfPossible];
        [_view.webView _didDismissContextMenu];
    }
}

- (void)ensureContextMenuInteraction
{
    if (!_dateTimeContextMenuInteraction) {
        _dateTimeContextMenuInteraction = adoptNS([[UIContextMenuInteraction alloc] initWithDelegate:self]);
        [_view addInteraction:_dateTimeContextMenuInteraction.get()];
    }
}

- (void)showDateTimePicker
{
#if HAVE(UICONTEXTMENU_LOCATION)
    [self ensureContextMenuInteraction];
    [_dateTimeContextMenuInteraction _presentMenuAtLocation:_interactionPoint];
#endif
}

#endif

- (void)reset:(id)sender
{
    [self setDateTimePickerToInitialValue];
    [_view page]->setFocusedElementValue(String());
}

- (void)ok:(id)sender
{
#if USE(UICONTEXTMENU)
    [_dateTimeContextMenuInteraction dismissMenu];
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

    return [components hour];
}

- (double)minute
{
    NSCalendar *calendar = [NSCalendar currentCalendar];
    NSDateComponents *components = [calendar components:NSCalendarUnitMinute fromDate:[_datePicker date]];

    return [components minute];
}

- (void)dealloc
{
    [_datePicker removeTarget:self action:NULL forControlEvents:UIControlEventValueChanged];
#if USE(UICONTEXTMENU)
    [self removeContextMenuInteraction];
#endif
    [super dealloc];
}

- (BOOL)shouldPresentGregorianCalendar:(const FocusedElementInformation&)nodeInfo
{
    return nodeInfo.autofillFieldName == WebCore::AutofillFieldName::CcExpMonth
        || nodeInfo.autofillFieldName == WebCore::AutofillFieldName::CcExp
        || nodeInfo.autofillFieldName == WebCore::AutofillFieldName::CcExpYear;
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
    [_view updateFocusedElementValueAsNumber:([date timeIntervalSince1970] + [self _timeZoneOffsetFromGMT:date]) * kMillisecondsPerSecond];
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
    [_view updateFocusedElementValue:[dateFormatter stringFromDate:[_datePicker date]]];
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

- (void)setDateTimePickerToInitialValue
{
    if ([_initialValue isEqual: @""]) {
        [_datePicker setDate:[NSDate date]];
        [self _dateChanged];
    } else if (_formatString) {
        // Convert the string value to a date object for the fields where we have a format string.
        RetainPtr<NSDateFormatter> dateFormatter = [self dateFormatterForPicker];
        NSDate *parsedDate = [dateFormatter dateFromString:[self _sanitizeInputValueForFormatter:_initialValue]];
        [_datePicker setDate:parsedDate ? parsedDate : [NSDate date]];
    } else {
        // Convert the number value to a date object for the fields affected by timezones.
        NSTimeInterval secondsSince1970 = _initialValueAsNumber / kMillisecondsPerSecond;
        NSInteger timeZoneOffset = [self _timeZoneOffsetFromGMT:[NSDate dateWithTimeIntervalSince1970:secondsSince1970]];
        NSTimeInterval adjustedSecondsSince1970 = secondsSince1970 - timeZoneOffset;
        [_datePicker setDate:[NSDate dateWithTimeIntervalSince1970:adjustedSecondsSince1970]];
    }
}

- (void)controlBeginEditing
{
    if (_presenting)
        return;

    _presenting = YES;

    auto elementType = _view.focusedElementInformation.elementType;
    if (elementType == InputType::Time || elementType == InputType::DateTimeLocal)
        [_view startRelinquishingFirstResponderToFocusedElement];

    // Set the time zone in case it changed.
    _datePicker.get().timeZone = [NSTimeZone localTimeZone];

    // Currently no value for the <input>. Start the picker with the current time.
    // Also, update the actual <input> value.
    _initialValue = _view.focusedElementInformation.value;
    _initialValueAsNumber = _view.focusedElementInformation.valueAsNumber;
    [self setDateTimePickerToInitialValue];
    
#if USE(UICONTEXTMENU)
    WebKit::InteractionInformationRequest positionInformationRequest { WebCore::IntPoint(_view.focusedElementInformation.lastInteractionLocation) };
    [_view doAfterPositionInformationUpdate:^(WebKit::InteractionInformationAtPosition interactionInformation) {
        [self showDateTimePicker];
    } forRequest:positionInformationRequest];
#endif

}

- (void)setHour:(NSInteger)hour minute:(NSInteger)minute
{
    NSString *timeString = [NSString stringWithFormat:@"%.2ld:%.2ld", (long)hour, (long)minute];
    [_datePicker setDate:[[self dateFormatterForPicker] dateFromString:timeString]];
    [self _dateChanged];
}

- (WKDateTimeContextMenuViewController *)viewController
{
    return _viewController.get();
}

- (void)controlEndEditing
{
    _presenting = NO;

    [_view stopRelinquishingFirstResponderToFocusedElement];

#if USE(UICONTEXTMENU)
    [self removeContextMenuInteraction];
#endif
}
@end

@implementation WKDateTimeInputControl

- (instancetype)initWithView:(WKContentView *)view
{
    UIDatePickerMode mode;

    switch (view.focusedElementInformation.elementType) {
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

#endif // PLATFORM(IOS_FAMILY)
