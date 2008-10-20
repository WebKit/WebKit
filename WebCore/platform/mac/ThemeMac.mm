/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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
#import "ThemeMac.h"

#import "GraphicsContext.h"
#import "ScrollView.h"

using namespace std;

namespace WebCore {

enum {
    topMargin,
    rightMargin,
    bottomMargin,
    leftMargin
};

Theme* platformTheme()
{
    static ThemeMac themeMac;
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

static LengthSize sizeFromFont(const Font& font, const LengthSize& zoomedSize, float zoomFactor, const IntSize* sizes)
{
    IntSize controlSize = sizes[controlSizeForFont(font)];
    if (zoomFactor != 1.0f)
        controlSize = IntSize(controlSize.width() * zoomFactor, controlSize.height() * zoomFactor);
    LengthSize result = zoomedSize;
    if (zoomedSize.width().isIntrinsicOrAuto() && controlSize.width() > 0)
        result.setWidth(Length(controlSize.width(), Fixed));
    if (zoomedSize.height().isIntrinsicOrAuto() && controlSize.height() > 0)
        result.setHeight(Length(controlSize.height(), Fixed));
    return result;
}

static void setControlSize(NSCell* cell, const IntSize* sizes, const IntSize& minZoomedSize, float zoomFactor)
{
    NSControlSize size;
    if (minZoomedSize.width() >= static_cast<int>(sizes[NSRegularControlSize].width() * zoomFactor) &&
        minZoomedSize.height() >= static_cast<int>(sizes[NSRegularControlSize].height() * zoomFactor))
        size = NSRegularControlSize;
    else if (minZoomedSize.width() >= static_cast<int>(sizes[NSSmallControlSize].width() * zoomFactor) &&
             minZoomedSize.height() >= static_cast<int>(sizes[NSSmallControlSize].height() * zoomFactor))
        size = NSSmallControlSize;
    else
        size = NSMiniControlSize;
    if (size != [cell controlSize]) // Only update if we have to, since AppKit does work even if the size is the same.
        [cell setControlSize:size];
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
        
    // Window inactive state does not need to be checked explicitly, since we paint parented to 
    // a view in a window whose key state can be detected.
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

static LengthSize checkboxSize(ControlPart part, const Font& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, checkboxSizes());
}

static NSButtonCell* checkbox(ControlStates states, const IntRect& zoomedRect, float zoomFactor)
{
    static NSButtonCell* checkboxCell;
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
    // Determine the width and height needed for the control and prepare the cell for painting.
    NSButtonCell* checkboxCell = checkbox(states, zoomedRect, zoomFactor);

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
    
    [checkboxCell drawWithFrame:NSRect(inflatedRect) inView:scrollView->documentView()];
    [checkboxCell setControlView:nil];

    context->restore();
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

static LengthSize radioSize(ControlPart part, const Font& font, const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width().isIntrinsicOrAuto() && !zoomedSize.height().isIntrinsicOrAuto())
        return zoomedSize;

    // Use the font size to determine the intrinsic width of the control.
    return sizeFromFont(font, zoomedSize, zoomFactor, radioSizes());
}

static NSButtonCell* radio(ControlStates states, const IntRect& zoomedRect, float zoomFactor)
{
    static NSButtonCell* radioCell;
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
    NSButtonCell* radioCell = radio(states, zoomedRect, zoomFactor);

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
    
    [radioCell drawWithFrame:NSRect(inflatedRect) inView:scrollView->documentView()];
    [radioCell setControlView:nil];

    context->restore();
}

// Theme overrides

int ThemeMac::baselinePositionAdjustment(ControlPart part) const
{
    if (part == CheckboxPart || part == RadioPart)
        return -2;
    return Theme::baselinePositionAdjustment(part);
}

LengthSize ThemeMac::controlSize(ControlPart part, const Font& font, const LengthSize& zoomedSize, float zoomFactor) const
{
    switch (part) {
        case CheckboxPart:
            return checkboxSize(part, font, zoomedSize, zoomFactor);
        case RadioPart:
            return radioSize(part, font, zoomedSize, zoomFactor);
        default:
            return zoomedSize;
    }
}

void ThemeMac::inflateControlPaintRect(ControlPart part, ControlStates states, IntRect& zoomedRect, float zoomFactor) const
{
    switch (part) {
        case CheckboxPart: {
            // We inflate the rect as needed to account for padding included in the cell to accommodate the checkbox
            // shadow" and the check.  We don't consider this part of the bounds of the control in WebKit.
            NSCell* cell = checkbox(states, zoomedRect, zoomFactor);
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
            NSCell* cell = radio(states, zoomedRect, zoomFactor);
            NSControlSize controlSize = [cell controlSize];
            IntSize zoomedSize = radioSizes()[controlSize];
            zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
            zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
            zoomedRect = inflateRect(zoomedRect, zoomedSize, radioMargins(controlSize), zoomFactor);
            break;
        }
        default:
            break;
    }
}

void ThemeMac::paint(ControlPart part, ControlStates states, GraphicsContext* context, const IntRect& zoomedRect, float zoomFactor, ScrollView* scrollView) const
{
    switch (part) {
        case CheckboxPart:
            paintCheckbox(states, context, zoomedRect, zoomFactor, scrollView);
            break;
        case RadioPart:
            paintRadio(states, context, zoomedRect, zoomFactor, scrollView);
            break;
        default:
            break;
    }
}

}
