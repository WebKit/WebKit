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

#ifndef TestInvocation_h
#define TestInvocation_h

#include <string>
#include <WebKit2/WKRetainPtr.h>
#include <wtf/Noncopyable.h>

namespace WTR {

class TestInvocation {
    WTF_MAKE_NONCOPYABLE(TestInvocation);
public:
    TestInvocation(const std::string& pathOrURL);
    ~TestInvocation();

    void setIsPixelTest(const std::string& expectedPixelHash);
    
    void invoke();
    void didReceiveMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);
    WKRetainPtr<WKTypeRef> didReceiveSynchronousMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody);

private:
    void dump(const char*);
    void dumpPixelsAndCompareWithExpected(WKImageRef);
    
    WKRetainPtr<WKURLRef> m_url;
    std::string m_pathOrURL;
    
    bool m_dumpPixels;
    std::string m_expectedPixelHash;

    // Invocation state
    bool m_gotInitialResponse;
    bool m_gotFinalMessage;
    bool m_gotRepaint;
    bool m_error;
};

} // namespace WTR

#endif // TestInvocation_h
