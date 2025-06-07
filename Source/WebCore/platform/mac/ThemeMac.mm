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

LengthBox ThemeMac::controlBorder(StyleAppearance appearance, const FontCascade& font, const LengthBox& zoomedBox, float zoomFactor) const
{
    switch (appearance) {
    case StyleAppearance::SquareButton:
    case StyleAppearance::ColorWell:
    case StyleAppearance::DefaultButton:
    case StyleAppearance::Button:
        return LengthBox(0, zoomedBox.right().value(), 0, zoomedBox.left().value());
    default:
        return Theme::controlBorder(appearance, font, zoomedBox, zoomFactor);
    }
}

bool ThemeMac::userPrefersContrast() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldIncreaseContrast];
}

bool ThemeMac::userPrefersDifferentiationWithoutColor() const
{
    return [[NSWorkspace sharedWorkspace] accessibilityDisplayShouldDifferentiateWithoutColor];
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

#endif // PLATFORM(MAC)
