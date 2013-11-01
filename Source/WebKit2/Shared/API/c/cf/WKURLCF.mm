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

#import "config.h"
#import "WKURLCF.h"

#import "WKAPICast.h"
#import "WKNSURL.h"
#import <WebCore/CFURLExtras.h>
#import <objc/runtime.h>
#import <wtf/PassRefPtr.h>
#import <wtf/RefPtr.h>
#import <wtf/text/CString.h>
#import <wtf/text/WTFString.h>

using namespace WebCore;
using namespace WebKit;

#if WK_API_ENABLED
static inline Class wkNSURLClass()
{
    static dispatch_once_t once;
    static Class wkNSURLClass;
    dispatch_once(&once, ^{
        wkNSURLClass = [WKNSURL class];
    });
    return wkNSURLClass;
}
#endif // WK_API_ENABLED

WKURLRef WKURLCreateWithCFURL(CFURLRef cfURL)
{
    if (!cfURL)
        return 0;

#if WK_API_ENABLED
    // Since WKNSURL is an internal class with no subclasses, we can do a simple equality check.
    if (object_getClass((NSURL *)cfURL) == wkNSURLClass()) {
        return toAPI(static_cast<WebURL*>(&[(WKNSURL *)cfURL retain]._apiObject));
    }
#endif

    CString urlBytes;
    getURLBytes(cfURL, urlBytes);

    return toCopiedURLAPI(urlBytes.data());
}

CFURLRef WKURLCopyCFURL(CFAllocatorRef allocatorRef, WKURLRef URLRef)
{
    ASSERT(!toImpl(URLRef)->string().isNull());

    // We first create a CString and then create the CFURL from it. This will ensure that the CFURL is stored in 
    // UTF-8 which uses less memory and is what WebKit clients might expect.

    // This pattern of using UTF-8 and then falling back to Latin1 on failure matches URL::createCFString with the
    // major differnce being that URL does not do a UTF-8 conversion and instead chops off the high bits of the UTF-16
    // character sequence.

    CString buffer = toImpl(URLRef)->string().utf8();
    CFURLRef result = CFURLCreateAbsoluteURLWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(buffer.data()), buffer.length(), kCFStringEncodingUTF8, 0, true);
    if (!result)
        result = CFURLCreateAbsoluteURLWithBytes(kCFAllocatorDefault, reinterpret_cast<const UInt8*>(buffer.data()), buffer.length(), kCFStringEncodingISOLatin1, 0, true);
    return result;
}
