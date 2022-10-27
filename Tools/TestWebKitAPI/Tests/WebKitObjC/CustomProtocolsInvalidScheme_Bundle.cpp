/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#if WK_HAVE_C_SPI

#include "InjectedBundleTest.h"
#include <WebKit/WKBundlePage.h>

namespace TestWebKitAPI {

static WKBundlePagePolicyAction decidePolicyForNavigationAction(WKBundlePageRef, WKBundleFrameRef, WKBundleNavigationActionRef, WKURLRequestRef, WKTypeRef*, const void*)
{
    return WKBundlePagePolicyActionPassThrough;
}

class CustomProtocolInvalidSchemeTest : public InjectedBundleTest {
public:
    CustomProtocolInvalidSchemeTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }

private:
    void didCreatePage(WKBundleRef, WKBundlePageRef bundlePage) override
    {
        WKBundlePagePolicyClientV0 policyClient;
        memset(&policyClient, 0, sizeof(policyClient));

        policyClient.base.version = 0;
        policyClient.decidePolicyForNavigationAction = decidePolicyForNavigationAction;
        
ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        WKBundlePageSetPolicyClient(bundlePage, &policyClient.base);
ALLOW_DEPRECATED_DECLARATIONS_END
    }
};

static InjectedBundleTest::Register<CustomProtocolInvalidSchemeTest> registrar("CustomProtocolInvalidSchemeTest");

} // namespace TestWebKitAPI

#endif
