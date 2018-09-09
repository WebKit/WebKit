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

#include "config.h"
#include "PlatformUtilities.h"

#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKStringCF.h>
#include <WebKit/WKURLCF.h>
#include <WebKit/WKURLResponseNS.h>
#include <wtf/RetainPtr.h>
#include <wtf/StdLibExtras.h>
#include <wtf/mac/AppKitCompatibilityDeclarations.h>

namespace TestWebKitAPI {
namespace Util {

WKStringRef createInjectedBundlePath()
{
    NSString *nsString = [[[NSBundle mainBundle] bundlePath] stringByAppendingPathComponent:@"InjectedBundleTestWebKitAPI.bundle"];
    return WKStringCreateWithCFString((__bridge CFStringRef)nsString);
}

WKURLRef createURLForResource(const char* resource, const char* extension)
{
    NSURL *nsURL = [[NSBundle mainBundle] URLForResource:[NSString stringWithUTF8String:resource] withExtension:[NSString stringWithUTF8String:extension] subdirectory:@"TestWebKitAPI.resources"];
    return WKURLCreateWithCFURL((__bridge CFURLRef)nsURL);
}

WKURLRef URLForNonExistentResource()
{
    NSURL *nsURL = [NSURL URLWithString:@"file:///does-not-exist.html"];
    return WKURLCreateWithCFURL((__bridge CFURLRef)nsURL);
}

WKRetainPtr<WKStringRef> MIMETypeForWKURLResponse(WKURLResponseRef wkResponse)
{
    RetainPtr<NSURLResponse> response = adoptNS(WKURLResponseCopyNSURLResponse(wkResponse));
    return adoptWK(WKStringCreateWithCFString((__bridge CFStringRef)[response.get() MIMEType]));
}

bool isKeyDown(WKNativeEventPtr event)
{
    return [event type] == NSEventTypeKeyDown;
}

} // namespace Util
} // namespace TestWebKitAPI
