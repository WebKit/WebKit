/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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
#import "WebKitTestRunnerWindow.h"

#import "PlatformWebView.h"
#import <wtf/MainThread.h>
#import <wtf/Vector.h>

@implementation WebKitTestRunnerWindow

static Vector<WebKitTestRunnerWindow *> allWindows;

@synthesize platformWebView = _platformWebView;

+ (WebKitTestRunnerWindow *)_WTR_keyWindow
{
    ASSERT(isMainThread());
    for (auto window : allWindows) {
        if ([window isKeyWindow])
            return window;
    }
    return nil;
}

- (instancetype)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)windowStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)deferCreation
{
    ASSERT(isMainThread());
    allWindows.append(self);
    return [super initWithContentRect:contentRect styleMask:windowStyle backing:bufferingType defer:deferCreation];
}

- (void)close
{
    ASSERT(isMainThread());
    ASSERT(allWindows.contains(self));
    allWindows.removeFirst(self);
    ASSERT(!allWindows.contains(self));
    [super close];
}

- (void)dealloc
{
    ASSERT(isMainThread());
    ASSERT(!allWindows.contains(self)); // The window needs to stop being key before deallocation, otherwise AppKit spins waiting for this to happen (see <rdar://problem/50948871>).
    [super dealloc];
}

- (BOOL)isKeyWindow
{
    return !_platformWebView || _platformWebView->windowIsKey();
}

- (void)setFrameOrigin:(NSPoint)point
{
    _fakeOrigin = point;
}

- (void)setFrame:(NSRect)windowFrame display:(BOOL)displayViews animate:(BOOL)performAnimation
{
    NSRect currentFrame = [super frame];
    _fakeOrigin = windowFrame.origin;
    [super setFrame:NSMakeRect(currentFrame.origin.x, currentFrame.origin.y, windowFrame.size.width, windowFrame.size.height) display:displayViews animate:performAnimation];
}

- (void)setFrame:(NSRect)windowFrame display:(BOOL)displayViews
{
    NSRect currentFrame = [super frame];
    _fakeOrigin = windowFrame.origin;
    [super setFrame:NSMakeRect(currentFrame.origin.x, currentFrame.origin.y, windowFrame.size.width, windowFrame.size.height) display:displayViews];
}

- (NSRect)frameRespectingFakeOrigin
{
    NSRect currentFrame = [self frame];
    return NSMakeRect(_fakeOrigin.x, _fakeOrigin.y, currentFrame.size.width, currentFrame.size.height);
}

@end
