/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "TestRunner.h"

#import "ActivateFonts.h"
#import "InjectedBundle.h"
#import <JavaScriptCore/JSStringRefCF.h>

namespace WTR {

void TestRunner::platformInitialize()
{
}

JSRetainPtr<JSStringRef> TestRunner::pathToLocalResource(JSStringRef url)
{
    return url; // Do nothing on Cocoa.
}

JSRetainPtr<JSStringRef> TestRunner::inspectorTestStubURL()
{
#if PLATFORM(IOS_FAMILY)
    return nullptr;
#else
    auto inspectorBundle = CFBundleGetBundleWithIdentifier(CFSTR("com.apple.WebInspectorUI"));
    if (!inspectorBundle)
        return nullptr;
    auto resourceURL = adoptCF(CFBundleCopyResourceURL(inspectorBundle, CFSTR("TestStub"), CFSTR("html"), NULL));
    return resourceURL ? adopt(JSStringCreateWithCFString(CFURLGetString(resourceURL.get()))) : nullptr;
#endif
}

void TestRunner::installFakeHelvetica(JSStringRef configuration)
{
    WTR::installFakeHelvetica(toWK(configuration).get());
}

} // namespace WTR
