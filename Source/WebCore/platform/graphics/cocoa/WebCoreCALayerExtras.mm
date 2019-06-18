/*
 * Copyright (C) 2013-2017 Apple Inc. All rights reserved.
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
#import "WebCoreCALayerExtras.h"

#import <pal/spi/cocoa/QuartzCoreSPI.h>

@implementation CALayer (WebCoreCALayerExtras)

- (void)web_disableAllActions
{
    NSNull *nullValue = [NSNull null];
    self.style = @{
        @"actions" : @{
            @"anchorPoint" : nullValue,
            @"anchorPointZ" : nullValue,
            @"backgroundColor" : nullValue,
            @"borderColor" : nullValue,
            @"borderWidth" : nullValue,
            @"bounds" : nullValue,
            @"contents" : nullValue,
            @"contentsRect" : nullValue,
            @"contentsScale" : nullValue,
            @"cornerRadius" : nullValue,
            @"opacity" : nullValue,
            @"position" : nullValue,
            @"shadowColor" : nullValue,
            @"sublayerTransform" : nullValue,
            @"sublayers" : nullValue,
            @"transform" : nullValue,
            @"zPosition" : nullValue
        }
    };
}

- (void)_web_setLayerBoundsOrigin:(CGPoint)origin
{
    CGRect bounds = [self bounds];
    bounds.origin = origin;
    [self setBounds:bounds];
}

- (void)_web_setLayerTopLeftPosition:(CGPoint)position
{
    CGSize layerSize = [self bounds].size;
    CGPoint anchorPoint = [self anchorPoint];
    CGPoint newPosition = CGPointMake(position.x + anchorPoint.x * layerSize.width, position.y + anchorPoint.y * layerSize.height);
    if (isnan(newPosition.x) || isnan(newPosition.y)) {
        WTFLogAlways("Attempt to call [CALayer setPosition] with NaN: newPosition=(%f, %f) position=(%f, %f) anchorPoint=(%f, %f)",
            newPosition.x, newPosition.y, position.x, position.y, anchorPoint.x, anchorPoint.y);
        ASSERT_NOT_REACHED();
        return;
    }
    
    [self setPosition:newPosition];
}

+ (CALayer *)_web_renderLayerWithContextID:(uint32_t)contextID
{
    CALayerHost *layerHost = [CALayerHost layer];
    layerHost.contextId = contextID;
    return layerHost;
}

@end
