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

#import "KWQNSViewExtras.h"

#define MIN_INTERSECT_FOR_REVEAL 32

@implementation NSView (KWQNSViewExtras)

- (void)_KWQ_scrollFrameToVisible
{
    [self _KWQ_scrollRectToVisible:[self bounds] forceCentering:NO];
}

- (void)_KWQ_scrollRectToVisible:(NSRect)rect forceCentering:(BOOL)forceCentering
{
    [self _KWQ_scrollRectToVisible:rect inView:self forceCentering:forceCentering];
}

- (void)_KWQ_scrollRectToVisible:(NSRect)rect inView:(NSView *)view forceCentering:(BOOL)forceCentering
{
    [[self superview] _KWQ_scrollRectToVisible:rect inView:view forceCentering:forceCentering];
}

- (void)_KWQ_scrollPointRecursive:(NSPoint)p
{
    [self _KWQ_scrollPointRecursive:p inView:self];
}

- (void)_KWQ_scrollPointRecursive:(NSPoint)p inView:(NSView *)view
{
    p = [self convertPoint: p fromView:view];
    [self scrollPoint: p];
    [[self superview] _KWQ_scrollPointRecursive:p inView:self];
}

@end

@implementation NSClipView (KWQNSViewExtras)

- (void)_KWQ_scrollRectToVisible:(NSRect)rect inView:(NSView *)view forceCentering:(BOOL)forceCentering
{
    NSRect exposeRect = [self convertRect:rect fromView:view];
    NSRect visibleRect = [self bounds];
    
    if (forceCentering || !NSContainsRect(visibleRect, exposeRect)) {
    
        // First check whether enough of the desired rect is already visible horizontally. If so, and we're not forcing centering,
        // we don't want to scroll horizontally because doing so is surprising.
        if (!forceCentering && NSIntersectionRect(visibleRect, exposeRect).size.width >= MIN_INTERSECT_FOR_REVEAL) {
            exposeRect.origin.x = visibleRect.origin.x;
            exposeRect.size.width = visibleRect.size.width;
        } else if (exposeRect.size.width >= visibleRect.size.width) {
            if (forceCentering) {
                float expLeft = exposeRect.origin.x;
                float expRight = exposeRect.origin.x + exposeRect.size.width;
                float visLeft = visibleRect.origin.x;
                float visRight = visibleRect.origin.x + visibleRect.size.width;
                float diffLeft = visLeft - expLeft;
                float diffRight = expRight - visRight;
                if (diffLeft >= 0 && diffRight >= 0) {
                    // Exposed rect fills the visible rect.
                    // We don't want the view to budge.
                    exposeRect.origin.x = visLeft;
                }
                else if (diffLeft < 0) {
                    // Scroll so left of visible region shows left of expose rect.
                    // Leave expose origin as it is to make this happen.
                }
                else {
                    // Scroll so right of visible region shows right of expose rect.
                    exposeRect.origin.x = expRight - visibleRect.size.width;
                }
            }
            exposeRect.size.width = visibleRect.size.width;
        } else {
            exposeRect.origin.x -= (visibleRect.size.width - exposeRect.size.width) / 2.0;
            exposeRect.size.width += (visibleRect.size.width - exposeRect.size.width);
        }
               
        // Make an expose rectangle that will end up centering the passed-in rectangle vertically.
        if (exposeRect.size.height >= visibleRect.size.height) {
            if (forceCentering) {
                if ([self isFlipped]) {
                    float expTop = exposeRect.origin.y;
                    float expBottom = exposeRect.origin.y + exposeRect.size.height;
                    float visTop = visibleRect.origin.y;
                    float visBottom = visibleRect.origin.y + visibleRect.size.height;
                    float diffTop = visTop - expTop;
                    float diffBottom = expBottom - visBottom;
                    if (diffTop >= 0 && diffBottom >= 0) {
                        // Exposed rect fills the visible rect.
                        // We don't want the view to budge.
                        exposeRect.origin.y = visTop;
                    }
                    else if (diffTop < 0) {
                        // Scroll so top of visible region shows top of expose rect.
                        // Leave expose origin as it is to make this happen.
                    }
                    else {
                        // Scroll so bottom of visible region shows bottom of expose rect.
                        exposeRect.origin.y = expBottom - visibleRect.size.height;
                    }
                }
                else {
                    float expTop = exposeRect.origin.y + exposeRect.size.height;
                    float expBottom = exposeRect.origin.y;
                    float visTop = visibleRect.origin.y + visibleRect.size.height;
                    float visBottom = visibleRect.origin.y;
                    float diffTop = expTop - visTop;
                    float diffBottom = visBottom - expBottom;
                    if (diffTop >= 0 && diffBottom >= 0) {
                        // Exposed rect fills the visible rect.
                        // We don't want the view to budge.
                        exposeRect.origin.y = visBottom;
                    }
                    else if (diffTop < 0) {
                        // Scroll so top of visible region shows top of expose rect.
                        exposeRect.origin.y = expTop + visibleRect.size.height;
                    }
                    else {
                        // Scroll so bottom of visible region shows bottom of expose rect.
                        // Leave expose origin as it is to make this happen.
                    }
                } 
            }
            exposeRect.size.height = visibleRect.size.height;
        } 
        else {
            if ([self isFlipped]) {
                exposeRect.origin.y -= (visibleRect.size.height - exposeRect.size.height) / 2.0;
            } 
            else {
                exposeRect.origin.y += (visibleRect.size.height - exposeRect.size.height) / 2.0;
            }
            exposeRect.size.height += (visibleRect.size.height - exposeRect.size.height);
        }
        
        [self scrollRectToVisible:exposeRect];
    }

    [super _KWQ_scrollRectToVisible:rect inView:view forceCentering:forceCentering];
}


@end
