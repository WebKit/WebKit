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

#import "WebInspectorOutlineView.h"

@implementation WebInspectorOutlineView
- (BOOL)isOpaque
{
    return NO;
}

- (NSColor *)_highlightColorForCell:(NSCell *)cell
{
    // return nil to prevent normal selection drawing
    return nil;
}

- (void)_highlightRow:(int)row clipRect:(NSRect)clip {
    NSColor *selectionColor = nil;
    if ([[self window] firstResponder] == self && [[self window] isKeyWindow])
        selectionColor = [[NSColor alternateSelectedControlColor] colorWithAlphaComponent:0.333];
    selectionColor = [[NSColor alternateSelectedControlColor] colorWithAlphaComponent:0.45];

    if ([self numberOfColumns] > 0) {
        NSRect highlight = [self rectOfRow:row];
        NSRect drawRect = NSIntersectionRect(clip, highlight);
        BOOL solid = ([[self window] firstResponder] == self && [[self window] isKeyWindow]);
        if (NSContainsRect(drawRect,clip) && !NSEqualRects(drawRect,highlight)) {
            // draw a solid rect when the outline view wants to draw inside the row rect
            if ([NSGraphicsContext currentContextDrawingToScreen] && solid) {
                [selectionColor set];
                NSRectFillUsingOperation(drawRect, NSCompositeSourceOver);
            }
        } else {
            // draw a rounded rect for the full row
            if ([NSGraphicsContext currentContextDrawingToScreen]) {
                NSBezierPath *path = [[NSBezierPath alloc] init];

                float radius = 6.0;
                NSRect irect = NSInsetRect(highlight, radius, radius);
                if (!solid)
                    irect = NSInsetRect(irect, 1.0, 1.0);
                [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(irect), NSMinY(irect)) radius:radius startAngle:180. endAngle:270.];
                [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(irect), NSMinY(irect)) radius:radius startAngle:270. endAngle:360.];
                [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMaxX(irect), NSMaxY(irect)) radius:radius startAngle:0. endAngle:90.];
                [path appendBezierPathWithArcWithCenter:NSMakePoint(NSMinX(irect), NSMaxY(irect)) radius:radius startAngle:90. endAngle:180.];
                [path closePath];

                NSRectClip(clip);
                [selectionColor set];
                if (solid) {
                    [path fill];
                } else {
                    [path setLineWidth:2.0];
                    [path stroke];
                }

                [path release];
            }
        }
    }
}

- (void)drawBackgroundInClipRect:(NSRect)clipRect
{
    // don't draw a background
}
@end
