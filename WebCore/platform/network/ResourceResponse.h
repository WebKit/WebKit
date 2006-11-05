// -*- mode: c++; c-basic-offset: 4 -*-
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

#ifndef ResourceResponse_h_
#define ResourceResponse_h_

#include "HTTPHeaderMap.h"
#include "KURL.h"

namespace WebCore {

class ResourceResponse {
public:

    ResourceResponse() 
        : m_expectedContentLength(0)
        , m_httpStatusCode(0)
        , m_expirationDate(0) 
    {
    }

    ResourceResponse(const KURL& url, const String& mimeType, long long expectedLength, const String& textEncodingName, const String& filename)
        : m_url(url)
        , m_mimeType(mimeType)
        , m_expectedContentLength(expectedLength)
        , m_textEncodingName(textEncodingName)
        , m_suggestedFilename(filename)
        , m_httpStatusCode(0)
        , m_expirationDate(0)
    {
    }
 
    const KURL& url() const { return m_url; }
    const String& mimeType() const { return m_mimeType; }
    long long expectedContentLength() const { return m_expectedContentLength; }
    const String& textEncodingName() const { return m_textEncodingName; }

    // FIXME should compute this on the fly
    const String& suggestedFilename() const { return m_suggestedFilename; }

    int httpStatusCode() const { return m_httpStatusCode; }
    void setHTTPStatusCode(int statusCode) { m_httpStatusCode = statusCode; }
    const String& httpStatusText() const { return m_httpStatusText; }
    void setHTTPStatusText(const String& statusText) { m_httpStatusText = statusText; }
    String httpHeaderField(const String& name) const { return m_httpHeaderFields.get(name); }
    const HTTPHeaderMap& httpHeaderFields() const { return m_httpHeaderFields; }
    HTTPHeaderMap& httpHeaderFields() { return m_httpHeaderFields; }

    bool isMultipart() const { return m_mimeType == "multipart/x-mixed-replace"; }

    void setExpirationDate(time_t expirationDate) { m_expirationDate = expirationDate; }
    time_t expirationDate() const { return m_expirationDate; }

    void setLastModifiedDate(time_t lastModifiedDate) { m_lastModifiedDate = lastModifiedDate; }
    time_t lastModifiedDate() const { return m_lastModifiedDate; }

 private:
    KURL m_url;
    String m_mimeType;
    long long m_expectedContentLength;
    String m_textEncodingName;
    String m_suggestedFilename;
    int m_httpStatusCode;
    String m_httpStatusText;
    HTTPHeaderMap m_httpHeaderFields;
    time_t m_expirationDate;
    time_t m_lastModifiedDate;
};

} // namespace WebCore

#endif // ResourceResponse_h_
