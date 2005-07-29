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

#import "TestViewerSplitView.h"

@implementation TestViewerSplitView

- (void)drawRect:(NSRect)rect
{
    NSArray *subviews = [self subviews];
    int subviewCount = [subviews count];
    for (int x=0; x < subviewCount; x++) {
        NSView *subview = [subviews objectAtIndex:x];
        [subview drawRect:rect];
//        NSString *label = [subviewLabels objectAtIndex:0];
//        [label drawAtPoint:[subview frame].origin withAttributes:NULL];
    }
}

- (void)retileSubviews
{
    NSRect bounds = [self bounds];
    NSArray *subviews = [self subviews];
    int subviewCount = [subviews count];
    if (!subviewCount) return;
    float subviewWidth = bounds.size.width / subviewCount;
    
    for (int x=0; x < subviewCount; x++) {
        [[subviews objectAtIndex:x] setFrame:NSMakeRect(x * subviewWidth, 0, subviewWidth, bounds.size.height)];
    }
}

- (void)didAddSubview:(NSView *)subview
{
    [super didAddSubview:subview];
    [self retileSubviews];
}

- (void)willRemoveSubview:(NSView *)subview
{
    [super willRemoveSubview:subview];
    [self retileSubviews];
}

- (void)setFrame:(NSRect)newFrame
{
    // ideally we also want to catch when the bounds changes without the
    // frame changing, but we're not bothering with that now - ECS 7/29/05
    [super setFrame:newFrame];
    [self retileSubviews];
}

@end
