/*
 * Copyright (C) 2008-2017 Apple Inc. All rights reserved.
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
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "config.h"
#import "ThemeMac.h"

#if PLATFORM(MAC)

#import "AXObjectCache.h"
#import "ColorMac.h"
#import "ControlStates.h"
#import "GraphicsContext.h"
#import "GraphicsContextCG.h"
#import "ImageBuffer.h"
#import "LengthSize.h"
#import "LocalCurrentGraphicsContext.h"
#import "LocalDefaultSystemAppearance.h"
#import "ScrollView.h"
#import <pal/spi/cocoa/NSButtonCellSPI.h>
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <pal/spi/mac/NSViewSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>

static NSRect focusRingClipRect;
static BOOL themeWindowHasKeyAppearance;
static bool _useFormSemanticContext;

@interface WebCoreThemeWindow : NSWindow
@end

@interface WebCoreThemeView : NSControl
@end

@implementation WebCoreThemeWindow

- (BOOL)hasKeyAppearance
{
    return themeWindowHasKeyAppearance;
}

- (BOOL)isKeyWindow
{
    return themeWindowHasKeyAppearance;
}
@end

@implementation WebCoreThemeView {
    RetainPtr<WebCoreThemeWindow> _window;
}

- (instancetype)init
{
    if (!(self = [super init]))
        return nil;

    // Using defer:YES prevents us from wasting any window server resources for this window, since we're not actually
    // going to draw into it. The other arguments match what you get when calling -[NSWindow init].
    _window = adoptNS([[WebCoreThemeWindow alloc] initWithContentRect:NSMakeRect(100, 100, 100, 100) styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES]);

    return self;
}

- (NSWindow *)window
{
    return _window.get();
}

- (BOOL)isFlipped
{
    return YES;
}

- (NSText *)currentEditor
{
    return nil;
}

- (BOOL)_automaticFocusRingDisabled
{
    return YES;
}

- (NSRect)_focusRingVisibleRect
{
    if (NSIsEmptyRect(focusRingClipRect))
        return [self visibleRect];
    return focusRingClipRect;
}

- (NSView *)_focusRingClipAncestor
{
    return self;
}

- (NSWindow *)_viewRoot
{
    return _window.get();
}

- (void)addSubview:(NSView *)subview
{
    // By doing nothing in this method we forbid controls from adding subviews.
    // This tells AppKit to not use layer-backed animation for control rendering.
    UNUSED_PARAM(subview);
}

@end

// FIXME: Default buttons really should be more like push buttons and not like buttons.

namespace WebCore {

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};

Theme& Theme::singleton()
{
    static NeverDestroyed<ThemeMac> themeMac;
    return themeMac;
}

// Helper functions used by a bunch of different control parts.

static NSControlSize controlSizeForFont(const FontCascade& font)
{
    int fontSize = font.pixelSize();
    if (fontSize >= 21 && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= 16)
        return NSControlSizeRegular;
    if (fontSize >= 11)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static LengthSize sizeFromNSControlSize(NSControlSize nsControlSize, const LengthSize& zoomedSize, float zoomFactor, const std::array<IntSize, 4>& sizes)
{
    IntSize controlSize = sizes[nsControlSize];
    if (zoomFactor != 1.0f)
        controlSize = IntSize(controlSize.width() * zoomFactor, controlSize.height() * zoomFactor);
    LengthSize result = zoomedSize;
    if (zoomedSize.width.isIntrinsicOrAuto() && controlSize.width() > 0)
        result.width = { controlSize.width(), LengthType::Fixed };
    if (zoomedSize.height.isIntrinsicOrAuto() && controlSize.height() > 0)
        result.height = { controlSize.height(), LengthType::Fixed };
    return result;
}

static LengthSize sizeFromFont(const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor, const std::array<IntSize, 4>& sizes)
{
    return sizeFromNSControlSize(controlSizeForFont(font), zoomedSize, zoomFactor, sizes);
}

static NSControlSize controlSizeFromPixelSize(const std::array<IntSize, 4>& sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    if (ThemeMac::supportsLargeFormControls()
        && minZoomedSize.width() >= static_cast<int>(sizes[NSControlSizeLarge].width() * zoomFactor)
        && minZoomedSize.height() >= static_cast<int>(sizes[NSControlSizeLarge].height() * zoomFactor))
        return NSControlSizeLarge;
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSControlSizeRegular].width() * zoomFactor)
        && minZoomedSize.height() >= static_cast<int>(sizes[NSControlSizeRegular].height() * zoomFactor))
        return NSControlSizeRegular;
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSControlSizeSmall].width() * zoomFactor)
        && minZoomedSize.height() >= static_cast<int>(sizes[NSControlSizeSmall].height() * zoomFactor))
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static void setControlSize(NSCell* cell, const std::array<IntSize, 4>& sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    auto size = controlSizeFromPixelSize(sizes, minZoomedSize, zoomFactor);
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:size];
}

static void updateStates(NSCell* cell, const ControlStates& controlStates, bool useAnimation = false)
{
    // The animated state cause this thread to start and stop repeatedly on CoreAnimation synchronize calls.
    // This short burts of activity in between are not long enough for VoiceOver to retrieve accessibility attributes and makes the process appear unresponsive.
    if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
        useAnimation = false;
    
    auto states = controlStates.states();

    // Hover state is not supported by Aqua.
    
    // Pressed state
    bool oldPressed = [cell isHighlighted];
    bool pressed = states.contains(ControlStates::States::Pressed);
    if (pressed != oldPressed) {
        [(NSButtonCell*)cell _setHighlighted:pressed animated:useAnimation];
    }
    
    // Enabled state
    bool oldEnabled = [cell isEnabled];
    bool enabled = states.contains(ControlStates::States::Enabled);
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];

    // Checked and Indeterminate
    bool oldIndeterminate = [cell state] == NSControlStateValueMixed;
    bool indeterminate = states.contains(ControlStates::States::Indeterminate);
    bool checked = states.contains(ControlStates::States::Checked);
    bool oldChecked = [cell state] == NSControlStateValueOn;
    if (oldIndeterminate != indeterminate || checked != oldChecked) {
        NSControlStateValue newState = indeterminate ? NSControlStateValueMixed : (checked ? NSControlStateValueOn : NSControlStateValueOff);
        [(NSButtonCell*)cell _setState:newState animated:useAnimation];
    }

    // Presenting state
    if (states.contains(ControlStates::States::Presenting))
        [(NSButtonCell*)cell _setHighlighted:YES animated:NO];

    // Window inactive state does not need to be checked explicitly, since we paint parented to 
    // a view in a window whose key state can be detected.
}

static FloatRect inflateRect(const FloatRect& zoomedRect, const IntSize& zoomedSize, const int* margins, float zoomFactor)
{
    // Only do the inflation if the available width/height are too small.
    // Otherwise try to fit the glow/check space into the available box's width/height.
    int widthDelta = zoomedRect.width() - (zoomedSize.width() + margins[leftMargin] * zoomFactor + margins[rightMargin] * zoomFactor);
    int heightDelta = zoomedRect.height() - (zoomedSize.height() + margins[topMargin] * zoomFactor + margins[bottomMargin] * zoomFactor);
    FloatRect result(zoomedRect);
    if (widthDelta < 0) {
        result.setX(result.x() - margins[leftMargin] * zoomFactor);
        result.setWidth(result.width() - widthDelta);
    }
    if (heightDelta < 0) {
        result.setY(result.y() - margins[topMargin] * zoomFactor);
        result.setHeight(result.height() - heightDelta);
    }
    return result;
}

// Checkboxes and radio buttons

static const std::array<IntSize, 4>& checkboxSizes()
{
    static const std::array<IntSize, 4> sizes = { { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10), IntSize(16, 16) } };
    return sizes;
}

static const int* checkboxMargins(NSControlSize controlSize)
{
    static const int margins[4][4] =
    {
        // top right bottom left
        { 2, 2, 2, 2 },
        { 2, 1, 2, 1 },
        { 0, 0, 1, 0 },
        { 2, 2, 2, 2 },
    };
    return margins[controlSize];
}

static LengthSize checkboxSize(const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return zoomedSize;

    return sizeFromNSControlSize(NSControlSizeSmall, zoomedSize, zoomFactor, checkboxSizes());
}

// Radio Buttons

static const std::array<IntSize, 4>& radioSizes()
{
    static std::array<IntSize, 4> sizes;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        if (ThemeMac::supportsLargeFormControls()) {
            sizes = { { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10), IntSize(16, 16) } };
            return;
        }
        sizes = { { IntSize(16, 16), IntSize(12, 12), IntSize(10, 10), IntSize(0, 0) } };
    });
    return sizes;
}

static const int* radioMargins(NSControlSize controlSize)
{
    static const int margins[4][4] =
    {
        // top right bottom left
        { 1, 0, 1, 2 },
        { 1, 1, 2, 1 },
        { 0, 0, 1, 1 },
        { 1, 0, 1, 2 },
    };
    return margins[controlSize];
}

static LengthSize radioSize(const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return zoomedSize;

    return sizeFromNSControlSize(NSControlSizeSmall, zoomedSize, zoomFactor, radioSizes());
}
    
static void configureToggleButton(NSCell* cell, StyleAppearance appearance, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor, bool isStateChange)
{
    // Set the control size based off the rectangle we're painting into.
    setControlSize(cell, appearance == StyleAppearance::Checkbox ? checkboxSizes() : radioSizes(), zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states, isStateChange);
}
    
static RetainPtr<NSButtonCell> createToggleButtonCell(StyleAppearance appearance)
{
    RetainPtr<NSButtonCell> toggleButtonCell = adoptNS([[NSButtonCell alloc] init]);
    
    if (appearance == StyleAppearance::Checkbox) {
        [toggleButtonCell setButtonType:NSButtonTypeSwitch];
        [toggleButtonCell setAllowsMixedState:YES];
    } else {
        ASSERT(appearance == StyleAppearance::Radio);
        [toggleButtonCell setButtonType:NSButtonTypeRadio];
    }
    
    [toggleButtonCell setTitle:nil];
    [toggleButtonCell setFocusRingType:NSFocusRingTypeExterior];
    return toggleButtonCell;
}
    
static NSButtonCell *sharedRadioCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *radioCell = createToggleButtonCell(StyleAppearance::Radio).leakRef();

    configureToggleButton(radioCell, StyleAppearance::Radio, states, zoomedSize, zoomFactor, false);
    return radioCell;
}
    
static NSButtonCell *sharedCheckboxCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *checkboxCell = createToggleButtonCell(StyleAppearance::Checkbox).leakRef();

    configureToggleButton(checkboxCell, StyleAppearance::Checkbox, states, zoomedSize, zoomFactor, false);
    return checkboxCell;
}

// Buttons

// Buttons really only constrain height. They respect width.
static const std::array<IntSize, 4>& buttonSizes()
{
    static const std::array<IntSize, 4> sizes = { { IntSize(0, 20), IntSize(0, 16), IntSize(0, 13), IntSize(0, 28) } };
    return sizes;
}

static const int* buttonMargins(NSControlSize controlSize)
{
    // FIXME: These values may need to be reevaluated. They appear to have been originally chosen
    // to reflect the size of shadows around native form controls on macOS, but as of macOS 10.15,
    // these margins extend well past the boundaries of a native button cell's shadows.
    static const int margins[4][4] =
    {
        { 5, 7, 7, 7 },
        { 4, 6, 7, 6 },
        { 1, 2, 2, 2 },
        { 6, 6, 6, 6 },
    };
    return margins[controlSize];
}

enum ButtonCellType { NormalButtonCell, DefaultButtonCell };

static RetainPtr<NSButtonCell> buttonCell(ButtonCellType type)
{
    auto cell = adoptNS([[NSButtonCell alloc] init]);
    [cell setTitle:nil];
    [cell setButtonType:NSButtonTypeMomentaryPushIn];
    if (type == DefaultButtonCell)
        [cell setKeyEquivalent:@"\r"];
    return cell;
}

static void setUpButtonCell(NSButtonCell *cell, StyleAppearance appearance, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    // Set the control size based off the rectangle we're painting into.
    const std::array<IntSize, 4>& sizes = buttonSizes();
    switch (appearance) {
    case StyleAppearance::SquareButton:
        [cell setBezelStyle:NSBezelStyleShadowlessSquare];
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
        [cell setBezelStyle:NSBezelStyleTexturedSquare];
        break;
#endif
    default:
        auto largestControlSize = ThemeMac::supportsLargeFormControls() ? NSControlSizeLarge : NSControlSizeRegular;
        NSBezelStyle style = (zoomedSize.height() > buttonSizes()[largestControlSize].height() * zoomFactor) ? NSBezelStyleShadowlessSquare : NSBezelStyleRounded;
        [cell setBezelStyle:style];
        break;
    }

    setControlSize(cell, sizes, zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states);
}

static NSButtonCell *button(StyleAppearance appearance, const ControlStates& controlStates, const IntSize& zoomedSize, float zoomFactor)
{
    auto states = controlStates.states();
    NSButtonCell *cell;
    if (states.contains(ControlStates::States::Default)) {
        static NeverDestroyed<RetainPtr<NSButtonCell>> defaultCell = buttonCell(DefaultButtonCell);
        cell = defaultCell.get().get();
    } else {
        static NeverDestroyed<RetainPtr<NSButtonCell>> normalCell = buttonCell(NormalButtonCell);
        cell = normalCell.get().get();
    }
    setUpButtonCell(cell, appearance, controlStates, zoomedSize, zoomFactor);
    return cell;
}

// Stepper

static const std::array<IntSize, 4>& stepperSizes()
{
    static const std::array<IntSize, 4> sizes = { { IntSize(19, 27), IntSize(15, 22), IntSize(13, 15), IntSize(19, 27) } };
    return sizes;
}

// We don't use controlSizeForFont() for steppers because the stepper height
// should be equal to or less than the corresponding text field height,
static NSControlSize stepperControlSizeForFont(const FontCascade& font)
{
    int fontSize = font.pixelSize();
    if (fontSize >= 23 && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= 18)
        return NSControlSizeRegular;
    if (fontSize >= 13)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

// This will ensure that we always return a valid NSView, even if ScrollView doesn't have an associated document NSView.
// If the ScrollView doesn't have an NSView, we will return a fake NSView set up in the way AppKit expects.
NSView *ThemeMac::ensuredView(ScrollView* scrollView, const ControlStates& controlStates, bool useUnparentedView)
{
    if (!useUnparentedView) {
        if (NSView *documentView = scrollView->documentView())
            return documentView;
    }

    // Use a fake view.
    static WebCoreThemeView *themeView = [[WebCoreThemeView alloc] init];
    [themeView setFrameSize:NSSizeFromCGSize(scrollView->totalContentsSize())];
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [themeView setAppearance:[NSAppearance currentAppearance]];
    ALLOW_DEPRECATED_DECLARATIONS_END

#if USE(NSVIEW_SEMANTICCONTEXT)
    if (_useFormSemanticContext)
        [themeView _setSemanticContext:NSViewSemanticContextForm];
#endif

    themeWindowHasKeyAppearance = controlStates.states().contains(ControlStates::States::WindowActive);

    return themeView;
}

bool ThemeMac::useFormSemanticContext()
{
    return _useFormSemanticContext;
}

void ThemeMac::setUseFormSemanticContext(bool use)
{
    _useFormSemanticContext = use;
}

void ThemeMac::setFocusRingClipRect(const FloatRect& rect)
{
    focusRingClipRect = rect;
}

// Theme overrides

int ThemeMac::baselinePositionAdjustment(StyleAppearance appearance) const
{
    if (appearance == StyleAppearance::Checkbox || appearance == StyleAppearance::Radio)
        return -2;
    return Theme::baselinePositionAdjustment(appearance);
}

std::optional<FontCascadeDescription> ThemeMac::controlFont(StyleAppearance appearance, const FontCascade& font, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::PushButton: {
        FontCascadeDescription fontDescription;
        fontDescription.setIsAbsoluteSize(true);

        NSFont* nsFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSizeForFont(font)]];
        fontDescription.setOneFamily("-apple-system"_s);
        fontDescription.setComputedSize([nsFont pointSize] * zoomFactor);
        fontDescription.setSpecifiedSize([nsFont pointSize] * zoomFactor);
        return fontDescription;
    }
    default:
        return std::nullopt;
    }
}

LengthSize ThemeMac::controlSize(StyleAppearance appearance, const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::Checkbox:
        return checkboxSize(zoomedSize, zoomFactor);
    case StyleAppearance::Radio:
        return radioSize(zoomedSize, zoomFactor);
    case StyleAppearance::PushButton:
        // Height is reset to auto so that specified heights can be ignored.
        return sizeFromFont(font, { zoomedSize.width, { } }, zoomFactor, buttonSizes());
    case StyleAppearance::InnerSpinButton:
        if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
            return zoomedSize;
        return sizeFromNSControlSize(stepperControlSizeForFont(font), zoomedSize, zoomFactor, stepperSizes());
    default:
        return zoomedSize;
    }
}

LengthSize ThemeMac::minimumControlSize(StyleAppearance appearance, const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return { { 0, LengthType::Fixed }, { static_cast<int>(15 * zoomFactor), LengthType::Fixed } };
    case StyleAppearance::InnerSpinButton: {
        auto& base = stepperSizes()[NSControlSizeMini];
        return { { static_cast<int>(base.width() * zoomFactor), LengthType::Fixed },
            { static_cast<int>(base.height() * zoomFactor), LengthType::Fixed } };
    }
    default:
        return Theme::minimumControlSize(appearance, font, zoomedSize, zoomFactor);
    }
}

LengthBox ThemeMac::controlBorder(StyleAppearance appearance, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case StyleAppearance::ColorWell:
#endif
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return LengthBox(0, zoomedBox.right().value(), 0, zoomedBox.left().value());
    default:
        return Theme::controlBorder(appearance, font, zoomedBox, zoomFactor);
    }
}

LengthBox ThemeMac::controlPadding(StyleAppearance appearance, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::PushButton: {
        // Just use 8px. AppKit wants to use 11px for mini buttons, but that padding is just too large
        // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
        // by definition constrained, since we select mini only for small cramped environments).
        // This also guarantees the HTML <button> will match our rendering by default, since we're using
        // a consistent padding.
        int padding = 8 * zoomFactor;
        return LengthBox(2, padding, 3, padding);
    }
    default:
        return Theme::controlPadding(appearance, font, zoomedBox, zoomFactor);
    }
}

void ThemeMac::inflateControlPaintRect(StyleAppearance appearance, const ControlStates& states, FloatRect& zoomedRect, float zoomFactor) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    IntSize zoomRectSize = IntSize(zoomedRect.size());
    switch (appearance) {
    case StyleAppearance::Checkbox: {
        // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
        // shadow" and the check. We don't consider this part of the bounds of the control in WebKit.
        NSCell *cell = sharedCheckboxCell(states, zoomRectSize, zoomFactor);
        NSControlSize controlSize = [cell controlSize];
        IntSize zoomedSize = checkboxSizes()[controlSize];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, checkboxMargins(controlSize), zoomFactor);
        break;
    }
    case StyleAppearance::Radio: {
        // We inflate the rect as needed to account for padding included in the cell to accommodate the radio button
        // shadow". We don't consider this part of the bounds of the control in WebKit.
        NSCell *cell = sharedRadioCell(states, zoomRectSize, zoomFactor);
        NSControlSize controlSize = [cell controlSize];
        IntSize zoomedSize = radioSizes()[controlSize];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, radioMargins(controlSize), zoomFactor);
        break;
    }
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button: {
        NSButtonCell *cell = button(appearance, states, zoomRectSize, zoomFactor);
        NSControlSize controlSize = [cell controlSize];

        // We inflate the rect as needed to account for the Aqua button's shadow.
        if ([cell bezelStyle] == NSBezelStyleRounded) {
            IntSize zoomedSize = buttonSizes()[controlSize];
            zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
            zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
            zoomedRect = inflateRect(zoomedRect, zoomedSize, buttonMargins(controlSize), zoomFactor);
        }
        break;
    }
    case StyleAppearance::InnerSpinButton: {
        static const int stepperMargin[4] = { 0, 0, 0, 0 };
        auto controlSize = controlSizeFromPixelSize(stepperSizes(), zoomRectSize, zoomFactor);
        IntSize zoomedSize = stepperSizes()[controlSize];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, stepperMargin, zoomFactor);
        break;
    }
    default:
        break;
    }
    END_BLOCK_OBJC_EXCEPTIONS
}

bool ThemeMac::userPrefersReducedMotion() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}

bool ThemeMac::userPrefersContrast() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
}

bool ThemeMac::supportsLargeFormControls()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    static bool hasSupport = [[NSAppearance currentAppearance] _usesMetricsAppearance];
    ALLOW_DEPRECATED_DECLARATIONS_END
    return hasSupport;
}

}

#endif // PLATFORM(MAC)
