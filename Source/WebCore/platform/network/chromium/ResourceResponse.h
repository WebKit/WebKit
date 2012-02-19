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

#include "File.h"
#include "NotImplemented.h"
#include "ResourceResponseBase.h"
#include <wtf/text/CString.h>

namespace WebCore {

    class ResourceResponse : public ResourceResponseBase {
    public:
        class ExtraData : public RefCounted<ExtraData> {
        public:
            virtual ~ExtraData() { }
        };

        ResourceResponse()
            : m_appCacheID(0)
            , m_isMultipartPayload(false)
            , m_wasFetchedViaSPDY(false)
            , m_wasNpnNegotiated(false)
            , m_wasAlternateProtocolAvailable(false)
            , m_wasFetchedViaProxy(false)
            , m_responseTime(0)
            , m_remotePort(0)
        {
        }

        ResourceResponse(const KURL& url, const String& mimeType, long long expectedLength, const String& textEncodingName, const String& filename)
            : ResourceResponseBase(url, mimeType, expectedLength, textEncodingName, filename)
            , m_appCacheID(0)
            , m_isMultipartPayload(false)
            , m_wasFetchedViaSPDY(false)
            , m_wasNpnNegotiated(false)
            , m_wasAlternateProtocolAvailable(false)
            , m_wasFetchedViaProxy(false)
            , m_responseTime(0)
            , m_remotePort(0)
        {
        }

        const CString& getSecurityInfo() const { return m_securityInfo; }
        void setSecurityInfo(const CString& securityInfo) { m_securityInfo = securityInfo; }

        long long appCacheID() const { return m_appCacheID; }
        void setAppCacheID(long long id) { m_appCacheID = id; }

        const KURL& appCacheManifestURL() const { return m_appCacheManifestURL; }
        void setAppCacheManifestURL(const KURL& url) { m_appCacheManifestURL = url; }

        bool wasFetchedViaSPDY() const { return m_wasFetchedViaSPDY; }
        void setWasFetchedViaSPDY(bool value) { m_wasFetchedViaSPDY = value; }

        bool wasNpnNegotiated() const { return m_wasNpnNegotiated; }
        void setWasNpnNegotiated(bool value) { m_wasNpnNegotiated = value; }

        bool wasAlternateProtocolAvailable() const
        {
          return m_wasAlternateProtocolAvailable;
        }
        void setWasAlternateProtocolAvailable(bool value)
        {
          m_wasAlternateProtocolAvailable = value;
        }

        bool wasFetchedViaProxy() const { return m_wasFetchedViaProxy; }
        void setWasFetchedViaProxy(bool value) { m_wasFetchedViaProxy = value; }

        bool isMultipartPayload() const { return m_isMultipartPayload; }
        void setIsMultipartPayload(bool value) { m_isMultipartPayload = value; }

        double responseTime() const { return m_responseTime; }
        void setResponseTime(double responseTime) { m_responseTime = responseTime; }

        const String& remoteIPAddress() const { return m_remoteIPAddress; }
        void setRemoteIPAddress(const String& value) { m_remoteIPAddress = value; }

        unsigned short remotePort() const { return m_remotePort; }
        void setRemotePort(unsigned short value) { m_remotePort = value; }

        const File* downloadedFile() const { return m_downloadedFile.get(); }
        void setDownloadedFile(PassRefPtr<File> downloadedFile) { m_downloadedFile = downloadedFile; }

        // Extra data associated with this response.
        ExtraData* extraData() const { return m_extraData.get(); }
        void setExtraData(PassRefPtr<ExtraData> extraData) { m_extraData = extraData; }

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

        PassOwnPtr<CrossThreadResourceResponseData> doPlatformCopyData(PassOwnPtr<CrossThreadResourceResponseData>) const;
        void doPlatformAdopt(PassOwnPtr<CrossThreadResourceResponseData>);

        // The id of the appcache this response was retrieved from, or zero if
        // the response was not retrieved from an appcache.
        long long m_appCacheID;

        // The manifest url of the appcache this response was retrieved from, if any.
        // Note: only valid for main resource responses.
        KURL m_appCacheManifestURL;

        // Set to true if this is part of a multipart response.
        bool m_isMultipartPayload;

        // Was the resource fetched over SPDY.  See http://dev.chromium.org/spdy
        bool m_wasFetchedViaSPDY;

        // Was the resource fetched over a channel which used TLS/Next-Protocol-Negotiation (also SPDY related).
        bool m_wasNpnNegotiated;

        // Was the resource fetched over a channel which specified "Alternate-Protocol"
        // (e.g.: Alternate-Protocol: 443:npn-spdy/1).
        bool m_wasAlternateProtocolAvailable;

        // Was the resource fetched over an explicit proxy (HTTP, SOCKS, etc).
        bool m_wasFetchedViaProxy;

        // The time at which the response headers were received.  For cached
        // responses, this time could be "far" in the past.
        double m_responseTime;

        // Remote IP address of the socket which fetched this resource.
        String m_remoteIPAddress;

        // Remote port number of the socket which fetched this resource.
        unsigned short m_remotePort;

        // The downloaded file if the load streamed to a file.
        RefPtr<File> m_downloadedFile;

        // ExtraData associated with the response.
        RefPtr<ExtraData> m_extraData;
    };

    struct CrossThreadResourceResponseData : public CrossThreadResourceResponseDataBase {
        long long m_appCacheID;
        KURL m_appCacheManifestURL;
        bool m_isMultipartPayload;
        bool m_wasFetchedViaSPDY;
        bool m_wasNpnNegotiated;
        bool m_wasAlternateProtocolAvailable;
        bool m_wasFetchedViaProxy;
        double m_responseTime;
        String m_remoteIPAddress;
        unsigned short m_remotePort;
        String m_downloadFilePath;
    };

} // namespace WebCore

#endif
