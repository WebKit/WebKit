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
#import <Carbon/Carbon.h>
#import <pal/spi/cocoa/NSButtonCellSPI.h>
#import <pal/spi/mac/NSGraphicsSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>

static NSRect focusRingClipRect;
static BOOL themeWindowHasKeyAppearance;

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

@implementation WebCoreThemeView

- (NSWindow *)window
{
    // Using defer:YES prevents us from wasting any window server resources for this window, since we're not actually
    // going to draw into it. The other arguments match what you get when calling -[NSWindow init].
    static WebCoreThemeWindow *window = [[WebCoreThemeWindow alloc] initWithContentRect:NSMakeRect(100, 100, 100, 100)
        styleMask:NSWindowStyleMaskTitled backing:NSBackingStoreBuffered defer:YES];
    return window;
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
    if (fontSize >= 16)
        return NSControlSizeRegular;
    if (fontSize >= 11)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static LengthSize sizeFromNSControlSize(NSControlSize nsControlSize, const LengthSize& zoomedSize, float zoomFactor, const std::array<IntSize, 3>& sizes)
{
    IntSize controlSize = sizes[nsControlSize];
    if (zoomFactor != 1.0f)
        controlSize = IntSize(controlSize.width() * zoomFactor, controlSize.height() * zoomFactor);
    LengthSize result = zoomedSize;
    if (zoomedSize.width.isIntrinsicOrAuto() && controlSize.width() > 0)
        result.width = { controlSize.width(), Fixed };
    if (zoomedSize.height.isIntrinsicOrAuto() && controlSize.height() > 0)
        result.height = { controlSize.height(), Fixed };
    return result;
}

static LengthSize sizeFromFont(const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor, const std::array<IntSize, 3>& sizes)
{
    return sizeFromNSControlSize(controlSizeForFont(font), zoomedSize, zoomFactor, sizes);
}

static ControlSize controlSizeFromPixelSize(const std::array<IntSize, 3>& sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSControlSizeRegular].width() * zoomFactor)
        && minZoomedSize.height() >= static_cast<int>(sizes[NSControlSizeRegular].height() * zoomFactor))
        return NSControlSizeRegular;
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSControlSizeSmall].width() * zoomFactor)
        && minZoomedSize.height() >= static_cast<int>(sizes[NSControlSizeSmall].height() * zoomFactor))
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static void setControlSize(NSCell* cell, const std::array<IntSize, 3>& sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    ControlSize size = controlSizeFromPixelSize(sizes, minZoomedSize, zoomFactor);
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:(NSControlSize)size];
}

static void updateStates(NSCell* cell, const ControlStates& controlStates, bool useAnimation = false)
{
    // The animated state cause this thread to start and stop repeatedly on CoreAnimation synchronize calls.
    // This short burts of activity in between are not long enough for VoiceOver to retrieve accessibility attributes and makes the process appear unresponsive.
    if (AXObjectCache::accessibilityEnhancedUserInterfaceEnabled())
        useAnimation = false;
    
    ControlStates::States states = controlStates.states();

    // Hover state is not supported by Aqua.
    
    // Pressed state
    bool oldPressed = [cell isHighlighted];
    bool pressed = states & ControlStates::PressedState;
    if (pressed != oldPressed) {
        [(NSButtonCell*)cell _setHighlighted:pressed animated:useAnimation];
    }
    
    // Enabled state
    bool oldEnabled = [cell isEnabled];
    bool enabled = states & ControlStates::EnabledState;
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];

    // Checked and Indeterminate
    bool oldIndeterminate = [cell state] == NSControlStateValueMixed;
    bool indeterminate = (states & ControlStates::IndeterminateState);
    bool checked = states & ControlStates::CheckedState;
    bool oldChecked = [cell state] == NSControlStateValueOn;
    if (oldIndeterminate != indeterminate || checked != oldChecked) {
        NSControlStateValue newState = indeterminate ? NSControlStateValueMixed : (checked ? NSControlStateValueOn : NSControlStateValueOff);
        [(NSButtonCell*)cell _setState:newState animated:useAnimation];
    }

    // Presenting state
    if (states & ControlStates::PresentingState)
        [(NSButtonCell*)cell _setHighlighted:YES animated:NO];

    // Window inactive state does not need to be checked explicitly, since we paint parented to 
    // a view in a window whose key state can be detected.
}

static ThemeDrawState convertControlStatesToThemeDrawState(ThemeButtonKind kind, const ControlStates& controlStates)
{
    ControlStates::States states = controlStates.states();

    if (!(states & ControlStates::EnabledState))
        return kThemeStateUnavailableInactive;

    // Do not process PressedState if !EnabledState.
    if (states & ControlStates::PressedState) {
        if (kind == kThemeIncDecButton || kind == kThemeIncDecButtonSmall || kind == kThemeIncDecButtonMini)
            return states & ControlStates::SpinUpState ? kThemeStatePressedUp : kThemeStatePressedDown;
        return kThemeStatePressed;
    }
    return kThemeStateActive;
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

static const std::array<IntSize, 3>& checkboxSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10) } };
    return sizes;
}

static const int* checkboxMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        // top right bottom left
        { 2, 2, 2, 2 },
        { 2, 1, 2, 1 },
        { 0, 0, 1, 0 },
    };
    return margins[controlSize];
}

static LengthSize checkboxSize(const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, checkboxSizes());
}

// Radio Buttons

static const std::array<IntSize, 3>& radioSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(16, 16), IntSize(12, 12), IntSize(10, 10) } };
    return sizes;
}

static const int* radioMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        // top right bottom left
        { 1, 0, 1, 2 },
        { 1, 1, 2, 1 },
        { 0, 0, 1, 1 },
    };
    return margins[controlSize];
}

static LengthSize radioSize(const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, radioSizes());
}
    
static void configureToggleButton(NSCell* cell, ControlPart buttonType, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor, bool isStateChange)
{
    // Set the control size based off the rectangle we're painting into.
    setControlSize(cell, buttonType == CheckboxPart ? checkboxSizes() : radioSizes(), zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states, isStateChange);
}
    
static RetainPtr<NSButtonCell> createToggleButtonCell(ControlPart buttonType)
{
    RetainPtr<NSButtonCell> toggleButtonCell = adoptNS([[NSButtonCell alloc] init]);
    
    if (buttonType == CheckboxPart) {
        [toggleButtonCell setButtonType:NSButtonTypeSwitch];
        [toggleButtonCell setAllowsMixedState:YES];
    } else {
        ASSERT(buttonType == RadioPart);
        [toggleButtonCell setButtonType:NSButtonTypeRadio];
    }
    
    [toggleButtonCell setTitle:nil];
    [toggleButtonCell setFocusRingType:NSFocusRingTypeExterior];
    return toggleButtonCell;
}
    
static NSButtonCell *sharedRadioCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *radioCell = createToggleButtonCell(RadioPart).leakRef();

    configureToggleButton(radioCell, RadioPart, states, zoomedSize, zoomFactor, false);
    return radioCell;
}
    
static NSButtonCell *sharedCheckboxCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *checkboxCell = createToggleButtonCell(CheckboxPart).leakRef();

    configureToggleButton(checkboxCell, CheckboxPart, states, zoomedSize, zoomFactor, false);
    return checkboxCell;
}

static bool drawCellFocusRingWithFrameAtTime(NSCell *cell, NSRect cellFrame, NSView *controlView, NSTimeInterval timeOffset)
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    CGContextRef cgContext = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
    ALLOW_DEPRECATED_DECLARATIONS_END

    CGContextStateSaver stateSaver(cgContext);

    CGFocusRingStyle focusRingStyle;
    bool needsRepaint = NSInitializeCGFocusRingStyleForTime(NSFocusRingOnly, &focusRingStyle, timeOffset);

    // We want to respect the CGContext clipping and also not overpaint any
    // existing focus ring. The way to do this is set accumulate to
    // -1. According to CoreGraphics, the reasoning for this behavior has been
    // lost in time.
    focusRingStyle.accumulate = -1;

    // FIXME: This color should be shared with RenderThemeMac. For now just use the same NSColor color.
    // The color is expected to be opaque, since CoreGraphics will apply opacity when drawing (because opacity is normally animated).
    auto color = colorWithOverrideAlpha(colorFromNSColor([NSColor keyboardFocusIndicatorColor]).rgb(), 1);
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color)));
    CGContextSetStyle(cgContext, style.get());

    CGContextBeginTransparencyLayerWithRect(cgContext, NSRectToCGRect(cellFrame), nullptr);
    [cell drawFocusRingMaskWithFrame:cellFrame inView:controlView];
    CGContextEndTransparencyLayer(cgContext);

    return needsRepaint;
}

static bool drawCellFocusRing(NSCell *cell, NSRect cellFrame, NSView *controlView)
{
    drawCellFocusRingWithFrameAtTime(cell, cellFrame, controlView, std::numeric_limits<double>::max());
    return false;
}

static void paintToggleButton(ControlPart buttonType, ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    RetainPtr<NSButtonCell> toggleButtonCell = static_cast<NSButtonCell *>(controlStates.platformControl());
    IntSize zoomedRectSize = IntSize(zoomedRect.size());

    if (controlStates.isDirty()) {
        if (!toggleButtonCell)
            toggleButtonCell = createToggleButtonCell(buttonType);
        configureToggleButton(toggleButtonCell.get(), buttonType, controlStates, zoomedRectSize, zoomFactor, true);
    } else {
        if (!toggleButtonCell) {
            if (buttonType == CheckboxPart)
                toggleButtonCell = sharedCheckboxCell(controlStates, zoomedRectSize, zoomFactor);
            else {
                ASSERT(buttonType == RadioPart);
                toggleButtonCell = sharedRadioCell(controlStates, zoomedRectSize, zoomFactor);
            }
        }
        configureToggleButton(toggleButtonCell.get(), buttonType, controlStates, zoomedRectSize, zoomFactor, false);
    }
    controlStates.setDirty(false);

    GraphicsContextStateSaver stateSaver(context);

    NSControlSize controlSize = [toggleButtonCell controlSize];
    IntSize zoomedSize = buttonType == CheckboxPart ? checkboxSizes()[controlSize] : radioSizes()[controlSize];
    zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    const int* controlMargins = buttonType == CheckboxPart ? checkboxMargins(controlSize) : radioMargins(controlSize);
    FloatRect inflatedRect = inflateRect(zoomedRect, zoomedSize, controlMargins, zoomFactor);

    if (zoomFactor != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
        inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
        context.translate(inflatedRect.location());
        context.scale(zoomFactor);
        context.translate(-inflatedRect.location());
    }
    LocalCurrentGraphicsContext localContext(context);

    NSView *view = ThemeMac::ensuredView(scrollView, controlStates, true /* useUnparentedView */);

    bool needsRepaint = false;
    bool useImageBuffer = pageScaleFactor != 1.0f || zoomFactor != 1.0f;
    bool isCellFocused = controlStates.states() & ControlStates::FocusState;

    if ([toggleButtonCell _stateAnimationRunning]) {
        context.translate(inflatedRect.location());
        context.scale(FloatSize(1, -1));
        context.translate(0, -inflatedRect.height());

        [toggleButtonCell _renderCurrentAnimationFrameInContext:context.platformContext() atLocation:NSMakePoint(0, 0)];
        if (![toggleButtonCell _stateAnimationRunning] && isCellFocused)
            needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(toggleButtonCell.get(), context, inflatedRect, view, false, true, useImageBuffer, deviceScaleFactor);
    } else
        needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(toggleButtonCell.get(), context, inflatedRect, view, true, isCellFocused, useImageBuffer, deviceScaleFactor);

    [toggleButtonCell setControlView:nil];

    needsRepaint |= [toggleButtonCell _stateAnimationRunning];
    controlStates.setNeedsRepaint(needsRepaint);
    if (needsRepaint)
        controlStates.setPlatformControl(toggleButtonCell.get());

    END_BLOCK_OBJC_EXCEPTIONS
}

// Buttons

// Buttons really only constrain height. They respect width.
static const std::array<IntSize, 3>& buttonSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) } };
    return sizes;
}

static const int* buttonMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        { 4, 6, 7, 6 },
        { 4, 5, 6, 5 },
        { 0, 1, 1, 1 },
    };
    return margins[controlSize];
}

enum ButtonCellType { NormalButtonCell, DefaultButtonCell };

static NSButtonCell *leakButtonCell(ButtonCellType type)
{
    NSButtonCell *cell = [[NSButtonCell alloc] init];
    [cell setTitle:nil];
    [cell setButtonType:NSButtonTypeMomentaryPushIn];
    if (type == DefaultButtonCell)
        [cell setKeyEquivalent:@"\r"];
    return cell;
}

static void setUpButtonCell(NSButtonCell *cell, ControlPart part, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    // Set the control size based off the rectangle we're painting into.
    const std::array<IntSize, 3>& sizes = buttonSizes();
    switch (part) {
    case SquareButtonPart:
        [cell setBezelStyle:NSBezelStyleShadowlessSquare];
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
        [cell setBezelStyle:NSBezelStyleTexturedSquare];
        break;
#endif
    default:
        NSBezelStyle style = (zoomedSize.height() > buttonSizes()[NSControlSizeRegular].height() * zoomFactor) ? NSBezelStyleShadowlessSquare : NSBezelStyleRounded;
        [cell setBezelStyle:style];
        break;
    }

    setControlSize(cell, sizes, zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states);
}

static NSButtonCell *button(ControlPart part, const ControlStates& controlStates, const IntSize& zoomedSize, float zoomFactor)
{
    ControlStates::States states = controlStates.states();
    NSButtonCell *cell;
    if (states & ControlStates::DefaultState) {
        static NSButtonCell *defaultCell = leakButtonCell(DefaultButtonCell);
        cell = defaultCell;
    } else {
        static NSButtonCell *normalCell = leakButtonCell(NormalButtonCell);
        cell = normalCell;
    }
    setUpButtonCell(cell, part, controlStates, zoomedSize, zoomFactor);
    return cell;
}
    
static void paintButton(ControlPart part, ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    // Determine the width and height needed for the control and prepare the cell for painting.
    ControlStates::States states = controlStates.states();
    NSButtonCell *buttonCell = button(part, controlStates, IntSize(zoomedRect.size()), zoomFactor);
    GraphicsContextStateSaver stateSaver(context);

    NSControlSize controlSize = [buttonCell controlSize];
    IntSize zoomedSize = buttonSizes()[controlSize];
    zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    FloatRect inflatedRect = zoomedRect;
    if ([buttonCell bezelStyle] == NSBezelStyleRounded) {
        // Center the button within the available space.
        if (inflatedRect.height() > zoomedSize.height()) {
            inflatedRect.setY(inflatedRect.y() + (inflatedRect.height() - zoomedSize.height()) / 2);
            inflatedRect.setHeight(zoomedSize.height());
        }

        // Now inflate it to account for the shadow.
        inflatedRect = inflateRect(inflatedRect, zoomedSize, buttonMargins(controlSize), zoomFactor);

        if (zoomFactor != 1.0f) {
            inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
            inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
            context.translate(inflatedRect.location());
            context.scale(zoomFactor);
            context.translate(-inflatedRect.location());
        }
    }
    
    LocalCurrentGraphicsContext localContext(context);
    
    NSView *view = ThemeMac::ensuredView(scrollView, controlStates);
    NSWindow *window = [view window];
    NSButtonCell *previousDefaultButtonCell = [window defaultButtonCell];

    bool useImageBuffer = pageScaleFactor != 1.0f || zoomFactor != 1.0f;
    bool needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(buttonCell, context, inflatedRect, view, true, states & ControlStates::FocusState, useImageBuffer, deviceScaleFactor);
    if (states & ControlStates::DefaultState)
        [window setDefaultButtonCell:buttonCell];
    else if ([previousDefaultButtonCell isEqual:buttonCell])
        [window setDefaultButtonCell:nil];
    
    controlStates.setNeedsRepaint(needsRepaint);

    [buttonCell setControlView:nil];

    if (![previousDefaultButtonCell isEqual:buttonCell])
        [window setDefaultButtonCell:previousDefaultButtonCell];

    END_BLOCK_OBJC_EXCEPTIONS
}

// Stepper

static const std::array<IntSize, 3>& stepperSizes()
{
    static const std::array<IntSize, 3> sizes = { { IntSize(19, 27), IntSize(15, 22), IntSize(13, 15) } };
    return sizes;
}

// We don't use controlSizeForFont() for steppers because the stepper height
// should be equal to or less than the corresponding text field height,
static NSControlSize stepperControlSizeForFont(const FontCascade& font)
{
    int fontSize = font.pixelSize();
    if (fontSize >= 18)
        return NSControlSizeRegular;
    if (fontSize >= 13)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static void paintStepper(ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView*)
{
    // We don't use NSStepperCell because there are no ways to draw an
    // NSStepperCell with the up button highlighted.

    HIThemeButtonDrawInfo drawInfo;
    drawInfo.version = 0;
    drawInfo.state = convertControlStatesToThemeDrawState(kThemeIncDecButton, states);
    drawInfo.adornment = kThemeAdornmentDefault;
    ControlSize controlSize = controlSizeFromPixelSize(stepperSizes(), IntSize(zoomedRect.size()), zoomFactor);
    if (controlSize == NSControlSizeSmall)
        drawInfo.kind = kThemeIncDecButtonSmall;
    else if (controlSize == NSControlSizeMini)
        drawInfo.kind = kThemeIncDecButtonMini;
    else
        drawInfo.kind = kThemeIncDecButton;

    IntRect rect(zoomedRect);
    GraphicsContextStateSaver stateSaver(context);
    if (zoomFactor != 1.0f) {
        rect.setWidth(rect.width() / zoomFactor);
        rect.setHeight(rect.height() / zoomFactor);
        context.translate(rect.location());
        context.scale(zoomFactor);
        context.translate(-rect.location());
    }
    CGRect bounds(rect);
    CGRect backgroundBounds;
    HIThemeGetButtonBackgroundBounds(&bounds, &drawInfo, &backgroundBounds);
    // Center the stepper rectangle in the specified area.
    backgroundBounds.origin.x = bounds.origin.x + (bounds.size.width - backgroundBounds.size.width) / 2;
    if (backgroundBounds.size.height < bounds.size.height) {
        int heightDiff = clampToInteger(bounds.size.height - backgroundBounds.size.height);
        backgroundBounds.origin.y = bounds.origin.y + (heightDiff / 2) + 1;
    }

    LocalCurrentGraphicsContext localContext(context);
    HIThemeDrawButton(&backgroundBounds, &drawInfo, localContext.cgContext(), kHIThemeOrientationNormal, 0);
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
    [themeView setAppearance:[NSAppearance currentAppearance]];

    themeWindowHasKeyAppearance = !(controlStates.states() & ControlStates::WindowInactiveState);

    return themeView;
}

void ThemeMac::setFocusRingClipRect(const FloatRect& rect)
{
    focusRingClipRect = rect;
}

const float buttonFocusRectOutlineWidth = 3.0f;

static inline bool drawCellOrFocusRingIntoRectWithView(NSCell *cell, NSRect rect, NSView *view, bool drawButtonCell, bool drawFocusRing)
{
    if (drawButtonCell) {
        if ([cell isKindOfClass:[NSSliderCell class]]) {
            // For slider cells, draw only the knob.
            [(NSSliderCell *)cell drawKnob:rect];
        } else
            [cell drawWithFrame:rect inView:view];
    }
    if (drawFocusRing)
        return drawCellFocusRing(cell, rect, view);

    return false;
}

bool ThemeMac::drawCellOrFocusRingWithViewIntoContext(NSCell *cell, GraphicsContext& context, const FloatRect& rect, NSView *view, bool drawButtonCell, bool drawFocusRing, bool useImageBuffer, float deviceScaleFactor)
{
    ASSERT(drawButtonCell || drawFocusRing);
    bool needsRepaint = false;
    if (useImageBuffer) {
        NSRect imageBufferDrawRect = NSRect(FloatRect(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth, rect.width(), rect.height()));
        auto imageBuffer = ImageBuffer::createCompatibleBuffer(rect.size() + 2 * FloatSize(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth), deviceScaleFactor, ColorSpaceSRGB, context);
        if (!imageBuffer)
            return needsRepaint;
        {
            LocalCurrentGraphicsContext localContext(imageBuffer->context());
            needsRepaint = drawCellOrFocusRingIntoRectWithView(cell, imageBufferDrawRect, view, drawButtonCell, drawFocusRing);
        }
        context.drawConsumingImageBuffer(WTFMove(imageBuffer), rect.location() - FloatSize(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth));
        return needsRepaint;
    }
    if (drawButtonCell)
        needsRepaint = drawCellOrFocusRingIntoRectWithView(cell, NSRect(rect), view, drawButtonCell, drawFocusRing);
    
    return needsRepaint;
}

// Color Well

#if ENABLE(INPUT_TYPE_COLOR)
static void paintColorWell(ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Determine the width and height needed for the control and prepare the cell for painting.
    ControlStates::States states = controlStates.states();
    NSButtonCell *buttonCell = button(ColorWellPart, controlStates, IntSize(zoomedRect.size()), zoomFactor);
    GraphicsContextStateSaver stateSaver(context);

    NSControlSize controlSize = [buttonCell controlSize];
    IntSize zoomedSize = buttonSizes()[controlSize];
    zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    FloatRect inflatedRect = zoomedRect;

    LocalCurrentGraphicsContext localContext(context);

    NSView *view = ThemeMac::ensuredView(scrollView, controlStates);
    NSWindow *window = [view window];
    NSButtonCell *previousDefaultButtonCell = [window defaultButtonCell];

    bool useImageBuffer = pageScaleFactor != 1.0f || zoomFactor != 1.0f;
    bool needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(buttonCell, context, inflatedRect, view, true, states & ControlStates::FocusState, useImageBuffer, deviceScaleFactor);
    if ([previousDefaultButtonCell isEqual:buttonCell])
        [window setDefaultButtonCell:nil];

    controlStates.setNeedsRepaint(needsRepaint);

    [buttonCell setControlView:nil];

    if (![previousDefaultButtonCell isEqual:buttonCell])
        [window setDefaultButtonCell:previousDefaultButtonCell];

    END_BLOCK_OBJC_EXCEPTIONS
}
#endif

// Theme overrides

int ThemeMac::baselinePositionAdjustment(ControlPart part) const
{
    if (part == CheckboxPart || part == RadioPart)
        return -2;
    return Theme::baselinePositionAdjustment(part);
}

std::optional<FontCascadeDescription> ThemeMac::controlFont(ControlPart part, const FontCascade& font, float zoomFactor) const
{
    switch (part) {
    case PushButtonPart: {
        FontCascadeDescription fontDescription;
        fontDescription.setIsAbsoluteSize(true);

        NSFont* nsFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSizeForFont(font)]];
        fontDescription.setOneFamily(AtomicString("-apple-system", AtomicString::ConstructFromLiteral));
        fontDescription.setComputedSize([nsFont pointSize] * zoomFactor);
        fontDescription.setSpecifiedSize([nsFont pointSize] * zoomFactor);
        return fontDescription;
    }
    default:
        return std::nullopt;
    }
}

LengthSize ThemeMac::controlSize(ControlPart part, const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (part) {
    case CheckboxPart:
        return checkboxSize(font, zoomedSize, zoomFactor);
    case RadioPart:
        return radioSize(font, zoomedSize, zoomFactor);
    case PushButtonPart:
        // Height is reset to auto so that specified heights can be ignored.
        return sizeFromFont(font, { zoomedSize.width, { } }, zoomFactor, buttonSizes());
    case InnerSpinButtonPart:
        if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
            return zoomedSize;
        return sizeFromNSControlSize(stepperControlSizeForFont(font), zoomedSize, zoomFactor, stepperSizes());
    default:
        return zoomedSize;
    }
}

LengthSize ThemeMac::minimumControlSize(ControlPart part, const FontCascade& font, float zoomFactor) const
{
    switch (part) {
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case DefaultButtonPart:
    case ButtonPart:
        return { { 0, Fixed }, { static_cast<int>(15 * zoomFactor), Fixed } };
    case InnerSpinButtonPart: {
        auto& base = stepperSizes()[NSControlSizeMini];
        return { { static_cast<int>(base.width() * zoomFactor), Fixed },
            { static_cast<int>(base.height() * zoomFactor), Fixed } };
    }
    default:
        return Theme::minimumControlSize(part, font, zoomFactor);
    }
}

LengthBox ThemeMac::controlBorder(ControlPart part, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (part) {
    case SquareButtonPart:
#if ENABLE(INPUT_TYPE_COLOR)
    case ColorWellPart:
#endif
    case DefaultButtonPart:
    case ButtonPart:
        return LengthBox(0, zoomedBox.right().value(), 0, zoomedBox.left().value());
    default:
        return Theme::controlBorder(part, font, zoomedBox, zoomFactor);
    }
}

LengthBox ThemeMac::controlPadding(ControlPart part, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (part) {
    case PushButtonPart: {
        // Just use 8px. AppKit wants to use 11px for mini buttons, but that padding is just too large
        // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
        // by definition constrained, since we select mini only for small cramped environments).
        // This also guarantees the HTML <button> will match our rendering by default, since we're using
        // a consistent padding.
        int padding = 8 * zoomFactor;
        return LengthBox(2, padding, 3, padding);
    }
    default:
        return Theme::controlPadding(part, font, zoomedBox, zoomFactor);
    }
}

void ThemeMac::inflateControlPaintRect(ControlPart part, const ControlStates& states, FloatRect& zoomedRect, float zoomFactor) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    IntSize zoomRectSize = IntSize(zoomedRect.size());
    switch (part) {
        case CheckboxPart: {
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
        case RadioPart: {
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
        case PushButtonPart:
        case DefaultButtonPart:
        case ButtonPart: {
            NSButtonCell *cell = button(part, states, zoomRectSize, zoomFactor);
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
        case InnerSpinButtonPart: {
            static const int stepperMargin[4] = { 0, 0, 0, 0 };
            ControlSize controlSize = controlSizeFromPixelSize(stepperSizes(), zoomRectSize, zoomFactor);
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

void ThemeMac::paint(ControlPart part, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor, bool useSystemAppearance, bool useDarkAppearance)
{
    UNUSED_PARAM(useSystemAppearance);

    LocalDefaultSystemAppearance localAppearance(useDarkAppearance);

    switch (part) {
        case CheckboxPart:
            paintToggleButton(part, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor, pageScaleFactor);
            break;
        case RadioPart:
            paintToggleButton(part, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor, pageScaleFactor);
            break;
        case PushButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
        case SquareButtonPart:
            paintButton(part, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor, pageScaleFactor);
            break;
#if ENABLE(INPUT_TYPE_COLOR)
        case ColorWellPart:
            paintColorWell(states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor, pageScaleFactor);
            break;
#endif
        case InnerSpinButtonPart:
            paintStepper(states, context, zoomedRect, zoomFactor, scrollView);
            break;
        default:
            break;
    }
}

bool ThemeMac::userPrefersReducedMotion() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}

}

#endif // PLATFORM(MAC)
