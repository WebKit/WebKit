/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
#import "OffscreenWindow.h"

@implementation OffscreenWindow {
    BOOL _isKeyWindow;
}

- (instancetype)initWithSize:(CGSize)size
{
    return [self initWithSize:size isKeyWindow:YES];
}

- (instancetype)initWithSize:(CGSize)size isKeyWindow:(BOOL)isKeyWindow
{
    _isKeyWindow = isKeyWindow;

    NSRect rect = NSMakeRect(0, 0, size.width, size.height);
    NSRect windowRect = NSOffsetRect(rect, -10000, [(NSScreen *)[[NSScreen screens] objectAtIndex:0] frame].size.height - rect.size.height + 10000);
    self = [super initWithContentRect:windowRect styleMask:NSWindowStyleMaskBorderless backing:NSBackingStoreBuffered defer:YES];
    if (!self)
        return nil;

    self.colorSpace = [[NSScreen mainScreen] colorSpace];
    [self orderBack:nil];
    self.autodisplay = NO;
    self.releasedWhenClosed = NO;

    return self;
}

- (BOOL)isKeyWindow
{
    return _isKeyWindow;
}

- (BOOL)isVisible
{
    return YES;
}

@end
