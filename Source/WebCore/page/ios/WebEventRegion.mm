/*
 * Copyright (C) 2008, 2009 Apple Inc. All rights reserved.
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
#import "WebEventRegion.h"
 
#if ENABLE(IOS_TOUCH_EVENTS)

#import "FloatQuad.h"

using namespace WebCore;

@interface WebEventRegion(Private)
- (FloatQuad)quad;
@end

@implementation WebEventRegion

- (id)initWithPoints:(CGPoint)inP1 :(CGPoint)inP2 :(CGPoint)inP3 :(CGPoint)inP4
{
    if (!(self = [super init]))
        return nil;
        
    p1 = inP1;
    p2 = inP2;
    p3 = inP3;
    p4 = inP4;
    return self;
}

- (id)copyWithZone:(NSZone *)zone
{
    UNUSED_PARAM(zone);
    return [self retain];
}

- (NSString *)description
{
    return [NSString stringWithFormat:@"p1:{%g, %g} p2:{%g, %g} p3:{%g, %g} p4:{%g, %g}", p1.x, p1.y, p2.x, p2.y, p3.x, p3.y, p4.x, p4.y];
}

- (BOOL)hitTest:(CGPoint)point
{
    return [self quad].containsPoint(point);
}

// FIXME: Overriding isEqual: without overriding hash will cause trouble if this ever goes into an NSSet or is the key in an NSDictionary,
// since two equal objects could have different hashes.
- (BOOL)isEqual:(id)other
{
    if (![other isKindOfClass:[WebEventRegion class]])
        return NO;
    return CGPointEqualToPoint(p1, ((WebEventRegion *)other)->p1)
        && CGPointEqualToPoint(p2, ((WebEventRegion *)other)->p2)
        && CGPointEqualToPoint(p3, ((WebEventRegion *)other)->p3)
        && CGPointEqualToPoint(p4, ((WebEventRegion *)other)->p4);
}

- (FloatQuad)quad
{
    return FloatQuad(p1, p2, p3, p4);
}

- (CGPoint)p1
{
    return p1;
}

- (CGPoint)p2
{
    return p2;
}

- (CGPoint)p3
{
    return p3;
}

- (CGPoint)p4
{
    return p4;
}

@end

#endif // ENABLE(IOS_TOUCH_EVENTS)
