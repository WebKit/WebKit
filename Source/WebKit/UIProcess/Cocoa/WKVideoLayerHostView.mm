/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WKVideoLayerHostView.h"

#include "WKVideoLayerHost.h"
#include <wtf/WeakObjCPtr.h>
#include <wtf/cocoa/TypeCastsCocoa.h>

#if PLATFORM(IOS_FAMILY)
using CocoaRect = CGRect;
#else
using CocoaRect = NSRect;
#endif

@implementation WKVideoLayerHostView {
#if PLATFORM(IOS_FAMILY)
    WeakObjCPtr<UIWindow> _window;
#endif
#if USE(EXTENSIONKIT)
    RetainPtr<CocoaView> _visibilityPropagationView;
    RetainPtr<BELayerHierarchyHostingView> _hostingView;
#endif
}

- (instancetype)initWithFrame:(CocoaRect)frame {
    self = [super initWithFrame:frame];
    if (!self)
        return nil;

#if USE(EXTENSIONKIT)
    _hostingView = adoptNS([[BELayerHierarchyHostingView alloc] init]);
    [_hostingView setFrame:[self bounds]];
    [self addSubview:_hostingView.get()];
#endif

    return self;
}

#if USE(EXTENSIONKIT)
- (void)layoutSubviews
{
    [super layoutSubviews];
    [_hostingView setFrame:[self bounds]];
}

- (BELayerHierarchyHostingView *)layerHierarchyHostingView
{
    return _hostingView.get();
}
#endif

#if PLATFORM(IOS_FAMILY)
+ (Class)layerClass {
    return [WKVideoLayerHost class];
}
#else
- (CALayer *)makeBackingLayer {
    return adoptNS([[WKVideoLayerHost alloc] init]).autorelease();
}
#endif

- (WKVideoLayerHost *)layerHost {
    return checked_objc_cast<WKVideoLayerHost>([self layer]);
}

- (BOOL)clipsToBounds {
    return NO;
}

#if PLATFORM(IOS_FAMILY)
- (void)willMoveToWindow:(UIWindow *)newWindow {
    _window = newWindow;
    [super willMoveToWindow:newWindow];
}

- (UIWindow *)window {
    if (!_window)
        return nil;
    return [super window];
}
#endif

#if USE(EXTENSIONKIT)
- (CocoaView *)visibilityPropagationView
{
    return _visibilityPropagationView.get();
}

- (void)setVisibilityPropagationView:(CocoaView *)visibilityPropagationView
{
    [_visibilityPropagationView removeFromSuperview];
    _visibilityPropagationView = visibilityPropagationView;
    [self addSubview:_visibilityPropagationView.get()];
}
#endif // USE(EXTENSIONKIT)

@end

