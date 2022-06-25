/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <WebKit/WKBundle.h>
#include <WebKit/WKRetainPtr.h>
#include <map>
#include <string>

namespace TestWebKitAPI {

class InjectedBundleTest;

class InjectedBundleController {
public:
    static InjectedBundleController& singleton();

    void initialize(WKBundleRef, WKTypeRef);

    void dumpTestNames();
    void initializeTestNamed(WKBundleRef bundle, const std::string&, WKTypeRef userData);

    typedef std::unique_ptr<InjectedBundleTest> (*CreateInjectedBundleTestFunction)(const std::string&);
    void registerCreateInjectedBundleTestFunction(const std::string&, CreateInjectedBundleTestFunction);

    WKBundleRef bundle() const { return m_bundle.get(); }
    
private:
    InjectedBundleController();
    ~InjectedBundleController();

    void platformInitialize();

    static void didCreatePage(WKBundleRef, WKBundlePageRef, const void* clientInfo);
    static void willDestroyPage(WKBundleRef, WKBundlePageRef, const void* clientInfo);
    static void didReceiveMessage(WKBundleRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo);
    static void didReceiveMessageToPage(WKBundleRef, WKBundlePageRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo);

    std::map<std::string, CreateInjectedBundleTestFunction> m_createInjectedBundleTestFunctions;
    WKRetainPtr<WKBundleRef> m_bundle;
    std::unique_ptr<InjectedBundleTest> m_currentTest;
};

} // namespace TestWebKitAPI
