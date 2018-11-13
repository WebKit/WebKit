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

namespace WebCore {
class URL;
}

namespace WebKit {
class SafeBrowsingResult;
enum class ContinueUnsafeLoad : bool;
}

OBJC_CLASS WKSafeBrowsingTextView;

#if PLATFORM(MAC)
using RectType = NSRect;
@interface WKSafeBrowsingWarning : NSView<NSTextViewDelegate>
#else
using RectType = CGRect;
@interface WKSafeBrowsingWarning : UIScrollView<UITextViewDelegate>
#endif
{
@package
    CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, WebCore::URL>&&)> _completionHandler;
    RefPtr<const WebKit::SafeBrowsingResult> _result;
    RetainPtr<NSMutableArray<WKSafeBrowsingTextView *>> _textViews;
}

- (instancetype)initWithFrame:(RectType)frame safeBrowsingResult:(const WebKit::SafeBrowsingResult&)result completionHandler:(CompletionHandler<void(Variant<WebKit::ContinueUnsafeLoad, WebCore::URL>&&)>&&)completionHandler;

@end
