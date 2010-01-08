/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2008 Google, Inc.
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

#include "CString.h"
#include "NotImplemented.h"
#include "ResourceResponseBase.h"

namespace WebCore {

    class ResourceResponse : public ResourceResponseBase {
    public:
        ResourceResponse()
            : m_isContentFiltered(false)
            , m_appCacheID(0)
            , m_wasFetchedViaSPDY(false)
        {
        }

        ResourceResponse(const KURL& url, const String& mimeType, long long expectedLength, const String& textEncodingName, const String& filename)
            : ResourceResponseBase(url, mimeType, expectedLength, textEncodingName, filename)
            , m_isContentFiltered(false)
            , m_appCacheID(0)
            , m_wasFetchedViaSPDY(false)
        {
        }

        const CString& getSecurityInfo() const { return m_securityInfo; }
        void setSecurityInfo(const CString& securityInfo)
        {
            m_securityInfo = securityInfo;
        }

        bool isContentFiltered() const { return m_isContentFiltered; }
        void setIsContentFiltered(bool isContentFiltered)
        {
            m_isContentFiltered = isContentFiltered;
        }

        long long appCacheID() const { return m_appCacheID; }
        void setAppCacheID(long long id)
        {
            m_appCacheID = id;
        }

        const KURL& appCacheManifestURL() const { return m_appCacheManifestURL; }
        void setAppCacheManifestURL(const KURL& url)
        {
            m_appCacheManifestURL = url;
        }

        bool wasFetchedViaSPDY() const { return m_wasFetchedViaSPDY; }
        void setWasFetchedViaSPDY(bool value)
        {
            m_wasFetchedViaSPDY = value;
        }

    private:
        friend class ResourceResponseBase;

        // An opaque value that contains some information regarding the security of
        // the connection for this request, such as SSL connection info (empty
        // string if not over HTTPS).
        CString m_securityInfo;

        void doUpdateResourceResponse()
        {
            notImplemented();
        }

        // Whether the contents for this response has been altered/blocked (usually
        // for security reasons.
        bool m_isContentFiltered;

        // The id of the appcache this response was retrieved from, or zero if
        // the response was not retrieved from an appcache.
        long long m_appCacheID;

        // The manifest url of the appcache this response was retrieved from, if any.
        // Note: only valid for main resource responses.
        KURL m_appCacheManifestURL;

        bool m_wasFetchedViaSPDY;
    };

} // namespace WebCore

#endif
