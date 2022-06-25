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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "WebAuthenticationFlags.h"
#include <wtf/CompletionHandler.h>
#include <wtf/HashSet.h>
#include <wtf/RefCounted.h>
#include <wtf/spi/cocoa/SecuritySPI.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS LAContext;

namespace WebCore {
class AuthenticatorAssertionResponse;
}

namespace API {

class WebAuthenticationPanelClient {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~WebAuthenticationPanelClient() = default;

    virtual void updatePanel(WebKit::WebAuthenticationStatus) const { }
    virtual void dismissPanel(WebKit::WebAuthenticationResult) const { }
    virtual void requestPin(uint64_t, CompletionHandler<void(const WTF::String&)>&& completionHandler) const { completionHandler(WTF::String()); }
    virtual void selectAssertionResponse(Vector<Ref<WebCore::AuthenticatorAssertionResponse>>&&, WebKit::WebAuthenticationSource, CompletionHandler<void(WebCore::AuthenticatorAssertionResponse*)>&& completionHandler) const { completionHandler(nullptr); }
    virtual void decidePolicyForLocalAuthenticator(CompletionHandler<void(WebKit::LocalAuthenticatorPolicy)>&& completionHandler) const { completionHandler(WebKit::LocalAuthenticatorPolicy::Disallow); }
    virtual void requestLAContextForUserVerification(CompletionHandler<void(LAContext *)>&& completionHandler) const { completionHandler(nullptr); }
};

} // namespace API

#endif // ENABLE(WEB_AUTHN)
