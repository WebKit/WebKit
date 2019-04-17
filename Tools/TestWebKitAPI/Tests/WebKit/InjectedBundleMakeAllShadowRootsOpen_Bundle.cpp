/*
 * Copyright (C) 2010, 2016 Apple Inc. All rights reserved.
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
#include <WebKit/WKBundlePageGroup.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKBundleScriptWorld.h>
#include <WebKit/WKRetainPtr.h>
#include <assert.h>

namespace TestWebKitAPI {

class InjectedBundleMakeAllShadowRootOpenTest : public InjectedBundleTest {
public:
    InjectedBundleMakeAllShadowRootOpenTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    { }

    virtual void initialize(WKBundleRef bundle, WKTypeRef userData)
    {
        assert(WKGetTypeID(userData) == WKBundlePageGroupGetTypeID());
        WKBundlePageGroupRef pageGroup = static_cast<WKBundlePageGroupRef>(userData);

        auto world = WKBundleScriptWorldCreateWorld();
        WKBundleScriptWorldMakeAllShadowRootsOpen(world);

        WKRetainPtr<WKStringRef> source = adoptWK(WKStringCreateWithUTF8CString(
            "window.onload = function () {\n"
            "    const element = document.createElement('div');\n"
            "    const queryMethodName = 'collectMatchingElementsInFlatTree';\n"
            "    element.attachShadow({mode: 'closed'});\n"
            // Test 1
            "    alert(element.shadowRoot ? 'PASS: shadowRoot created in injected bundle' : 'FAIL');\n"
            // Test 2
            "    alert(document.querySelector('shadow-host').shadowRoot ? 'PASS: shadowRoot created by normal world' : 'FAIL');\n"
            // Test 3
            "    alert(window[queryMethodName] ? `PASS: ${queryMethodName} exists` : `FAIL: ${queryMethodName} does not exist`);\n"
            // Test 4
            "    document.dispatchEvent(new CustomEvent('testnormalworld', {detail: queryMethodName}));\n"
            // Test 5
            "    const queryMethod = window[queryMethodName];\n"
            "    let queryResult = Array.from(queryMethod(document, 'span'));\n"
            "    alert('Found:' + queryResult.map((span) => span.textContent).join(','));\n"
            // Test 6
            "    const innerHost = queryMethod(document, 'inner-host')[0];\n"
            "    queryResult = Array.from(queryMethod(innerHost, 'span'));\n"
            "    alert('Found:' + queryResult.map((span) => span.textContent).join(','));\n"
            // Test 7
            "    alert(window.matchingElementInFlatTree ? `PASS: matchingElementInFlatTree exists` : `FAIL: matchingElementInFlatTree does not exist`);\n"
            // Test 8
            "    document.dispatchEvent(new CustomEvent('testnormalworld', {detail: 'matchingElementInFlatTree'}));\n"
            // Test 9
            "    queryResult = window.matchingElementInFlatTree(document, 'span');\n"
            "    alert('Found:' + (queryResult ? queryResult.textContent : 'null'));\n"
            // Test 10
            "    queryResult = window.matchingElementInFlatTree(innerHost, 'span');\n"
            "    alert('Found:' + (queryResult ? queryResult.textContent : 'null'));\n"
            // Test 11
            "    alert(`Found:${queryMethod(document, 'div').length} divs`);\n"
            // Test 12
            "    queryResult = window.matchingElementInFlatTree(document, 'div');\n"
            "    alert(`Found:${!!queryResult}`);\n"
            "}\n"));
        WKBundleAddUserScript(bundle, pageGroup, world, source.get(), 0, 0, 0, kWKInjectAtDocumentStart, kWKInjectInAllFrames);
    }
};

static InjectedBundleTest::Register<InjectedBundleMakeAllShadowRootOpenTest> registrar("InjectedBundleMakeAllShadowRootOpenTest");

} // namespace TestWebKitAPI

#endif
