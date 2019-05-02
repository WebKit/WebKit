/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#import "WKFoundation.h"
#import <wtf/CompletionHandler.h>
#import <wtf/RefPtr.h>
#import <wtf/RetainPtr.h>
#import <wtf/WeakObjCPtr.h>

#if PLATFORM(MAC)
#import <AppKit/AppKit.h>
#else
#import <UIKit/UIKit.h>
#endif

namespace WebKit {
class SafeBrowsingWarning;
enum class ContinueUnsafeLoad : bool;
}

OBJC_CLASS WKSafeBrowsingTextView;

#if PLATFORM(MAC)
using ViewType = NSView;
using RectType = NSRect;
using ColorType = NSColor;
#else
using ViewType = UIView;
using RectType = CGRect;
using ColorType = UIColor;
#endif

@interface WKSafeBrowsingBox : ViewType {
@package
#if PLATFORM(MAC)
    RetainPtr<ColorType> _backgroundColor;
#endif
}
- (void)setSafeBrowsingBackgroundColor:(ColorType *)color;
@end

#if PLATFORM(MAC)
@interface WKSafeBrowsingWarning : WKSafeBrowsingBox<NSTextViewDelegate>
#else
@interface WKSafeBrowsingWarning : UIScrollView<UITextViewDelegate>
#endif
{
@package
    CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)> _completionHandler;
    RefPtr<const WebKit::SafeBrowsingWarning> _warning;
    WeakObjCPtr<WKSafeBrowsingTextView> _details;
    WeakObjCPtr<WKSafeBrowsingBox> _box;
#if PLATFORM(WATCHOS)
    WeakObjCPtr<UIResponder> _previousFirstResponder;
#endif
}

- (instancetype)initWithFrame:(RectType)frame safeBrowsingWarning:(const WebKit::SafeBrowsingWarning&)warning completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, URL>&&)>&&)completionHandler;

- (BOOL)forMainFrameNavigation;

@end
