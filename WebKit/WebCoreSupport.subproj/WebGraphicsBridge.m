/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
                                        * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
                                        * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#import "WebGraphicsBridge.h"

#import "WebAssertions.h"

#ifndef USE_APPEARANCE
#define USE_APPEARANCE 1
#endif
#import <AppKit/NSInterfaceStyle_Private.h>
#import <AppKit/NSWindow_Private.h>
#import <AppKit/NSView_Private.h>
#import <CoreGraphics/CGContextGState.h>
#import <CoreGraphics/CGStyle.h>

@interface NSView (AppKitSecretsWebGraphicsBridgeKnowsAbout)
- (NSView *)_clipViewAncestor;
@end

@implementation WebGraphicsBridge

+ (void)createSharedBridge
{
    if (![self sharedBridge]) {
        [[[self alloc] init] release];
    }
    ASSERT([[self sharedBridge] isKindOfClass:self]);
}

+ (WebGraphicsBridge *)sharedBridge;
{
    return (WebGraphicsBridge *)[super sharedBridge];
}

- (void)setFocusRingStyle:(NSFocusRingPlacement)placement radius:(int)radius color:(NSColor *)color
{
    // get clip bounds
    NSView *clipView = [[NSView focusView] _clipViewAncestor];
    NSRect clipRect = [clipView convertRect:[clipView _focusRingVisibleRect] toView:nil];
    
#if SCALE
    if (__NSHasDisplayScaleFactor(NULL)) {
        float scaleFactor = [[view window] _scaleFactor];
        clipRect.size.width *= scaleFactor;
        clipRect.size.height *= scaleFactor;
    }
#endif
    
    // put together the style
    CGFocusRingStyle focusRingStyle;
    CGStyleRef focusRingStyleRef;
    focusRingStyle.version    = 0;
    focusRingStyle.ordering   = (CGFocusRingOrdering)placement;
    focusRingStyle.alpha      = .5;
    focusRingStyle.radius     = radius ? radius : kCGFocusRingRadiusDefault;
    focusRingStyle.threshold  = kCGFocusRingThresholdDefault;
    focusRingStyle.bounds     = *(CGRect*)&clipRect;
    focusRingStyle.accumulate = 0;
    focusRingStyle.tint = _NSDefaultControlTint() == NSBlueControlTint ? kCGFocusRingTintBlue : kCGFocusRingTintGraphite;
    
    NSColor *ringColor = [color colorUsingColorSpaceName:NSCalibratedRGBColorSpace];
    if (ringColor) {
        float c[4];
        [ringColor getRed:&c[0] green:&c[1] blue:&c[2] alpha:&c[3]];
        CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
        CGColorRef colorRef = CGColorCreate(colorSpace, c);
        CGColorSpaceRelease(colorSpace);
        focusRingStyleRef = CGStyleCreateFocusRingWithColor(&focusRingStyle, colorRef);
        CGColorRelease(colorRef);
    }
    else {
        focusRingStyleRef = CGStyleCreateFocusRing(&focusRingStyle);
    }
    CGContextSetStyle([[NSGraphicsContext currentContext] graphicsPort], focusRingStyleRef);
    CGStyleRelease(focusRingStyleRef);
}

@end
