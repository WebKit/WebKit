/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#pragma once

#include "StringFunctions.h"
#include <JavaScriptCore/RegularExpression.h>
#include <WebKit/WKRetainPtr.h>
#include <WebKit/WKURL.h>

namespace WTR {

inline bool isLocalHost(WKStringRef host)
{
    return WKStringIsEqualToUTF8CString(host, "127.0.0.1") || WKStringIsEqualToUTF8CString(host, "localhost");
}

inline bool isHTTPOrHTTPSScheme(WKStringRef scheme)
{
    return WKStringIsEqualToUTF8CStringIgnoringCase(scheme, "http") || WKStringIsEqualToUTF8CStringIgnoringCase(scheme, "https");
}

inline bool isExternalURLBlockable(WKStringRef host, WKStringRef scheme)
{
    return host && !WKStringIsEmpty(host)
        && isHTTPOrHTTPSScheme(scheme)
        && !WKStringIsEqualToUTF8CString(host, "255.255.255.255")
        && !isLocalHost(host);
}

inline bool isMainFrameURLExternal(WKURLRef mainFrameURL)
{
    if (!mainFrameURL)
        return false;
    auto host = adoptWK(WKURLCopyHostName(mainFrameURL));
    auto scheme = adoptWK(WKURLCopyScheme(mainFrameURL));
    return isHTTPOrHTTPSScheme(scheme.get()) && !isLocalHost(host.get());
}

inline WTF::String sanitizeExternalURL(WKStringRef urlString)
{
    auto result = toWTFString(urlString);
    replace(result, JSC::Yarr::RegularExpression("\\?key=[-0123456789abcdefABCDEF]+"_s), "?key=GENERATED_KEY"_s);
    replace(result, JSC::Yarr::RegularExpression("&key=[-0123456789abcdefABCDEF]+"_s), "&key=GENERATED_KEY"_s);
    replace(result, JSC::Yarr::RegularExpression("%3Fkey%3D[-0123456789abcdefABCDEF]+"_s), "%3Fkey%3DGENERATED_KEY"_s);
    replace(result, JSC::Yarr::RegularExpression("%26key%3D[-0123456789abcdefABCDEF]+"_s), "%26key%3DGENERATED_KEY"_s);
    replace(result, JSC::Yarr::RegularExpression("%253Fkey%253D[-0123456789abcdefABCDEF]+"_s), "%253Fkey%253DGENERATED_KEY"_s);
    replace(result, JSC::Yarr::RegularExpression("%2526key%253D[-0123456789abcdefABCDEF]+"_s), "%2526key%253DGENERATED_KEY"_s);
    replace(result, JSC::Yarr::RegularExpression("reportID=[-0123456789abcdefABCDEF]+"_s), "reportID=GENERATED_REPORT_ID"_s);
    return result;
}

} // namespace WTR
