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

using namespace WebCore;
using namespace WebKit;

#if WK_API_ENABLED
static inline Class wkNSStringClass()
{
    static dispatch_once_t once;
    static Class wkNSStringClass;
    dispatch_once(&once, ^{
        wkNSStringClass = [WKNSString class];
    });
    return wkNSStringClass;
}
#endif // WK_API_ENABLED

WKStringRef WKStringCreateWithCFString(CFStringRef cfString)
{
#if WK_API_ENABLED
    // Since WKNSString is an internal class with no subclasses, we can do a simple equality check.
    if (object_getClass((NSString *)cfString) == wkNSStringClass())
        return toAPI(static_cast<API::String*>(&[(WKNSString *)[(NSString *)cfString retain] _apiObject]));
#endif
    String string(cfString);
    return toCopiedAPI(string);
}

CFStringRef WKStringCopyCFString(CFAllocatorRef allocatorRef, WKStringRef stringRef)
{
    ASSERT(!toImpl(stringRef)->string().isNull());

    // NOTE: This does not use StringImpl::createCFString() since that function
    // expects to be called on the thread running WebCore.
    if (toImpl(stringRef)->string().is8Bit())
        return CFStringCreateWithBytes(allocatorRef, reinterpret_cast<const UInt8*>(toImpl(stringRef)->string().characters8()), toImpl(stringRef)->string().length(), kCFStringEncodingISOLatin1, true);
    return CFStringCreateWithCharacters(allocatorRef, reinterpret_cast<const UniChar*>(toImpl(stringRef)->string().characters16()), toImpl(stringRef)->string().length());
}
