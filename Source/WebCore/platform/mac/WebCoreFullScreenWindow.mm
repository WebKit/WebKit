/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#if !PLATFORM(IOS_FAMILY)

#import "WebCoreFullScreenWindow.h"

// FIXME: This isn't really an NSWindowController method - it's a method that
// the NSWindowController subclass that's using WebCoreFullScreenWindow needs to implement.
// It should probably be a protocol method.
@interface NSWindowController ()
- (void)performClose:(id)sender;
@end

@implementation WebCoreFullScreenWindow

- (id)initWithContentRect:(NSRect)contentRect styleMask:(NSUInteger)aStyle backing:(NSBackingStoreType)bufferingType defer:(BOOL)flag
{
    self = [super initWithContentRect:contentRect styleMask:aStyle backing:bufferingType defer:flag];
    if (!self)
        return nil;
    [self setOpaque:NO];
    [self setBackgroundColor:[NSColor clearColor]];
    [self setIgnoresMouseEvents:NO];
    [self setAcceptsMouseMovedEvents:YES];
    [self setReleasedWhenClosed:NO];
    [self setHasShadow:NO];

    return self;
}

- (NSRect)constrainFrameRect:(NSRect)frameRect toScreen:(NSScreen *)screen
{
    UNUSED_PARAM(screen);
    return frameRect;
}

- (BOOL)canBecomeMainWindow
{
    return NO;
}

- (BOOL)canBecomeKeyWindow
{
    return YES;
}

- (void)keyDown:(NSEvent *)theEvent
{
    if ([[theEvent charactersIgnoringModifiers] isEqual:@"\e"]) // Esacpe key-code
        [self cancelOperation:self];
    else [super keyDown:theEvent];
}

- (void)cancelOperation:(id)sender
{
    [[self windowController] cancelOperation:sender];
}

- (void)performClose:(id)sender
{
    [[self windowController] performClose:sender];
}

- (void)setStyleMask:(NSUInteger)styleMask
{
    // Changing the styleMask of a NSWindow can reset the firstResponder if the frame view changes,
    // so save off the existing one, and restore it if necessary after the call to -setStyleMask:.
    NSResponder* savedFirstResponder = [self firstResponder];

    [super setStyleMask:styleMask];

    if ([self firstResponder] != savedFirstResponder
        && [savedFirstResponder isKindOfClass:[NSView class]]
        && [(NSView*)savedFirstResponder isDescendantOf:[self contentView]])
        [self makeFirstResponder:savedFirstResponder];
}
@end

#endif // !PLATFORM(IOS_FAMILY)
