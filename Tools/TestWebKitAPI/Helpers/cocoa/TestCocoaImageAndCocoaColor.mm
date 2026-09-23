/*
 * Copyright (C) 2022-2025 Apple Inc. All rights reserved.
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
#import "Helpers/cocoa/TestCocoaImageAndCocoaColor.h"

namespace TestWebKitAPI::Util {

CocoaColor *pixelColor(CocoaImage *image, CGPoint point)
{
#if USE(APPKIT)
    auto imageRef = [image CGImageForProposedRect:nullptr context:nil hints:nil];
    auto *bitmap = [[NSBitmapImageRep alloc] initWithCGImage:imageRef];
    auto *color = [bitmap colorAtX:point.x y:point.y];
    return color;
#else
    image = [image.imageAsset imageWithTraitCollection:UITraitCollection.currentTraitCollection];

    UIGraphicsBeginImageContext(image.size);

    [image drawAtPoint:CGPointZero];

    auto context = UIGraphicsGetCurrentContext();
    auto *data = (unsigned char *)CGBitmapContextGetData(context);
    if (!data)
        return nil;

    unsigned offset = ((image.size.width * point.y) + point.x) * 4;
    auto *color = [UIColor colorWithRed:data[offset] / 255.0 green:data[offset + 1] / 255.0 blue:data[offset + 2] / 255.0 alpha:data[offset + 3] / 255.0];

    UIGraphicsEndImageContext();

    return color;
#endif
}

CocoaColor *toSRGBColor(CocoaColor *color)
{
#if USE(APPKIT)
    return [color colorUsingColorSpace:NSColorSpace.sRGBColorSpace];
#else
    return color;
#endif
}

bool compareColors(CocoaColor *color1, CocoaColor *color2, float tolerance)
{
    if (color1 == color2 || [color1 isEqual:color2])
        return true;

    if (!color1 || !color2)
        return false;

    color1 = toSRGBColor(color1);
    color2 = toSRGBColor(color2);

    CGFloat red1, green1, blue1, alpha1;
    [color1 getRed:&red1 green:&green1 blue:&blue1 alpha:&alpha1];

    CGFloat red2, green2, blue2, alpha2;
    [color2 getRed:&red2 green:&green2 blue:&blue2 alpha:&alpha2];

    return fabs(red1 - red2) < tolerance && fabs(green1 - green2) < tolerance && fabs(blue1 - blue2) < tolerance && fabs(alpha1 - alpha2) < tolerance;
}

} // namespace TestWebKitAPI::Util
