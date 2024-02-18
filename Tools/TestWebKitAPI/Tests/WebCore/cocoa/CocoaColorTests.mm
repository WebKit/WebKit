/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#import "Test.h"
#import <WebCore/AttributedString.h>
#import <WebCore/ColorCocoa.h>

#import <WebCore/UIFoundationSoftLink.h>
#import <pal/ios/UIKitSoftLink.h>

using namespace WebCore;

namespace TestWebKitAPI {

static void testPlatformColor(const RetainPtr<id>& platformColor)
{
    Color color = colorFromCocoaColor(platformColor.get());
    RetainPtr<PlatformColor> convertedPlatformColor = cocoaColor(color);
    // Passing -isEqual: test for Objective-C color objects is a non-goal
    // since there are many subclasses and none test for color equality
    // between different subclasses.

    Color convertedColor = colorFromCocoaColor(convertedPlatformColor.get());
    EXPECT_EQ(color, convertedColor);
}

TEST(Color, Platform_Color)
{
    RetainPtr<PlatformColor> colorWithWhiteAlpha = [PlatformColorClass colorWithWhite:0.3 alpha:0.7];
    testPlatformColor(colorWithWhiteAlpha);

    RetainPtr<PlatformColor> colorWithHueSaturationBrightnessAlpha = [PlatformColorClass colorWithHue:0.2 saturation:0.4 brightness:0.6 alpha:0.8];
    testPlatformColor(colorWithHueSaturationBrightnessAlpha);

    RetainPtr<PlatformColor> colorWithRedGreenBlueAlpha = [PlatformColorClass colorWithRed:0.2 green:0.4 blue:0.6 alpha:0.8];
    testPlatformColor(colorWithRedGreenBlueAlpha);

    RetainPtr<PlatformColor> colorWithDisplayP3RedGreenBlueAlpha = [PlatformColorClass colorWithDisplayP3Red:0.2 green:0.4 blue:0.6 alpha:0.8];
    testPlatformColor(colorWithDisplayP3RedGreenBlueAlpha);

    auto sRGBColorSpace = adoptCF(CGColorSpaceCreateWithName(kCGColorSpaceSRGB));
    constexpr CGFloat testComponents[4] = { 1, .75, .5, .25 };
    auto cgColor = adoptCF(CGColorCreate(sRGBColorSpace.get(), testComponents));
    RetainPtr<PlatformColor> colorWithCGColor = [PlatformColorClass colorWithCGColor:cgColor.get()];
    testPlatformColor(colorWithCGColor);
}

TEST(Color, Platform_NSColor)
{
    @autoreleasepool {
        RetainPtr<PlatformNSColor> colorWithCalibratedRedGreenBlueAlpha = [PlatformNSColorClass colorWithCalibratedRed:0.75 green:0.25 blue:0.50 alpha:0.88];
        testPlatformColor(colorWithCalibratedRedGreenBlueAlpha);

        RetainPtr<PlatformNSColor> colorWithCalibratedWhiteAlpha = [PlatformNSColorClass colorWithCalibratedWhite:0.67 alpha:0.88];
        testPlatformColor(colorWithCalibratedWhiteAlpha);
    }
}

} // namespace TestWebKitAPI
