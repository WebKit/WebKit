/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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
 
 /*
    This class an it's companion KWQTextStorage are used to optimize 
    our interaction with NSLayoutManager.  They do very little.
    KWQTextContainer has a fixed geometry.  It only ever containers
    a short run of text corresponding to a khtml TextSlave.
    
    Both KWQTextStorage and KWQTextContainer are used as singletons.  They
    are set/reset for layout and rendering.
 */
 

#import <Cocoa/Cocoa.h>

#include <kwqdebug.h>

#import <KWQTextContainer.h>

@implementation KWQTextContainer

const float LargeNumberForText = 1.0e7;

static KWQTextContainer *sharedInstance = nil;

+ (KWQTextContainer *)sharedInstance
{
    if (sharedInstance == nil)
        sharedInstance = [[KWQTextContainer alloc] initWithContainerSize: NSMakeSize(0,0)];
    return sharedInstance;
}


- (id)initWithContainerSize:(NSSize)aSize
{
    [super initWithContainerSize:aSize];
    return self;
}

- (void)replaceLayoutManager:(NSLayoutManager *)aLayoutManager
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (void)replaceLayoutManager:(NSLayoutManager *)aLayoutManager"];
}

- (BOOL)isSimpleRectangularTextContainer
{
    return YES;
}

- (BOOL)containsPoint:(NSPoint)aPoint
{
    return YES;
}


- (NSSize)containerSize
{
    return NSMakeSize(LargeNumberForText, LargeNumberForText);
}


- (void)setContainerSize:(NSSize)aSize
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (void)setContainerSize:(NSSize)aSize"];
}

- (void)setHeightTracksTextView:(BOOL)flag
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (void)setHeightTracksTextView:(BOOL)flag"];
}

- (BOOL)heightTracksTextView
{
    return NO;
}

- (void)setWidthTracksTextView:(BOOL)flag
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (void)setWidthTracksTextView:(BOOL)flag"];
}

- (BOOL)widthTracksTextView
{
    return NO;
}

- (void)setLayoutManager:(NSLayoutManager *)aLayoutManager
{
}


- (NSLayoutManager *)layoutManager
{
    [NSException raise:@"NOT IMPLEMENTED" format:@"- (NSLayoutManager *)layoutManager"];
    return nil;
}


- (void)setLineFragmentPadding:(float)aFloat
{
}


- (float)lineFragmentPadding
{
    return 0.0f;
}


- (void)setTextView:(NSTextView *)aTextView
{
}

- (NSTextView *)textView
{
    return nil;
}

- (NSRect)lineFragmentRectForProposedRect:(NSRect)proposedRect sweepDirection:(NSLineSweepDirection)sweepDirection movementDirection:(NSLineMovementDirection)movementDirection remainingRect:(NSRect *)remainingRect
{
    return proposedRect;
}

@end

