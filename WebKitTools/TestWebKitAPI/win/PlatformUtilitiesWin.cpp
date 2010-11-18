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

#include "PlatformUtilities.h"

#include <WebKit2/WKStringCF.h>
#include <WebKit2/WKURLCF.h>
#include <wtf/RetainPtr.h>

namespace TestWebKitAPI {
namespace Util {

#ifdef DEBUG_ALL
const char* injectedBundleDLL = "\\InjectedBundle_debug.dll";
#else
const char* injectedBundleDLL = "\\InjectedBundle.dll";
#endif

void run(bool* done)
{
    while (!*done) {
        MSG msg;
        BOOL result = ::GetMessageW(&msg, 0, 0, 0);
        if (!result || result == -1)
            break;
        ::TranslateMessage(&msg);
        ::DispatchMessage(&msg);
    }
}

RetainPtr<CFStringRef> cf(const char* utf8String)
{
    return RetainPtr<CFStringRef>(AdoptCF, CFStringCreateWithCString(kCFAllocatorDefault, utf8String, kCFStringEncodingUTF8));
}

WKStringRef createInjectedBundlePath()
{
    RetainPtr<CFURLRef> executableURL(AdoptCF, CFBundleCopyExecutableURL(CFBundleGetMainBundle()));
    RetainPtr<CFURLRef> executableContainerURL(AdoptCF, CFURLCreateCopyDeletingLastPathComponent(0, executableURL.get()));
    RetainPtr<CFStringRef> dllFilename(AdoptCF, CFStringCreateWithCStringNoCopy(0, injectedBundleDLL, kCFStringEncodingWindowsLatin1, 0));
    RetainPtr<CFURLRef> bundleURL(AdoptCF, CFURLCreateCopyAppendingPathComponent(0, executableContainerURL.get(), dllFilename.get(), false));
    RetainPtr<CFStringRef> bundlePath(AdoptCF, CFURLCopyFileSystemPath(bundleURL.get(), kCFURLWindowsPathStyle));
    return WKStringCreateWithCFString(bundlePath.get());
}

WKURLRef createURLForResource(const char* resource, const char* extension)
{
    RetainPtr<CFURLRef> url(AdoptCF, CFBundleCopyResourceURL(CFBundleGetMainBundle(), cf(resource).get(), cf(extension).get(), 0));
    return WKURLCreateWithCFURL(url.get());
}

WKURLRef URLForNonExistentResource()
{
    return WKURLCreateWithUTF8CString("file:///does-not-exist.html");
}

bool isKeyDown(WKNativeEventPtr event)
{
    return event->message == WM_KEYDOWN;
}

} // namespace Util
} // namespace TestWebKitAPI
