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

#ifndef TestController_h
#define TestController_h

#include <WebKit2/WKRetainPtr.h>
#include <string>
#include <vector>

namespace WTR {

class TestController {
public:
    static TestController& shared();

    // Initialize the TestController.
    void initialize(int argc, const char *argv[]);

    // Returns true if all the tests passed, false otherwise.
    bool run();

    bool verbose() const { return m_verbose; }

    WKStringRef injectedBundlePath() { return m_injectedBundlePath.get(); }

private:
    TestController();
    ~TestController();

    void runTestingServerLoop();
    void runTest(const char* pathOrURL);
    
    void initializeInjectedBundlePath();

    bool m_dumpTree;
    bool m_dumpPixels;
    bool m_threaded;
    bool m_forceComplexText;    
    bool m_verbose;
    bool m_printSeparators;
    bool m_usingServerMode;
    std::vector<std::string> m_paths;
    WKRetainPtr<WKStringRef> m_injectedBundlePath;
};

} // namespace WTR

#endif // TestController_h
