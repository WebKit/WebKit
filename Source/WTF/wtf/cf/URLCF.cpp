/*
 * Copyright (C) 2004, 2008, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include <wtf/URL.h>

#include <CoreFoundation/CFURL.h>
#include <wtf/URLParser.h>
#include <wtf/cf/CFURLExtras.h>
#include <wtf/cocoa/RuntimeApplicationChecksCocoa.h>
#include <wtf/text/CString.h>

namespace WTF {

URL::URL(CFURLRef url)
{
    // FIXME: Why is it OK to ignore the base URL in the CFURL here?
    if (!url)
        invalidate();
    else
        *this = URLParser(bytesAsString(url)).result();
}

RetainPtr<CFURLRef> URL::createCFURL(const String& string)
{
    if (string.is8Bit() && string.containsOnlyASCII()) [[likely]] {
        auto characters = string.span8();
        return adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, byteCast<UInt8>(characters.data()), characters.size(), kCFStringEncodingUTF8, nullptr, true));
    }
    CString utf8 = string.utf8();
    auto utf8Span = utf8.span();
    return adoptCF(CFURLCreateAbsoluteURLWithBytes(nullptr, byteCast<UInt8>(utf8Span.data()), utf8Span.size(), kCFStringEncodingUTF8, nullptr, true));
}

RetainPtr<CFURLRef> URL::createCFURL() const
{
    if (isNull())
        return nullptr;

    if (isEmpty())
        return emptyCFURL();

    if (!isValid() && linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ConvertsInvalidURLsToNull))
        return nullptr;

    RetainPtr result = createCFURL(m_string);

    // This additional check is only needed for invalid URLs, for which we've already returned null with new SDKs.
    if (!linkedOnOrAfterSDKWithBehavior(SDKAlignedBehavior::ConvertsInvalidURLsToNull)
        && protocolIsInHTTPFamily()
        && !isSameOrigin(result.get(), *this))
        return nullptr;

    return result;
}

String URL::fileSystemPath() const
{
    auto cfURL = createCFURL();
    if (!cfURL)
        return String();

    return adoptCF(CFURLCopyFileSystemPath(cfURL.get(), kCFURLPOSIXPathStyle)).get();
}

}
