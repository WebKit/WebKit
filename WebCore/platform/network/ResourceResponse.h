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

#ifndef ResourceResponse_h
#define ResourceResponse_h

#include "HTTPHeaderMap.h"
#include "KURL.h"

#if PLATFORM(MAC)
#include <wtf/RetainPtr.h>
#ifdef __OBJC__
@class NSURLResponse;
#else
class NSURLResponse;
#endif
#elif USE(CFNETWORK)
#include <wtf/RetainPtr.h>
typedef struct _CFURLResponse* CFURLResponseRef;
#endif

namespace WebCore {

class ResourceResponse {
public:

    ResourceResponse() 
        : m_expectedContentLength(0)
        , m_httpStatusCode(0)
        , m_expirationDate(0)
        , m_isUpToDate(true)
        , m_isNull(true)
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
        , m_isUpToDate(true)
        , m_isNull(false)
    {
    }
 
    bool isNull() const { return m_isNull; }
    bool isHTTP() const;
    
    const KURL& url() const;
    void setUrl(const KURL& url);

    const String& mimeType() const;
    void setMimeType(const String& mimeType);

    long long expectedContentLength() const;
    void setExpectedContentLength(long long expectedContentLength);

    const String& textEncodingName() const;
    void setTextEncodingName(const String& name);

    // FIXME should compute this on the fly
    const String& suggestedFilename() const;
    void setSuggestedFilename(const String&);

    int httpStatusCode() const;
    void setHTTPStatusCode(int);
    
    const String& httpStatusText() const;
    void setHTTPStatusText(const String&);
    
    String httpHeaderField(const String& name) const;
    void setHTTPHeaderField(const String& name, const String& value);
    const HTTPHeaderMap& httpHeaderFields() const;

    bool isMultipart() const { return mimeType() == "multipart/x-mixed-replace"; }

    bool isAttachment() const;

    void setExpirationDate(time_t);
    time_t expirationDate() const;

    void setLastModifiedDate(time_t);
    time_t lastModifiedDate() const;

#if PLATFORM(MAC)
    ResourceResponse(NSURLResponse* nsResponse)
        : m_isUpToDate(false)
        , m_isNull(!nsResponse)
        , m_nsResponse(nsResponse) { }
    
    NSURLResponse *nsURLResponse() const;
#elif USE(CFNETWORK)
    ResourceResponse(CFURLResponseRef cfResponse)
        : m_isUpToDate(false)
        , m_isNull(!cfResponse)
        , m_cfResponse(cfResponse) { }
    
    CFURLResponseRef cfURLResponse() const;
#endif
    
 private:
    void updateResourceResponse() const; 
    
#if PLATFORM(MAC) || USE(CFNETWORK)
    void doUpdateResourceResponse();
#endif
    
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
    mutable bool m_isUpToDate;
    bool m_isNull;
#if PLATFORM(MAC)
    RetainPtr<NSURLResponse> m_nsResponse;
#elif USE(CFNETWORK)
    RetainPtr<CFURLResponseRef> m_cfResponse;      
#endif
    
};

bool operator==(const ResourceResponse& a, const ResourceResponse& b);
inline bool operator!=(const ResourceResponse& a, const ResourceResponse& b) { return !(a == b); }

} // namespace WebCore

#endif // ResourceResponse_h
