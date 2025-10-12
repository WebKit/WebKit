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
#import "WKStringCF.h"

#import "WKAPICast.h"
#import "WKNSString.h"
#import <objc/runtime.h>
#import <wtf/text/WTFString.h>

static inline Class wkNSStringClassSingleton()
{
    static dispatch_once_t once;
    static Class wkNSStringClass;
    dispatch_once(&once, ^{
        wkNSStringClass = [WKNSString class];
    });
    return wkNSStringClass;
}

WKStringRef WKStringCreateWithCFString(CFStringRef cfString)
{
    // Since WKNSString is an internal class with no subclasses, we can do a simple equality check.
    if (object_getClass((__bridge NSString *)cfString) == wkNSStringClassSingleton())
        return WebKit::toAPI(RefPtr { downcast<API::String>(&[(WKNSString *)(__bridge NSString *)CFRetain(cfString) _apiObject]) }.get());
    String string(cfString);
    return WebKit::toCopiedAPI(string);
}

CFStringRef WKStringCopyCFString(CFAllocatorRef allocatorRef, WKStringRef stringRef)
{
    RefPtr apiString = WebKit::toImpl(stringRef);
    ASSERT(!apiString->string().isNull());

    auto string = apiString->string();

    // NOTE: This does not use StringImpl::createCFString() since that function
    // expects to be called on the thread running WebCore.
    if (string.is8Bit()) {
        auto characters = string.span8();
        return CFStringCreateWithBytes(allocatorRef, byteCast<UInt8>(characters.data()), characters.size(), kCFStringEncodingISOLatin1, true);
    }
    auto characters = string.span16();
    return CFStringCreateWithCharacters(allocatorRef, reinterpret_cast<const UniChar*>(characters.data()), characters.size());
}
