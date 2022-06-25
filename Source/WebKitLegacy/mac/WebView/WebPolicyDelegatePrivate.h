/*
 * Copyright (C) 2005, 2008 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <WebKitLegacy/WebPolicyDelegate.h>

@class WebHistoryItem;
@class WebPolicyDecisionListenerPrivate;

extern NSString *WebActionFormKey; // HTMLFormElement

typedef enum {
    WebNavigationTypePlugInRequest = WebNavigationTypeOther + 1
} WebExtraNavigationType;

@interface WebPolicyDecisionListener : NSObject <WebPolicyDecisionListener>
{
@private
    WebPolicyDecisionListenerPrivate *_private;
}
- (id)_initWithTarget:(id)target action:(SEL)action;
- (void)_invalidate;
@end

@interface NSObject (WebPolicyDelegatePrivate)
// Needed for <rdar://problem/3951283> can view pages from the back/forward cache that should be disallowed by Parental Controls
- (BOOL)webView:(WebView *)webView shouldGoToHistoryItem:(WebHistoryItem *)item;
#if TARGET_OS_IPHONE
- (BOOL)webView:(WebView *)webView shouldLoadMediaURL:(NSURL *)url inFrame:(WebFrame *)frame;
#endif
@end
