/*
 * Copyright (C) 2015-2017 Apple Inc. All rights reserved.
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

#import <wtf/Platform.h>

#if PLATFORM(MAC)

#if USE(APPLE_INTERNAL_SDK)

#if HAVE(CPP20_INCOMPATIBLE_INTERNAL_HEADERS)
#define CGCOLORTAGGEDPOINTER_H_
#endif

#import <AppKit/NSWindow_Private.h>

#else

#import <AppKit/NSWindow.h>

@interface NSWindow ()

- (id)_oldFirstResponderBeforeBecoming;
- (id)_newFirstResponderAfterResigning;
- (void)_setCursorForMouseLocation:(NSPoint)point;
- (void)exitFullScreenMode:(id)sender;
- (void)enterFullScreenMode:(id)sender;

enum {
    NSWindowChildOrderingPriorityPopover = 20,
};
- (NSInteger)_childWindowOrderingPriority;
@end

enum {
    NSSideUtilityWindowMask = 1 << 9,
    NSSmallWindowMask = 1 << 10,
    _NSCarbonWindowMask = 1 << 25,
};

#endif

extern NSString *NSWindowWillOrderOnScreenNotification;
extern NSString *NSWindowWillOrderOffScreenNotification;

#endif // PLATFORM(MAC)
