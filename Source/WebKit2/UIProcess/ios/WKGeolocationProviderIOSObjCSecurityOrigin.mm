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
#import "WKGeolocationProviderIOS.h"

#import <Foundation/NSURL.h>
#import <UIKit/UIWebGeolocationPolicyDecider.h>
#import <UIKit/UIWindow.h>
#import <WebCore/SecurityOrigin.h>
#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

// FIXME: Remove use of WebKit1 from WebKit2
#import <WebKit/WebAllowDenyPolicyListener.h>
#import <WebKit/WebSecurityOriginPrivate.h>

@interface WebSecurityOrigin (WebInternal)
- (id)_initWithWebCoreSecurityOrigin:(WebCore::SecurityOrigin *)origin;
@end

namespace WebKit {

void decidePolicyForGeolocationRequestFromOrigin(WebCore::SecurityOrigin*, const String& urlString, id<WebAllowDenyPolicyListener>, UIWindow* window);

void decidePolicyForGeolocationRequestFromOrigin(WebCore::SecurityOrigin* origin, const String& urlString, id<WebAllowDenyPolicyListener> listener, UIWindow* window)
{
    RetainPtr<WebSecurityOrigin> securityOrigin = adoptNS([[WebSecurityOrigin alloc] _initWithWebCoreSecurityOrigin:origin]);
    RetainPtr<NSURL> requestUrl = adoptNS([[NSURL alloc] initWithString:urlString]);
    [[UIWebGeolocationPolicyDecider sharedPolicyDecider] decidePolicyForGeolocationRequestFromOrigin:securityOrigin.get() requestingURL:requestUrl.get() window:window listener:listener];
}

} // namespace WebKit
