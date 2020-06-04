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
#include <wtf/text/CString.h>

namespace WTF {

URL::URL(CFURLRef url)
{
    if (!url) {
        invalidate();
        return;
    }

    // FIXME: Why is it OK to ignore base URL here?
    CString urlBytes;
    getURLBytes(url, urlBytes);
    URLParser parser(urlBytes.data());
    *this = parser.result();
}

#if !USE(FOUNDATION)
RetainPtr<CFURLRef> URL::createCFURL() const
{
    RetainPtr<CFURLRef> cfURL;
    if (LIKELY(m_string.is8Bit()))
        cfURL = WTF::createCFURLFromBuffer(reinterpret_cast<const char*>(m_string.characters8()), m_string.length());
    else {
        CString utf8 = m_string.utf8();
        cfURL = WTF::createCFURLFromBuffer(utf8.data(), utf8.length());
    }

    if (protocolIsInHTTPFamily() && !isCFURLSameOrigin(cfURL.get(), *this))
        return nullptr;

    return cfURL;
}
#endif

String URL::fileSystemPath() const
{
    RetainPtr<CFURLRef> cfURL = createCFURL();
    if (!cfURL)
        return String();

#if PLATFORM(WIN)
    CFURLPathStyle pathStyle = kCFURLWindowsPathStyle;
#else
    CFURLPathStyle pathStyle = kCFURLPOSIXPathStyle;
#endif
    return adoptCF(CFURLCopyFileSystemPath(cfURL.get(), pathStyle)).get();
}

}
