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
#import <objc/runtime.h>
#import <wtf/cf/CFURLExtras.h>

static inline Class wkNSURLClassSingleton()
{
    static dispatch_once_t once;
    static Class wkNSURLClass;
    dispatch_once(&once, ^{
        wkNSURLClass = [WKNSURL class];
    });
    return wkNSURLClass;
}

WKURLRef WKURLCreateWithCFURL(CFURLRef cfURL)
{
    if (!cfURL)
        return nullptr;

    // Since WKNSURL is an internal class with no subclasses, we can do a simple equality check.
    if (object_getClass((__bridge NSURL *)cfURL) == wkNSURLClassSingleton())
        return WebKit::toAPI(RefPtr { downcast<API::URL>(&[(WKNSURL *)(__bridge NSURL *)CFRetain(cfURL) _apiObject]) }.get());

    // FIXME: Why is it OK to ignore the base URL in the CFURL here?
    return WebKit::toCopiedURLAPI(bytesAsString(cfURL));
}

CFURLRef WKURLCopyCFURL(CFAllocatorRef allocatorRef, WKURLRef URLRef)
{
    auto& string = WebKit::toImpl(URLRef)->string();
    if (string.isNull())
        return nullptr;

    // We first create a CString and then create the CFURL from it. This will ensure that the CFURL is stored in 
    // UTF-8 which uses less memory and is what WebKit clients might expect.

    auto buffer = string.utf8();
    auto bufferSpan = buffer.span();
    return CFURLCreateAbsoluteURLWithBytes(nullptr, byteCast<UInt8>(bufferSpan.data()), bufferSpan.size(), kCFStringEncodingUTF8, nullptr, true);
}
