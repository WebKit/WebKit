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
#include "WKURL.h"

#include "WKAPICast.h"

WKTypeID WKURLGetTypeID()
{
    return WebKit::toAPI(API::URL::APIType);
}

WKURLRef WKURLCreateWithUTF8CString(const char* string)
{
    return WebKit::toAPILeakingRef(API::URL::create(String::fromUTF8(string)));
}

WKURLRef WKURLCreateWithUTF8String(const char* string, size_t length)
{
    return WebKit::toAPILeakingRef(API::URL::create(String::fromUTF8(unsafeMakeSpan(string, length))));
}

WKURLRef WKURLCreateWithBaseURL(WKURLRef baseURL, const char* relative)
{
    return WebKit::toAPILeakingRef(API::URL::create(protect(WebKit::toImpl(baseURL)).get(), String::fromUTF8(relative)));
}

WKStringRef WKURLCopyString(WKURLRef url)
{
    return WebKit::toCopiedAPI(WebKit::toImpl(url)->string());
}

bool WKURLIsEqual(WKURLRef a, WKURLRef b)
{
    return API::URL::equals(*protect(WebKit::toImpl(a)), *protect(WebKit::toImpl(b)));
}

WKStringRef WKURLCopyHostName(WKURLRef url)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(url))->host());
}

WKStringRef WKURLCopyScheme(WKURLRef url)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(url))->protocol());
}

WK_EXPORT WKStringRef WKURLCopyPath(WKURLRef url)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(url))->path());
}

WKStringRef WKURLCopyLastPathComponent(WKURLRef url)
{
    return WebKit::toCopiedAPI(protect(WebKit::toImpl(url))->lastPathComponent());
}
