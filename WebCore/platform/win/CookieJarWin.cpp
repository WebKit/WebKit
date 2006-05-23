/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "KURL.h"
#include "PlatformString.h"
#include "DeprecatedString.h"
#include <windows.h>
#include <Wininet.h>

namespace WebCore
{

void setCookies(KURL const& url,KURL const& policyURL, String const& value)
{
    // FIXME: Deal with the policy URL.
    DeprecatedString str = url.url();
    str.append((UChar)'\0');
    DeprecatedString val = value.deprecatedString();
    val.append((UChar)'\0');
    InternetSetCookie((UChar*)str.unicode(), 0, (UChar*)val.unicode());
}

String cookies(KURL const& url)
{
    DeprecatedString str = url.url();
    str.append((UChar)'\0');

    DWORD count;
    InternetGetCookie((UChar*)str.unicode(), 0, 0, &count);
    if (count <= 1) // Null terminator counts as 1.
        return String();

    UChar* buffer = new UChar[count];
    InternetGetCookie((UChar*)str.unicode(), 0, buffer, &count);
    String& result = String(buffer, count-1); // Ignore the null terminator.
    delete buffer;
    return result;
}

bool cookiesEnabled()
{
    // FIXME: For now just assume cookies are always on.
    return true;
}

}
