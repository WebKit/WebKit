/*
 * Copyright (C) 2008-2023 Apple Inc. All rights reserved.
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

#import "FontCascade.h"
#import "LengthSize.h"
#import <pal/spi/mac/CoreUISPI.h>
#import <pal/spi/mac/NSAppearanceSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/StdLibExtras.h>

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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
    auto fontSize = font.size();
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
    auto fontSize = font.size();
    if (fontSize >= 23 && ThemeMac::supportsLargeFormControls())
        return NSControlSizeLarge;
    if (fontSize >= 18)
        return NSControlSizeRegular;
    if (fontSize >= 13)
        return NSControlSizeSmall;
    return NSControlSizeMini;
}

// Switch

static const std::array<IntSize, 4>& switchSizes()
{
    static const std::array<IntSize, 4> sizes =
    {
        IntSize { 38, 22 },
        IntSize { 32, 18 },
        IntSize { 26, 15 },
        IntSize { 38, 22 }
    };
    return sizes;
}

static const int* switchMargins(NSControlSize controlSize)
{
    static const int margins[4][4] =
    {
        // top right bottom left
        { 2, 2, 1, 2 },
        { 2, 2, 1, 2 },
        { 1, 1, 0, 1 },
        { 2, 2, 1, 2 },
    };
    return margins[controlSize];
}

static const int* visualSwitchMargins(NSControlSize controlSize, bool isVertical)
{
    auto margins = switchMargins(controlSize);
    if (isVertical) {
        static const int verticalMargins[4] = { margins[3], margins[0], margins[1], margins[2] };
        return verticalMargins;
    }
    return margins;
}

static LengthSize switchSize(const LengthSize& zoomedSize, float zoomFactor)
{
    // If the width and height are both specified, then we have nothing to do.
    if (!zoomedSize.width.isIntrinsicOrAuto() && !zoomedSize.height.isIntrinsicOrAuto())
        return zoomedSize;

    return sizeFromNSControlSize(NSControlSizeSmall, zoomedSize, zoomFactor, switchSizes());
}

// Theme overrides

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
    case StyleAppearance::Switch:
        return switchSize(zoomedSize, zoomFactor);
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

void ThemeMac::inflateControlPaintRect(StyleAppearance appearance, FloatRect& zoomedRect, float zoomFactor, bool isVertical)
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS
    auto zoomRectSize = IntSize(zoomedRect.size());
    switch (appearance) {
    case StyleAppearance::Checkbox: {
        auto size = controlSizeFromPixelSize(checkboxSizes(), zoomRectSize, zoomFactor);
        auto zoomedSize = checkboxSizes()[size];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, checkboxMargins(size), zoomFactor);
        break;
    }
    case StyleAppearance::Radio: {
        auto size = controlSizeFromPixelSize(radioSizes(), zoomRectSize, zoomFactor);
        auto zoomedSize = radioSizes()[size];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        zoomedRect = inflateRect(zoomedRect, zoomedSize, radioMargins(size), zoomFactor);
        break;
    }
    case StyleAppearance::Switch: {
        auto logicalZoomRectSize = isVertical ? zoomRectSize.transposedSize() : zoomRectSize;
        auto controlSize = controlSizeFromPixelSize(switchSizes(), logicalZoomRectSize, zoomFactor);
        auto zoomedSize = switchSizes()[controlSize];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedSize.width() * zoomFactor);
        if (isVertical)
            zoomedSize = zoomedSize.transposedSize();
        zoomedRect = inflateRect(zoomedRect, zoomedSize, visualSwitchMargins(controlSize, isVertical), zoomFactor);
        break;
    }
    case StyleAppearance::PushButton:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button: {
        auto largestControlSize = ThemeMac::supportsLargeFormControls() ? NSControlSizeLarge : NSControlSizeRegular;
        if (zoomedRect.height() > buttonSizes()[largestControlSize].height() * zoomFactor)
            break;
        auto size = controlSizeFromPixelSize(buttonSizes(), zoomRectSize, zoomFactor);
        auto zoomedSize = buttonSizes()[size];
        zoomedSize.setHeight(zoomedSize.height() * zoomFactor);
        zoomedSize.setWidth(zoomedRect.width()); // Buttons don't ever constrain width, so the zoomed width can just be honored.
        zoomedRect = inflateRect(zoomedRect, zoomedSize, buttonMargins(size), zoomFactor);
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

bool ThemeMac::userPrefersContrast() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
}

bool ThemeMac::userPrefersReducedMotion() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldReduceMotion];
}

bool ThemeMac::supportsLargeFormControls()
{
    static bool hasSupport = [[NSAppearance currentDrawingAppearance] _usesMetricsAppearance];
    return hasSupport;
}

}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // PLATFORM(MAC)
