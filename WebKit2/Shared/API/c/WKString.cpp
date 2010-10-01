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

#include "WKString.h"

#include "WKAPICast.h"

using namespace WebKit;

WKTypeID WKStringGetTypeID()
{
    return toRef(WebString::APIType);
}

WKStringRef WKStringCreateWithUTF8CString(const char* string)
{
    RefPtr<WebString> webString = WebString::createFromUTF8String(string);
    return toRef(webString.release().leakRef());
}

bool WKStringIsEmpty(WKStringRef stringRef)
{
    return toWK(stringRef)->isEmpty();
}

size_t WKStringGetMaximumUTF8CStringSize(WKStringRef stringRef)
{
    return toWK(stringRef)->maximumUTF8CStringSize();
}

size_t WKStringGetUTF8CString(WKStringRef stringRef, char* buffer, size_t bufferSize)
{
    return toWK(stringRef)->getUTF8CString(buffer, bufferSize);
}

bool WKStringIsEqual(WKStringRef aRef, WKStringRef bRef)
{
    return toWK(aRef)->equal(toWK(bRef));
}

bool WKStringIsEqualToUTF8CString(WKStringRef aRef, const char* b)
{
    return toWK(aRef)->equalToUTF8String(b);
}
