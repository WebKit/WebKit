/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
 * Copyright (C) 2009 Google Inc. All Rights Reserved.
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
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "config.h"
#import "ThemeChromiumMac.h"

#import "BlockExceptions.h"
#import "GraphicsContext.h"
#import "LocalCurrentGraphicsContext.h"
#import "ScrollView.h"
#import "WebCoreSystemInterface.h"
#import <Carbon/Carbon.h>
#import <objc/runtime.h>
#include "PlatformContextSkia.h"
#include "skia/ext/skia_utils_mac.h"
#include <wtf/StdLibExtras.h>

using namespace std;

// This file (and its associated .h file) is a clone of ThemeMac.mm.
// Because the original file is designed to run in-process inside a Cocoa view,
// we must maintain a fork. Please maintain this file by performing parallel
// changes to it.
//
// The only changes from ThemeMac should be:
// - The classname change from ThemeMac to ThemeChromiumMac.
// - The import of FlippedView() and its use as the parent view for cell
//   rendering.
// - In updateStates() the code to update the cells' inactive state.
// - In paintButton() the code to save/restore the window's default button cell.
// - The Snow Leopard focus ring bug fix and its use around every call to
//   -[NSButtonCell drawWithFrame:inView:].
//
// For all other differences, if it was introduced in this file, then the
// maintainer forgot to include it in the list; otherwise it is an update that
// should have been applied to this file but was not.

// FIXME: Default buttons really should be more like push buttons and not like buttons.

// --- START fix for Snow Leopard focus ring bug ---

// There is a bug in the Cocoa focus ring drawing code. The code calls +[NSView
// focusView] (to get the currently focused view) and then calls an NSRect-
// returning method on that view to obtain a clipping rect. However, if there is
// no focused view (as there won't be if the destination is a context), the rect
// returned from the method invocation on nil is garbage.
//
// The garbage fortunately does not clip the focus ring on Leopard, but
// unfortunately does so on Snow Leopard. Therefore, if a runtime test shows
// that focus ring drawing fails, we swizzle NSView to ensure it returns a valid
// view with a valid clipping rectangle.
//
// FIXME: After the referenced bug is fixed on all supported platforms, remove
// this code.
//
// References:
//  <http://crbug.com/27493>
//  <rdar://problem/7604051> (<http://openradar.appspot.com/7604051>)

@interface TCMVisibleView : NSView

@end

@implementation TCMVisibleView

- (struct CGRect)_focusRingVisibleRect
{
    return CGRectZero;
}

- (id)_focusRingClipAncestor
{
    return self;
}

@end

@interface NSView (TCMInterposing)
+ (NSView *)TCMInterposing_focusView;
@end

namespace FocusIndicationFix {

bool currentOSHasSetFocusRingStyleInBitmapBug()
{
    UInt32 pixel = 0;
    UInt32* pixelPlane = &pixel;
    UInt32** pixelPlanes = &pixelPlane;
    NSBitmapImageRep *bitmap = [[NSBitmapImageRep alloc] initWithBitmapDataPlanes:(UInt8**)pixelPlanes
                                                                       pixelsWide:1
                                                                       pixelsHigh:1
                                                                    bitsPerSample:8
                                                                  samplesPerPixel:4
                                                                         hasAlpha:YES
                                                                         isPlanar:NO
                                                                   colorSpaceName:NSCalibratedRGBColorSpace
                                                                     bitmapFormat:NSAlphaFirstBitmapFormat
                                                                      bytesPerRow:4
                                                                     bitsPerPixel:32];
    [NSGraphicsContext saveGraphicsState];
    [NSGraphicsContext setCurrentContext:[NSGraphicsContext graphicsContextWithBitmapImageRep:bitmap]];
    NSSetFocusRingStyle(NSFocusRingOnly);
    NSRectFill(NSMakeRect(0, 0, 1, 1));
    [NSGraphicsContext restoreGraphicsState];
    [bitmap release];

    return !pixel;
}

bool swizzleFocusView()
{
    if (!currentOSHasSetFocusRingStyleInBitmapBug())
        return false;

    Class nsview = [NSView class];
    Method m1 = class_getClassMethod(nsview, @selector(focusView));
    Method m2 = class_getClassMethod(nsview, @selector(TCMInterposing_focusView));
    if (m1 && m2) {
        method_exchangeImplementations(m1, m2);
        return true;
    }

    return false;
}

static bool interpose = false;

// A class to restrict the amount of time spent messing with interposing. It
// only stacks one-deep.
class ScopedFixer {
public:
    ScopedFixer()
    {
        static bool swizzled = swizzleFocusView();
        interpose = swizzled;
    }

    ~ScopedFixer()
    {
        interpose = false;
    }
};

}  // namespace FocusIndicationFix

@implementation NSView (TCMInterposing)

+ (NSView *)TCMInterposing_focusView
{
    NSView *view = [self TCMInterposing_focusView];  // call original (was swizzled)
    if (!view && FocusIndicationFix::interpose) {
        static TCMVisibleView* fixedView = [[TCMVisibleView alloc] init];
        view = fixedView;
    }

    return view;
}

@end

// --- END fix for Snow Leopard focus ring bug ---

namespace WebCore {

// Pick up utility function from RenderThemeChromiumMac.
extern NSView* FlippedView();

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};

Theme* platformTheme()
{
    DEFINE_STATIC_LOCAL(ThemeChromiumMac, themeMac, ());
    return &themeMac;
}

// Helper functions used by a bunch of different control parts.

static NSControlSize controlSizeForFont(const Font& font)
{
    int fontSize = font.pixelSize();
    if (fontSize >= 16)
        return NSRegularControlSize;
    if (fontSize >= 11)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

static LengthSize sizeFromNSControlSize(NSControlSize nsControlSize, const LengthSize& zoomedSize, float zoomFactor, const IntSize* sizes)
{
    IntSize controlSize = sizes[nsControlSize];
    if (zoomFactor != 1.0f)
        controlSize = IntSize(controlSize.width() * zoomFactor, controlSize.height() * zoomFactor);
    LengthSize result = zoomedSize;
    if (zoomedSize.width().isIntrinsicOrAuto() && controlSize.width() > 0)
        result.setWidth(Length(controlSize.width(), Fixed));
    if (zoomedSize.height().isIntrinsicOrAuto() && controlSize.height() > 0)
        result.setHeight(Length(controlSize.height(), Fixed));
    return result;
}

static LengthSize sizeFromFont(const Font& font, const LengthSize& zoomedSize, float zoomFactor, const IntSize* sizes)
{
    return sizeFromNSControlSize(controlSizeForFont(font), zoomedSize, zoomFactor, sizes);
}

static ControlSize controlSizeFromPixelSize(const IntSize* sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSRegularControlSize].width() * zoomFactor) &&
        minZoomedSize.height() >= static_cast<int>(sizes[NSRegularControlSize].height() * zoomFactor))
        return NSRegularControlSize;
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSSmallControlSize].width() * zoomFactor) &&
        minZoomedSize.height() >= static_cast<int>(sizes[NSSmallControlSize].height() * zoomFactor))
        return NSSmallControlSize;
    return NSMiniControlSize;
}

static void setControlSize(NSCell* cell, const IntSize* sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    ControlSize size = controlSizeFromPixelSize(sizes, minZoomedSize, zoomFactor);
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:(NSControlSize)size];
}

static void updateStates(NSCell* cell, ControlStates states)
{
    // Hover state is not supported by Aqua.

    // Pressed state
    bool oldPressed = [cell isHighlighted];
    bool pressed = states & PressedState;
    if (pressed != oldPressed)
        [cell setHighlighted:pressed];

    // Enabled state
    bool oldEnabled = [cell isEnabled];
    bool enabled = states & EnabledState;
    if (enabled != oldEnabled)
        [cell setEnabled:enabled];

    // Focused state
    bool oldFocused = [cell showsFirstResponder];
    bool focused = states & FocusState;
    if (focused != oldFocused)
        [cell setShowsFirstResponder:focused];

    // Checked and Indeterminate
    bool oldIndeterminate = [cell state] == NSMixedState;
    bool indeterminate = (states & IndeterminateState);
    bool checked = states & CheckedState;
    bool oldChecked = [cell state] == NSOnState;
    if (oldIndeterminate != indeterminate || checked != oldChecked)
        [cell setState:indeterminate ? NSMixedState : (checked ? NSOnState : NSOffState)];

    // Window Inactive state
    NSControlTint oldTint = [cell controlTint];
    bool windowInactive = (states & WindowInactiveState);
    NSControlTint tint = windowInactive ? static_cast<NSControlTint>(NSClearControlTint)
                                        : [NSColor currentControlTint];
    if (tint != oldTint)
        [cell setControlTint:tint];
}

static ThemeDrawState convertControlStatesToThemeDrawState(ThemeButtonKind kind, ControlStates states)
{
    if (states & ReadOnlyState)
        return kThemeStateUnavailableInactive;
    if (!(states & EnabledState))
        return kThemeStateUnavailableInactive;

    // Do not process PressedState if !EnabledState or ReadOnlyState.
    if (states & PressedState) {
        if (kind == kThemeIncDecButton || kind == kThemeIncDecButtonSmall || kind == kThemeIncDecButtonMini)
            return states & SpinUpState ? kThemeStatePressedUp : kThemeStatePressedDown;
        return kThemeStatePressed;
    }
    return kThemeStateActive;
}

static IntRect inflateRect(const IntRect& zoomedRect, const IntSize& zoomedSize, const int* margins, float zoomFactor)
{
    // Only do the inflation if the available width/height are too small.  Otherwise try to
    // fit the glow/check space into the available box's width/height.
    int widthDelta = zoomedRect.width() - (zoomedSize.width() + margins[leftMargin] * zoomFactor + margins[rightMargin] * zoomFactor);
    int heightDelta = zoomedRect.height() - (zoomedSize.height() + margins[topMargin] * zoomFactor + margins[bottomMargin] * zoomFactor);
    IntRect result(zoomedRect);
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

// Checkboxes

static const IntSize* checkboxSizes()
{
    static const IntSize sizes[3] = { IntSize(14, 14), IntSize(12, 12), IntSize(10, 10) };
    return sizes;
}

static const int* checkboxMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        { 3, 4, 4, 2 },
        { 4, 3, 3, 3 },
        { 4, 3, 3, 3 },
    };
    return margins[controlSize];
}

static LengthSize checkboxSize(const Font& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, checkboxSizes());
}

static NSButtonCell *checkbox(ControlStates states, const IntRect& zoomedRect, float zoomFactor)
{
    static NSButtonCell *checkboxCell;
    if (!checkboxCell) {
        checkboxCell = [[NSButtonCell alloc] init];
        [checkboxCell setButtonType:NSSwitchButton];
        [checkboxCell setTitle:nil];
        [checkboxCell setAllowsMixedState:YES];
        [checkboxCell setFocusRingType:NSFocusRingTypeExterior];
    }

    // Set the control size based off the rectangle we're painting into.
    setControlSize(checkboxCell, checkboxSizes(), zoomedRect.size(), zoomFactor);

    // Update the various states we respond to.
    updateStates(checkboxCell, states);

    return checkboxCell;
}

// FIXME: Share more code with radio buttons.
static void paintCheckbox(ControlStates states, GraphicsContext* context, const IntRect& zoomedRect, float zoomFactor, ScrollView* scrollView)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Determine the width and height needed for the control and prepare the cell for painting.
    NSButtonCell *checkboxCell = checkbox(states, zoomedRect, zoomFactor);
    LocalCurrentGraphicsContext localContext(context);

    context->save();

    NSControlSize controlSize = [checkboxCell controlSize];
    IntSize zoomedSize = checkboxSizes()[controlSize];
    zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    IntRect inflatedRect = inflateRect(zoomedRect, zoomedSize, checkboxMargins(controlSize), zoomFactor);

    if (zoomFactor != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
        inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
        context->translate(inflatedRect.x(), inflatedRect.y());
        context->scale(FloatSize(zoomFactor, zoomFactor));
        context->translate(-inflatedRect.x(), -inflatedRect.y());
    }

    {
        FocusIndicationFix::ScopedFixer fix;
        [checkboxCell drawWithFrame:NSRect(inflatedRect) inView:FlippedView()];
    }
    [checkboxCell setControlView:nil];

    context->restore();

    END_BLOCK_OBJC_EXCEPTIONS
}

// Radio Buttons

static const IntSize* radioSizes()
{
    static const IntSize sizes[3] = { IntSize(14, 15), IntSize(12, 13), IntSize(10, 10) };
    return sizes;
}

static const int* radioMargins(NSControlSize controlSize)
{
    static const int margins[3][4] =
    {
        { 2, 2, 4, 2 },
        { 3, 2, 3, 2 },
        { 1, 0, 2, 0 },
    };
    return margins[controlSize];
}

static LengthSize radioSize(const Font& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, radioSizes());
}

static NSButtonCell *radio(ControlStates states, const IntRect& zoomedRect, float zoomFactor)
{
    static NSButtonCell *radioCell;
    if (!radioCell) {
        radioCell = [[NSButtonCell alloc] init];
        [radioCell setButtonType:NSRadioButton];
        [radioCell setTitle:nil];
        [radioCell setFocusRingType:NSFocusRingTypeExterior];
    }

    // Set the control size based off the rectangle we're painting into.
    setControlSize(radioCell, radioSizes(), zoomedRect.size(), zoomFactor);

    // Update the various states we respond to.
    updateStates(radioCell, states);

    return radioCell;
}

static void paintRadio(ControlStates states, GraphicsContext* context, const IntRect& zoomedRect, float zoomFactor, ScrollView* scrollView)
{
    // Determine the width and height needed for the control and prepare the cell for painting.
    NSButtonCell *radioCell = radio(states, zoomedRect, zoomFactor);
    LocalCurrentGraphicsContext localContext(context);

    context->save();

    NSControlSize controlSize = [radioCell controlSize];
    IntSize zoomedSize = radioSizes()[controlSize];
    zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    IntRect inflatedRect = inflateRect(zoomedRect, zoomedSize, radioMargins(controlSize), zoomFactor);

    if (zoomFactor != 1.0f) {
        inflatedRect.setWidth(inflatedRect.width() / zoomFactor);
        inflatedRect.setHeight(inflatedRect.height() / zoomFactor);
        context->translate(inflatedRect.x(), inflatedRect.y());
        context->scale(FloatSize(zoomFactor, zoomFactor));
        context->translate(-inflatedRect.x(), -inflatedRect.y());
    }

    BEGIN_BLOCK_OBJC_EXCEPTIONS
    {
        FocusIndicationFix::ScopedFixer fix;
        [radioCell drawWithFrame:NSRect(inflatedRect) inView:FlippedView()];
    }
    [radioCell setControlView:nil];
    END_BLOCK_OBJC_EXCEPTIONS

    context->restore();
}

// Buttons

// Buttons really only constrain height. They respect width.
static const IntSize* buttonSizes()
{
    static const IntSize sizes[3] = { IntSize(0, 21), IntSize(0, 18), IntSize(0, 15) };
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

static void setupButtonCell(NSButtonCell *&buttonCell, ControlPart part, ControlStates states, const IntRect& zoomedRect, float zoomFactor)
{
    if (!buttonCell) {
        buttonCell = [[NSButtonCell alloc] init];
        [buttonCell setTitle:nil];
        [buttonCell setButtonType:NSMomentaryPushInButton];
        if (states & DefaultState)
            [buttonCell setKeyEquivalent:@"\r"];
    }

    // Set the control size based off the rectangle we're painting into.
    const IntSize* sizes = buttonSizes();
    if (part == SquareButtonPart || zoomedRect.height() > buttonSizes()[NSRegularControlSize].height() * zoomFactor) {
        // Use the square button
        if ([buttonCell bezelStyle] != NSShadowlessSquareBezelStyle)
            [buttonCell setBezelStyle:NSShadowlessSquareBezelStyle];
    } else if ([buttonCell bezelStyle] != NSRoundedBezelStyle)
        [buttonCell setBezelStyle:NSRoundedBezelStyle];

    setControlSize(buttonCell, sizes, zoomedRect.size(), zoomFactor);

    // Update the various states we respond to.
    updateStates(buttonCell, states);
}

static NSButtonCell *button(ControlPart part, ControlStates states, const IntRect& zoomedRect, float zoomFactor)
{
    bool isDefault = states & DefaultState;
    static NSButtonCell *cells[2];
    setupButtonCell(cells[isDefault], part, states, zoomedRect, zoomFactor);
    return cells[isDefault];
}

static void paintButton(ControlPart part, ControlStates states, GraphicsContext* context, const IntRect& zoomedRect, float zoomFactor, ScrollView* scrollView)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS

    // Determine the width and height needed for the control and prepare the cell for painting.
    NSButtonCell *buttonCell = button(part, states, zoomedRect, zoomFactor);
    LocalCurrentGraphicsContext localContext(context);

    NSControlSize controlSize = [buttonCell controlSize];
    IntSize zoomedSize = buttonSizes()[controlSize];
    zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
    zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
    IntRect inflatedRect = zoomedRect;
    if ([buttonCell bezelStyle] == NSRoundedBezelStyle) {
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
            context->translate(inflatedRect.x(), inflatedRect.y());
            context->scale(FloatSize(zoomFactor, zoomFactor));
            context->translate(-inflatedRect.x(), -inflatedRect.y());
        }
    }

    {
        LocalCurrentGraphicsContext localContext(context);
        FocusIndicationFix::ScopedFixer fix;
        [buttonCell drawWithFrame:NSRect(inflatedRect) inView:FlippedView()];
    }
    [buttonCell setControlView:nil];

    END_BLOCK_OBJC_EXCEPTIONS
}

// Stepper

static const IntSize* stepperSizes()
{
    static const IntSize sizes[3] = { IntSize(19, 27), IntSize(15, 22), IntSize(13, 15) };
    return sizes;
}

// We don't use controlSizeForFont() for steppers because the stepper height
// should be equal to or less than the corresponding text field height,
static NSControlSize stepperControlSizeForFont(const Font& font)
{
    int fontSize = font.pixelSize();
    if (fontSize >= 18)
        return NSRegularControlSize;
    if (fontSize >= 13)
        return NSSmallControlSize;
    return NSMiniControlSize;
}

static void paintStepper(ControlStates states, GraphicsContext* context, const IntRect& zoomedRect, float zoomFactor, ScrollView*)
{
    // We don't use NSStepperCell because there are no ways to draw an
    // NSStepperCell with the up button highlighted.

    HIThemeButtonDrawInfo drawInfo;
    drawInfo.version = 0;
    drawInfo.state = convertControlStatesToThemeDrawState(kThemeIncDecButton, states);
    drawInfo.adornment = kThemeAdornmentDefault;
    ControlSize controlSize = controlSizeFromPixelSize(stepperSizes(), zoomedRect.size(), zoomFactor);
    if (controlSize == NSSmallControlSize)
        drawInfo.kind = kThemeIncDecButtonSmall;
    else if (controlSize == NSMiniControlSize)
        drawInfo.kind = kThemeIncDecButtonMini;
    else
        drawInfo.kind = kThemeIncDecButton;

    IntRect rect(zoomedRect);
    context->save();
    if (zoomFactor != 1.0f) {
        rect.setWidth(rect.width() / zoomFactor);
        rect.setHeight(rect.height() / zoomFactor);
        context->translate(rect.x(), rect.y());
        context->scale(FloatSize(zoomFactor, zoomFactor));
        context->translate(-rect.x(), -rect.y());
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
    gfx::SkiaBitLocker bitLocker(context->platformContext()->canvas());
    CGContextRef cgContext = bitLocker.cgContext();
    HIThemeDrawButton(&backgroundBounds, &drawInfo, cgContext, kHIThemeOrientationNormal, 0);
    context->restore();
}

// Theme overrides

int ThemeChromiumMac::baselinePositionAdjustment(ControlPart part) const
{
    if (part == CheckboxPart || part == RadioPart)
        return -2;
    return Theme::baselinePositionAdjustment(part);
}

FontDescription ThemeChromiumMac::controlFont(ControlPart part, const Font& font, float zoomFactor) const
{
    switch (part) {
        case PushButtonPart: {
            FontDescription fontDescription;
            fontDescription.setIsAbsoluteSize(true);
            fontDescription.setGenericFamily(FontDescription::SerifFamily);

            NSFont* nsFont = [NSFont systemFontOfSize:[NSFont systemFontSizeForControlSize:controlSizeForFont(font)]];
            fontDescription.firstFamily().setFamily([nsFont familyName]);
            fontDescription.setComputedSize([nsFont pointSize] * zoomFactor);
            fontDescription.setSpecifiedSize([nsFont pointSize] * zoomFactor);
            return fontDescription;
        }
        default:
            return Theme::controlFont(part, font, zoomFactor);
    }
}

LengthSize ThemeChromiumMac::controlSize(ControlPart part, const Font& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (part) {
        case CheckboxPart:
            return checkboxSize(font, zoomedSize, zoomFactor);
        case RadioPart:
            return radioSize(font, zoomedSize, zoomFactor);
        case PushButtonPart:
            // Height is reset to auto so that specified heights can be ignored.
            return sizeFromFont(font, LengthSize(zoomedSize.width(), Length()), zoomFactor, buttonSizes());
        case InnerSpinButtonPart:
            if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
                return zoomedSize;
            return sizeFromNSControlSize(stepperControlSizeForFont(font), zoomedSize, zoomFactor, stepperSizes());
        default:
            return zoomedSize;
    }
}

LengthSize ThemeChromiumMac::minimumControlSize(ControlPart part, const Font& font, float zoomFactor) const
{
    switch (part) {
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
            return LengthSize(Length(0, Fixed), Length(static_cast<int>(15 * zoomFactor), Fixed));
        case InnerSpinButtonPart: {
            IntSize base = stepperSizes()[NSMiniControlSize];
            return LengthSize(Length(static_cast<int>(base.width() * zoomFactor), Fixed),
                              Length(static_cast<int>(base.height() * zoomFactor), Fixed));
        }
        default:
            return Theme::minimumControlSize(part, font, zoomFactor);
    }
}

LengthBox ThemeChromiumMac::controlBorder(ControlPart part, const Font& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (part) {
        case SquareButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
            return LengthBox(0, zoomedBox.right().value(), 0, zoomedBox.left().value());
        default:
            return Theme::controlBorder(part, font, zoomedBox, zoomFactor);
    }
}

LengthBox ThemeChromiumMac::controlPadding(ControlPart part, const Font& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (part) {
        case PushButtonPart: {
            // Just use 8px.  AppKit wants to use 11px for mini buttons, but that padding is just too large
            // for real-world Web sites (creating a huge necessary minimum width for buttons whose space is
            // by definition constrained, since we select mini only for small cramped environments.
            // This also guarantees the HTML <button> will match our rendering by default, since we're using a consistent
            // padding.
            const int padding = 8 * zoomFactor;
            return LengthBox(0, padding, 0, padding);
        }
        default:
            return Theme::controlPadding(part, font, zoomedBox, zoomFactor);
    }
}

void ThemeChromiumMac::inflateControlPaintRect(ControlPart part, ControlStates states, IntRect& zoomedRect, float zoomFactor) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    switch (part) {
        case CheckboxPart: {
            // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
            // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
            NSCell *cell = checkbox(states, zoomedRect, zoomFactor);
            NSControlSize controlSize = [cell controlSize];
            IntSize zoomedSize = checkboxSizes()[controlSize];
            zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
            zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
            zoomedRect = inflateRect(zoomedRect, zoomedSize, checkboxMargins(controlSize), zoomFactor);
            break;
        }
        case RadioPart: {
            // We inflate the rect as needed to account for padding included in the cell to accommodate the radio button
            // shadow".  We don't consider this part of the bounds of the control in WebKit.
            NSCell *cell = radio(states, zoomedRect, zoomFactor);
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
            NSButtonCell *cell = button(part, states, zoomedRect, zoomFactor);
            NSControlSize controlSize = [cell controlSize];

            // We inflate the rect as needed to account for the Aqua button's shadow.
            if ([cell bezelStyle] == NSRoundedBezelStyle) {
                IntSize zoomedSize = buttonSizes()[controlSize];
                zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
                zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
                zoomedRect = inflateRect(zoomedRect, zoomedSize, buttonMargins(controlSize), zoomFactor);
            }
            break;
        }
        case InnerSpinButtonPart: {
            static const int stepperMargin[4] = { 0, 0, 0, 0 };
            ControlSize controlSize = controlSizeFromPixelSize(stepperSizes(), zoomedRect.size(), zoomFactor);
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

void ThemeChromiumMac::paint(ControlPart part, ControlStates states, GraphicsContext* context, const IntRect& zoomedRect, float zoomFactor, ScrollView* scrollView) const
{
    switch (part) {
        case CheckboxPart:
            paintCheckbox(states, context, zoomedRect, zoomFactor, scrollView);
            break;
        case RadioPart:
            paintRadio(states, context, zoomedRect, zoomFactor, scrollView);
            break;
        case PushButtonPart:
        case DefaultButtonPart:
        case ButtonPart:
        case SquareButtonPart:
            paintButton(part, states, context, zoomedRect, zoomFactor, scrollView);
            break;
        case InnerSpinButtonPart:
            paintStepper(states, context, zoomedRect, zoomFactor, scrollView);
            break;
        default:
            break;
    }
}

}
