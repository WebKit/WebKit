/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
#include "ResourceResponseCFNet.h"

#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include "ResourceResponse.h"
#include <CFNetwork/CFURLResponsePriv.h>
#include <wtf/RetainPtr.h>

using namespace std;

// We would like a better value for a maximum time_t,
// but there is no way to do that in C with any certainty.
// INT_MAX should work well enough for our purposes.
#define MAX_TIME_T ((time_t)INT_MAX)    

namespace WebCore {

CFURLResponseRef ResourceResponse::cfURLResponse() const
{  
    return m_cfResponse.get();
}

static inline bool filenameHasSaneExtension(const String& filename)
{
    int dot = filename.find('.');

    // The dot can't be the first or last character in the filename.
    int length = filename.length();
    return dot > 0 && dot < length - 1;
}

static time_t toTimeT(CFAbsoluteTime time)
{
    static const double maxTimeAsDouble = std::numeric_limits<time_t>::max();
    static const double minTimeAsDouble = std::numeric_limits<time_t>::min();
    return min(max(minTimeAsDouble, time + kCFAbsoluteTimeIntervalSince1970), maxTimeAsDouble);
}

void ResourceResponse::doUpdateResourceResponse()
{
    if (!m_cfResponse.get())
        return;

    // FIXME: We may need to do MIME type sniffing here (unless that is done in CFURLResponseGetMIMEType).

    m_url = CFURLResponseGetURL(m_cfResponse.get());
    m_mimeType = CFURLResponseGetMIMEType(m_cfResponse.get());
    m_expectedContentLength = CFURLResponseGetExpectedContentLength(m_cfResponse.get());
    m_textEncodingName = CFURLResponseGetTextEncodingName(m_cfResponse.get());

    m_expirationDate = toTimeT(CFURLResponseGetExpirationTime(m_cfResponse.get()));
    m_lastModifiedDate = toTimeT(CFURLResponseGetLastModifiedDate(m_cfResponse.get()));

    RetainPtr<CFStringRef> suggestedFilename(AdoptCF, CFURLResponseCopySuggestedFilename(m_cfResponse.get()));
    m_suggestedFilename = suggestedFilename.get();

    CFHTTPMessageRef httpResponse = CFURLResponseGetHTTPResponse(m_cfResponse.get());
    if (httpResponse) {
        m_httpStatusCode = CFHTTPMessageGetResponseStatusCode(httpResponse);

        RetainPtr<CFStringRef> statusLine(AdoptCF, CFHTTPMessageCopyResponseStatusLine(httpResponse));
        String statusText(statusLine.get());
        int spacePos = statusText.find(" ");
        if (spacePos != -1)
            statusText = statusText.substring(spacePos + 1);
        m_httpStatusText = statusText;

        RetainPtr<CFDictionaryRef> headers(AdoptCF, CFHTTPMessageCopyAllHeaderFields(httpResponse));
        CFIndex headerCount = CFDictionaryGetCount(headers.get());
        Vector<const void*, 128> keys(headerCount);
        Vector<const void*, 128> values(headerCount);
        CFDictionaryGetKeysAndValues(headers.get(), keys.data(), values.data());
        for (int i = 0; i < headerCount; ++i)
            m_httpHeaderFields.set((CFStringRef)keys[i], (CFStringRef)values[i]);
    } else
        m_httpStatusCode = 0;

    // FIXME: This is a workaround for <rdar://problem/5695848>. This code should be removed once that bug is fixed.
    if (m_url.isLocalFile() && ( m_mimeType == "text/html" || m_mimeType == "text/plain" )) {
        const String& path = m_url.path();
        static const String xhtmlExt(".xhtml");
        static const String htmlExt(".html");
        static const String xmlExt(".xml");
        static const String svgExt(".svg");
        static const String jsExt(".js");
        static const String cssExt(".css");
        if (path.endsWith(xhtmlExt, false))
            m_mimeType = "application/xhtml+xml";
        else if (path.endsWith(htmlExt, false))
            m_mimeType = "text/html";
        else if (path.endsWith(xmlExt, false))
            m_mimeType = "text/xml";
        else if (path.endsWith(jsExt, false))
            m_mimeType = "text/javascript";
        else if (path.endsWith(svgExt, false))
            m_mimeType = "image/svg+xml";
        else if (path.endsWith(cssExt, false))
            m_mimeType = "text/css";
    }
}

}
