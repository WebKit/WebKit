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

@implementation NSView (KWQNSViewExtras)

- (void)_KWQ_scrollFrameToVisible
{
    [self _KWQ_scrollRectToVisible:[self bounds]];
}

- (void)_KWQ_scrollRectToVisible:(NSRect)rect
{
    [self _KWQ_scrollRectToVisible:rect inView:self];
}

- (void)_KWQ_scrollRectToVisible:(NSRect)rect inView:(NSView *)view
{
    [[self superview] _KWQ_scrollRectToVisible:rect inView:view];
}

@end

@implementation NSClipView (KWQNSViewExtras)

- (void)_KWQ_scrollRectToVisible:(NSRect)rect inView:(NSView *)view
{
    NSRect exposeRect = [self convertRect:rect fromView:view];
    NSRect visibleRect = [self bounds];
    
    if (!NSContainsRect(visibleRect, exposeRect)) {
        // Make an expose rectangle that will end up centering the passed-in rectangle horizontally.
        if (exposeRect.size.width >= visibleRect.size.width) {
            exposeRect.size.width = visibleRect.size.width;
        } else {
            exposeRect.origin.x -= (visibleRect.size.width - exposeRect.size.width) / 2.0;
            exposeRect.size.width += (visibleRect.size.width - exposeRect.size.width);
        }
        
        // Make an expose rectangle that will end up centering the passed-in rectangle vertically.
        if (exposeRect.size.height >= visibleRect.size.height) {
            exposeRect.size.height = visibleRect.size.height;
        } else {
            if ([self isFlipped]) {
                exposeRect.origin.y -= (visibleRect.size.height - exposeRect.size.height) / 2.0;
            } else {
                exposeRect.origin.y += (visibleRect.size.height - exposeRect.size.height) / 2.0;
            }
            exposeRect.size.height += (visibleRect.size.height - exposeRect.size.height);
        }
        
        [self scrollRectToVisible:exposeRect];
    }

    [super _KWQ_scrollRectToVisible:rect inView:view];
}

@end
