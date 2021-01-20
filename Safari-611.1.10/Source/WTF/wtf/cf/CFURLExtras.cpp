/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
#include <wtf/cf/CFURLExtras.h>

#include <wtf/URL.h>
#include <wtf/text/CString.h>

namespace WTF {

void getURLBytes(CFURLRef url, URLCharBuffer& result)
{
    CFIndex bytesLength = CFURLGetBytes(url, nullptr, 0);
    result.resize(bytesLength);
    CFIndex finalLength = CFURLGetBytes(url, reinterpret_cast<UInt8*>(result.data()), bytesLength);
    ASSERT_UNUSED(finalLength, finalLength == bytesLength);
}

void getURLBytes(CFURLRef url, CString& result)
{
    CFIndex bytesLength = CFURLGetBytes(url, nullptr, 0);
    char* bytes;
    result = CString::newUninitialized(bytesLength, bytes);
    CFIndex finalLength = CFURLGetBytes(url, reinterpret_cast<UInt8*>(bytes), bytesLength);
    ASSERT_UNUSED(finalLength, finalLength == bytesLength);
}

bool isCFURLSameOrigin(CFURLRef cfURL, const URL& url)
{
    ASSERT(url.protocolIsInHTTPFamily());

    if (url.hasCredentials())
        return protocolHostAndPortAreEqual(url, URL { cfURL });

    URLCharBuffer bytes;
    getURLBytes(cfURL, bytes);
    StringView cfURLString { reinterpret_cast<const LChar*>(bytes.data()), static_cast<unsigned>(bytes.size()) };

    if (!url.hasPath())
        return StringView { url.string() } == cfURLString;

    auto urlWithoutPath = StringView { url.string() }.substring(0, url.pathStart() + 1);
    return cfURLString.startsWith(urlWithoutPath);
}

}
