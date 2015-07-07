/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2013 University of Szeged. All rights reserved.
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

#ifndef RemoteNetworkingContext_h
#define RemoteNetworkingContext_h

#include <WebCore/NetworkingContext.h>
#include <WebCore/SessionID.h>

namespace WebKit {

class RemoteNetworkingContext final : public WebCore::NetworkingContext {
public:
    static PassRefPtr<RemoteNetworkingContext> create(WebCore::SessionID sessionID, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
    {
        return adoptRef(new RemoteNetworkingContext(sessionID, shouldClearReferrerOnHTTPSToHTTPRedirect));
    }
    virtual ~RemoteNetworkingContext();

    // FIXME: Remove platform-specific code and use SessionTracker.
    static void ensurePrivateBrowsingSession(WebCore::SessionID);

    virtual bool shouldClearReferrerOnHTTPSToHTTPRedirect() const override { return m_shouldClearReferrerOnHTTPSToHTTPRedirect; }

private:
    RemoteNetworkingContext(WebCore::SessionID sessionID, bool shouldClearReferrerOnHTTPSToHTTPRedirect)
        : m_sessionID(sessionID)
        , m_shouldClearReferrerOnHTTPSToHTTPRedirect(shouldClearReferrerOnHTTPSToHTTPRedirect)
#if PLATFORM(COCOA)
        , m_needsSiteSpecificQuirks(false)
        , m_localFileContentSniffingEnabled(false)
#endif
    { }

    virtual bool isValid() const override;
    virtual WebCore::NetworkStorageSession& storageSession() const override;

#if PLATFORM(COCOA)
    void setNeedsSiteSpecificQuirks(bool value) { m_needsSiteSpecificQuirks = value; }
    virtual bool needsSiteSpecificQuirks() const override;
    void setLocalFileContentSniffingEnabled(bool value) { m_localFileContentSniffingEnabled = value; }
    virtual bool localFileContentSniffingEnabled() const override;
    virtual RetainPtr<CFDataRef> sourceApplicationAuditData() const override;
    virtual String sourceApplicationIdentifier() const override;
    virtual WebCore::ResourceError blockedError(const WebCore::ResourceRequest&) const override;
#endif

    WebCore::SessionID m_sessionID;
    bool m_shouldClearReferrerOnHTTPSToHTTPRedirect;

#if PLATFORM(COCOA)
    bool m_needsSiteSpecificQuirks;
    bool m_localFileContentSniffingEnabled;
#endif
};

}

#endif // RemoteNetworkingContext_h
