/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#import "WKModelInteractionGestureRecognizer.h"

#if ENABLE(ARKIT_INLINE_PREVIEW_IOS)

#import "RemoteLayerTreeViews.h"
#import "WKModelView.h"
#import <UIKit/UIGestureRecognizerSubclass.h>
#import <pal/spi/ios/SystemPreviewSPI.h>

@implementation WKModelInteractionGestureRecognizer

- (void)touchesBegan:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    ASSERT([event touchesForGestureRecognizer:self].count);

    if (![self.view isKindOfClass:[WKModelView class]]) {
        [self setState:UIGestureRecognizerStateFailed];
        return;
    }

    [((WKModelView *)self.view).preview touchesBegan:touches withEvent:event];
}

- (void)touchesMoved:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    [self setState:UIGestureRecognizerStateChanged];

    [((WKModelView *)self.view).preview touchesMoved:touches withEvent:event];
}

- (void)touchesEnded:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    auto finalTouchesEnded = [touches isEqualToSet:[event touchesForGestureRecognizer:self]];
    [self setState:finalTouchesEnded ? UIGestureRecognizerStateEnded : UIGestureRecognizerStateChanged];

    [((WKModelView *)self.view).preview touchesEnded:touches withEvent:event];
}

- (void)touchesCancelled:(NSSet<UITouch *> *)touches withEvent:(UIEvent *)event
{
    auto finalTouchesCancelled = [touches isEqualToSet:[event touchesForGestureRecognizer:self]];
    [self setState:finalTouchesCancelled ? UIGestureRecognizerStateCancelled : UIGestureRecognizerStateChanged];

    [((WKModelView *)self.view).preview touchesCancelled:touches withEvent:event];
}

@end

#endif
