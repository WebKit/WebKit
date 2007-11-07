/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebNodeHighlightView.h"
#import "WebNodeHighlight.h"
#import "WebNSViewExtras.h"

#import <WebKit/DOMCore.h>
#import <WebKit/DOMExtensions.h>

#import <JavaScriptCore/Assertions.h>

#define OVERLAY_MAX_ALPHA 0.7
#define OVERLAY_WHITE_VALUE 0.1

#define WHITE_FRAME_THICKNESS 1.0

@interface WebNodeHighlightView (FileInternal)
- (NSArray *)_holes;
@end

@implementation WebNodeHighlightView

- (id)initWithWebNodeHighlight:(WebNodeHighlight *)webNodeHighlight
{
    self = [self initWithFrame:NSZeroRect];
    if (!self)
        return nil;

    _webNodeHighlight = [webNodeHighlight retain];

    return self;
}

- (void)dealloc
{
    [self detachFromWebNodeHighlight];
    [super dealloc];
}

- (void)detachFromWebNodeHighlight
{
    [_webNodeHighlight release];
    _webNodeHighlight = nil;
}

- (void)drawRect:(NSRect)rect 
{
    [NSGraphicsContext saveGraphicsState];

    // draw translucent gray fill, out of which we will cut holes
    [[NSColor colorWithCalibratedWhite:OVERLAY_WHITE_VALUE alpha:(_fractionFadedIn * OVERLAY_MAX_ALPHA)] set];
    NSRectFill(rect);

    // determine set of holes
    NSArray *holes = [self _holes];
    int holeCount = [holes count];
    int holeIndex;

    // Draw white frames around holes in first pass, so they will be erased in
    // places where holes overlap or abut.
    [[NSColor colorWithCalibratedWhite:1.0 alpha:_fractionFadedIn] set];

    // white frame is just outside of the hole that the delegate returned
    for (holeIndex = 0; holeIndex < holeCount; ++holeIndex) {
        NSRect hole = [[holes objectAtIndex:holeIndex] rectValue];
        hole = NSInsetRect(hole, -WHITE_FRAME_THICKNESS, -WHITE_FRAME_THICKNESS);
        NSRectFill(hole);
    }

    [[NSColor clearColor] set];

    // Erase holes in second pass.
    for (holeIndex = 0; holeIndex < holeCount; ++holeIndex)
        NSRectFill([[holes objectAtIndex:holeIndex] rectValue]);

    [NSGraphicsContext restoreGraphicsState];
}

- (WebNodeHighlight *)webNodeHighlight
{
    return _webNodeHighlight;
}

- (float)fractionFadedIn
{
    return _fractionFadedIn;
}

- (void)setFractionFadedIn:(float)fraction
{
    ASSERT_ARG(fraction, fraction >= 0.0 && fraction <= 1.0);

    if (_fractionFadedIn == fraction)
        return;
    
    _fractionFadedIn = fraction;
    [self setNeedsDisplay:YES];
}

- (void)setHolesNeedUpdateInRect:(NSRect)rect
{
    // Redisplay a slightly larger rect to account for white border around holes
    rect = NSInsetRect(rect, -1 * WHITE_FRAME_THICKNESS, 
                             -1 * WHITE_FRAME_THICKNESS);

    [self setNeedsDisplayInRect:rect];
}

@end

@implementation WebNodeHighlightView (FileInternal)

- (NSArray *)_holes
{
    DOMNode *node = [_webNodeHighlight highlightedNode];

    // FIXME: node view needs to be the correct frame document view, it isn't always the main frame
    NSView *nodeView = [_webNodeHighlight targetView];

    NSArray *lineBoxRects = nil;
    if ([node isKindOfClass:[DOMElement class]]) {
        DOMCSSStyleDeclaration *style = [[node ownerDocument] getComputedStyle:(DOMElement *)node pseudoElement:@""];
        if ([[style getPropertyValue:@"display"] isEqualToString:@"inline"])
            lineBoxRects = [node lineBoxRects];
    } else if ([node isKindOfClass:[DOMText class]]) {
#if ENABLE(SVG)
        if (![[node parentNode] isKindOfClass:NSClassFromString(@"DOMSVGElement")])
#endif
            lineBoxRects = [node lineBoxRects];
    }

    if (![lineBoxRects count]) {
        NSRect boundingBox = [nodeView _web_convertRect:[node boundingBox] toView:self];
        return [NSArray arrayWithObject:[NSValue valueWithRect:boundingBox]];
    }

    NSMutableArray *rects = [[NSMutableArray alloc] initWithCapacity:[lineBoxRects count]];

    unsigned lineBoxRectCount = [lineBoxRects count];
    for (unsigned lineBoxRectIndex = 0; lineBoxRectIndex < lineBoxRectCount; ++lineBoxRectIndex) {
        NSRect r = [[lineBoxRects objectAtIndex:lineBoxRectIndex] rectValue];
        NSRect overlayViewRect = [nodeView _web_convertRect:r toView:self];
        [rects addObject:[NSValue valueWithRect:overlayViewRect]];
    }

    return [rects autorelease];
}

@end
