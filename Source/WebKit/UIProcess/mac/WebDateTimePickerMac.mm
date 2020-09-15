/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebDateTimePickerMac.h"

#if ENABLE(DATE_AND_TIME_INPUT_TYPES) && USE(APPKIT)

#import "AppKitSPI.h"
#import "WebPageProxy.h"

constexpr CGFloat kCalendarWidth = 139;
constexpr CGFloat kCalendarHeight = 148;
constexpr CGFloat kCalendarCornerRadius = 10;
constexpr NSString * kDateFormatString = @"yyyy-MM-dd";
constexpr NSString * kDateTimeFormatString = @"yyyy-MM-dd'T'HH:mm";
constexpr NSString * kDateTimeWithSecondsFormatString = @"yyyy-MM-dd'T'HH:mm:ss";
constexpr NSString * kDateTimeWithMillisecondsFormatString = @"yyyy-MM-dd'T'HH:mm:ss.SSS";
constexpr NSString * kDefaultLocaleIdentifier = @"en_US_POSIX";
constexpr NSString * kDefaultTimeZoneIdentifier = @"UTC";

@interface WKDateTimePicker : NSObject

- (id)initWithParams:(WebCore::DateTimeChooserParameters&&)params inView:(NSView *)view;
- (void)showPicker:(WebKit::WebDateTimePickerMac&)picker;
- (void)updatePicker:(WebCore::DateTimeChooserParameters&&)params;
- (void)invalidate;

@end

@interface WKDateTimePickerWindow : NSWindow
@end

@interface WKDateTimePickerBackdropView : NSView
@end

namespace WebKit {

Ref<WebDateTimePickerMac> WebDateTimePickerMac::create(WebPageProxy& page, NSView *view)
{
    return adoptRef(*new WebDateTimePickerMac(page, view));
}

WebDateTimePickerMac::~WebDateTimePickerMac()
{
    [m_picker invalidate];
}

WebDateTimePickerMac::WebDateTimePickerMac(WebPageProxy& page, NSView *view)
    : WebDateTimePicker(page)
    , m_view(view)
{
}

void WebDateTimePickerMac::endPicker()
{
    [m_picker invalidate];
    m_picker = nil;
    WebDateTimePicker::endPicker();
}

void WebDateTimePickerMac::showDateTimePicker(WebCore::DateTimeChooserParameters&& params)
{
    if (m_picker) {
        [m_picker updatePicker:WTFMove(params)];
        return;
    }

    m_picker = adoptNS([[WKDateTimePicker alloc] initWithParams:WTFMove(params) inView:m_view.get().get()]);
    [m_picker showPicker:*this];
}

void WebDateTimePickerMac::didChooseDate(StringView date)
{
    if (!m_page)
        return;

    m_page->didChooseDate(date);
}

} // namespace WebKit

@implementation WKDateTimePickerWindow {
    RetainPtr<WKDateTimePickerBackdropView> _backdropView;
}

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)styleMask backing:(NSBackingStoreType)backingStoreType defer:(BOOL)defer
{
    self = [super initWithContentRect:contentRect styleMask:styleMask backing:backingStoreType defer:defer];
    if (!self)
        return nil;

    self.hasShadow = YES;
    self.releasedWhenClosed = NO;
    self.titleVisibility = NSWindowTitleHidden;
    self.titlebarAppearsTransparent = YES;
    self.movable = NO;
    self.backgroundColor = [NSColor clearColor];
    self.opaque = NO;

    _backdropView = adoptNS([[WKDateTimePickerBackdropView alloc] initWithFrame:contentRect]);
    [_backdropView setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setContentView:_backdropView.get()];

    return self;
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)hasKeyAppearance
{
    return YES;
}

- (NSWindowShadowOptions)shadowOptions
{
    return NSWindowShadowSecondaryWindow;
}

@end

@implementation WKDateTimePickerBackdropView

- (void)drawRect:(NSRect)dirtyRect
{
    [NSGraphicsContext saveGraphicsState];

    [[NSColor controlBackgroundColor] setFill];

    NSRect frame = self.frame;
    NSPoint topLeft = NSMakePoint(NSMinX(frame), NSMaxY(frame));
    NSPoint topRight = NSMakePoint(NSMaxX(frame), NSMaxY(frame));
    NSPoint bottomRight = NSMakePoint(NSMaxX(frame), NSMinY(frame));
    NSPoint bottomLeft = NSMakePoint(NSMinX(frame), NSMinY(frame));

    NSBezierPath *path = [NSBezierPath bezierPath];
    [path moveToPoint:topLeft];
    [path lineToPoint:NSMakePoint(topRight.x - kCalendarCornerRadius, topRight.y)];
    [path curveToPoint:NSMakePoint(topRight.x, topRight.y - kCalendarCornerRadius) controlPoint1:topRight controlPoint2:topRight];
    [path lineToPoint:NSMakePoint(bottomRight.x, bottomRight.y + kCalendarCornerRadius)];
    [path curveToPoint:NSMakePoint(bottomRight.x - kCalendarCornerRadius, bottomRight.y) controlPoint1:bottomRight controlPoint2:bottomRight];
    [path lineToPoint:NSMakePoint(bottomLeft.x + kCalendarCornerRadius, bottomLeft.y)];
    [path curveToPoint:NSMakePoint(bottomLeft.x, bottomLeft.y + kCalendarCornerRadius) controlPoint1:bottomLeft controlPoint2:bottomLeft];
    [path lineToPoint:topLeft];

    [path fill];

    [NSGraphicsContext restoreGraphicsState];
}

@end

@implementation WKDateTimePicker {
    WeakPtr<WebKit::WebDateTimePickerMac> _picker;
    WebCore::DateTimeChooserParameters _params;
    WeakObjCPtr<NSView> _presentingView;

    RetainPtr<WKDateTimePickerWindow> _enclosingWindow;
    RetainPtr<NSDatePicker> _datePicker;
    RetainPtr<NSDateFormatter> _dateFormatter;
}

- (id)initWithParams:(WebCore::DateTimeChooserParameters&&)params inView:(NSView *)view
{
    if (!(self = [super init]))
        return self;

    _presentingView = view;

    NSRect windowRect = [[_presentingView window] convertRectToScreen:[_presentingView convertRect:params.anchorRectInRootView toView:nil]];
    windowRect.origin.y = NSMinY(windowRect) - kCalendarHeight;
    windowRect.size.width = kCalendarWidth;
    windowRect.size.height = kCalendarHeight;

    // Use a UTC timezone as all incoming double values are UTC timestamps. This also ensures that
    // the date value of the NSDatePicker matches the date value returned by JavaScript. The timezone
    // has no effect on the value returned to the WebProcess, as a timezone-agnostic format string is
    // used to return the date.
    NSTimeZone *timeZone = [NSTimeZone timeZoneWithName:kDefaultTimeZoneIdentifier];

    _enclosingWindow = adoptNS([[WKDateTimePickerWindow alloc] initWithContentRect:NSZeroRect styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [_enclosingWindow setFrame:windowRect display:YES];

    _datePicker = adoptNS([[NSDatePicker alloc] initWithFrame:[_enclosingWindow contentView].bounds]);
    [_datePicker setBezeled:NO];
    [_datePicker setDrawsBackground:NO];
    [_datePicker setDatePickerStyle:NSDatePickerStyleClockAndCalendar];
    [_datePicker setDatePickerElements:NSDatePickerElementFlagYearMonthDay];
    [_datePicker setTimeZone:timeZone];
    [_datePicker setTarget:self];
    [_datePicker setAction:@selector(didChooseDate:)];

    auto englishLocale = adoptNS([[NSLocale alloc] initWithLocaleIdentifier:kDefaultLocaleIdentifier]);
    _dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [_dateFormatter setLocale:englishLocale.get()];
    [_dateFormatter setTimeZone:timeZone];

    [self updatePicker:WTFMove(params)];

    return self;
}

- (void)showPicker:(WebKit::WebDateTimePickerMac&)picker
{
    _picker = makeWeakPtr(picker);

    [[_enclosingWindow contentView] addSubview:_datePicker.get()];
    [[_presentingView window] addChildWindow:_enclosingWindow.get() ordered:NSWindowAbove];
}

- (void)updatePicker:(WebCore::DateTimeChooserParameters&&)params
{
    _params = WTFMove(params);

    NSString *currentDateValueString = _params.currentValue;

    [_dateFormatter setDateFormat:[self dateFormatStringForType:_params.type value:currentDateValueString]];

    if (![currentDateValueString length])
        [_datePicker setDateValue:[self initialDateForEmptyValue]];
    else
        [_datePicker setDateValue:[_dateFormatter dateFromString:currentDateValueString]];

    [_datePicker setMinDate:[NSDate dateWithTimeIntervalSince1970:_params.minimum / 1000.0]];
    [_datePicker setMaxDate:[NSDate dateWithTimeIntervalSince1970:_params.maximum / 1000.0]];
}

- (void)invalidate
{
    [_datePicker removeFromSuperviewWithoutNeedingDisplay];
    [_datePicker setTarget:nil];
    [_datePicker setAction:nil];
    _datePicker = nil;

    _dateFormatter = nil;

    [[_presentingView window] removeChildWindow:_enclosingWindow.get()];
    [_enclosingWindow close];
    _enclosingWindow = nil;
}

- (void)didChooseDate:(id)sender
{
    if (sender != _datePicker)
        return;

    String dateString = [_dateFormatter stringFromDate:[_datePicker dateValue]];
    _picker->didChooseDate(StringView(dateString));
}

- (NSString *)dateFormatStringForType:(NSString *)type value:(NSString *)value
{
    if ([type isEqualToString:@"datetime-local"]) {
        // Add two additional characters for the string delimiters in 'T'.
        NSUInteger valueLengthForFormat = value.length + 2;
        if (valueLengthForFormat == kDateTimeFormatString.length)
            return kDateTimeFormatString;
        if (valueLengthForFormat == kDateTimeWithSecondsFormatString.length)
            return kDateTimeWithSecondsFormatString;
        return kDateTimeWithMillisecondsFormatString;
    }

    return kDateFormatString;
}

- (NSDate *)initialDateForEmptyValue
{
    NSDate *now = [NSDate date];
    NSTimeZone *defaultTimeZone = [NSTimeZone defaultTimeZone];
    NSInteger offset = [defaultTimeZone secondsFromGMTForDate:now];
    return [now dateByAddingTimeInterval:offset];
}

@end

#endif // ENABLE(DATE_AND_TIME_INPUT_TYPES) && USE(APPKIT)
