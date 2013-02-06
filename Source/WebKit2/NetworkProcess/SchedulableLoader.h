/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef SchedulableLoader_h
#define SchedulableLoader_h

#include "HostRecord.h"
#include "NetworkConnectionToWebProcess.h"
#include "NetworkResourceLoadParameters.h"
#include <wtf/MainThread.h>
#include <wtf/RefCounted.h>

#if ENABLE(NETWORK_PROCESS)

namespace WebKit {

class SchedulableLoader : public RefCounted<SchedulableLoader> {
public:
    virtual ~SchedulableLoader();

    ResourceLoadIdentifier identifier() const { return m_identifier; }
    uint64_t webPageID() const { return m_webPageID; }
    uint64_t webFrameID() const { return m_webFrameID; }
    const WebCore::ResourceRequest& request() const { return m_request; }
    WebCore::ResourceLoadPriority priority() const { return m_priority; }
    WebCore::ContentSniffingPolicy contentSniffingPolicy() const { return m_contentSniffingPolicy; }
    WebCore::StoredCredentials allowStoredCredentials() const { return m_allowStoredCredentials; }
    bool inPrivateBrowsingMode() const { return m_inPrivateBrowsingMode; }

    NetworkConnectionToWebProcess* connectionToWebProcess() const { return m_connection.get(); }
    void connectionToWebProcessDidClose();

    virtual void start() = 0;
    
    virtual bool isSynchronous() { return false; }

    void setHostRecord(HostRecord* hostRecord) { ASSERT(isMainThread()); m_hostRecord = hostRecord; }
    HostRecord* hostRecord() const { ASSERT(isMainThread()); return m_hostRecord.get(); }

protected:
    SchedulableLoader(const NetworkResourceLoadParameters&, NetworkConnectionToWebProcess*);

    void consumeSandboxExtensions();
    void invalidateSandboxExtensions();

    bool shouldClearReferrerOnHTTPSToHTTPRedirect() const { return m_shouldClearReferrerOnHTTPSToHTTPRedirect; }

private:
    ResourceLoadIdentifier m_identifier;
    uint64_t m_webPageID;
    uint64_t m_webFrameID;
    WebCore::ResourceRequest m_request;
    WebCore::ResourceLoadPriority m_priority;
    WebCore::ContentSniffingPolicy m_contentSniffingPolicy;
    WebCore::StoredCredentials m_allowStoredCredentials;
    bool m_inPrivateBrowsingMode;

    Vector<RefPtr<SandboxExtension> > m_requestBodySandboxExtensions;
    RefPtr<SandboxExtension> m_resourceSandboxExtension;

    RefPtr<NetworkConnectionToWebProcess> m_connection;
    
    RefPtr<HostRecord> m_hostRecord;

    bool m_shouldClearReferrerOnHTTPSToHTTPRedirect;
};

} // namespace WebKit

#endif // ENABLE(NETWORK_PROCESS)

#endif // SchedulableLoader_h
