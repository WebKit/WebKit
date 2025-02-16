/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "ColorCocoa.h"

#if PLATFORM(IOS_FAMILY)

#import "ColorSpaceCG.h"
#import <UIKit/UIKit.h>

namespace WebCore {

Color colorFromCocoaColor(UIColor *color)
{
    if (!color)
        return { };

    // FIXME: ExtendedColor - needs to handle color spaces.

    // FIXME: Make this work for a UIColor that was created from a pattern or a DispayP3 color.
    CGFloat redComponent;
    CGFloat greenComponent;
    CGFloat blueComponent;
    CGFloat alpha;

    BOOL success = [color getRed:&redComponent green:&greenComponent blue:&blueComponent alpha:&alpha];
    if (!success) {
        // The color space conversion above can fail if the UIColor is in an incompatible color space.
        // To workaround this we simply draw a one pixel image of the color and use that pixel's color.
        uint8_t pixel[4];
        auto bitmapContext = adoptCF(CGBitmapContextCreate(pixel, 1, 1, 8, 4, sRGBColorSpaceRef(), kCGImageAlphaPremultipliedLast));

        CGContextSetFillColorWithColor(bitmapContext.get(), color.CGColor);
        CGContextFillRect(bitmapContext.get(), CGRectMake(0, 0, 1, 1));

        return makeFromComponentsClamping<SRGBA<uint8_t>>(pixel[0], pixel[1], pixel[2], pixel[3]);
    }

    return convertColor<SRGBA<uint8_t>>(SRGBA<float> { static_cast<float>(redComponent), static_cast<float>(greenComponent), static_cast<float>(blueComponent), static_cast<float>(alpha) });
}

} // namespace WebCore

#endif
