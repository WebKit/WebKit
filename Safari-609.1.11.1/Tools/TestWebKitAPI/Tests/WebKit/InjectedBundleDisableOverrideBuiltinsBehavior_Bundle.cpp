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

#include "config.h"

#if WK_HAVE_C_SPI

#include "InjectedBundleTest.h"
#include <WebKit/WKBundlePageGroup.h>
#include <WebKit/WKBundlePrivate.h>
#include <WebKit/WKBundleScriptWorld.h>
#include <WebKit/WKRetainPtr.h>
#include <assert.h>

namespace TestWebKitAPI {

class InjectedBundleNoDisableOverrideBuiltinsBehaviorTest : public InjectedBundleTest {
public:
    InjectedBundleNoDisableOverrideBuiltinsBehaviorTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    { }

    virtual void initialize(WKBundleRef bundle, WKTypeRef userData)
    {
        assert(WKGetTypeID(userData) == WKBundlePageGroupGetTypeID());
        WKBundlePageGroupRef pageGroup = static_cast<WKBundlePageGroupRef>(userData);

        WKRetainPtr<WKStringRef> source = adoptWK(WKStringCreateWithUTF8CString(
            "window.onload = function () {\n"
            "    const form = document.getElementById('test').parentNode;\n"
            "    alert(form.tagName != 'FORM' ? 'PASS: [OverrideBuiltins] was not disabled' : 'FAIL: [OverrideBuiltins] was disabled');\n"
            "}\n"));

        auto normalWorld = WKBundleScriptWorldNormalWorld();
        WKBundleAddUserScript(bundle, pageGroup, normalWorld, source.get(), 0, 0, 0, kWKInjectAtDocumentEnd, kWKInjectInAllFrames);

        auto isolatedWorld = WKBundleScriptWorldCreateWorld();
        WKBundleAddUserScript(bundle, pageGroup, isolatedWorld, source.get(), 0, 0, 0, kWKInjectAtDocumentEnd, kWKInjectInAllFrames);
    }
};

class InjectedBundleDisableOverrideBuiltinsBehaviorTest : public InjectedBundleTest {
public:
    InjectedBundleDisableOverrideBuiltinsBehaviorTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    { }

    virtual void initialize(WKBundleRef bundle, WKTypeRef userData)
    {
        assert(WKGetTypeID(userData) == WKBundlePageGroupGetTypeID());
        WKBundlePageGroupRef pageGroup = static_cast<WKBundlePageGroupRef>(userData);

        WKRetainPtr<WKStringRef> source = adoptWK(WKStringCreateWithUTF8CString(
            "window.onload = function () {\n"
            "    const form = document.getElementById('test').parentNode;\n"
            "    alert(form.tagName === 'FORM' ? 'PASS: [OverrideBuiltins] was disabled' : 'FAIL: [OverrideBuiltins] was not disabled');\n"
            "}\n"));

        auto normalWorld = WKBundleScriptWorldNormalWorld();
        WKBundleScriptWorldDisableOverrideBuiltinsBehavior(normalWorld);
        WKBundleAddUserScript(bundle, pageGroup, normalWorld, source.get(), 0, 0, 0, kWKInjectAtDocumentEnd, kWKInjectInAllFrames);

        auto isolatedWorld = WKBundleScriptWorldCreateWorld();
        WKBundleScriptWorldDisableOverrideBuiltinsBehavior(isolatedWorld);
        WKBundleAddUserScript(bundle, pageGroup, isolatedWorld, source.get(), 0, 0, 0, kWKInjectAtDocumentEnd, kWKInjectInAllFrames);
    }
};

static InjectedBundleTest::Register<InjectedBundleNoDisableOverrideBuiltinsBehaviorTest> registrar1("InjectedBundleNoDisableOverrideBuiltinsBehaviorTest");
static InjectedBundleTest::Register<InjectedBundleDisableOverrideBuiltinsBehaviorTest> registrar2("InjectedBundleDisableOverrideBuiltinsBehaviorTest");

} // namespace TestWebKitAPI

#endif
