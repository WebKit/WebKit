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

#import "WebNodeHighlight.h"
#import "WebNodeHighlightView.h"
#import <WebKitSystemInterface.h>

@implementation WebNodeHighlightView
- (NSBezierPath *)roundedRect:(NSRect)rect withRadius:(float)radius
{
    NSBezierPath *path = [[NSBezierPath alloc] init];

    NSRect irect = NSInsetRect( rect, radius, radius );
    [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(irect), NSMinY(irect)) radius:radius startAngle:180. endAngle:270.];
    [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(irect), NSMinY(irect)) radius:radius startAngle:270. endAngle:360.];
    [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(irect), NSMaxY(irect)) radius:radius startAngle:0. endAngle:90.];
    [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(irect), NSMaxY(irect)) radius:radius startAngle:90. endAngle:180.];
    [path closePath];

    return [path autorelease];
}

- (id)initWithHighlight:(WebNodeHighlight *)highlight andRects:(NSArray *)rects forView:(NSView *)view
{
    if (![self init])
        return nil;

    _highlight = highlight; // don't retain, would cause a circular retain

    NSRect visibleRect = [view visibleRect];

    NSRect rect = NSZeroRect;
    NSBezierPath *path = nil;
    NSBezierPath *straightPath = nil;

    if([rects count] == 1) {
        NSValue *value = (NSValue *)[rects objectAtIndex:0];
        rect = NSInsetRect([value rectValue], -1.0, -1.0);
        rect = NSIntersectionRect(rect, visibleRect);
        if (!NSIsEmptyRect(rect))
            path = [[self roundedRect:rect withRadius:3.0] retain];

        // shift everything to the corner
        NSAffineTransform *transform = [[NSAffineTransform alloc] init];
        [transform translateXBy:(NSMinX(rect) * -1.0) + 2.5 yBy:(NSMinY(rect) * -1.0) + 2.5];
        [path transformUsingAffineTransform:transform];
        [straightPath transformUsingAffineTransform:transform];
        [transform release];
    } else if ([rects count] > 1) {
        path = [[NSBezierPath alloc] init];
        straightPath = [path copy];

        // roundedRect: returns an autoreleased path, so release them soon with a pool
        NSAutoreleasePool *pool = [[NSAutoreleasePool alloc] init];

        NSEnumerator *enumerator = [rects objectEnumerator];
        NSValue *value = nil;
        while ((value = [enumerator nextObject])) {
            rect = NSIntersectionRect([value rectValue], visibleRect);
            if (!NSIsEmptyRect(rect)) {
                [straightPath appendBezierPathWithRect:rect];
                [path appendBezierPath:[self roundedRect:rect withRadius:3.0]];
            }
        }

        [pool drain];
        [pool release];

        rect = [path bounds];

        [straightPath setWindingRule:NSNonZeroWindingRule];
        [straightPath setLineJoinStyle:NSRoundLineJoinStyle];
        [straightPath setLineCapStyle:NSRoundLineCapStyle];

        // multiple rects we get from WebCore need flipped to show up correctly
        NSAffineTransform *transform = [[NSAffineTransform alloc] init];
        [transform scaleXBy:1.0 yBy:-1.0];
        [path transformUsingAffineTransform:transform];
        [straightPath transformUsingAffineTransform:transform];
        [transform release];

        // shift everything to the corner
        transform = [[NSAffineTransform alloc] init];
        [transform translateXBy:(NSMinX(rect) * -1.0) + 2.5 yBy:NSMaxY(rect) + 2.5];
        [path transformUsingAffineTransform:transform];
        [straightPath transformUsingAffineTransform:transform];
        [transform release];
    }

    if (!path || [path isEmpty]) {
        [self release];
        return nil;
    }

    [path setWindingRule:NSNonZeroWindingRule];
    [path setLineJoinStyle:NSRoundLineJoinStyle];
    [path setLineCapStyle:NSRoundLineCapStyle];

    // make the drawing area larger for the focus ring blur
    rect = [path bounds];
    rect.size.width += 5.0;
    rect.size.height += 5.0;
    [self setFrameSize:rect.size];

    // draw into an image
    _highlightRingImage = [[NSImage alloc] initWithSize:rect.size];
    [_highlightRingImage lockFocus];
    [NSGraphicsContext saveGraphicsState];

    if (straightPath) {
        [[NSColor redColor] set];
        [path setLineWidth:4.0];
        [path stroke];

        // clear the center to eliminate thick inner strokes for overlapping rects
        [[NSGraphicsContext currentContext] setCompositingOperation:NSCompositeClear];
        [path fill];

        // stroke the straight line path with a light color to show any inner rects
        [[NSGraphicsContext currentContext] setCompositingOperation:NSCompositeDestinationOver];
        [[[NSColor redColor] colorWithAlphaComponent:0.6] set];
        [straightPath setLineWidth:1.0];
        [straightPath stroke];
    } else {
        [[NSColor redColor] set];
        [path setLineWidth:2.0];
        [path stroke];
    }

    [NSGraphicsContext restoreGraphicsState];
    [_highlightRingImage unlockFocus];

    [path release];
    [straightPath release];

    return self;
}

- (void)dealloc
{
    [_highlightRingImage release];
    [super dealloc];
}

- (BOOL)isOpaque
{
    return NO;
}

- (void)drawRect:(NSRect)rect
{
    double alpha = 1.0 - [_highlight fractionComplete];
    if (alpha > 1.0)
        alpha = 1.0;
    else if (alpha < 0.0)
        alpha = 0.0;

    [_highlightRingImage drawInRect:rect fromRect:rect operation:NSCompositeCopy fraction:alpha];
}
@end
