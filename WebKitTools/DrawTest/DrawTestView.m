/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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

#import "DrawTestView.h"

#import <WebCore+SVG/DrawViewPrivate.h>
#import <WebCore+SVG/DrawDocumentPrivate.h>

@implementation DrawTestView

- (id)initWithFrame:(NSRect)frame
{
    if (self = [super initWithFrame:frame]) {
        //[self setValue:[NSNumber numberWithBool:YES] forKey:@"showDebugString"];
        //[self setValue:[NSNumber numberWithBool:YES] forKey:@"showDebugAxes"];
    }
    return self;
}

- (void)drawAxes:(float)length
{
    NSBezierPath *xAxis = [NSBezierPath bezierPath];
    [xAxis moveToPoint:NSMakePoint(-.5 * length, 0)];
    [xAxis lineToPoint:NSMakePoint(length, 0)];
    [[NSColor redColor] set];
    [xAxis stroke];
    
    NSBezierPath *yAxis = [NSBezierPath bezierPath];
    [yAxis moveToPoint:NSMakePoint(0, -.5 * length)];
    [yAxis lineToPoint:NSMakePoint(0, length)];
    [[NSColor greenColor] set];
    [yAxis stroke];
}

- (void)drawRect:(NSRect)dirtyRect
{
    [super drawRect:dirtyRect];
    
    if (_showDebugString) {
        // draw the current zoom/pan
        NSString *infoString = [NSString stringWithFormat:@"viewport origin: %@ canvas size: %@ zoom: %f",
            NSStringFromPoint([self canvasVisibleOrigin]),
            NSStringFromSize([[self document] canvasSize]), [self canvasZoom]];
        [infoString drawAtPoint:NSMakePoint(5, 13) withAttributes:nil];
    }
    
    if (_showDebugAxes) {
        CGContextRef context = (CGContextRef)[[NSGraphicsContext currentContext] graphicsPort];
        CGContextSaveGState(context);
        CGContextConcatCTM(context, CGAffineTransformInvert([self transformFromViewToCanvas]));
        [self drawAxes:100];
        CGContextRestoreGState(context);
    }
}

- (IBAction)toggleShowDebugString:(id)sender
{
    [self setValue:[NSNumber numberWithBool:!_showDebugString] forKey:@"showDebugString"];
    [self setNeedsDisplay:YES];
}

- (IBAction)toggleShowDebugAxes:(id)sender
{
    [self setValue:[NSNumber numberWithBool:!_showDebugAxes] forKey:@"showDebugAxes"];
    [self setNeedsDisplay:YES];
}

- (IBAction)toggleFilterSupport:(id)sender
{
    [DrawView setFilterSupportEnabled:[DrawView isFilterSupportEnabled]];
    [self setNeedsDisplay:YES];
}

- (BOOL)acceptsFirstResponder
{
    return YES;
}

@end
