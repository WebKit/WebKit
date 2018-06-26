/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#import "CustomBundleObject.h"
#import "InjectedBundleTest.h"
#import "PlatformUtilities.h"
#import <WebKit/WKBundlePrivate.h>
#import <WebKit/WKRetainPtr.h>
#import <WebKit/WKStringCF.h>

namespace TestWebKitAPI {
    
class CustomBundleParameterTest : public InjectedBundleTest {
public:
    CustomBundleParameterTest(const std::string& identifier)
        : InjectedBundleTest(identifier)
    {
    }
    
    virtual void didCreatePage(WKBundleRef bundle, WKBundlePageRef page)
    {
        WKTypeRef typeName = WKStringCreateWithCFString((__bridge CFStringRef)[CustomBundleObject className]);
        auto array = adoptWK(WKArrayCreateAdoptingValues(&typeName, 1));

        WKBundleExtendClassesForParameterCoder(bundle, array.get());
        
        WKRetainPtr<WKDoubleRef> returnCode = adoptWK(WKDoubleCreate(1234));
        WKBundlePostMessage(bundle, Util::toWK("DidRegisterCustomClass").get(), returnCode.get());
    }
};

static InjectedBundleTest::Register<CustomBundleParameterTest> registrar("CustomBundleParameterTest");
    
} // namespace TestWebKitAPI

#endif
