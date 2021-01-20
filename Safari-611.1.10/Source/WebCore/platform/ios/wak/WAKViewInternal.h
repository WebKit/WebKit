/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if TARGET_OS_IPHONE

#import "WAKView.h"
#import "WKView.h"

@interface WAKView ()
{
@package
    WKViewContext viewContext;
    WKViewRef viewRef;

    // This array is only used to keep WAKViews alive.
    // The actual subviews are maintained by the WKView.
    NSMutableSet *subviewReferences;

    BOOL _isHidden;
    BOOL _drawsOwnDescendants;
}

- (WKViewRef)_viewRef;
+ (WAKView *)_wrapperForViewRef:(WKViewRef)_viewRef;
- (id)_initWithViewRef:(WKViewRef)view;
- (BOOL)_handleResponderCall:(WKViewResponderCallbackType)type;
- (NSMutableSet *)_subviewReferences;
- (BOOL)_selfHandleEvent:(WebEvent *)event;

@end

static inline WAKView *WAKViewForWKViewRef(WKViewRef view)
{
    if (!view)
        return nil;
    WAKView *wrapper = (WAKView *)view->wrapper;
    if (wrapper)
        return wrapper;
    return [WAKView _wrapperForViewRef:view];
}

#endif
