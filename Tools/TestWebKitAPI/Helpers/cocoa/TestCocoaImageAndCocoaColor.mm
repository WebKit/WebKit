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
#import "Helpers/cocoa/TestCocoaImageUtilities.h"
#import <wtf/RetainPtr.h>
#import <wtf/cocoa/TypeCastsCocoa.h>

namespace TestWebKitAPI::Util {

CocoaColor *pixelColor(CocoaImage *image, CGPoint point)
{
    return [TestCocoaImageUtilities pixelColorOfImage:image atPoint:point];
}

bool compareColors(CocoaColor *color1, CocoaColor *color2, float tolerance)
{
    return [TestCocoaImageUtilities compareColor:color1 toColor:color2 tolerance:tolerance];
}

NSData *makePDFData(CGSize size, SEL colorSelector)
{
    NSMutableData *data = [NSMutableData data];
    RetainPtr consumer = adoptCF(CGDataConsumerCreateWithCFData((__bridge CFMutableDataRef)data));

    auto mediaBox = CGRectMake(0, 0, size.width, size.height);
    RetainPtr context = adoptCF(CGPDFContextCreate(consumer.get(), &mediaBox, nullptr));

    NSDictionary *pageInfo = @{ bridge_cast(kCGPDFContextMediaBox): [NSData dataWithBytes:&mediaBox length:sizeof(mediaBox)] };
    CGPDFContextBeginPage(context.get(), (__bridge CFDictionaryRef)pageInfo);

    CocoaColor *color = [CocoaColor performSelector:colorSelector];
    CGContextSetFillColorWithColor(context.get(), color.CGColor);
    CGContextFillRect(context.get(), mediaBox);

    CGPDFContextEndPage(context.get());
    CGPDFContextClose(context.get());

    return data;
}

} // namespace TestWebKitAPI::Util
