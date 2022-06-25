/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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
#import "UIGamepadProvider.h"

#if ENABLE(GAMEPAD) && PLATFORM(MAC)

#import "WebPageProxy.h"
#import "WKAPICast.h"
#import "WKViewInternal.h"
#import "WKWebViewInternal.h"
#import <wtf/ProcessPrivilege.h>

namespace WebKit {

WebPageProxy* UIGamepadProvider::platformWebPageProxyForGamepadInput()
{
    ASSERT(hasProcessPrivilege(ProcessPrivilege::CanCommunicateWithWindowServer));
    auto responder = [[NSApp keyWindow] firstResponder];

    if ([responder isKindOfClass:[WKWebView class]])
        return ((WKWebView *)responder)->_page.get();

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    if ([responder isKindOfClass:[WKView class]])
        return toImpl(((WKView *)responder).pageRef);
    ALLOW_DEPRECATED_DECLARATIONS_END

    return nullptr;
}

}

#endif // ENABLE(GAMEPAD) && PLATFORM(MAC)
