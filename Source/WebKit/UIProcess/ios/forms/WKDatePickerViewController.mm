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
#import "WKDatePickerViewController.h"

#if PLATFORM(WATCHOS)

#import <WebCore/LocalizedStrings.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/text/WTFString.h>

static const CGFloat pickerViewHorizontalMargin = 4;
static const CGFloat pickerViewBorderRadius = 6;
static const CGFloat pickerViewUnfocusedBorderWidth = 1;
static const CGFloat pickerViewFocusedBorderWidth = 2;
static const CGFloat pickerViewGranularityLabelBorderRadius = 8;
static const CGFloat pickerViewGranularityLabelFontSize = 10;
static const CGFloat pickerViewGranularityLabelHeight = 16;
static const CGFloat pickerViewGranularityLabelHorizontalPadding = 12;
static const CGFloat pickerViewGranularityLabelBottomMargin = 2;
static const CGFloat datePickerSetButtonTitleFontSize = 16;

static CGFloat datePickerSetButtonHeight()
{
    return [[UIDevice currentDevice] puic_deviceVariant] == PUICDeviceVariantCompact ? 32 : 40;
}

static CGFloat datePickerVerticalMargin()
{
    return [[UIDevice currentDevice] puic_deviceVariant] == PUICDeviceVariantCompact ? 10 : 14;
}

static NSString *pickerViewStandaloneYearFormat = @"y";
static NSString *pickerViewStandaloneDateFormat = @"dd";
static NSString *pickerViewStandaloneMonthFormat = @"MMM";
static NSString *inputValueDateFormat = @"yyyy-MM-dd";

#pragma mark - WKDatePickerWheelLabel

@interface WKDatePickerWheelLabel : UILabel

- (BOOL)needsUpdateForIndex:(NSUInteger)index selectedDate:(NSDate *)date;
@property (nonatomic) NSUInteger index;
@property (nonatomic, copy) NSDate *lastSelectedDate;

@end

@implementation WKDatePickerWheelLabel {
    RetainPtr<NSDate> _lastSelectedDate;
}

- (instancetype)initWithFrame:(CGRect)frame
{
    if (!(self = [super initWithFrame:frame]))
        return nil;

    self.index = NSNotFound;
    self.lastSelectedDate = nil;
    self.textColor = [UIColor whiteColor];
    self.textAlignment = NSTextAlignmentCenter;
    self.opaque = YES;
    self.adjustsFontSizeToFitWidth = YES;
    return self;
}

- (NSDate *)lastSelectedDate
{
    return _lastSelectedDate.get();
}

- (void)setLastSelectedDate:(NSDate *)date
{
    _lastSelectedDate = adoptNS([date copy]);
}

- (BOOL)needsUpdateForIndex:(NSUInteger)index selectedDate:(NSDate *)date
{
    return self.index != index || ![self.lastSelectedDate isEqualToDate:date];
}

@end

#pragma mark - WKDatePickerWheel

@interface WKDatePickerWheel : PUICPickerView
- (instancetype)initWithController:(WKDatePickerViewController *)controller style:(PUICPickerViewStyle)style NS_DESIGNATED_INITIALIZER;
@property (nonatomic) BOOL drawsFocusOutline;
@property (nonatomic, getter=isChangingValue) BOOL changingValue;
@end

@interface WKDatePickerViewController () <PUICPickerViewDataSource, PUICPickerViewDelegate>
@property (nonatomic, copy) NSDate *date;
@property (nonatomic, copy) NSDate *minimumDate;
@property (nonatomic, copy) NSDate *maximumDate;
- (void)didBeginInteractingWithDatePicker:(WKDatePickerWheel *)datePicker;
@end

@interface WKDatePickerWheel () <UIGestureRecognizerDelegate>
@end

@implementation WKDatePickerWheel {
    WeakObjCPtr<WKDatePickerViewController> _controller;
    RetainPtr<UILongPressGestureRecognizer> _gesture;
    BOOL _drawsFocusOutline;
}

- (instancetype)initWithStyle:(PUICPickerViewStyle)style
{
    return [self initWithController:nil style:style];
}

- (instancetype)initWithController:(WKDatePickerViewController *)controller style:(PUICPickerViewStyle)style
{
    if (!(self = [super initWithStyle:style]))
        return nil;

    self._continuousCornerRadius = pickerViewBorderRadius;
    self.layer.borderColor = [UIColor systemGrayColor].CGColor;
    self.layer.borderWidth = pickerViewUnfocusedBorderWidth;
    self.drawsFocusOutline = NO;
    self.changingValue = NO;

    _controller = controller;
    _gesture = adoptNS([[UILongPressGestureRecognizer alloc] initWithTarget:self action:@selector(gestureRecognized:)]);
    [_gesture setDelegate:self];
    [_gesture setMinimumPressDuration:0];
    [self addGestureRecognizer:_gesture.get()];

    return self;
}

- (void)gestureRecognized:(UIGestureRecognizer *)gesture
{
    if (gesture.state == UIGestureRecognizerStateBegan)
        [_controller didBeginInteractingWithDatePicker:self];
}

- (void)setDrawsFocusOutline:(BOOL)drawsFocusOutline
{
    if (_drawsFocusOutline == drawsFocusOutline)
        return;

    _drawsFocusOutline = drawsFocusOutline;
    self.layer.borderColor = (_drawsFocusOutline ? [UIColor systemGreenColor] : [UIColor systemGrayColor]).CGColor;
    self.layer.borderWidth = _drawsFocusOutline ? pickerViewFocusedBorderWidth : pickerViewUnfocusedBorderWidth;
}

- (BOOL)drawsFocusOutline
{
    return _drawsFocusOutline;
}

#pragma mark - UIGestureRecognizerDelegate

- (BOOL)gestureRecognizer:(UIGestureRecognizer *)gestureRecognizer shouldRecognizeSimultaneouslyWithGestureRecognizer:(UIGestureRecognizer *)otherGestureRecognizer
{
    return YES;
}

@end

struct EraAndYear {
    NSInteger era;
    NSInteger year;
};

@implementation WKDatePickerViewController {
    NSInteger _day;
    NSInteger _month;
    NSInteger _year;
    NSInteger _era;

    RetainPtr<WKDatePickerWheel> _monthPicker;
    RetainPtr<WKDatePickerWheel> _dayPicker;
    RetainPtr<WKDatePickerWheel> _yearAndEraPicker;
    RetainPtr<WKDatePickerWheel> _focusedPicker;
    RetainPtr<UILabel> _monthLabel;
    RetainPtr<UILabel> _dayLabel;
    RetainPtr<UILabel> _yearLabel;
    RetainPtr<UIButton> _setButton;

    RetainPtr<NSTimeZone> _timeZoneForDateFormatting;
    RetainPtr<NSCalendar> _calendar;

    RetainPtr<NSDate> _minimumDate;
    RetainPtr<NSDate> _maximumDate;
    RetainPtr<NSDate> _cachedDate;

    BOOL _displayCombinedEraAndYear;
    RetainPtr<NSDateFormatter> _standaloneYearFormatter;
    RetainPtr<NSDateFormatter> _standaloneDayFormatter;
    RetainPtr<NSDateFormatter> _standaloneMonthFormatter;
    RetainPtr<NSDateFormatter> _standaloneInputValueFormatter;

    RetainPtr<PUICStatusBarGlobalContextViewAssertion> _statusBarAssertion;
}

@dynamic delegate;

- (instancetype)initWithDelegate:(id <WKQuickboardViewControllerDelegate>)delegate
{
    return [super initWithDelegate:delegate];
}

- (void)viewDidLoad
{
    [super viewDidLoad];

    self.headerView.hidden = YES;

    NSString *localeIdentifier = [NSLocale currentLocale].localeIdentifier;
    _displayCombinedEraAndYear = [localeIdentifier containsString:@"calendar=japanese"] || [localeIdentifier isEqualToString:@"ja_JP_TRADITIONAL"];
    _timeZoneForDateFormatting = [NSTimeZone localTimeZone];
    _calendar = [NSCalendar currentCalendar];
    [_calendar setTimeZone:_timeZoneForDateFormatting.get()];

    _standaloneYearFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [_standaloneYearFormatter setDateFormat:pickerViewStandaloneYearFormat];
    [_standaloneYearFormatter setLocale:NSLocale.currentLocale];
    [_standaloneYearFormatter setCalendar:_calendar.get()];
    [_standaloneYearFormatter setTimeZone:_timeZoneForDateFormatting.get()];

    _standaloneDayFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [_standaloneDayFormatter setDateFormat:[NSDateFormatter dateFormatFromTemplate:pickerViewStandaloneDateFormat options:0 locale:NSLocale.currentLocale]];
    [_standaloneDayFormatter setCalendar:_calendar.get()];
    [_standaloneDayFormatter setTimeZone:_timeZoneForDateFormatting.get()];

    _standaloneMonthFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [_standaloneMonthFormatter setDateFormat:[NSDateFormatter dateFormatFromTemplate:pickerViewStandaloneMonthFormat options:0 locale:NSLocale.currentLocale]];
    [_standaloneMonthFormatter setCalendar:_calendar.get()];
    [_standaloneMonthFormatter setTimeZone:_timeZoneForDateFormatting.get()];

    _standaloneInputValueFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [_standaloneInputValueFormatter setDateFormat:inputValueDateFormat];
    [_standaloneInputValueFormatter setCalendar:[NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian]];
    [_standaloneInputValueFormatter setTimeZone:_timeZoneForDateFormatting.get()];

    self.minimumDate = self.defaultMinimumDate;
    self.maximumDate = self.defaultMaximumDate;
    self.date = [self _dateFromInitialText] ?: [NSDate date];

    _monthPicker = adoptNS([[WKDatePickerWheel alloc] initWithController:self style:PUICPickerViewStyleList]);
    [self _configurePickerView:_monthPicker.get()];
    _monthLabel = [self _createAndConfigureGranularityLabelWithText:WebCore::datePickerMonthLabelTitle()];

    _dayPicker = adoptNS([[WKDatePickerWheel alloc] initWithController:self style:PUICPickerViewStyleList]);
    [self _configurePickerView:_dayPicker.get()];
    _dayLabel = [self _createAndConfigureGranularityLabelWithText:WebCore::datePickerDayLabelTitle()];

    _yearAndEraPicker = adoptNS([[WKDatePickerWheel alloc] initWithController:self style:PUICPickerViewStyleList]);
    [self _configurePickerView:_yearAndEraPicker.get()];
    _yearLabel = [self _createAndConfigureGranularityLabelWithText:WebCore::datePickerYearLabelTitle()];

    [self _updateSelectedPickerViewIndices];

    [_dayPicker setDrawsFocusOutline:YES];
    [_dayLabel setHidden:NO];
    _focusedPicker = _dayPicker;

    _setButton = [UIButton buttonWithType:UIButtonTypeSystem];
    [_setButton setTitle:WebCore::datePickerSetButtonTitle() forState:UIControlStateNormal];
    [_setButton setTitleColor:[UIColor blackColor] forState:UIControlStateNormal];
    [_setButton setBackgroundColor:[UIColor systemGreenColor]];
    [_setButton _setContinuousCornerRadius:datePickerSetButtonHeight() / 2];
    [_setButton titleLabel].font = [UIFont systemFontOfSize:datePickerSetButtonTitleFontSize];
    [_setButton addTarget:self action:@selector(_setButtonPressed) forControlEvents:UIControlEventTouchUpInside];
    [self.contentView addSubview:_setButton.get()];

    self.view.opaque = YES;
    self.view.backgroundColor = [UIColor blackColor];
}

- (BOOL)prefersStatusBarHidden
{
    return NO;
}

- (void)viewWillAppear:(BOOL)animated
{
    [super viewWillAppear:animated];

    _statusBarAssertion = [[PUICApplication sharedPUICApplication] _takeStatusBarGlobalContextAssertionAnimated:NO];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_handleStatusBarNavigation) name:PUICStatusBarNavigationBackButtonPressedNotification object:nil];
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(_handleStatusBarNavigation) name:PUICStatusBarTitleTappedNotification object:nil];

    configureStatusBarForController(self, self.delegate);
}

- (void)viewDidAppear:(BOOL)animated
{
    [super viewDidAppear:animated];
    [self becomeFirstResponder];
}

- (void)viewDidDisappear:(BOOL)animated
{
    [super viewDidDisappear:animated];

    _statusBarAssertion = nil;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)_handleStatusBarNavigation
{
    [self.delegate quickboardInputCancelled:self];
}

- (void)viewWillLayoutSubviews
{
    [super viewWillLayoutSubviews];

    auto viewBounds = self.view.bounds;
    auto width = CGRectGetWidth(viewBounds);
    auto height = CGRectGetHeight(viewBounds);

    [_setButton setFrame:CGRectMake(0, height - datePickerSetButtonHeight(), width, datePickerSetButtonHeight())];

    [_monthLabel sizeToFit];
    [_dayLabel sizeToFit];
    [_yearLabel sizeToFit];

    CGRect pickerLayoutFrame = CGRectMake(0, CGRectGetMaxY(self.headerView.frame), width, CGRectGetMinY([_setButton frame]) - CGRectGetMaxY(self.headerView.frame));
    pickerLayoutFrame = UIRectInsetEdges(pickerLayoutFrame, UIRectEdgeTop | UIRectEdgeBottom, datePickerVerticalMargin());
    CGFloat pickerViewLayoutOriginY = CGRectGetMinY(pickerLayoutFrame) + pickerViewGranularityLabelHeight + pickerViewGranularityLabelBottomMargin;
    CGFloat pickerViewColumnWidth = CGRectGetWidth(pickerLayoutFrame) / 3;
    CGFloat pickerViewLayoutHeight = CGRectGetHeight(pickerLayoutFrame) -  pickerViewGranularityLabelHeight - pickerViewGranularityLabelBottomMargin;

    // FIXME: Respect locale and interface direction when mapping picker views and labels to left, middle and right positions.
    WKDatePickerWheel *leftPicker = _monthPicker.get();
    WKDatePickerWheel *middlePicker = _dayPicker.get();
    WKDatePickerWheel *rightPicker = _yearAndEraPicker.get();
    UILabel *leftLabel = _monthLabel.get();
    UILabel *middleLabel = _dayLabel.get();
    UILabel *rightLabel = _yearLabel.get();

    leftPicker.frame = UIRectInsetEdges(CGRectMake(0, pickerViewLayoutOriginY, pickerViewColumnWidth, pickerViewLayoutHeight), UIRectEdgeRight, pickerViewHorizontalMargin);
    leftLabel.frame = UIRectInsetEdges(CGRectMake(0, CGRectGetMinY(pickerLayoutFrame), CGRectGetWidth(leftLabel.bounds), pickerViewGranularityLabelHeight), UIRectEdgeRight, -pickerViewGranularityLabelHorizontalPadding);

    middlePicker.frame = UIRectInsetEdges(CGRectMake(pickerViewColumnWidth, pickerViewLayoutOriginY, pickerViewColumnWidth, pickerViewLayoutHeight), UIRectEdgeRight | UIRectEdgeLeft, pickerViewHorizontalMargin / 2);
    middleLabel.frame = UIRectInsetEdges(CGRectMake((CGRectGetWidth(pickerLayoutFrame) - CGRectGetWidth(middleLabel.bounds)) / 2, CGRectGetMinY(pickerLayoutFrame), CGRectGetWidth(middleLabel.bounds), pickerViewGranularityLabelHeight), UIRectEdgeRight | UIRectEdgeLeft, -pickerViewGranularityLabelHorizontalPadding / 2);

    rightPicker.frame = UIRectInsetEdges(CGRectMake(2 * pickerViewColumnWidth, pickerViewLayoutOriginY, pickerViewColumnWidth, pickerViewLayoutHeight), UIRectEdgeLeft, pickerViewHorizontalMargin);
    rightLabel.frame = UIRectInsetEdges(CGRectMake(CGRectGetWidth(pickerLayoutFrame) - CGRectGetWidth(rightLabel.bounds), CGRectGetMinY(pickerLayoutFrame), CGRectGetWidth(rightLabel.bounds), pickerViewGranularityLabelHeight), UIRectEdgeLeft, -pickerViewGranularityLabelHorizontalPadding);
}

- (BOOL)becomeFirstResponder
{
    return [_focusedPicker becomeFirstResponder];
}

- (NSDate *)defaultMinimumDate
{
    auto components = adoptNS([[NSDateComponents alloc] init]);
    [components setDay:1];
    [components setMonth:1];
    [components setYear:1];
    NSDate *minDateInGregorianCalendar = [[NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian] dateFromComponents:components.get()];
    NSDate *minDateInCurrentCalendar = nil;
    if (![_calendar rangeOfUnit:NSCalendarUnitYear startDate:&minDateInCurrentCalendar interval:nil forDate:minDateInGregorianCalendar]) {
        // Fall back to the Gregorian calendar date if the year containing the min date couldn't be computed using the current calendar.
        ASSERT_NOT_REACHED();
        return minDateInGregorianCalendar;
    }

    if ([_calendar compareDate:minDateInCurrentCalendar toDate:minDateInGregorianCalendar toUnitGranularity:NSCalendarUnitDay] == NSOrderedAscending)
        minDateInCurrentCalendar = [_calendar dateByAddingUnit:NSCalendarUnitYear value:1 toDate:minDateInCurrentCalendar options:0];

    return minDateInCurrentCalendar;
}

- (NSDate *)defaultMaximumDate
{
    auto components = adoptNS([[NSDateComponents alloc] init]);
    [components setDay:1];
    [components setMonth:1];
    [components setYear:10000];
    NSDate *dayAfterMaxDateInGregorianCalendar = [[NSCalendar calendarWithIdentifier:NSCalendarIdentifierGregorian] dateFromComponents:components.get()];
    NSDate *dayAfterMaxDateInCurrentCalendar = nil;
    NSDate *dayAfterMaxDate = nil;
    if ([_calendar rangeOfUnit:NSCalendarUnitYear startDate:&dayAfterMaxDateInCurrentCalendar interval:nil forDate:dayAfterMaxDateInGregorianCalendar])
        dayAfterMaxDate = dayAfterMaxDateInCurrentCalendar;
    else {
        // Fall back to the Gregorian calendar date if the year containing the max date couldn't be computed using the current calendar.
        ASSERT_NOT_REACHED();
        dayAfterMaxDate = dayAfterMaxDateInGregorianCalendar;
    }
    return [_calendar dateByAddingUnit:NSCalendarUnitDay value:-1 toDate:dayAfterMaxDate options:0];
}

- (NSString *)_valueForInput
{
    return [_standaloneInputValueFormatter stringFromDate:self.date];
}

- (NSDate *)_dateFromInitialText
{
    return [_standaloneInputValueFormatter dateFromString:[self.delegate initialValueForViewController:self]];
}

- (void)_setButtonPressed
{
    [self _canonicalizeAndUpdateSelectedDate];

    auto attributedStringValue = adoptNS([[NSAttributedString alloc] initWithString:[self _valueForInput]]);
    [self.delegate quickboard:self textEntered:attributedStringValue.get()];
}

- (void)_updateSelectedPickerViewIndices
{
    [_monthPicker setSelectedIndex:[self _indexFromMonth:_month]];
    [_dayPicker setSelectedIndex:[self _indexFromDay:_day]];
    [_yearAndEraPicker setSelectedIndex:[self _indexFromYear:_year era:_era]];
}

- (void)_configurePickerView:(WKDatePickerWheel *)picker
{
    picker.dataSource = self;
    picker.delegate = self;
    [self.contentView addSubview:picker];
}

- (void)setMinimumDate:(NSDate *)date
{
    if ([_minimumDate isEqualToDate:date])
        return;

    _minimumDate = adoptNS([date copy]);
    [_monthPicker reloadData];
    [_yearAndEraPicker reloadData];
    [_dayPicker reloadData];
}

- (NSDate *)minimumDate
{
    return _minimumDate.get();
}

- (void)setMaximumDate:(NSDate *)date
{
    if ([_minimumDate isEqualToDate:date])
        return;

    _maximumDate = adoptNS([date copy]);
    [_monthPicker reloadData];
    [_yearAndEraPicker reloadData];
    [_dayPicker reloadData];
}

- (NSDate *)maximumDate
{
    return _maximumDate.get();
}

- (void)setDate:(NSDate *)date
{
    if ([self.date isEqualToDate:date])
        return;

    if ([date compare:_minimumDate.get()] == NSOrderedAscending)
        date = _minimumDate.get();

    if ([date compare:_maximumDate.get()] == NSOrderedDescending)
        date = _maximumDate.get();

    [self setDateFromComponents:[_calendar components:NSCalendarUnitDay | NSCalendarUnitMonth | NSCalendarUnitYear | NSCalendarUnitEra fromDate:date]];
}

- (void)setDateFromComponents:(NSDateComponents *)components
{
    [self setDay:components.day month:components.month year:components.year era:components.era];
}

- (void)setDay:(NSInteger)day month:(NSInteger)month year:(NSInteger)year era:(NSInteger)era
{
    _day = day;
    _month = month;
    _year = year;
    _era = era;
    _cachedDate = nil;
}

- (NSDate *)date
{
    if (!_cachedDate)
        _cachedDate = [self _dateComponentForDay:_day month:_month year:_year era:_era].date;
    return _cachedDate.get();
}

- (NSDateComponents *)_dateComponentForDay:(NSInteger)day month:(NSInteger)month year:(NSInteger)year era:(NSInteger)era
{
    NSDateComponents *dateComponents = [[[NSDateComponents alloc] init] autorelease];
    dateComponents.day = day;
    dateComponents.month = month;
    dateComponents.year = year;
    dateComponents.era = era;
    dateComponents.calendar = _calendar.get();
    return dateComponents;
}

- (void)_adjustDateToValidDateIfNecessary
{
    NSDateComponents *adjustedComponents = [self _dateComponentForDay:_day month:_month year:_year era:_era];
    BOOL didAdjust = NO;
    while (![adjustedComponents isValidDateInCalendar:_calendar.get()]) {
        if (adjustedComponents.day <= 1) {
            ASSERT_NOT_REACHED();
            break;
        }
        --adjustedComponents.day;
        didAdjust = YES;
    }

    [self setDateFromComponents:adjustedComponents];

    if (didAdjust)
        [self _updateSelectedPickerViewIndices];
}

- (RetainPtr<UILabel>)_createAndConfigureGranularityLabelWithText:(NSString *)localizedText
{
    auto label = adoptNS([[UILabel alloc] init]);
    [label setText:localizedText];
    [label setTextAlignment:NSTextAlignmentCenter];
    [label setFont:[UIFont systemFontOfSize:pickerViewGranularityLabelFontSize weight:UIFontWeightHeavy]];
    [label layer].backgroundColor = [UIColor systemGreenColor].CGColor;
    [label _setContinuousCornerRadius:pickerViewGranularityLabelBorderRadius];
    [label setTextColor:[UIColor blackColor]];
    [label setHidden:YES];
    [self.contentView addSubview:label.get()];
    return label;
}

- (void)_canonicalizeAndUpdateSelectedDate
{
    auto eraAndYear = [self _eraAndYearFromIndex:[_yearAndEraPicker selectedIndex]];
    [self setDay:[self _dayFromIndex:[_dayPicker selectedIndex]] month:[self _monthFromIndex:[_monthPicker selectedIndex]] year:eraAndYear.year era:eraAndYear.era];
    [self _adjustDateToValidDateIfNecessary];
    [_dayPicker reloadData];
}

#pragma mark - PUICPickerViewDataSource

- (NSInteger)numberOfItemsInPickerView:(WKDatePickerWheel *)pickerView
{
    if (pickerView == _monthPicker)
        return [_calendar rangeOfUnit:NSCalendarUnitMonth inUnit:NSCalendarUnitYear forDate:self.date].length;

    if (pickerView == _dayPicker)
        return [_calendar rangeOfUnit:NSCalendarUnitDay inUnit:NSCalendarUnitMonth forDate:self.date].length;

    if (pickerView == _yearAndEraPicker) {
        NSDateComponents *yearAndEraComponents = [_calendar components:NSCalendarUnitYear | NSCalendarUnitEra fromDate:self.maximumDate];
        return 1 + [self _indexFromYear:yearAndEraComponents.year era:yearAndEraComponents.era];
    }

    return 0;
}

- (UIView *)pickerView:(WKDatePickerWheel *)pickerView viewForItemAtIndex:(NSInteger)index
{
    auto label = retainPtr((WKDatePickerWheelLabel *)[pickerView dequeueReusableItemView]) ?: adoptNS([[WKDatePickerWheelLabel alloc] init]);
    if (![label needsUpdateForIndex:index selectedDate:self.date])
        return label.autorelease();

    if (pickerView == _monthPicker) {
        NSDateComponents *dateComponentsAtIndex = [self _dateComponentForDay:NSNotFound month:[self _monthFromIndex:index] year:_year era:_era];
        [label setText:[_standaloneMonthFormatter stringFromDate:dateComponentsAtIndex.date].localizedUppercaseString];
    }

    if (pickerView == _dayPicker) {
        NSDateComponents *dateComponentsAtIndex = [self _dateComponentForDay:[self _dayFromIndex:index] month:_month year:_year era:_era];
        [label setText:[_standaloneDayFormatter stringFromDate:dateComponentsAtIndex.date]];
    }

    if (pickerView == _yearAndEraPicker) {
        auto eraAndYear = [self _eraAndYearFromIndex:index];
        NSDate *dateForEraAndYear = [self _dateComponentForDay:NSNotFound month:NSNotFound year:eraAndYear.year era:eraAndYear.era].date;
        NSString *eraPrefix = _displayCombinedEraAndYear ? [[[_calendar eraSymbols] objectAtIndex:eraAndYear.era] substringToIndex:1] : @"";
        [label setText:[eraPrefix stringByAppendingString:[_standaloneYearFormatter stringFromDate:dateForEraAndYear]]];
    }

    [label setIndex:index];
    [label setLastSelectedDate:self.date];
    return label.autorelease();
}

- (void)didBeginInteractingWithDatePicker:(WKDatePickerWheel *)datePicker
{
    if (datePicker == _focusedPicker)
        return;

    [_monthPicker setDrawsFocusOutline:datePicker == _monthPicker];
    [_monthLabel setHidden:![_monthPicker drawsFocusOutline]];
    [_dayPicker setDrawsFocusOutline:datePicker == _dayPicker];
    [_dayLabel setHidden:![_dayPicker drawsFocusOutline]];
    [_yearAndEraPicker setDrawsFocusOutline:datePicker == _yearAndEraPicker];
    [_yearLabel setHidden:![_yearAndEraPicker drawsFocusOutline]];

    _focusedPicker = datePicker;
    [self becomeFirstResponder];
}

- (void)pickerView:(WKDatePickerWheel *)pickerView didSelectItemAtIndex:(NSInteger)index
{
}

- (void)pickerViewWillBeginSelection:(WKDatePickerWheel *)pickerView
{
    if (pickerView == _monthPicker)
        [_monthPicker setChangingValue:YES];
    else if (pickerView == _yearAndEraPicker)
        [_yearAndEraPicker setChangingValue:YES];
    else if (pickerView == _dayPicker)
        [_dayPicker setChangingValue:YES];
}

- (void)pickerViewDidEndSelection:(WKDatePickerWheel *)pickerView
{
    if (pickerView == _monthPicker)
        [_monthPicker setChangingValue:NO];
    else if (pickerView == _yearAndEraPicker)
        [_yearAndEraPicker setChangingValue:NO];
    else if (pickerView == _dayPicker)
        [_dayPicker setChangingValue:NO];
    else if (![_monthPicker isChangingValue] && ![_yearAndEraPicker isChangingValue] && ![_dayPicker isChangingValue])
        [self _canonicalizeAndUpdateSelectedDate];
}

#pragma mark - Conversion between picker view indices and era, year, month, day

- (NSInteger)_dayFromIndex:(NSUInteger)dayIndex
{
    return dayIndex + 1;
}

- (EraAndYear)_eraAndYearFromIndex:(NSUInteger)yearIndex
{
    NSDate *dateAtYearIndex = [_calendar dateByAddingUnit:NSCalendarUnitYear value:yearIndex toDate:self.minimumDate options:0];
    NSDateComponents *components = [_calendar components:NSCalendarUnitYear | NSCalendarUnitEra fromDate:dateAtYearIndex];
    return { [components era], [components year] };
}

- (NSInteger)_monthFromIndex:(NSUInteger)monthIndex
{
    return monthIndex + 1;
}

- (NSUInteger)_indexFromDay:(NSInteger)day
{
    return day - 1;
}

- (NSUInteger)_indexFromYear:(NSInteger)year era:(NSInteger)era
{
    NSDate *dateForEraAndYear = [self _dateComponentForDay:NSNotFound month:NSNotFound year:year era:era].date;
    NSDateComponents *components = [_calendar components:NSCalendarUnitYear fromDate:self.minimumDate toDate:dateForEraAndYear options:0];
    return components.year;
}

- (NSUInteger)_indexFromMonth:(NSInteger)month
{
    return month - 1;
}

@end


#endif
