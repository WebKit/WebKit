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
#import <algorithm>
#import <wtf/RetainPtr.h>
#import <wtf/SetForScope.h>

@class WKDateTimePickerViewController;

@protocol WKDateTimePickerViewControllerDelegate <NSObject>
- (void)dateTimePickerViewControllerDidPressResetButton:(WKDateTimePickerViewController *)dateTimePickerViewController;
- (void)dateTimePickerViewControllerDidPressDoneButton:(WKDateTimePickerViewController *)dateTimePickerViewController;
@end

@interface WKDateTimePickerViewController : UIViewController
- (instancetype)initWithDatePicker:(UIDatePicker *)datePicker;
- (void)setDelegate:(id <WKDateTimePickerViewControllerDelegate>)delegate;
@end

@implementation WKDateTimePickerViewController {
    CGSize _contentSize;

    RetainPtr<UIDatePicker> _datePicker;
    WeakObjCPtr<id <WKDateTimePickerViewControllerDelegate>> _delegate;
}

static const CGFloat kDateTimePickerButtonFontSize = 17;
static const CGFloat kDateTimePickerToolbarHeight = 44;
static const CGFloat kDateTimePickerSeparatorHeight = 1;
static const CGFloat kDateTimePickerViewMargin = 16;

static const CGFloat kDateTimePickerDefaultWidth = 320;
static const CGFloat kDateTimePickerTimeControlWidth = 218;
static const CGFloat kDateTimePickerTimeControlHeight = 172;

- (instancetype)initWithDatePicker:(UIDatePicker *)datePicker
{
    if (!(self = [super init]))
        return nil;

    _datePicker = datePicker;
    [_datePicker setTranslatesAutoresizingMaskIntoConstraints:NO];

    return self;
}

- (void)setDelegate:(id <WKDateTimePickerViewControllerDelegate>)delegate
{
    _delegate = delegate;
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    CGSize contentSize = self.preferredContentSize;
    CGRect contentFrame = CGRectMake(0, 0, contentSize.width, contentSize.height);

    UIView *contentView = nil;
#if HAVE(UIBLUREFFECT_STYLE_SYSTEM_MATERIAL)
    auto backgroundView = adoptNS([[UIVisualEffectView alloc] initWithEffect:[UIBlurEffect effectWithStyle:UIBlurEffectStyleSystemMaterial]]);
    [backgroundView setFrame:contentFrame];
    [self.view addSubview:backgroundView.get()];
    contentView = [backgroundView contentView];
#else
    auto backgroundView = adoptNS([[UIView alloc] initWithFrame:contentFrame]);
    [backgroundView setBackgroundColor:UIColor.systemBackgroundColor];
    [self.view addSubview:backgroundView.get()];
    contentView = backgroundView.get();
#endif

    [contentView addSubview:_datePicker.get()];

    CGSize datePickerSize = self.preferredDatePickerSize;
    UIEdgeInsets datePickerInsets = [self datePickerInsets];

    [NSLayoutConstraint activateConstraints:@[
        [[_datePicker topAnchor] constraintEqualToAnchor:contentView.topAnchor constant:datePickerInsets.top],
        [[_datePicker leadingAnchor] constraintEqualToAnchor:contentView.leadingAnchor constant:datePickerInsets.left],
        [[_datePicker widthAnchor] constraintEqualToConstant:datePickerSize.width],
        [[_datePicker heightAnchor] constraintEqualToConstant:datePickerSize.height],
    ]];

    auto toolbarView = adoptNS([[UIView alloc] init]);
    [toolbarView setTranslatesAutoresizingMaskIntoConstraints:NO];
    [contentView addSubview:toolbarView.get()];

    [NSLayoutConstraint activateConstraints:@[
        [[toolbarView bottomAnchor] constraintEqualToAnchor:contentView.bottomAnchor],
        [[toolbarView leadingAnchor] constraintEqualToAnchor:contentView.leadingAnchor],
        [[toolbarView heightAnchor] constraintEqualToConstant:kDateTimePickerToolbarHeight],
        [[toolbarView widthAnchor] constraintEqualToAnchor:contentView.widthAnchor],
    ]];

    auto separatorView = adoptNS([[UIView alloc] init]);
    [separatorView setBackgroundColor:UIColor.separatorColor];

    NSString *resetString = WEB_UI_STRING_KEY("Reset", "Reset Button Date/Time Context Menu", "Reset button in date input context menu");
    UIButton *resetButton = [UIButton buttonWithType:UIButtonTypeSystem];
    resetButton.titleLabel.font = [UIFont systemFontOfSize:kDateTimePickerButtonFontSize];
    [resetButton setTitle:resetString forState:UIControlStateNormal];
    [resetButton addTarget:self action:@selector(resetButtonPressed:) forControlEvents:UIControlEventTouchUpInside];

    NSString *doneString = WebCore::formControlDoneButtonTitle();
    UIButton *doneButton = [UIButton buttonWithType:UIButtonTypeSystem];
    doneButton.titleLabel.font = [UIFont boldSystemFontOfSize:kDateTimePickerButtonFontSize];
    [doneButton setTitle:doneString forState:UIControlStateNormal];
    [doneButton addTarget:self action:@selector(doneButtonPressed:) forControlEvents:UIControlEventTouchUpInside];

    for (UIView *subview in @[separatorView.get(), resetButton, doneButton]) {
        subview.translatesAutoresizingMaskIntoConstraints = NO;
        [toolbarView addSubview:subview];
    }

    [NSLayoutConstraint activateConstraints:@[
        [[separatorView topAnchor] constraintEqualToAnchor:[toolbarView topAnchor]],
        [[separatorView leadingAnchor] constraintEqualToAnchor:[toolbarView leadingAnchor]],
        [[separatorView heightAnchor] constraintEqualToConstant:kDateTimePickerSeparatorHeight],
        [[separatorView widthAnchor] constraintEqualToAnchor:[toolbarView widthAnchor]],
    ]];

    [NSLayoutConstraint activateConstraints:@[
        [resetButton.leadingAnchor constraintEqualToAnchor:[toolbarView leadingAnchor] constant:kDateTimePickerViewMargin],
        [resetButton.topAnchor constraintEqualToAnchor:[toolbarView topAnchor]],
        [resetButton.bottomAnchor constraintEqualToAnchor:[toolbarView bottomAnchor]],
    ]];

    [NSLayoutConstraint activateConstraints:@[
        [doneButton.trailingAnchor constraintEqualToAnchor:[toolbarView trailingAnchor] constant:-kDateTimePickerViewMargin],
        [doneButton.topAnchor constraintEqualToAnchor:[toolbarView topAnchor]],
        [doneButton.bottomAnchor constraintEqualToAnchor:[toolbarView bottomAnchor]],
    ]];
}

- (void)resetButtonPressed:(id)sender
{
    [_delegate dateTimePickerViewControllerDidPressResetButton:self];
}

- (void)doneButtonPressed:(id)sender
{
    [_delegate dateTimePickerViewControllerDidPressDoneButton:self];
}

- (UIEdgeInsets)datePickerInsets
{
#if HAVE(UIDATEPICKER_INSETS)
    UIEdgeInsets expectedInsets = UIEdgeInsetsMake(kDateTimePickerViewMargin, kDateTimePickerViewMargin, kDateTimePickerViewMargin, kDateTimePickerViewMargin);
    UIEdgeInsets appliedInsets = [_datePicker _appliedInsetsToEdgeOfContent];
    return UIEdgeInsetsSubtract(expectedInsets, appliedInsets, UIRectEdgeAll);
#else
    return UIEdgeInsetsZero;
#endif
}

- (CGSize)preferredDatePickerSize
{
    CGSize fittingSize = UILayoutFittingCompressedSize;
    UILayoutPriority horizontalPriority = UILayoutPriorityFittingSizeLevel;
    UILayoutPriority verticalPriority = UILayoutPriorityFittingSizeLevel;
    if ([_datePicker datePickerMode] != UIDatePickerModeTime)
        fittingSize.width = kDateTimePickerDefaultWidth;
    else {
        fittingSize.width = kDateTimePickerTimeControlWidth;
        fittingSize.height = kDateTimePickerTimeControlHeight;
        horizontalPriority = UILayoutPriorityRequired;
        verticalPriority = UILayoutPriorityRequired;
    }

    CGSize layoutSize = [_datePicker systemLayoutSizeFittingSize:fittingSize withHorizontalFittingPriority:horizontalPriority verticalFittingPriority:verticalPriority];
    return layoutSize;
}

- (CGSize)preferredContentSize
{
    // Cache the content size to workaround rdar://74749942.
    if (CGSizeEqualToSize(_contentSize, CGSizeZero)) {
        CGSize datePickerSize = [self preferredDatePickerSize];
        UIEdgeInsets datePickerInsets = [self datePickerInsets];

        _contentSize = CGSizeMake(datePickerSize.width + datePickerInsets.left + datePickerInsets.right, datePickerSize.height + kDateTimePickerToolbarHeight + datePickerInsets.top + datePickerInsets.bottom);
    }

    return _contentSize;
}

@end

@interface WKDateTimePicker : NSObject<WKFormControl
#if !HAVE(UIDATEPICKER_OVERLAY_PRESENTATION) && USE(UICONTEXTMENU)
, UIContextMenuInteractionDelegate
, WKDateTimePickerViewControllerDelegate
#endif
> {
    NSString *_formatString;
    RetainPtr<NSString> _initialValue;
    NSTimeInterval _initialValueAsNumber;
    BOOL _shouldRemoveTimeZoneInformation;
    WKContentView *_view;
    CGPoint _interactionPoint;
    RetainPtr<UIDatePicker> _datePicker;
#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    BOOL _isDismissingDatePicker;

    RetainPtr<_UIDatePickerOverlayPresentation> _datePickerPresentation;
    RetainPtr<UIToolbar> _accessoryView;
#elif USE(UICONTEXTMENU)
    RetainPtr<UIContextMenuInteraction> _dateTimeContextMenuInteraction;
    RetainPtr<WKDateTimePickerViewController> _dateTimePickerViewController;
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
static const NSTimeInterval kMillisecondsPerSecond = 1000;

static const CGFloat kDateTimePickerControlMargin = 6;

- (id)initWithView:(WKContentView *)view datePickerMode:(UIDatePickerMode)mode
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _interactionPoint = [_view lastInteractionLocation];
    _shouldRemoveTimeZoneInformation = NO;

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
        _shouldRemoveTimeZoneInformation = YES;
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
#elif USE(UICONTEXTMENU)
    _dateTimePickerViewController = adoptNS([[WKDateTimePickerViewController alloc] initWithDatePicker:_datePicker.get()]);
    [_dateTimePickerViewController setDelegate:self];
#endif

    return self;
}

#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)

- (void)handleDatePickerPresentationDismissal
{
    if (_isDismissingDatePicker)
        return;

    SetForScope<BOOL> isDismissingDatePicker { _isDismissingDatePicker, YES };
    [_view accessoryDone];
}

- (void)removeDatePickerPresentation
{
    if (_datePickerPresentation) {
        if (!_isDismissingDatePicker) {
            SetForScope<BOOL> isDismissingDatePicker { _isDismissingDatePicker, YES };
            [_datePickerPresentation dismissPresentationAnimated:NO];
        }

        _datePickerPresentation = nil;
        [_view.webView _didDismissContextMenu];
    }
}

#elif USE(UICONTEXTMENU)

- (UIEdgeInsets)_preferredEdgeInsetsForDateTimePicker
{
    CGSize pickerSize = [_dateTimePickerViewController preferredContentSize];
    CGRect windowBounds = _view.textEffectsWindow.bounds;
    CGRect elementFrameInWindowCoordinates = [_view convertRect:_view.focusedElementInformation.interactionRect toView:nil];

    // Attempt to present the date picker in a way that does not obscure the element.

    CGFloat topInsetForBottomAlignment = CGRectGetMaxY(elementFrameInWindowCoordinates) + kDateTimePickerControlMargin;
    CGFloat rightInsetForRightAlignment = CGRectGetWidth(windowBounds) - CGRectGetMaxX(elementFrameInWindowCoordinates);

    BOOL canPresentBelowElement = (topInsetForBottomAlignment + pickerSize.height) < CGRectGetHeight(windowBounds);
    BOOL canAlignToElementRight = (rightInsetForRightAlignment + pickerSize.width) < CGRectGetWidth(windowBounds);

    // Try to present the picker from the bottom right of the element.
    if (canPresentBelowElement && canAlignToElementRight)
        return UIEdgeInsetsMake(topInsetForBottomAlignment, 0, 0, rightInsetForRightAlignment);

    CGFloat leftInsetForLeftAlignment = CGRectGetMinX(elementFrameInWindowCoordinates);

    BOOL canAlignToElementLeft = (leftInsetForLeftAlignment + pickerSize.width) <= CGRectGetWidth(windowBounds);

    // Try to present the picker from the bottom left of the element.
    if (canPresentBelowElement && canAlignToElementLeft)
        return UIEdgeInsetsMake(topInsetForBottomAlignment, leftInsetForLeftAlignment, 0, 0);

    // Try to present the picker underneath the element.
    if (canPresentBelowElement)
        return UIEdgeInsetsMake(topInsetForBottomAlignment, 0, 0, 0);

    CGFloat bottomInsetForTopAlignment = CGRectGetHeight(windowBounds) - CGRectGetMinY(elementFrameInWindowCoordinates) + kDateTimePickerControlMargin;

    BOOL canPresentAboveElement = (bottomInsetForTopAlignment + pickerSize.height) < CGRectGetHeight(windowBounds);

    // Try to present the picker from the top right of the element.
    if (canPresentAboveElement && canAlignToElementRight)
        return UIEdgeInsetsMake(0, 0, bottomInsetForTopAlignment, rightInsetForRightAlignment);

    // Try to present the picker from the top left of the element.
    if (canPresentAboveElement && canAlignToElementLeft)
        return UIEdgeInsetsMake(0, leftInsetForLeftAlignment, bottomInsetForTopAlignment, 0);

    // Try to present the picker above the element.
    if (canPresentAboveElement)
        return UIEdgeInsetsMake(0, 0, bottomInsetForTopAlignment, 0);

    CGFloat rightInsetForPresentingBesideElementLeft = CGRectGetWidth(windowBounds) - CGRectGetMinX(elementFrameInWindowCoordinates) + kDateTimePickerControlMargin;
    BOOL canPresentBesideElementLeft = (rightInsetForPresentingBesideElementLeft + pickerSize.width) < CGRectGetWidth(windowBounds);

    // Try to present the picker to the left of the element.
    if (canPresentBesideElementLeft)
        return UIEdgeInsetsMake(0, 0, 0, rightInsetForPresentingBesideElementLeft);

    CGFloat leftInsetForPresentingBesideElementRight = CGRectGetMaxX(elementFrameInWindowCoordinates) + kDateTimePickerControlMargin;
    BOOL canPresentBesideElementRight = (leftInsetForPresentingBesideElementRight + pickerSize.width) < CGRectGetWidth(windowBounds);

    // Try to present the picker to the right of the element.
    if (canPresentBesideElementRight)
        return UIEdgeInsetsMake(0, leftInsetForPresentingBesideElementRight, 0, 0);

    // Present the picker from the center of the element.
    return UIEdgeInsetsZero;
}

- (UITargetedPreview *)contextMenuInteraction:(UIContextMenuInteraction *)interaction previewForHighlightingMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    return [_view _createTargetedContextMenuHintPreviewForFocusedElement];
}

- (_UIContextMenuStyle *)_contextMenuInteraction:(UIContextMenuInteraction *)interaction styleForMenuWithConfiguration:(UIContextMenuConfiguration *)configuration
{
    _UIContextMenuStyle *style = [_UIContextMenuStyle defaultStyle];
    style.hasInteractivePreview = YES;
    style.preferredBackgroundEffects = @[ [UIVisualEffect emptyEffect] ];
    style.preferredLayout = _UIContextMenuLayoutPreviewOnly;
#if HAVE(UICONTEXTMENU_STYLE_CUSTOM_PRESENTATION)
    style.prefersCenteredPreviewWhenActionsAreAbsent = NO;
    style.ignoresDefaultSizingRules = YES;
    style.preferredEdgeInsets = [self _preferredEdgeInsetsForDateTimePicker];
#endif
    return style;
}

- (UIContextMenuConfiguration *)contextMenuInteraction:(UIContextMenuInteraction *)interaction configurationForMenuAtLocation:(CGPoint)location
{
    return [UIContextMenuConfiguration configurationWithIdentifier:@"_UIDatePickerCompactEditor" previewProvider:^{
        return _dateTimePickerViewController.get();
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
        [_view _removeContextMenuHintContainerIfPossible];
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

- (void)dateTimePickerViewControllerDidPressResetButton:(WKDateTimePickerViewController *)dateTimePickerViewController
{
    [self reset:nil];
}

- (void)dateTimePickerViewControllerDidPressDoneButton:(WKDateTimePickerViewController *)dateTimePickerViewController
{
    [self done:nil];
}

#endif

- (void)showDateTimePicker
{
#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
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
#elif USE(UICONTEXTMENU) && HAVE(UICONTEXTMENU_LOCATION)
    [self ensureContextMenuInteraction];
    [_view presentContextMenu:_dateTimeContextMenuInteraction.get() atLocation:_interactionPoint];
#endif
}

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
#elif USE(UICONTEXTMENU)
    [_dateTimeContextMenuInteraction dismissMenu];
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
#elif USE(UICONTEXTMENU)
    [self removeContextMenuInteraction];
#endif
    [super dealloc];
}

- (NSInteger)_timeZoneOffsetFromGMT:(NSDate *)date
{
    if (!_shouldRemoveTimeZoneInformation)
        return 0;

    return [[_datePicker timeZone] secondsFromGMTForDate:date];
}

- (NSString *)_sanitizeInputValueForFormatter:(NSString *)value
{
    // The "time" input type may have seconds and milliseconds information which we
    // just ignore. For example: "01:56:20.391" is shortened to just "01:56".
    if (_view.focusedElementInformation.elementType == WebKit::InputType::Time)
        return [value substringToIndex:[kTimeFormatString length]];

    return value;
}

- (RetainPtr<NSDateFormatter>)dateFormatterForPicker
{
    RetainPtr<NSLocale> englishLocale = adoptNS([[NSLocale alloc] initWithLocaleIdentifier:@"en_US_POSIX"]);
    RetainPtr<NSDateFormatter> dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [dateFormatter setTimeZone:[_datePicker timeZone]];
    [dateFormatter setDateFormat:_formatString];
    [dateFormatter setLocale:englishLocale.get()];
    return dateFormatter;
}

- (void)_dateChangedSetAsNumber
{
    NSDate *date = [_datePicker date];
    [_view updateFocusedElementValueAsNumber:(date.timeIntervalSince1970 + [self _timeZoneOffsetFromGMT:date]) * kMillisecondsPerSecond];
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

- (void)setDateTimePickerToInitialValue
{
    if ([_initialValue isEqual: @""]) {
        [_datePicker setDate:[NSDate date]];
        [self _dateChanged];
    } else if (_formatString) {
        // Convert the string value to a date object for the fields where we have a format string.
        RetainPtr<NSDateFormatter> dateFormatter = [self dateFormatterForPicker];
        NSDate *parsedDate = [dateFormatter dateFromString:[self _sanitizeInputValueForFormatter:_initialValue.get()]];
        [_datePicker setDate:parsedDate ? parsedDate : [NSDate date]];
    } else {
        // Convert the number value to a date object for the fields affected by timezones.
        NSTimeInterval secondsSince1970 = _initialValueAsNumber / kMillisecondsPerSecond;
        NSInteger timeZoneOffset = [self _timeZoneOffsetFromGMT:[NSDate dateWithTimeIntervalSince1970:secondsSince1970]];
        NSTimeInterval adjustedSecondsSince1970 = secondsSince1970 - timeZoneOffset;
        [_datePicker setDate:[NSDate dateWithTimeIntervalSince1970:adjustedSecondsSince1970]];
    }
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
    _initialValueAsNumber = _view.focusedElementInformation.valueAsNumber;
    [self setDateTimePickerToInitialValue];

    [self showDateTimePicker];
}

- (void)controlEndEditing
{
    [_view stopRelinquishingFirstResponderToFocusedElement];

#if HAVE(UIDATEPICKER_OVERLAY_PRESENTATION)
    [self removeDatePickerPresentation];
#elif USE(UICONTEXTMENU)
    [self removeContextMenuInteraction];
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
