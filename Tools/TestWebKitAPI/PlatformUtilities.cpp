/*
 * Copyright (C) 2010-2019 Apple Inc. All rights reserved.
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
#include "PlatformUtilities.h"

#include <WebKit/WKContextConfigurationRef.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniqueArray.h>

namespace TestWebKitAPI {
namespace Util {

#if WK_HAVE_C_SPI

WKContextRef createContextWithInjectedBundle()
{
    WKRetainPtr<WKStringRef> injectedBundlePath = adoptWK(createInjectedBundlePath());
    auto configuration = adoptWK(WKContextConfigurationCreate());
    WKContextConfigurationSetInjectedBundlePath(configuration.get(), injectedBundlePath.get());
    WKContextConfigurationSetShouldConfigureJSCForTesting(configuration.get(), true);
    return WKContextCreateWithConfiguration(configuration.get());
}

WKDictionaryRef createInitializationDictionaryForInjectedBundleTest(const std::string& testName, WKTypeRef userData)
{
    WKMutableDictionaryRef initializationDictionary = WKMutableDictionaryCreate();

    WKRetainPtr<WKStringRef> testNameKey = adoptWK(WKStringCreateWithUTF8CString("TestName"));
    WKRetainPtr<WKStringRef> testNameString = adoptWK(WKStringCreateWithUTF8CString(testName.c_str()));
    WKDictionarySetItem(initializationDictionary, testNameKey.get(), testNameString.get());

    WKRetainPtr<WKStringRef> userDataKey = adoptWK(WKStringCreateWithUTF8CString("UserData"));
    WKDictionarySetItem(initializationDictionary, userDataKey.get(), userData);

    return initializationDictionary;
}

WKContextRef createContextForInjectedBundleTest(const std::string& testName, WKTypeRef userData)
{
    WKContextRef context = createContextWithInjectedBundle();

    WKRetainPtr<WKDictionaryRef> initializationDictionary = adoptWK(createInitializationDictionaryForInjectedBundleTest(testName, userData));
    WKContextSetInitializationUserDataForInjectedBundle(context, initializationDictionary.get());

    return context;
}

std::string toSTD(WKStringRef string)
{
    size_t bufferSize = WKStringGetMaximumUTF8CStringSize(string);
    auto buffer = makeUniqueWithoutFastMallocCheck<char[]>(bufferSize);
    size_t stringLength = WKStringGetUTF8CString(string, buffer.get(), bufferSize);
    return std::string(buffer.get(), stringLength - 1);
}

std::string toSTD(WKRetainPtr<WKStringRef> string)
{
    return toSTD(string.get());
}

WKRetainPtr<WKStringRef> toWK(const char* utf8String)
{
    return adoptWK(WKStringCreateWithUTF8CString(utf8String));
}

#endif // WK_HAVE_C_SPI

std::string toSTD(const char* string)
{
    return std::string(string);
}

} // namespace Util
} // namespace TestWebKitAPI
