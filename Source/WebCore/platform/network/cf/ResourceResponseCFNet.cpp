/*
 * Copyright (C) 2006-2007, 2016 Apple Inc.  All rights reserved.
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
#include "ResourceResponse.h"

#if USE(CFURLCONNECTION)

#include <pal/spi/win/CFNetworkSPIWin.h>

#include "HTTPParsers.h"
#include "MIMETypeRegistry.h"
#include <wtf/RetainPtr.h>

namespace WebCore {

static CFStringRef const commonHeaderFields[] = {
    CFSTR("Age"), CFSTR("Cache-Control"), CFSTR("Content-Type"), CFSTR("Date"), CFSTR("Etag"), CFSTR("Expires"), CFSTR("Last-Modified"), CFSTR("Pragma")
};

CFURLResponseRef ResourceResponse::cfURLResponse() const
{
    if (!m_cfResponse && !m_isNull) {
        RetainPtr<CFURLRef> url = m_url.createCFURL();
        // FIXME: This creates a very incomplete CFURLResponse, which does not even have a status code.
        m_cfResponse = adoptCF(CFURLResponseCreate(0, url.get(), m_mimeType.createCFString().get(), m_expectedContentLength, m_textEncodingName.createCFString().get(), kCFURLCacheStorageAllowed));
    }

    return m_cfResponse.get();
}

static void addToHTTPHeaderMap(const void* key, const void* value, void* context)
{
    HTTPHeaderMap* httpHeaderMap = (HTTPHeaderMap*)context;
    httpHeaderMap->set((CFStringRef)key, (CFStringRef)value);
}

void ResourceResponse::platformLazyInit(InitLevel initLevel)
{
    if (m_initLevel > initLevel)
        return;

    if (m_isNull || !m_cfResponse.get())
        return;

    if (m_initLevel < CommonFieldsOnly && initLevel >= CommonFieldsOnly) {
        m_url = CFURLResponseGetURL(m_cfResponse.get());
        m_mimeType = CFURLResponseGetMIMEType(m_cfResponse.get());
        m_expectedContentLength = CFURLResponseGetExpectedContentLength(m_cfResponse.get());
        m_textEncodingName = CFURLResponseGetTextEncodingName(m_cfResponse.get());

        // Workaround for <rdar://problem/8757088>, can be removed once that is fixed.
        unsigned textEncodingNameLength = m_textEncodingName.length();
        if (textEncodingNameLength >= 2 && m_textEncodingName[0U] == '"' && m_textEncodingName[textEncodingNameLength - 1] == '"')
            m_textEncodingName = m_textEncodingName.substring(1, textEncodingNameLength - 2);

        CFHTTPMessageRef httpResponse = CFURLResponseGetHTTPResponse(m_cfResponse.get());
        if (httpResponse) {
            m_httpStatusCode = CFHTTPMessageGetResponseStatusCode(httpResponse);
            
            if (initLevel < AllFields) {
                RetainPtr<CFDictionaryRef> headers = adoptCF(CFHTTPMessageCopyAllHeaderFields(httpResponse));
                for (auto& commonHeader : commonHeaderFields) {
                    CFStringRef value;
                    if (CFDictionaryGetValueIfPresent(headers.get(), commonHeader, (const void **)&value))
                        m_httpHeaderFields.set(commonHeader, value);
                }
            }
        } else
            m_httpStatusCode = 0;
    }

    if (m_initLevel < AllFields && initLevel == AllFields) {
        CFHTTPMessageRef httpResponse = CFURLResponseGetHTTPResponse(m_cfResponse.get());
        if (httpResponse) {
            m_httpVersion = AtomString { String(adoptCF(CFHTTPMessageCopyVersion(httpResponse)).get()).convertToASCIIUppercase() };

            RetainPtr<CFStringRef> statusLine = adoptCF(CFHTTPMessageCopyResponseStatusLine(httpResponse));
            m_httpStatusText = extractReasonPhraseFromHTTPStatusLine(statusLine.get());

            RetainPtr<CFDictionaryRef> headers = adoptCF(CFHTTPMessageCopyAllHeaderFields(httpResponse));
            CFDictionaryApplyFunction(headers.get(), addToHTTPHeaderMap, &m_httpHeaderFields);
        }
    }

    m_initLevel = initLevel;
}

CertificateInfo ResourceResponse::platformCertificateInfo() const
{
    return { };
}

String ResourceResponse::platformSuggestedFilename() const
{
    if (!cfURLResponse())
        return String();
    RetainPtr<CFStringRef> suggestedFilename = adoptCF(CFURLResponseCopySuggestedFilename(cfURLResponse()));
    return suggestedFilename.get();
}

bool ResourceResponse::platformCompare(const ResourceResponse& a, const ResourceResponse& b)
{
    // CFEqual crashes if you pass it 0 so do an early check before calling it.
    if (!a.cfURLResponse() || !b.cfURLResponse())
        return a.cfURLResponse() == b.cfURLResponse();
    return CFEqual(a.cfURLResponse(), b.cfURLResponse());
}


} // namespace WebCore

#endif // USE(CFURLCONNECTION)
