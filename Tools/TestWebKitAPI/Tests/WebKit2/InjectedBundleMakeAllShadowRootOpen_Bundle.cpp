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

        WKRetainPtr<WKStringRef> source(AdoptWK, WKStringCreateWithUTF8CString(
            "var element = document.createElement('div');"
            "element.attachShadow({mode: 'closed'});"
            "alert(element.shadowRoot ? 'PASS' : 'FAIL');"));
        WKBundleAddUserScript(bundle, pageGroup, world, source.get(), 0, 0, 0, kWKInjectAtDocumentStart, kWKInjectInAllFrames);
    }
};

static InjectedBundleTest::Register<InjectedBundleMakeAllShadowRootOpenTest> registrar("InjectedBundleMakeAllShadowRootOpenTest");

} // namespace TestWebKitAPI

#endif
