/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "Test.h"

#include "PlatformUtilities.h"
#include "PlatformWebView.h"
#include <WebKit2/WebKit2.h>

namespace TestWebKitAPI {
    
static bool done;

static void decidePolicyForResponse(WKPageRef, WKFrameRef, WKURLResponseRef response, WKURLRequestRef, WKFramePolicyListenerRef listener, WKTypeRef, const void*)
{
    TEST_ASSERT(WKStringIsEqualToUTF8CString(Util::MIMETypeForWKURLResponse(response).get(), "text/html"));

    WKFramePolicyListenerUse(listener);
    done = true;
}

TEST(WebKit2, AboutBlankLoad)
{
    WKRetainPtr<WKContextRef> context = adoptWK(WKContextCreate());
    PlatformWebView webView(context.get());

    WKPagePolicyClient policyClient;
    memset(&policyClient, 0, sizeof(policyClient));

    policyClient.decidePolicyForResponse = decidePolicyForResponse;
    WKPageSetPagePolicyClient(webView.page(), &policyClient);

    WKPageLoadURL(webView.page(), adoptWK(WKURLCreateWithUTF8CString("about:blank")).get());

    Util::run(&done);
}

} // namespace TestWebKitAPI
