/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#import "WebSystemBackdropLayer.h"

#import "GraphicsContextCG.h"
#import <QuartzCore/QuartzCore.h>

// FIXME: https://bugs.webkit.org/show_bug.cgi?id=146250
// These should provide the system recipes for the layers
// with the appropriate tinting, blending and blurring.

@implementation WebSystemBackdropLayer
@end

@implementation WebLightSystemBackdropLayer

- (id)init
{
    self = [super init];
    if (!self)
        return nil;

#ifndef NDEBUG
    [self setName:@"WebLightSystemBackdropLayer"];
#endif

    CGFloat components[4] = { 0.8, 0.8, 0.8, 0.8 };
    [super setBackgroundColor:adoptCF(CGColorCreate(WebCore::sRGBColorSpaceRef(), components)).get()];

    return self;
}

- (void)setBackgroundColor:(CGColorRef)backgroundColor
{
    // Empty implementation to stop clients pushing the wrong color.
    UNUSED_PARAM(backgroundColor);
}

@end

@implementation WebDarkSystemBackdropLayer

- (id)init
{
    self = [super init];
    if (!self)
        return nil;

#ifndef NDEBUG
    [self setName:@"WebDarkSystemBackdropLayer"];
#endif

    CGFloat components[4] = { 0.2, 0.2, 0.2, 0.8 };
    [super setBackgroundColor:adoptCF(CGColorCreate(WebCore::sRGBColorSpaceRef(), components)).get()];

    return self;
}

- (void)setBackgroundColor:(CGColorRef)backgroundColor
{
    // Empty implementation to stop clients pushing the wrong color.
    UNUSED_PARAM(backgroundColor);
}

@end
