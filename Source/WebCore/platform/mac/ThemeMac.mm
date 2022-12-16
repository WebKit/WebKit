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
#if HAVE(LARGE_CONTROL_SIZE)
    if (fontSize >= 21 && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
#endif
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
#if HAVE(LARGE_CONTROL_SIZE)
    if (ThemeMac::supportsLargeFormControls()
        && minZoomedSize.width() >= static_cast<int>(sizes[NSControlSizeLarge].width() * zoomFactor)
        && minZoomedSize.height() >= static_cast<int>(sizes[NSControlSizeLarge].height() * zoomFactor))
        return NSControlSizeLarge;
#endif
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
#if HAVE(LARGE_CONTROL_SIZE)
        if (ThemeMac::supportsLargeFormControls()) {
            sizes = { { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10), IntSize(16, 16) } };
            return;
        }
#endif
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
    
static void configureToggleButton(NSCell* cell, ControlPartType buttonType, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor, bool isStateChange)
{
    // Set the control size based off the rectangle we're painting into.
    setControlSize(cell, buttonType == ControlPartType::Checkbox ? checkboxSizes() : radioSizes(), zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states, isStateChange);
}
    
static RetainPtr<NSButtonCell> createToggleButtonCell(ControlPartType buttonType)
{
    RetainPtr<NSButtonCell> toggleButtonCell = adoptNS([[NSButtonCell alloc] init]);
    
    if (buttonType == ControlPartType::Checkbox) {
        [toggleButtonCell setButtonType:NSButtonTypeSwitch];
        [toggleButtonCell setAllowsMixedState:YES];
    } else {
        ASSERT(buttonType == ControlPartType::Radio);
        [toggleButtonCell setButtonType:NSButtonTypeRadio];
    }
    
    [toggleButtonCell setTitle:nil];
    [toggleButtonCell setFocusRingType:NSFocusRingTypeExterior];
    return toggleButtonCell;
}
    
static NSButtonCell *sharedRadioCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *radioCell = createToggleButtonCell(ControlPartType::Radio).leakRef();

    configureToggleButton(radioCell, ControlPartType::Radio, states, zoomedSize, zoomFactor, false);
    return radioCell;
}
    
static NSButtonCell *sharedCheckboxCell(const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    static NSButtonCell *checkboxCell = createToggleButtonCell(ControlPartType::Checkbox).leakRef();

    configureToggleButton(checkboxCell, ControlPartType::Checkbox, states, zoomedSize, zoomFactor, false);
    return checkboxCell;
}

static bool drawCellFocusRingWithFrameAtTime(NSCell *cell, NSRect cellFrame, NSView *controlView, NSTimeInterval timeOffset)
{
    CGContextRef cgContext = [[NSGraphicsContext currentContext] CGContext];

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
    auto color = colorFromCocoaColor([NSColor keyboardFocusIndicatorColor]).opaqueColor();
    auto style = adoptCF(CGStyleCreateFocusRingWithColor(&focusRingStyle, cachedCGColor(color).get()));
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

static void paintToggleButton(ControlPartType buttonType, ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor)
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
            if (buttonType == ControlPartType::Checkbox)
                toggleButtonCell = sharedCheckboxCell(controlStates, zoomedRectSize, zoomFactor);
            else {
                ASSERT(buttonType == ControlPartType::Radio);
                toggleButtonCell = sharedRadioCell(controlStates, zoomedRectSize, zoomFactor);
            }
        }
        configureToggleButton(toggleButtonCell.get(), buttonType, controlStates, zoomedRectSize, zoomFactor, false);
    }
    controlStates.setDirty(false);

    GraphicsContextStateSaver stateSaver(context);

    NSControlSize controlSize = [toggleButtonCell controlSize];
    IntSize zoomedSize = buttonType == ControlPartType::Checkbox ? checkboxSizes()[controlSize] : radioSizes()[controlSize];
    zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    const int* controlMargins = buttonType == ControlPartType::Checkbox ? checkboxMargins(controlSize) : radioMargins(controlSize);
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
    bool isCellFocused = controlStates.states().contains(ControlStates::States::Focused);

    if ([toggleButtonCell _stateAnimationRunning]) {
        context.translate(inflatedRect.location());
        context.scale(FloatSize(1, -1));
        context.translate(0, -inflatedRect.height());

        [toggleButtonCell _renderCurrentAnimationFrameInContext:context.platformContext() atLocation:NSMakePoint(0, 0)];
        if (![toggleButtonCell _stateAnimationRunning] && isCellFocused)
            needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(toggleButtonCell.get(), context, inflatedRect, view, false, true, deviceScaleFactor);
    } else
        needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(toggleButtonCell.get(), context, inflatedRect, view, true, isCellFocused, deviceScaleFactor);

    [toggleButtonCell setControlView:nil];

    needsRepaint |= [toggleButtonCell _stateAnimationRunning];
    controlStates.setNeedsRepaint(needsRepaint);
    if (needsRepaint)
        controlStates.setPlatformControl(toggleButtonCell.get());

    END_BLOCK_OBJC_EXCEPTIONS
}

// Buttons

// Buttons really only constrain height. They respect width.
static const std::array<IntSize, 4>& buttonSizes()
{
    static const std::array<IntSize, 4> sizes = { { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15), IntSize(0, 28) } };
    return sizes;
}

static const int* buttonMargins(NSControlSize controlSize)
{
    // FIXME: These values may need to be reevaluated. They appear to have been originally chosen
    // to reflect the size of shadows around native form controls on macOS, but as of macOS 10.15,
    // these margins extend well past the boundaries of a native button cell's shadows.
    static const int margins[4][4] =
    {
        { 4, 6, 7, 6 },
        { 4, 5, 6, 5 },
        { 0, 1, 1, 1 },
        { 4, 6, 7, 6 },
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

static void setUpButtonCell(NSButtonCell *cell, ControlPartType type, const ControlStates& states, const IntSize& zoomedSize, float zoomFactor)
{
    // Set the control size based off the rectangle we're painting into.
    const std::array<IntSize, 4>& sizes = buttonSizes();
    switch (type) {
    case ControlPartType::SquareButton:
        [cell setBezelStyle:NSBezelStyleShadowlessSquare];
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
        [cell setBezelStyle:NSBezelStyleTexturedSquare];
        break;
#endif
    default:
#if HAVE(LARGE_CONTROL_SIZE)
        auto largestControlSize = ThemeMac::supportsLargeFormControls() ? NSControlSizeLarge : NSControlSizeRegular;
#else
        auto largestControlSize = NSControlSizeRegular;
#endif
        NSBezelStyle style = (zoomedSize.height() > buttonSizes()[largestControlSize].height() * zoomFactor) ? NSBezelStyleShadowlessSquare : NSBezelStyleRounded;
        [cell setBezelStyle:style];
        break;
    }

    setControlSize(cell, sizes, zoomedSize, zoomFactor);

    // Update the various states we respond to.
    updateStates(cell, states);
}

static NSButtonCell *button(ControlPartType type, const ControlStates& controlStates, const IntSize& zoomedSize, float zoomFactor)
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
    setUpButtonCell(cell, type, controlStates, zoomedSize, zoomFactor);
    return cell;
}
    
static void paintButton(ControlPartType type, ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    
    // Determine the width and height needed for the control and prepare the cell for painting.
    auto states = controlStates.states();
    NSButtonCell *buttonCell = button(type, controlStates, IntSize(zoomedRect.size()), zoomFactor);
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

    bool needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(buttonCell, context, inflatedRect, view, true, states.contains(ControlStates::States::Focused), deviceScaleFactor);
    if (states.contains(ControlStates::States::Default))
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
#if HAVE(LARGE_CONTROL_SIZE)
    if (fontSize >= 23 && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
#endif
    if (fontSize >= 18)
        return NSControlSizeRegular;
    if (fontSize >= 13)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

static void paintStepper(ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView*)
{
    // We don't use NSStepperCell because there are no ways to draw an
    // NSStepperCell with the up button highlighted.

    NSString *coreUIState;
    auto states = controlStates.states();
    if (!states.contains(ControlStates::States::Enabled))
        coreUIState = (__bridge NSString *)kCUIStateDisabled;
    else if (states.contains(ControlStates::States::Pressed))
        coreUIState = (__bridge NSString *)kCUIStatePressed;
    else
        coreUIState = (__bridge NSString *)kCUIStateActive;

    NSString *coreUISize;
    auto controlSize = controlSizeFromPixelSize(stepperSizes(), IntSize(zoomedRect.size()), zoomFactor);
    if (controlSize == NSControlSizeMini)
        coreUISize = (__bridge NSString *)kCUISizeMini;
    else if (controlSize == NSControlSizeSmall)
        coreUISize = (__bridge NSString *)kCUISizeSmall;
    else
        coreUISize = (__bridge NSString *)kCUISizeRegular;

    IntRect rect(zoomedRect);
    GraphicsContextStateSaver stateSaver(context);
    if (zoomFactor != 1.0f) {
        rect.setWidth(rect.width() / zoomFactor);
        rect.setHeight(rect.height() / zoomFactor);
        context.translate(rect.location());
        context.scale(zoomFactor);
        context.translate(-rect.location());
    }

    LocalCurrentGraphicsContext localContext(context);
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    [[NSAppearance currentAppearance] _drawInRect:rect context:localContext.cgContext() options:@{
    ALLOW_DEPRECATED_DECLARATIONS_END
        (__bridge NSString *)kCUIWidgetKey: (__bridge NSString *)kCUIWidgetButtonLittleArrows,
        (__bridge NSString *)kCUISizeKey: coreUISize,
        (__bridge NSString *)kCUIStateKey: coreUIState,
        (__bridge NSString *)kCUIValueKey: states.contains(ControlStates::States::SpinUp) ? @1 : @0,
        (__bridge NSString *)kCUIIsFlippedKey: @NO,
        (__bridge NSString *)kCUIScaleKey: @1,
        (__bridge NSString *)kCUIMaskOnlyKey: @NO
    }];
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

    themeWindowHasKeyAppearance = !controlStates.states().contains(ControlStates::States::WindowInactive);

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

bool ThemeMac::drawCellOrFocusRingWithViewIntoContext(NSCell *cell, GraphicsContext& context, const FloatRect& rect, NSView *view, bool drawButtonCell, bool drawFocusRing, float deviceScaleFactor)
{
    ASSERT(drawButtonCell || drawFocusRing);
    bool needsRepaint = false;
    auto platformContext = context.platformContext();
    auto userCTM = AffineTransform(CGAffineTransformConcat(CGContextGetCTM(platformContext), CGAffineTransformInvert(CGContextGetBaseCTM(platformContext))));
    bool useImageBuffer = userCTM.xScale() != 1.0 || userCTM.yScale() != 1.0;

    if (useImageBuffer) {
        NSRect imageBufferDrawRect = NSRect(FloatRect(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth, rect.width(), rect.height()));
        auto imageBuffer = context.createImageBuffer(rect.size() + 2 * FloatSize(buttonFocusRectOutlineWidth, buttonFocusRectOutlineWidth), deviceScaleFactor);
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
static void paintColorWell(ControlStates& controlStates, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Determine the width and height needed for the control and prepare the cell for painting.
    auto states = controlStates.states();
    NSButtonCell *buttonCell = button(ControlPartType::ColorWell, controlStates, IntSize(zoomedRect.size()), zoomFactor);
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

    bool needsRepaint = ThemeMac::drawCellOrFocusRingWithViewIntoContext(buttonCell, context, inflatedRect, view, true, states.contains(ControlStates::States::Focused), deviceScaleFactor);
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

int ThemeMac::baselinePositionAdjustment(ControlPartType type) const
{
    if (type == ControlPartType::Checkbox || type == ControlPartType::Radio)
        return -2;
    return Theme::baselinePositionAdjustment(type);
}

std::optional<FontCascadeDescription> ThemeMac::controlFont(ControlPartType type, const FontCascade& font, float zoomFactor) const
{
    switch (type) {
    case ControlPartType::PushButton: {
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

LengthSize ThemeMac::controlSize(ControlPartType type, const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (type) {
    case ControlPartType::Checkbox:
        return checkboxSize(zoomedSize, zoomFactor);
    case ControlPartType::Radio:
        return radioSize(zoomedSize, zoomFactor);
    case ControlPartType::PushButton:
        // Height is reset to auto so that specified heights can be ignored.
        return sizeFromFont(font, { zoomedSize.width, { } }, zoomFactor, buttonSizes());
    case ControlPartType::InnerSpinButton:
        if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
            return zoomedSize;
        return sizeFromNSControlSize(stepperControlSizeForFont(font), zoomedSize, zoomFactor, stepperSizes());
    default:
        return zoomedSize;
    }
}

LengthSize ThemeMac::minimumControlSize(ControlPartType type, const FontCascade& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (type) {
    case ControlPartType::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
        return { { 0, LengthType::Fixed }, { static_cast<int>(15 * zoomFactor), LengthType::Fixed } };
    case ControlPartType::InnerSpinButton: {
        auto& base = stepperSizes()[NSControlSizeMini];
        return { { static_cast<int>(base.width() * zoomFactor), LengthType::Fixed },
            { static_cast<int>(base.height() * zoomFactor), LengthType::Fixed } };
    }
    default:
        return Theme::minimumControlSize(type, font, zoomedSize, zoomFactor);
    }
}

LengthBox ThemeMac::controlBorder(ControlPartType type, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (type) {
    case ControlPartType::SquareButton:
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
#endif
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
        return LengthBox(0, zoomedBox.right().value(), 0, zoomedBox.left().value());
    default:
        return Theme::controlBorder(type, font, zoomedBox, zoomFactor);
    }
}

LengthBox ThemeMac::controlPadding(ControlPartType type, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (type) {
    case ControlPartType::PushButton: {
        // Just use 8px. AppKit wants to use 11px for mini buttons, but that padding is just too large
        // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
        // by definition constrained, since we select mini only for small cramped environments).
        // This also guarantees the HTML <button> will match our rendering by default, since we're using
        // a consistent padding.
        int padding = 8 * zoomFactor;
        return LengthBox(2, padding, 3, padding);
    }
    default:
        return Theme::controlPadding(type, font, zoomedBox, zoomFactor);
    }
}

void ThemeMac::inflateControlPaintRect(ControlPartType type, const ControlStates& states, FloatRect& zoomedRect, float zoomFactor) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    IntSize zoomRectSize = IntSize(zoomedRect.size());
    switch (type) {
    case ControlPartType::Checkbox: {
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
    case ControlPartType::Radio: {
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
    case ControlPartType::PushButton:
    case ControlPartType::DefaultButton:
    case ControlPartType::Button: {
        NSButtonCell *cell = button(type, states, zoomRectSize, zoomFactor);
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
    case ControlPartType::InnerSpinButton: {
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

void ThemeMac::paint(ControlPartType type, ControlStates& states, GraphicsContext& context, const FloatRect& zoomedRect, float zoomFactor, ScrollView* scrollView, float deviceScaleFactor, float pageScaleFactor, bool useSystemAppearance, bool useDarkAppearance, const Color& tintColor)
{
    UNUSED_PARAM(useSystemAppearance);
    UNUSED_PARAM(pageScaleFactor);

    LocalDefaultSystemAppearance localAppearance(useDarkAppearance, tintColor);

    switch (type) {
    case ControlPartType::Checkbox:
        paintToggleButton(type, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor);
        break;
    case ControlPartType::Radio:
        paintToggleButton(type, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor);
        break;
    case ControlPartType::PushButton:
    case ControlPartType::DefaultButton:
    case ControlPartType::Button:
    case ControlPartType::SquareButton:
        paintButton(type, states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor);
        break;
#if ENABLE(INPUT_TYPE_COLOR)
    case ControlPartType::ColorWell:
        paintColorWell(states, context, zoomedRect, zoomFactor, scrollView, deviceScaleFactor);
        break;
#endif
    case ControlPartType::InnerSpinButton:
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

bool ThemeMac::userPrefersContrast() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
}

#if HAVE(LARGE_CONTROL_SIZE)

bool ThemeMac::supportsLargeFormControls()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    static bool hasSupport = [[NSAppearance currentAppearance] _usesMetricsAppearance];
    ALLOW_DEPRECATED_DECLARATIONS_END
    return hasSupport;
}

#endif // HAVE(LARGE_CONTROL_SIZE)

}

#endif // PLATFORM(MAC)
