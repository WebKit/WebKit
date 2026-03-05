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

#if USE(APPKIT)

#import "AppKitSPI.h"
#import "WebPageProxy.h"
#import <WebCore/LocalizedStrings.h>

constexpr CGFloat kCalendarWidth = 139;
constexpr CGFloat kCalendarHeight = 148;
constexpr CGFloat kCalendarCornerRadius = 10;
constexpr CGFloat kWindowBorderSize = 0.5;
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

@interface WKEscapeHandlingDatePicker : NSDatePicker
- (void)setDateTimePicker:(WKDateTimePicker *)dateTimePicker;
- (RetainPtr<WKDateTimePicker>)dateTimePicker;
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
        [m_picker updatePicker:WTF::move(params)];
        return;
    }

    m_picker = adoptNS([[WKDateTimePicker alloc] initWithParams:WTF::move(params) inView:m_view.get().get()]);
    [m_picker showPicker:*this];
}

void WebDateTimePickerMac::didChooseDate(StringView date)
{
    if (RefPtr page = m_page.get())
        page->didChooseDate(date);
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

    NSRect rect = NSInsetRect(self.frame, kWindowBorderSize, 0);
    NSPoint topLeft = NSMakePoint(NSMinX(rect), NSMaxY(rect));
    NSPoint topRight = NSMakePoint(NSMaxX(rect), NSMaxY(rect));
    NSPoint bottomRight = NSMakePoint(NSMaxX(rect), NSMinY(rect));
    NSPoint bottomLeft = NSMakePoint(NSMinX(rect), NSMinY(rect));

    RetainPtr path = adoptNS([[NSBezierPath alloc] init]);
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
    RetainPtr<WKEscapeHandlingDatePicker> _datePicker;
    RetainPtr<NSDateFormatter> _dateFormatter;
}

- (id)initWithParams:(WebCore::DateTimeChooserParameters&&)params inView:(NSView *)view
{
    if (!(self = [super init]))
        return self;

    _presentingView = view;

    RetainPtr presentingView = _presentingView.get();

    NSRect windowRect = [retainPtr([presentingView window]) convertRectToScreen:[presentingView convertRect:params.anchorRectInRootView toView:nil]];
    windowRect.origin.y = NSMinY(windowRect) - kCalendarHeight;
    windowRect.size.width = kCalendarWidth;
    windowRect.size.height = kCalendarHeight;

    // Use a UTC timezone as all incoming double values are UTC timestamps. This also ensures that
    // the date value of the NSDatePicker matches the date value returned by JavaScript. The timezone
    // has no effect on the value returned to the WebProcess, as a timezone-agnostic format string is
    // used to return the date.
    RetainPtr timeZone = [NSTimeZone timeZoneWithName:kDefaultTimeZoneIdentifier];

    _enclosingWindow = adoptNS([[WKDateTimePickerWindow alloc] initWithContentRect:NSZeroRect styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:NO]);
    [_enclosingWindow setFrame:windowRect display:YES];
    [[_enclosingWindow contentView] setFocusRingType:NSFocusRingTypeNone];
    RetainPtr title = WEB_UI_NSSTRING(@"Date Picker Window Accessibility Title", "Base accessibility text for the window containing the date picker of <input type='date'>");
    [_enclosingWindow setAccessibilityTitle:title.get()];

    // Setting _setSharesParentFirstResponder is necessary because AppKit normally disallows
    // a view from one window (in our case, _datePicker belonging to _enclosingWindow) to be
    // the first responder for a different window ([_presentingView window]). However, we need
    // this behavior for seamless keyboard navigation into and out of the date picker, so inform
    // AppKit that we explicitly do want to share first responders across windows.
    RetainPtr presentingWindow = [presentingView window];
    BOOL presentingWindowCanBeKey = [presentingWindow isKeyWindow] || [presentingWindow canBecomeKeyWindow];
    [_enclosingWindow _setSharesParentFirstResponder:presentingWindowCanBeKey];

    _datePicker = adoptNS([[WKEscapeHandlingDatePicker alloc] initWithFrame:[_enclosingWindow contentView].bounds]);
    [_datePicker setDateTimePicker:self];
    [_datePicker setBezeled:NO];
    [_datePicker setDrawsBackground:NO];
    [_datePicker setDatePickerStyle:NSDatePickerStyleClockAndCalendar];
    [_datePicker setDatePickerElements:NSDatePickerElementFlagYearMonthDay];
    [_datePicker setTimeZone:timeZone.get()];
    [_datePicker setTarget:self];
    [_datePicker setAction:@selector(didChooseDate:)];
    // Don't draw a focus ring around the entire calendar view as a focus indicator is inherently rendered
    // inside the calendar, e.g. on the currently focused day.
    [_datePicker setFocusRingType:NSFocusRingTypeNone];

    auto englishLocale = adoptNS([[NSLocale alloc] initWithLocaleIdentifier:kDefaultLocaleIdentifier]);
    _dateFormatter = adoptNS([[NSDateFormatter alloc] init]);
    [_dateFormatter setLocale:englishLocale.get()];
    [_dateFormatter setTimeZone:timeZone.get()];

    [self updatePicker:WTF::move(params)];

    return self;
}

- (void)showPicker:(WebKit::WebDateTimePickerMac&)picker
{
    _picker = picker;

    [retainPtr([_enclosingWindow contentView]) addSubview:_datePicker.get()];
    RetainPtr window = [_presentingView.get() window];
    [window addChildWindow:_enclosingWindow.get() ordered:NSWindowAbove];

    if (_params.wasActivatedByKeyboard) {
        // Make the date picker first responder to enable keyboard interaction.
        [window makeFirstResponder:_datePicker.get()];
    }
}

- (void)updatePicker:(WebCore::DateTimeChooserParameters&&)params
{
    _params = WTF::move(params);

    RetainPtr currentDateValueString = _params.currentValue.createNSString();

    RetainPtr<NSString> format = [self dateFormatStringForType:_params.type.createNSString().get()];
    [_dateFormatter setDateFormat:format.get()];

    if (![currentDateValueString length])
        [_datePicker setDateValue:[self initialDateForEmptyValue]];
    else {
        RetainPtr dateValue = [_dateFormatter dateFromString:currentDateValueString.get()];

        while (!dateValue && (format = [self dateFormatFallbackForFormat:format.get()])) {
            [_dateFormatter setDateFormat:format.get()];
            dateValue = [_dateFormatter dateFromString:currentDateValueString.get()];
        }

        [_datePicker setDateValue:dateValue.get()];
    }

    [_datePicker setMinDate:[NSDate dateWithTimeIntervalSince1970:_params.minimum / 1000.0]];
    [_datePicker setMaxDate:[NSDate dateWithTimeIntervalSince1970:_params.maximum / 1000.0]];

    [_enclosingWindow setAppearance:[NSAppearance appearanceNamed:_params.useDarkAppearance ? NSAppearanceNameDarkAqua : NSAppearanceNameAqua]];

    if (_params.wasActivatedByKeyboard)
        [retainPtr([_presentingView.get() window]) makeFirstResponder:_datePicker.get()];
}

- (void)invalidate
{
    [_datePicker removeFromSuperviewWithoutNeedingDisplay];
    [_datePicker setTarget:nil];
    [_datePicker setAction:nil];
    [_datePicker setDateTimePicker:nil];

    RetainPtr presentingView = _presentingView.get();
    RetainPtr window = [presentingView window];
    if ([window firstResponder] == _datePicker.get()) {
        // If the date picker was the first responder, restore first-respondership
        // to the webview so the user doesn't have to click on the webpage to
        // start moving focus with the keyboard inside web content.
        [window makeFirstResponder:_presentingView.get().get()];
    }

    _datePicker = nil;
    _dateFormatter = nil;

    [window removeChildWindow:_enclosingWindow.get()];
    [_enclosingWindow close];
    _enclosingWindow = nil;
}

- (void)handleEscapeKey
{
    if (RefPtr picker = _picker.get())
        picker->endPicker();
}

- (void)didChooseDate:(id)sender
{
    if (sender != _datePicker)
        return;

    String dateString = [_dateFormatter stringFromDate:retainPtr([_datePicker dateValue]).get()];
    Ref { *_picker }->didChooseDate(StringView(dateString));

    if (_params.wasActivatedByKeyboard) {
        // Choosing a date causes the backing <input> to gain focus, in turn calling
        // Document::setFocusedElement, and eventually WebChromeClient::makeFirstResponder(),
        // which steals first-respondership from our date picker. The act of choosing a date
        // with the keyboard does not dismiss the date picker, so we need to make sure it regains
        // first respondership in case the user wishes to continue interacting with it.
        [retainPtr([_presentingView.get() window]) makeFirstResponder:_datePicker.get()];
    }
}

- (NSString *)dateFormatStringForType:(NSString *)type
{
    if ([type isEqualToString:@"datetime-local"]) {
        if (_params.hasMillisecondField)
            return kDateTimeWithMillisecondsFormatString;
        if (_params.hasSecondField)
            return kDateTimeWithSecondsFormatString;
        return kDateTimeFormatString;
    }

    return kDateFormatString;
}

- (NSString *)dateFormatFallbackForFormat:(NSString *)format
{
    if ([format isEqualToString:kDateTimeWithMillisecondsFormatString])
        return kDateTimeWithSecondsFormatString;
    if ([format isEqualToString:kDateTimeWithSecondsFormatString])
        return kDateTimeFormatString;

    return nil;
}

- (NSDate *)initialDateForEmptyValue
{
    RetainPtr now = adoptNS([[NSDate alloc] init]);
    RetainPtr defaultTimeZone = [NSTimeZone defaultTimeZone];
    NSInteger offset = [defaultTimeZone secondsFromGMTForDate:now.get()];
    return [now dateByAddingTimeInterval:offset];
}

- (BOOL)wasActivatedByKeyboard
{
    return _params.wasActivatedByKeyboard;
}

@end

@implementation WKEscapeHandlingDatePicker {
    WeakObjCPtr<WKDateTimePicker> _dateTimePicker;
}

- (void)setDateTimePicker:(WKDateTimePicker *)dateTimePicker
{
    _dateTimePicker = dateTimePicker;
}

- (RetainPtr<WKDateTimePicker>)dateTimePicker
{
    return _dateTimePicker.get();
}

- (void)keyDown:(NSEvent *)event
{
    if (event.keyCode == 53) {
        // keyCode 53 is the escape key.
        [self.dateTimePicker handleEscapeKey];
        return;
    }
    [super keyDown:event];
}

- (BOOL)acceptsFirstResponder
{
    return [[self dateTimePicker] wasActivatedByKeyboard];
}

@end

#endif // USE(APPKIT)
