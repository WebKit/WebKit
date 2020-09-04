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
#import "WKFormPeripheralBase.h"

#if PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WKContentViewInteraction.h"
#import <pal/spi/cocoa/IOKitSPI.h>

@implementation WKFormPeripheralBase {
    RetainPtr<NSObject <WKFormControl>> _control;
}

- (instancetype)initWithView:(WKContentView *)view control:(RetainPtr<NSObject <WKFormControl>>&&)control
{
    if (!(self = [super init]))
        return nil;

    _view = view;
    _control = WTFMove(control);
    return self;
}

- (void)beginEditing
{
    if (_editing)
        return;

    _editing = YES;
    [_control controlBeginEditing];
}

- (void)endEditing
{
    if (!_editing)
        return;

    _editing = NO;
    [_control controlEndEditing];
}

- (UIView *)assistantView
{
    return [_control controlView];
}

- (NSObject <WKFormControl> *)control
{
    return _control.get();
}

- (BOOL)handleKeyEvent:(UIEvent *)event
{
    ASSERT(event._hidEvent);
    if ([_control respondsToSelector:@selector(controlHandleKeyEvent:)]) {
        if ([_control controlHandleKeyEvent:event])
            return YES;
    }
    if (!event._isKeyDown)
        return NO;
    UIPhysicalKeyboardEvent *keyEvent = (UIPhysicalKeyboardEvent *)event;
    if (keyEvent._inputFlags & kUIKeyboardInputModifierFlagsChanged)
        return NO;
    if (_editing && (keyEvent._keyCode == kHIDUsage_KeyboardEscape || [keyEvent._unmodifiedInput isEqualToString:UIKeyInputEscape])) {
        [_view accessoryDone];
        return YES;
    }
    if (!_editing && keyEvent._keyCode == kHIDUsage_KeyboardSpacebar) {
        [_view accessoryOpen];
        return YES;
    }
    return NO;
}

@end

#endif // PLATFORM(IOS_FAMILY)
